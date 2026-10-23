# The game SDK

This directory holds everything a mod needs to reach Phasmophobia without a raw address or field
offset in its code, and everything a person needs to add to that.

Terms used throughout:

- **Game SDK**: the whole kit, this project and the `sdk/` directory. It holds the game API, the
  behaviour notes and overviews, the fake backend for tests, the offline check and the reverse
  engineering (RE) tooling that reads the map.
- **Game API**: what a mod calls. It lives in the namespace `phasmo` (`phasmo::GhostAI::UnAppear`).
- **Game map**: the declaration of every mapped game type, method, field, enum value and inlined code
  place. The headers in `sdk/map` are the map.
- **Host contract**: `core/mod_api.h`, the contract between `ModHost.dll` and each mod DLL.

The audited game is Steam build 25811160. Notes marked as read from decompiled code are from that
build.

## What exists today, and what does not

Exists: the map and its resolver (at startup in the game, and offline), the game API, host contract
2.1 with its runtime services and static fields, hooks by entry, the fake backend,
`tools/gamecheck.ps1`, and four mapped types (GhostAI with 15 entries, Unity's Camera with 8,
LevelController with its static instance, Unity's Animator with one icall), plus one GameAssembly
export the host may use (`il2cpp_shutdown`). One mod call goes through the game API: Ghost Tells'
Shade calls `phasmo::GhostAI::UnAppear`.

Does not exist yet. These arrive with stage 2, when every bundled mod is on the game API:

- the downloadable SDK zip with a template mod;
- the mods folder in `%LOCALAPPDATA%\PhasmoMods\mods` for other authors' mods;
- the channel scheme for network messages of other authors' mods;
- the generated game API reference. Until then this README, the headers and `sdk/systems` are the
  documentation, and each header's `///` notes are the reference.

Until then, mods are built inside this repository, as the bundled ones are (see `CMakeLists.txt`, the
`GhostTells` target), and the host loads the mods that sit in a `mods` folder beside it. Everything below
describes what is in the code now.

## Making a mod

### The rules every mod follows

- **Main thread only.** The host calls `pgmod_start`, `pgmod_tick` and `pgmod_stop` on Unity's main
  thread, and the game API runs there too. Do not call it from another thread. Only the host contract's `log` may be
  called from any thread; everything else runs on the main thread.
- **Never keep a raw managed reference across frames.** The garbage collector moves and frees
  managed objects. Keep one in a `pg::api::Held` (`sdk/api/held.h`), which holds it through the host
  contract's `gc_hold`. (The bundled mods still use the host's copy, `il2cpp::Held` in `core/il2cpp.h`,
  as `mods/ghost_tells/shade.cpp` does with its ghost, until stage 2b moves them onto the game API.) `tests/held_scan_test.cpp` fails the build on a raw `void*` kept at namespace
  or struct scope. It scans `core/`, `host/` and the four bundled mod folders by name, so a new mod
  folder must be added to the list in that test.
- **Private lobbies only.** Play with mods in private lobbies only. Playing with mods risks a ban on
  the account. Nothing in the SDK changes that, and `send` already refuses outside a private room.
- **Nothing in the SDK touches the anti-cheat.** `tests/map_test.cpp` rejects an entry naming
  `SelfReport` or `Moderation`.
- **Nothing thrown crosses the host contract.** Only plain pointers, integers and C strings do. A mod
  catches its own exceptions. `pg::mod::guarded_start`, `guarded_tick` and `guarded_stop`
  (`core/mod_glue.h`) do that for the three exports.
- **A struct argument whose size is not 1, 2, 4 or 8 bytes is passed by address**, as the x64 calling
  convention does, and the callee may modify it. In a `phasmo::` method signature write such a
  parameter as a pointer. Pass a copy when the original must stay unchanged.

### The shape of a mod

A mod is a DLL and a `mod.ini` in its own folder, `mods/<folder>/` beside `ModHost.dll` (see
`mods/ghost_tells/`). The DLL links the static library `pgmod` (`core/mod_glue.cpp` and the game API).

`mod.ini` (`core/mod_list.h`) has one `key=value` per line; blank lines and `#` comments are skipped:

- `id=`: 1 to 40 characters of `a-z`, `0-9` and `-`. It must equal the `id` in `PgModInfo`.
- `name=`: shown to the player. `dll=`: the file name of the DLL beside the `mod.ini`, with no folder.
- `description=`: shown in the Mods panel. `requires=`: optional, the id of the one other mod this one
  cannot run without.

Where the host finds mods. When the launcher started the game, the host runs the DLLs the launcher
listed for that process (each copy's manifest sits beside it as `<copy>.ini`). Otherwise it reads every folder in
`mods` beside the host, in name order, and skips a folder with no `mod.ini` silently and a folder whose `mod.ini`
is invalid with a log line that says why. The host loads every mod it finds at once and keeps it loaded. Which mods start is the
player's choice, saved in `launcher.ini` (`enabled=` and the mod ids) and changed from the Mods panel.
A mod with `requires=` starts after that mod, and stays off when that mod is not chosen or did not
start.

The host refuses a DLL that lacks any of `pgmod_info`, `pgmod_start`, `pgmod_tick` or `pgmod_stop`
("is not a mod for this host"), one whose `apiVersion` is not compatible with the host's
(`pg_api_compatible`) or whose `channel` is 0, and one that repeats the id or channel of a mod already
loaded.

`PgModInfo` (`core/mod_api.h`), returned by `pgmod_info`, callable at any time:

| Member | Meaning |
| --- | --- |
| `apiVersion` | `PGMOD_API_VERSION`, the version the mod was built against. |
| `id` | Stable and lower-case, as in `mod.ini`. |
| `name` | Shown to the player. |
| `channel` | The mod's own network channel, 1 to 255, unique among the loaded mods and never 0 (the host's own). Today the author picks it. Stage 2 reserves 1 to 63 for the bundled mods and gives every other mod a channel from 64 to 255 derived from its id. |
| `flags` | `PgModFlags`, below. |

Flags:

- `PG_MOD_VANILLA_PEERS`: players without the mod can share the lobby, because the mod falls back to
  the vanilla game for them. A modded host removes players without the mods unless every mod it runs
  has this flag.
- `PG_MOD_RECORDS_VOICE`: the mod keeps other players' speech in memory during a contract. The consent
  notice then adds a "Voices are recorded." paragraph that names the mod.

The lifecycle, every call on the main thread:

- `pgmod_start(api)`: once, after the player agreed to the consent notice. It returns false to stay
  inactive, and the host then removes the hooks this start created. Call `pg::mod::bind(api, &info)`
  first.
- `pgmod_tick()`: every frame once started.
- `pgmod_stop()`: once, when the host stops the mod (newest started first). The mod may be started
  again later, and its hooks stay until the host removes them.
- `pgmod_requires`: the game entries the mod needs (see "What a mod requires"), in an array the mod
  keeps, unchanged, for as long as its DLL stays loaded.
- `pg::mod::guarded_start`, `guarded_tick` and `guarded_stop` (`core/mod_glue.h`) wrap the three
  bodies so that nothing thrown leaves the mod; `mods/ghost_tells/mod.cpp` shows all of it.

Networking. `send(channel, data, size, to, reliable, actor)` sends a message on the mod's own channel,
at most `PG_MAX_MESSAGE` (511) bytes, `to` being `PG_TO_OTHERS`, `PG_TO_MASTER` or `PG_TO_ACTOR` (with
`actor`). Payloads ride on Photon event 197 behind a one-byte channel. It returns false on the host's
own channel 0 and outside a private room, where the suite behaves as if it were not installed.
`set_receiver(channel, receiver)` registers `void receiver(int32_t sender, const uint8_t* data, size_t
size)`; null stops receiving. `sender` is the sending client's Photon actor number, taken from the
event, never the payload, so check it for any message only the master may send. The host drops the
receivers when it stops. `peer_has_mod(actor, modId)` says whether that client runs the mod.

Services. `register_service(name, table)` publishes a table of C function pointers for other mods and
`service(name)` returns it, or null when nobody registered that name. The table must stay valid for the
process lifetime, so version it in the name (`"ghost-tells.v1"`) and make the caller check for null
(`core/services.h` lists the bundled ones). A name is published once: the host refuses a second
`register_service` of a name already registered, logs it, and keeps the first table, so no mod can
take over another's service. The host forgets the services when it stops.

### Calling the game

A map header ends with the ready-made `phasmo::` wrappers for its type. Include the header and call
them:

```cpp
#include "GhostAI.h"

phasmo::GhostAI::UnAppear(ghost);                          // a method: the object comes first
const bool hunting = phasmo::GhostAI::isHunting.get(ghost);  // a field
const int32_t state = phasmo::GhostAI::state.get(ghost);     // compare it with phasmo::GhostState::idle()
// Change state through the method: it also starts the state's behaviour, which writing `state` skips.
phasmo::GhostAI::ChangeState(ghost, phasmo::GhostState::idle(), nullptr, false);
```

Six kinds of wrapper, from `sdk/api/game_api.h`:

| Wrapper | Declared as | Use |
| --- | --- | --- |
| `pg::api::Method<D, R(A...)>` | `static constexpr ... UnAppear{}` | `UnAppear(ghost)`; `available()` says whether it resolved. For an instance method the first parameter is the object. A call runs the declared body; `virtual_call(ghost, ...)` runs the override the object's class runs, as C# calls a virtual method, and refuses (logged once, nothing called) a null object or one not of the declaring type. |
| `pg::api::Field<D, T>` | `Field<D, int32_t>` | `get(obj)`, `set(obj, v)`. `T` is a plain value or `void*` for a managed reference, whose `set` goes through the GC write barrier (the host's `set_reference`). |
| `pg::api::StaticField<D, T>` | `StaticField<D, void*>` | `get()`, `set(v)`: a static field, one value for the class, read and written by the host (`static_get`, `static_set`). A reference's `set` goes through the GC write barrier. The first access runs the class's static constructor if it has not run, as the game's own first use would, so use it while the game plays (a tick, a detour), never at startup. When the host cannot read it (a `T` of the wrong size, a static constructor that threw) it logs once and gives the default. `T` is a number, a bool, an enum or `void*`: a struct does not compile, since a whole copy would store any reference inside it without the write barrier. |
| `pg::api::Icall<D, R(A...)>` | `static constexpr ... SetBoolID{}` | `SetBoolID(animator, id, value)`: a Unity engine function the runtime registered by its icall name. It is native code, so it takes no MethodInfo. |
| `pg::api::EnumValue<D>` | `idle{}` | `idle()` gives the value's number in this build. |
| `pg::api::Type<D>` | `type{}` | `klass()` gives the `Il2CppClass*`; `is(obj)` tells whether an object is of the type or a derived one (an interface type never matches); `type_object()` gives its `System.Type` (C#'s `typeof`); `make()` a new object, fields zeroed and no constructor run (not for engine objects, which come from `AddComponent` or `Instantiate`); `make_array(n)` a new array of the type. |

Every wrapper has two members besides its calls: `desc`, the map entry it wraps (its key, kind and
flags; `hook<M>` and `inside<M>` read it), and `use`, the entry's `pg::api::Use<D>`, whose `entry()`
and `resolved()` make the entry required (see "What a mod requires").

A wrapper's shape must be its entry's, or it does not compile. Where the entry lists `params`, a
`Method` or `Icall` takes one argument per param, after the object unless the entry is `kStatic`
(a method named without `params` is matched at any arity, so its wrapper is not checked). Where a
field's `type` is a C# value type by name (`bool`, `int`, `float` and the like), a `Field` or
`StaticField` whose `T` has another size does not compile. `tests/compile_fail_test.cmake` builds
wrong wrappers of every kind and expects each to fail with its own `static_assert`.

A call is never virtual: `UnAppear(ghost)` runs the body the map entry names, even when the object's
class overrides it. Use `virtual_call` to run the override.

A managed reference a wrapper gives (a `void*` field or return value, `type_object()`, `make()`,
`make_array()`) is valid for the current frame only. Do not keep it across frames: keep the object
in a `pg::api::Held`.

A call that cannot reach the game never jumps to null. An entry that did not resolve logs
`<key> is not available in this game build` once and returns the type's default (0, false, null) or
does nothing. A field read of a null object gives the default; a write to one does nothing. "Once"
holds across threads: each line's flag is atomic.

GameAssembly's exports (`il2cpp_shutdown`) are not for mods. `pg::api::Export<D>` gives one's
`address()` to the host's own code only: it exists where `PG_HOST` is defined (ModHost), and the host
contract's `game_entry` gives a mod null for one, so a mod that requires an export is never started
(gamecheck's `--mods` reports it as switched off).

### Strings, arrays, lists and engine objects

A mod never reads the runtime's object layouts; the host does, through the host contract's runtime
services. `sdk/api/runtime.h`:

```cpp
const std::string name = pg::api::to_utf8(str);         // a System.String as UTF-8; "" for null
void* text = pg::api::make_string("ടഴദബ");              // a new System.String
const pg::api::Array<int32_t> numbers{arr};              // value elements: numbers.size(), numbers[i]
const pg::api::RefArray objects{arr};                     // references: objects[i], objects.set(i, obj)
const pg::api::List players{list};                        // a List of references: players.size(), players[i]
if (pg::api::alive(camera)) ...                          // UnityEngine.Object's implicit bool
```

An index past the end logs once and touches nothing (a reference reads null, a value element is a
zeroed scratch copy). `RefArray::set` stores through the GC write barrier. These views and the raw
pointers they wrap are valid for the current frame only: keep the object itself in a `pg::api::Held`
and make the view again. An element reference from `Array` and the array `List::items()` gives are
valid only until the next managed call: a List replaces its backing array when it grows, so take
`items()` again after any game call, and never store either. An object of the wrong kind (a list
passed as a string, say) reads nothing: the host logs it once and the service answers 0 or null.
Before `bind` every service answers as for a null object.

### What a mod requires

What a mod requires is what its code calls. Every wrapper member is a template, so an entry is
registered only when code in the mod uses it: only `Use<D>::resolved` names the entry's
registration, so naming a wrapper or `Use<D>` registers nothing (MSVC would otherwise register an
entry wherever its `Use<D>` is named). A call, a field `get` or `set`,
an enum value, `klass()`, `is()`, `type_object()`, `make()`, `make_array()`, `virtual_call`, `available()`, `hook<M>` and `inside<M>` all use it. The mod's
`pgmod_requires` export lists exactly those entries' keys, less the entries whose map entry carries
`kOptional`. That flag belongs to the entry, so it holds for every mod; a mod cannot make an entry
optional for itself. Check an optional entry with `available()` before using it. `pg::mod::bind` (called
first in `pgmod_start`) checks `pg_api_compatible(host, mod)`, looks every used entry up through the
host's `game_entry`, and returns false with the missing keys logged when a required one did not
resolve. The host does not start a mod whose required entries are missing and logs which. A mod
without `pgmod_requires` requires nothing.

Ghost Tells today lists exactly one key, `GhostAI.UnAppear`. Including a header requires nothing by
itself; only the members the mod's code actually uses become required.

### Hooking and caller context

```cpp
// The detour takes the method's own arguments after `original`; the MethodInfo is passed on for it.
void detour(pg::api::Original<phasmo::GhostAI::ChangeState> original, void* ghost, int32_t state, void* interact,
            bool force) {
  original(ghost, state, interact, force);  // the rest of the chain of the body the game called
}
const bool hooked = pg::api::hook<phasmo::GhostAI::ChangeState>(&detour, "my hook", pg::api::Twins::All);
```

`hook<M>` creates a MinHook detour on the method through the host's single MinHook instance. It is
created, not enabled: call `enable_hooks` (or `hooks::enable_all()`). Hooks chain: the newest detour
runs first and its `original` reaches the previous one. A mod hooks a method once; a second `hook<M>`
in the same mod is refused and the first keeps its detour.

The obfuscator makes same-behaviour copies of some methods (twins). With `Twins::All` (the default)
every twin is hooked as well, so the detour runs whichever body the game calls; with
`Twins::CanonicalOnly` only the canonical body is hooked, which is what existing code hooked by raw
address and so what a migration uses. Each body has its own chain, and `original` continues the chain
of the body the call came in through, so hooks of either policy, and raw hooks on one body, coexist on
one method in any order: a hook on the canonical body alone never runs for a call through a twin. The
game API makes one entry function per body for this, up to `pg::api::kMaxBodies` (8); an all-twins
hook on a method with more bodies is refused and logged, with nothing hooked. A twin folded onto the
canonical address is one body, hooked once.

`pg::api::inside<M>()` tells whether a call of M is running on this thread, from a thread-local depth
the host's own hook on M keeps (`inside_entry` in the host contract). It answers only for methods the
host tracks (`PG_TRACK` in `host/context.cpp`). The host tracks none yet, so today it is always false.
It replaces return-address checks, which break when several mods chain detours on one function.

### The host contract, version 2.1

`PGMOD_API_VERSION` is a major number in the high 16 bits and a minor in the low 16 (2.1 today).
`pg_api_compatible(host, mod)` accepts a mod of the same major and a minor no newer than the host's.
Within a major, `PgHostApi` and every published struct only grow at the end; a new major refuses
older mods. Version 2.0 added these members, after the services of version 1:

- `game_entry(key)`: the resolved `PgGameEntry` for a map key (type, method, code, twins, field
  offset, enum value), or null when unknown or unresolved.
- `hook_entry(key, detour, original, name, allTwins)`: a hook by entry with one detour. Since 2.1 it is
  made from per-body hooks: without `allTwins` it hooks body 0, with it every body, and `original`
  receives body 0's chain. So a call that entered through a twin goes on along the canonical body's
  chain, past the twin's own older detours. The game API no longer calls it; `hook_body` replaced it.
- `set_reference(obj, offset, value)`: stores a managed reference through the GC write barrier. The
  only correct way to write a reference field; `Field<D, void*>::set` calls it.
- `inside_entry(key)`: the thread-local depth above.

Version 2.1 added the runtime services, after `inside_entry`. A mod built for 2.0 still loads. Each
runs on the main thread, never throws, and answers null, 0 or false for a null argument and for a key
that is unknown, unresolved or of the wrong kind:

- `string_new(utf8)` and `string_utf8(str, out, size)`: a managed string from UTF-8, and its UTF-8
  text (the whole length is returned, so a caller can size its buffer).
- `array_length(arr)`, `array_data(arr)` and `array_new(elementTypeKey, length)`; `list_count(list)`
  and `list_items(list)`.
- `type_object(typeKey)`, `object_new(typeKey)` and `is_instance(obj, typeKey)` (which follows the
  class's parents).
- `alive(unityObject)`: UnityEngine.Object's implicit bool.
- `virtual_method(obj, methodKey)`: the MethodInfo the object runs for the method; its first member is
  the address of its code.
- `gc_hold(obj)` and `gc_release(handle)`: strong GC handles, which `pg::api::Held` uses.
- `log_exception(where, exception)`: logs a caught managed exception with its stack.
- `static_get(key, out, size)` and `static_set(key, in, size)`, after `log_exception`: a static
  field's value, `size` bytes, read into `out` or written from `in` (a reference through the GC write
  barrier, `in` pointing at the pointer). Each runs the class's static constructor if it has not run
  yet, so the host never calls them while it resolves the map at startup, and a mod calls them only
  while the game plays. False, touching nothing, for a key that is not a resolved static field, a
  `size` other than the field's (checked before the class is initialised, so no static constructor
  runs), or a static constructor that threw (logged once per key).
- `hook_body(key, bodyIndex, detour, original, name)` and `body_count(key)`, after `static_set`: a
  method's bodies are its canonical body (index 0) and each distinct twin, and `hook_body` hooks one of
  them, chained on that body alone. `body_count` is 0 for an entry that is unknown, unresolved or not a
  method. `hook<M>` is built on these.

A mod's exports are `pgmod_info`, `pgmod_start`, `pgmod_tick`, `pgmod_stop` and (version 2 onwards,
exported by linking `pgmod`) `pgmod_requires`.

### Testing without the game

`sdk/fake/fake_game.h` is a host contract in which only the game functions are live. A test says what
each entry resolves to, binds the game API to it and calls the mod's code:

```cpp
pg::fake::reset();
pg::fake::method(pg::map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
std::string missing;
pg::api::bind(pg::fake::host(), missing);
phasmo::GhostAI::UnAppear(ghost);  // runs fake_unappear(ghost, nullptr)
```

The other givers are `field(d, offset)`, `static_field(d, storage, size, reference)`, `icall(d, fn)`,
`value(d, n)` and `type(d, klass)`. A static field reads and writes the test's own storage, and a
`size` other than the one given answers as the host does for a field it cannot reach. An entry not
given stays
unresolved, as in a build that lacks it. `hooked()` lists the keys hooked through `hook_entry` or `hook_body`, and
`barrier_writes()` counts reference stores. A fake object begins with its class pointer;
`parent(klass, parentClass)` declares inheritance, which `Type::is` and `virtual_call` follow, and
`override_method(d, klass, fn)` gives a class its override. `method(d, fn, owner)` names the
declaring class, so `virtual_call` refuses an object of another class. The runtime services work on the fake's
own memory: `array(elementSize, n)` and `list(items)` make arrays and lists, `destroy(obj)` makes
`alive` false, and `held()` counts the GC holds not yet released. `tests/game_api_test.cpp` shows
every path.

### Checking a mod against a game build

`tools/gamecheck.ps1` with `-GameDir` and `-Cpp2Il` resolves the map against a build without
running the game. Add `-Mods <folder>` to learn which mods the host would not start on that build
(see "Commands" below).

## Mapping the game

For whoever adds entries. The rules:

- **A mod never holds a game address or a field offset**, and never reaches the game around the map
  "just this once". A second way in recreates the scattered knowledge the map exists to remove.
  No hex address or field offset is added to `mods/`, `host/` or `core/`.
- **A wrong match that looks right is the worst outcome.** The resolver never picks among
  candidates: zero is `Missing`, more than one distinct behaviour is `Ambiguous`, an entry whose
  dependency did not resolve is `Blocked`.
- **Every RE fact belongs in an entry's `///` note or a system overview (`sdk/systems`), never only in
  an agent's memory note or a chat.** A memory note may point at the SDK; it does not hold the fact.
  `tests/map_test.cpp` fails an entry with no `///` note on the line above it.

### An entry

A map entry is a `pg::sdk::EntryDesc` (`sdk/api/entry.h`) in `sdk/map/<Type>.h`, with its `///` note
on the line above and its key listed in `kAllEntries` in `sdk/map/all.h` (`tests/map_test.cpp` fails
an entry missing from it, and a duplicate key). Every field has a default, so use designated
initialisers and name only what the entry needs:

```cpp
/// Ends an appearance: when the ghost's shown flag is set ... Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_UnAppear{.key = "GhostAI.UnAppear", .kind = pg::sdk::Kind::Method,
    .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "UnAppear"};
```

The fields: `key` (what `game_entry` and `pgmod_requires` use), `kind` (`Type`, `Method`, `Field`,
`EnumValue`, `Code`, `Icall`, `Export`), `confidence`, `flags` (`kStatic`, `kOptional`, `kTrack`;
`kStatic` makes a field a static one; `kTrack` is not read by anything yet), `owner` (the declaring
Type entry's key), `name` or `hint` (an icall's or export's name, as the runtime knows it),
`assembly` and `ns` (for a type; a null `ns` means the global namespace for a top-level type and any
for a nested one), `type` (a field's type or a method's return type), `params`, `fieldTypes`,
`values`, `facts`, and for a Code entry `pattern`, `within`, `patternOffset`. A map header ends with
its `phasmo::` wrappers; add one for each entry a mod may use (an export gets none).

### How the resolver finds an entry

Strategies, most robust first. It accepts an entry only when exactly one candidate survives every
check the entry declares.

1. **By real name** (`name`). Types by assembly, namespace and name; members by name in the owner.
   A method named without `params` matches by name alone at any arity, so an overloaded method is
   `Ambiguous` until `params` is given. A parameter list tells overloads apart.
2. **By shape** (`name` null), for obfuscated members and types: the owner, static or instance, a
   field's type, a method's return and parameter types, a type's `fieldTypes` (each listed type needs
   a field of its own) and an enum's `values` (the value names it must have). The obfuscated `hint`
   from the last mapped build is **only a report**: it never decides. Every candidate is checked, and
   the hint only names what changed (`renamed from ...`). A stale hint that now belongs to a
   wrong-shaped member is ignored. Compiler-generated types (a name starting with `<`, or a field
   named `<>...`, such as a coroutine class) are never shape candidates.
3. **By facts about the machine code**, to separate candidates that shape cannot (GhostAI has 13
   unnamed bools). A `pg::sdk::Fact` has `is` (`Stores`, `StoresConst`, `Loads`, `Calls`, `CalledBy`),
   `method` (the key of the method whose code is read), `value` and `negate`:
   - `Stores`/`Loads`: the fact method (or a twin of it) writes or reads the candidate field. When
     the fact method is an instance method of the field's owner (and gets `this` in rcx), a fact
     counts only an access through `this`; a candidate reached only through another register stays
     undecided, so the entry is `Ambiguous`. In a static method or a method of another type no
     register is known to hold the owner, so a positive fact matches an access at the field's
     offset through any register. Such a cross-owner fact is weak: give the entry a second,
     independent fact as well.
   - `StoresConst`: it stores the constant `value` there with an immediate `mov`.
   - `Calls`: the candidate method's code calls the fact method. `CalledBy`: the fact method calls
     the candidate.
   - `negate` turns a fact around. A negated **field** fact removes a candidate only for an access
     through `this`; an access through any other register leaves it undecided. Negated `Calls` and
     `CalledBy` facts are not supported yet (the reader does not see indirect or virtual calls), and
     an entry with one is `Missing`.
   - Facts **fail closed**: when the code a fact needs cannot be fully read (an instruction it cannot
     decode, a desynchronised stream, data it does not recognise), the candidate stays and cannot be
     accepted, so the entry is `Ambiguous`, never a wrong pick.
   - A field fact on a struct owner is unsupported (a struct's methods address fields without the
     object header, so the displacement would be wrong).
   - A store is keyed by its start offset: a 16-bit store that also clears the neighbouring bool is a
     store at its own start only. `sdk/map/GhostAI.h` explains the one such case.
4. **By code pattern inside one method** (`Kind::Code`), for behaviour that exists only as code
   inlined into another method. `pattern` is hex bytes with `??` as a wildcard, `within` is the key
   of the method entry it is searched in, and it must match exactly once there. `patternOffset` moves
   the result, which must still start an instruction of that method, read in full. A pattern is the
   last resort, and its note says why nothing better works.

Two kinds are found by name outside the metadata, with no owner, shape or facts:

- **An icall** (`Kind::Icall`), a Unity engine function, by its icall name
  (`"UnityEngine.Animator::SetBoolID"`), through the runtime's `il2cpp_resolve_icall` on the main
  thread when the host activates, before any mod starts. The startup worker never asks: the runtime's
  icall table is not locked, and the engine may still be registering icalls then.
  Only the running game registers icalls, so the offline check cannot resolve one: it lists it as
  checked at startup only and never counts it as a failure.
- **An export** (`Kind::Export`), a function GameAssembly.dll exports (`"il2cpp_shutdown"`), from the
  module's export table, in the game and offline alike. A name exported twice, or forwarded to
  another module, is `Missing`. Only the host may use an export: the host contract never gives one to
a mod.

A static field is a `Field` entry flagged `kStatic`, found like any field. The resolver only reads the
metadata; the field's value is read when a mod asks for it, never at startup.

Identity. A type string `"@Key"` means the very type the entry `Key` resolved to, not just its name,
and the same holds in `params` and `fieldTypes`. Where the model says which type a member uses, it
must be that type; where it does not, a name that several types share makes the entry `Ambiguous`.
The runtime spells types as `System.Collections.Generic.List<UnityEngine.Vector3>`,
`GhostAI/ദശസറദബഹജഭ` and `Player&`, and the offline dump as `List<Vector3>`, `ദശസറദബഹജഭ` and
`out Player`; both are canonicalised alike, so write the short form.

Twins and aliases. A method entry resolves to its canonical body (the lowest address) plus every
other body that is the same behaviour:

- **Aliases**: another method of the owner with the same signature folded onto the very same address.
  They need no extra hook.
- **Twins**: another method with the same signature whose body, read in full, is the same once placed:
  the same length and the same normalised instructions, with rip-relative operands compared by
  address. **Only fully read code counts**: incomplete code is never a twin, not even byte for byte.
  A copy that references other metadata is not a twin (see the decoy in `systems/ghost-ai.md`).

Several candidates that are all one behaviour count as one. The result is `Resolved` with the twins
and aliases listed. A method with no code of its own (abstract, extern, a shared generic, or code
outside GameAssembly) has no address, so its entry is `Missing`.

### Confidence and the note

`Confidence` is `Live` (seen in the game; the note gives the date), `Decompiled` (read in decompiled
code) or `Inferred` (from a name or Unity's documented API). A note never claims more than its
confidence, and an unverified note is never upgraded silently. A note says what the thing does in the
game, from the point of view of a mod author. State the build for facts read from code.

### Commands

Set `PG_GAME_DIR` (the Phasmophobia folder) and `PG_CPP2IL` (a Cpp2IL executable), or pass `-GameDir`
and `-Cpp2Il`. The game and Cpp2IL paths never come from the repository.

```powershell
.\tools\gamecheck.ps1                    # dump the game, resolve every entry, write re\dump, re\names.tsv
.\tools\gamecheck.ps1 -IfConfigured      # the same, or exit 0 quietly when the variables are not set
```

`gamecheck.ps1` copies four files out of the game folder (`GameAssembly.dll`, `Phasmophobia.exe`,
`globalgamemanagers`, `global-metadata.dat`), runs Cpp2IL with `--use-processor attributeinjector
--output-as diffable-cs` only when their hashes changed, and runs `build\gamecheck.exe --dump <dir>
--image <GameAssembly.dll> --addresses re\map-addresses.tsv [--mods <dir>]`. It prints one line per
entry (`OK`, `MISSING`, `AMBIGUOUS` or `BLOCKED`, the key, the value, then twins, aliases and
`renamed from`; `STARTUP`, the key and the icall's name for an icall, checked at startup only), then
a summary. It never runs game code and never writes in the game folder. Exit 0 when every required
entry it can check resolved and no mod would be switched off, 1 when one did not or one would, 2 on
bad arguments.

`-Mods <dir>` adds `--mods <dir>`: gamecheck loads each mod DLL under `<dir>`, reads its
`pgmod_requires` and prints `would switch off <mod id>: <keys>` for each mod the host would not start
on that build, and `would not load <mod id> (contract <version>)` for a mod built against a host
contract the host does not run (`pg_api_compatible`), which the host never loads. `validate.ps1`
runs `.\tools\gamecheck.ps1 -IfConfigured -Mods dist\PhasmoMods\mods`, the staged bundled mods, so
it fails when a mod would be switched off or would not load.

Reading a function's code needs Ghidra, `GHIDRA_INSTALL_DIR` and a one-time import into `re/ghidra`.
That import is not written down yet: `FINDINGS.md` item 30 describes the missing `re/README.md`.

```powershell
node re\decomp.mjs <out.c> <rva|key> [<rva|key> ...]   # decompile by RVA or map key (GhostAI.Hunting)
node re\slimpe.mjs <GameAssembly.dll> <out.dll> <rvaFrom>-<rvaTo>|<key> ...   # a slim PE for REA
```

`decomp.mjs` annotates known callees from `re\names.tsv`. `slimpe.mjs` builds a PE that keeps the real
image base and carries only the chosen ranges, so a tool that cannot analyse the 113 MB original can
show real addresses. Both need the tables `gamecheck.ps1` writes.

Derived data is never committed: `re/dump/`, `re/names.tsv`, `re/map-addresses.tsv` and `re/out/` are
gitignored. Map entries (names, shapes, facts, hints, notes) are committed.

### Adding an entry, in order

1. Read the game's code: `node re\decomp.mjs`, and the dump under `re\dump\cs`.
2. Write the entry and its `///` note in `sdk/map/<Type>.h`, with the first strategy in the list above
   that resolves it by itself. Add its key to `kAllEntries` in `sdk/map/all.h`, and the `phasmo::` wrapper.
3. Run `.\tools\gamecheck.ps1`. The entry must be `OK` with the value you expect. To prove the facts
   matter, drop one and confirm the entry turns `AMBIGUOUS` or `MISSING`.
4. Put anything you learned that is not in the entry's note into `sdk/systems/<system>.md`.
5. `.\validate.ps1`.

The system overviews: [ghost AI](systems/ghost-ai.md).
