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
2.0, hooks by entry, the fake backend, `tools/gamecheck.ps1`, and two mapped types (GhostAI with 15
entries, Unity's Camera with 8). One mod call goes through the game API: Ghost Tells' Shade calls
`phasmo::GhostAI::UnAppear`.

Does not exist yet. These arrive with stage 2, when every bundled mod is on the game API:

- the downloadable SDK zip with a template mod;
- the mods folder in `%LOCALAPPDATA%\PhasmoMods\mods` for other authors' mods;
- the channel scheme for network messages of other authors' mods;
- the generated game API reference. Until then this README, the headers and `sdk/systems` are the
  documentation, and each header's `///` notes are the reference.

Until then, mods are built inside this repository, as the bundled ones are (see `CMakeLists.txt`, the
`GhostTells` target), and the host loads only the bundled mods. Everything below describes what is in
the code now.

## Making a mod

### The rules every mod follows

- **Main thread only.** The host calls `pgmod_start`, `pgmod_tick` and `pgmod_stop` on Unity's main
  thread, and the game API runs there too. Do not call it from another thread. Only the host
  contract's `log` may be called from any thread.
- **Never keep a raw managed reference across frames.** The garbage collector moves and frees
  managed objects. Keep one in an `il2cpp::Held` (`core/il2cpp.h`), as `mods/ghost_tells/shade.cpp`
  does with its ghost. `tests/held_scan_test.cpp` fails the build on a raw `void*` kept at namespace
  or struct scope in `core/`, `host/` or `mods/`.
- **Private lobbies only.** Modded play belongs in private lobbies. Playing with mods carries a real
  risk that the game's moderation acts on the account, up to a ban. Nothing in the SDK changes that.
- **Nothing in the SDK touches the anti-cheat.** The SDK does not map, hook or call the game's
  anti-cheat or its moderation code, and a map entry that names them is rejected by
  `tests/map_test.cpp`. Do not add one.
- **Nothing thrown crosses the host contract.** Only plain pointers, integers and C strings do. A mod
  catches its own exceptions. `pg::mod::guarded_start`, `guarded_tick` and `guarded_stop`
  (`core/mod_glue.h`) do that for the three exports.
- **A struct argument larger than 8 bytes is passed by address**, as the x64 calling convention
  does, and the callee may modify it. In a `phasmo::` method signature write such a parameter as a
  pointer. Pass a copy when the original must stay unchanged.

### Calling the game

A map header ends with the ready-made `phasmo::` wrappers for its type. Include the header and call
them:

```cpp
#include "GhostAI.h"

if (world::appeared(ghost)) phasmo::GhostAI::UnAppear(ghost);   // a method: the object comes first
const bool hunting = phasmo::GhostAI::isHunting.get(ghost);     // a field
phasmo::GhostAI::state.set(ghost, phasmo::GhostState::hunting());  // an enum value is called
```

Four kinds of wrapper, from `sdk/api/game_api.h`:

| Wrapper | Declared as | Use |
| --- | --- | --- |
| `pg::api::Method<D, R(A...)>` | `static constexpr ... UnAppear{}` | `UnAppear(ghost)`; `available()` says whether it resolved. For an instance method the first parameter is the object. |
| `pg::api::Field<D, T>` | `Field<D, int32_t>` | `get(obj)`, `set(obj, v)`. `T` is a plain value or `void*` for a managed reference, whose `set` goes through the GC write barrier (the host's `set_reference`). |
| `pg::api::EnumValue<D>` | `idle{}` | `idle()` gives the value's number in this build. |
| `pg::api::Type<D>` | `type{}` | `klass()` gives the `Il2CppClass*`; `is(obj)` tells whether an object is of the type or a derived one. |

A call that cannot reach the game never jumps to null. An entry that did not resolve logs
`<key> is not available in this game build` once and returns the type's default (0, false, null) or
does nothing. A field read of a null object gives the default; a write to one does nothing.

### What a mod requires

What a mod requires is what its code calls. Every wrapper member is a template, so an entry is
registered only when code in the mod uses it (`Use<D>::registered`), and the mod's `pgmod_requires`
export lists exactly those entries' keys, less the ones marked `kOptional`. `pg::mod::bind` (called
first in `pgmod_start`) checks `pg_api_compatible(host, mod)`, looks every used entry up through the
host's `game_entry`, and returns false with the missing keys logged when a required one did not
resolve. The host does not start a mod whose required entries are missing and logs which. A mod
without `pgmod_requires` requires nothing.

Ghost Tells today lists exactly one key, `GhostAI.UnAppear`. Every wrapper header is included only for
what the mod's code calls: including `GhostAI.h` requires nothing by itself.

### Hooking and caller context

```cpp
// A game method's code takes the MethodInfo as its last argument, so the detour and `original` do too.
void (*original)(void*, int32_t, void*, bool, const void*) = nullptr;
const bool hooked = pg::api::hook<phasmo::GhostAI::ChangeState>(
    reinterpret_cast<void*>(&detour), reinterpret_cast<void**>(&original), "my hook", pg::api::Twins::All);
```

`hook<M>` creates a MinHook detour on the method through the host's single MinHook instance. It is
created, not enabled: call `enable_hooks` (or `hooks::enable_all()`). Hooks chain: the newest detour
runs first and its `original` reaches the previous one.

The obfuscator makes same-behaviour copies of some methods (twins). With `Twins::All` (the default)
every twin is hooked as well, so the detour runs whichever body the game calls. With
`Twins::CanonicalOnly` only the canonical body is hooked, which is what existing code hooked by raw
address and so what a migration uses. The host refuses a hook that mixes the two policies on one
method (an all-twins hook over a canonical-only one, or the reverse), and an all-twins hook over a raw
hook on a twin, and logs why. A twin folded onto the canonical address is hooked once.

`pg::api::inside<M>()` tells whether a call of M is running on this thread, from a thread-local depth
the host's own hook on M keeps (`inside_entry` in the host contract). It answers only for methods the
host tracks (`PG_TRACK` in `host/context.cpp`). The host tracks none yet, so today it is always false.
It replaces return-address checks, which break when several mods chain detours on one function.

### The host contract, version 2.0

`PGMOD_API_VERSION` is a major number in the high 16 bits and a minor in the low 16 (2.0 today).
`pg_api_compatible(host, mod)` accepts a mod of the same major and a minor no newer than the host's.
Within a major, `PgHostApi` and every published struct only grow at the end; a new major refuses
older mods. Version 2.0 added these members, after the services of version 1:

- `game_entry(key)`: the resolved `PgGameEntry` for a map key (type, method, code, twins, field
  offset, enum value), or null when unknown or unresolved.
- `hook_entry(key, detour, original, name, allTwins)`: the hook above.
- `set_reference(obj, offset, value)`: stores a managed reference through the GC write barrier. The
  only correct way to write a reference field; `Field<D, void*>::set` calls it.
- `inside_entry(key)`: the thread-local depth above.

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

The other givers are `field(d, offset)`, `value(d, n)` and `type(d, klass)`. An entry not given stays
unresolved, as in a build that lacks it. `hooked()` lists the keys hooked through `hook_entry`, and
`barrier_writes()` counts reference stores. `Type::is` sees only the exact class (no inheritance).
`tests/game_api_test.cpp` shows every path.

### Checking a mod against a game build

`tools/gamecheck.ps1` with `-GameDir` and `-Cpp2Il` resolves the map against a build without
running the game. `build\gamecheck.exe ... --mods <dir>` also loads each mod DLL in the folder, reads
its `pgmod_requires` and prints `would switch off <mod id>: <keys>` for each mod the host would not
start on that build (see "Commands" below).

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
`EnumValue`, `Code`), `confidence`, `flags` (`kStatic`, `kOptional`, `kTrack`; `kTrack` is not read
by anything yet), `owner` (the declaring Type entry's key), `name` or `hint`, `assembly` and `ns` (for
a type; a null `ns` means the global namespace for a top-level type and any for a nested one), `type`
(a field's type or a method's return type), `params`, `fieldTypes`, `values`, `facts`, and for a Code
entry `pattern`, `within`, `patternOffset`. A map header ends with its `phasmo::` wrappers; add one
for each entry a mod may use.

### How the resolver finds an entry

Strategies, most robust first. It accepts an entry only when exactly one candidate survives every
check the entry declares.

1. **By real name** (`name`). Types by assembly, namespace and name; members by name in the owner.
   A method named without `params` matches by name alone at any arity, so an overloaded method is
   `Ambiguous` until `params` is given. A parameter list is how overloads are told apart.
2. **By shape** (`name` null), for obfuscated members and types: the owner, static or instance, a
   field's type, a method's return and parameter types, a type's `fieldTypes` (each listed type needs
   a field of its own) and an enum's `values` (the value names it must have). The obfuscated `hint`
   from the last mapped build is **only a report**: it never decides. Every candidate is checked, and
   the hint only names what was renamed (`renamed from ...`). A stale hint that now belongs to a
   wrong-shaped member is ignored. Compiler-generated types (a name starting with `<`, or a field
   named `<>...`, such as a coroutine class) are never shape candidates.
3. **By facts about the machine code**, to separate candidates that shape cannot (GhostAI has 13
   unnamed bools). A `pg::sdk::Fact` has `is` (`Stores`, `StoresConst`, `Loads`, `Calls`, `CalledBy`),
   `method` (the key of the method whose code is read), `value` and `negate`:
   - `Stores`/`Loads`: the fact method (or a twin of it) writes or reads the candidate field.
   - `StoresConst`: it stores the constant `value` there with an immediate `mov`.
   - `Calls`: the candidate method's code calls the fact method. `CalledBy`: the fact method calls
     the candidate.
   - `negate` turns a fact around. A negated **field** fact removes a candidate only for an access
     through `this`; an access through any other register leaves it undecided.
   - Facts **fail closed**: when the code a fact needs cannot be fully read (an instruction it cannot
     decode, a desynchronised stream, data it does not recognise), the candidate stays and cannot be
     accepted, so the entry is `Ambiguous`, never a wrong pick.
   - A field fact on a struct owner is unsupported (a struct's methods address fields without the
     object header, so the displacement would be wrong).
   - A store is keyed by its start offset: a 16-bit store at 0x108 that also clears the bool at 0x109
     is a store at 0x108 only. `sdk/map/GhostAI.h` explains the one such case.
4. **By code pattern inside one method** (`Kind::Code`), for behaviour that exists only as code
   inlined into another method. `pattern` is hex bytes with `??` as a wildcard, `within` is the key
   of the method entry it is searched in, and it must match exactly once there. `patternOffset` moves
   the result. A pattern is the last resort, and its note says why nothing better works.

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
and aliases listed.

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
`renamed from`), then a summary. It never runs game code and never writes in the game folder. Exit 0
when every required entry resolved and no mod would be switched off, 1 when one did not or one would,
2 on bad arguments. `validate.ps1` runs it with `-IfConfigured`.

Reading a function's code (needs Ghidra and `GHIDRA_INSTALL_DIR`, and the one-time import into
`re/ghidra`):

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
2. Write the entry and its `///` note in `sdk/map/<Type>.h`, with the weakest strategy that resolves
   it by itself. Add its key to `kAllEntries` in `sdk/map/all.h`, and the `phasmo::` wrapper.
3. Run `.\tools\gamecheck.ps1`. The entry must be `OK` with the value you expect. To prove the facts
   matter, drop one and confirm the entry turns `AMBIGUOUS` or `MISSING`.
4. Put anything you learned that is not in the entry's note into `sdk/systems/<system>.md`.
5. `.\validate.ps1`.

The system overviews: [ghost AI](systems/ghost-ai.md).
