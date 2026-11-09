# Mapping the game: entries, the resolver and game builds

For whoever adds entries to the map: what an entry is, how the resolver finds one, the confidence a note
claims, the commands, game builds, and the checks that keep raw values out of the code. A mod author
needs only [the SDK's README](../README.md). The tools and the pitfalls of reading the game's code are in
[reverse engineering](reverse-engineering.md).

The rules:

- **A mod never holds a game address or a field offset**, and never reaches the game around the map
  "just this once". A second way in recreates the scattered knowledge the map exists to remove.
  No hex address or field offset is added to `mods/`, `host/` or `core/`.
- **A wrong match that looks right is the worst outcome.** The resolver never picks among
  candidates: zero is `Missing`, more than one distinct behaviour is `Ambiguous`, an entry whose
  dependency did not resolve is `Blocked`.
- **Every RE fact belongs in an entry's `///` note or a system overview (`sdk/systems`), never only in
  an agent's memory note or a chat.** A memory note may point at the SDK; it does not hold the fact.
  `tests/map_test.cpp` fails an entry with no `///` note on the line above it.

[Mapping the game: tools and pitfalls](reverse-engineering.md) has the tools, how obfuscated code is read, and the
mistakes earlier work made with the runtime, Unity and hooks. Each page in `systems/` keeps what an author needs; how its facts
were read and what the loader does with them are in the page of the same name in `internals/`.

## An entry

A map entry is a `pm::sdk::EntryDesc` (`sdk/api/entry.h`) in `sdk/map/<Type>.h`, with its `///` note
on the line above and its key listed in `kAllEntries` in `sdk/map/all.h` (`tests/map_test.cpp` fails
an entry missing from it, and a duplicate key). Every field has a default, so use designated
initialisers and name only what the entry needs:

```cpp
/// Ends an appearance: when the ghost's shown flag is set ... Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_UnAppear{.key = "GhostAI.UnAppear", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "UnAppear"};
```

The fields: `key` (what `game_entry` and `pmmod_requires` use), `kind` (`Type`, `Method`, `Field`,
`EnumValue`, `Code`, `Icall`, `Export`, `Scene`, `Layer`, `AnimatorParam`, `NativeSite`, `ShaderProperty`, `Inflated`), `confidence`, `flags` (`kStatic`, `kOptional`, `kTrack`;
`kStatic` makes a field a static one; `kTrack` is not read by anything yet), `owner` (the declaring
Type entry's key), `name` or `hint` (an icall's or export's name, as the runtime knows it),
`assembly` and `ns` (for a type; a null `ns` means the global namespace for a top-level type and any
for a nested one), `type` (a field's type or a method's return type), `params`, `fieldTypes`,
`values`, `facts`, for a field `within` (a field of an embedded struct) and `sameFieldAs`, for a
Code entry or a native site `pattern`, `within` and `patternOffset`, and for a scene fact `scene`,
`path` and `component`. A map header ends with its `phasmo::` wrappers; add one for each
entry a mod may use (an export gets none).

The model is read from every assembly a Type entry names, and a build that lacks a required one
cannot be resolved at all, for every mod. An assembly that only `kOptional` Type entries name is
optional: a build without it is read without it, and those entries stay unresolved. Give a type in a
Unity module the game might stop shipping `kOptional` (Cloth's module is one, `sdk/map/UnityCloth.h`),
and have the mod that needs it check `available()` at its start and fail with a logged reason.

## How the resolver finds an entry

Strategies, most robust first. It accepts an entry only when exactly one candidate survives every
check the entry declares.

1. **By real name** (`name`). Types by assembly, namespace and name; members by name in the owner.
   A method named without `params` matches by name alone at any arity, so an overloaded method is
   `Ambiguous` until `params` is given. A parameter list tells overloads apart.
2. **By shape** (`name` null), for obfuscated members and types: the owner, static or instance, a
   field's type, a method's return and parameter types, a type's `fieldTypes` (each listed type needs
   a field of its own) and an enum's `values` (the value names it must have). A type with no fields
   to go by, a static class, may instead give `type` and `params`, the shape of a static method it
   declares; several types declaring one are `Ambiguous`. An entry never names an obfuscated member:
   the game shuffles those names on every update, so one means nothing a build later (the raw scan's
   `no_obfuscated_name_in_the_docs` fails on one). A readable `hint`, where the member has one, is **only
   a report**: it never decides, every candidate is checked, and the hint only names what changed
   (`renamed from ...`). Compiler-generated types (a name starting with `<`, or a field
   named `<>...`, such as a coroutine class) are never shape candidates, unless the type entry has the
   flag `kCompilerGenerated`, which makes them the only candidates (`RewardManager.Results` is the reward
   screen's async state machine, found by its four fields). The models name a generic definition
   without its arity, so `UnityEvent` and `UnityEvent<T0>` share a name; the flag `kNotGeneric` on a
   top-level type entry keeps only the type that is no generic definition (`UnityEvent`,
   `UnityAction`). The dump gives the arity by the file's name (`UnityEvent`1.cs`), so only a
   top-level type's is known offline. A field may also name its
   `access` (`pm::sdk::Visibility`: `Private`, `Internal`, `Public` and the rest, as C# declares it;
   both models read it, the dump from the modifiers, the runtime from the field's flags); a field
   whose accessibility the model does not know never matches one asked for (`GhostHuntState.target`
   is the hunt state's one internal Player field). A type's `fieldTypes` say only what fields it must
   have, so a look-alike that declares the same fields and more fits too. The flag `kExactFields` on a
   type found by its field types asks for exactly those: every instance field the type declares (not
   those it inherits; statics and constants do not count) must be one of the listed, so the superset
   look-alike is no candidate (`GhostWanderState` is one class's eight fields, and a state class
   declares those and one more float). Both models list a type's declared fields alike
   (`tests/runtime_model_test.cpp` checks the flag on each). Two exact matches are `Ambiguous`; the flag
   on any other entry is `Missing`. Adding `kFieldOrder` to an exact list also asks that the fields lie in
   the listed order by offset, which fixes the struct's size (`RewardManager.EventScore`, whose size a
   mod's copy of `RewardManager.EventRow`'s answer depends on); alone, the flag is `Missing`.
   An interface has no fields to go by. A type entry whose `within` names another type entry is the one
   interface that type declares in its own base list (`IGhostState`, by `GhostWanderState`, and through it
   `GhostStateMachine`, the class whose one field is one). Two interfaces are `Ambiguous`; none, or a base no
   model type has (it could be an interface the model lacks), is `Missing`.
3. **By facts about the machine code**, to separate candidates that shape cannot (GhostAI has 13
   unnamed bools). A `pm::sdk::Fact` has `is` (`Stores`, `StoresConst`, `Loads`, `Calls`, `CalledBy`,
   `Called`, `Compares`, `IsNot`),
   `method` (the key of the method whose code is read), `value` and `negate`:
   - `Stores`/`Loads`: the fact method (or a twin of it) writes or reads the candidate field. When
     the fact method is an instance method of the field's owner (and gets `this` in rcx), a fact
     counts only an access through `this`; a candidate reached only through another register stays
     undecided, so the entry is `Ambiguous`. A register holds `this` when every write to it is a copy
     of `this`, or a restore that hands the caller's value back unused: a pop where the function
     pushed it, or an epilogue's load from the stack (through `rsp`, or through `r11` after
     `lea r11, [rsp+..]`) followed only by other restores, the frame's release and the return or tail
     jump (a `jmp` back to the function's own start is such a tail call: it runs the function
     afresh). In a static method or a method of another type no
     register is known to hold the owner, so a positive fact matches an access at the field's
     offset through any register. Such a cross-owner fact is weak: give the entry a second,
     independent fact as well. Two kinds of access never count, whatever the method: one through a
     register just loaded from a fixed address (`mov r, [rip+X]`, a runtime metadata slot: the
     class-initialised check `cmp dword [rcx+0E0h], 0` every method makes reads a class, never an
     object), and one through a register loaded from fields of `this` (straight-line code, up to three
     loads deep) whose last type and the owner are of unrelated families: neither among the other's
     possible ancestors, read from the base lists (each base name standing for every model type of that
     name) and known only when every base names a model type. The fact method may also be a leaf the
     exception table does not list, a constructor that stores its defaults and tail-jumps to its base
     (`LocalPlayer.ctor`): it is read as straight-line code up to its ret or tail jump, with rcx
     holding `this` until an instruction writes it, and one that branches cannot decide.
   - `StoresConst`: it stores the constant `value` there with an immediate `mov`.
   - `Called` names no method: the candidate method is called (or jumped to) directly from another
     method of its own type. The obfuscator's decoys, near-copies with other constants, are never
     called, so this keeps the real one (`GhostHuntState.RefreshBase` and `.UpdateSpeed`). The other
     methods are read in full, a leaf function the exception table does not list as straight-line code
     up to its `ret` or tail jump; one that cannot be read leaves an uncalled candidate undecided
     (`Ambiguous`), since it might hide the call. The fact is a bet on how the obfuscator places its
     decoys (uncalled, and in the same type as the real method): a build that called a decoy, or
     called the real method only through a virtual or indirect call, would leave the entry
     `Ambiguous` or `Missing`, never pick the decoy silently, and `tools/gamecheck.ps1` (with
     `-Expect` while values are migrated) is the net that shows it.
   - `Calls`: the candidate method's code calls the fact method. `CalledBy`: the fact method calls
     the candidate.
   - `Compares` names an EnumValue entry, not a method: the candidate method's own code, read in
     full, compares something with that value's number (a `cmp` with it as the immediate). The
     obfuscator's look-alikes of a handler that acts for one ghost type compare with other constants,
     so `Compares GhostType.Yurei` keeps the incense's real Yurei trap among five
     (`WhiteSage.YureiTrap`). A leaf the exception table does not list cannot decide it. The
     comparison is read in the operand's width: a byte compare with 0x80 is one with 128, and a value
     the operand cannot hold never matches. A positive `Compares` is a bet, as a positive `Calls` is:
     it does not ask what the compared operand holds, so another number of the same value compared
     for another reason would match too; the entry's other facts, and `-Expect` while values are
     migrated, are the net. A negated `Compares` is refused (`Missing`): the reader sees only `cmp`
     with an immediate, not a `test` against 0, a compare of two registers, a `sub` or `dec` before a
     jump, or a jump table, so "does not compare" cannot fail closed.
   - `IsNot` names a Field entry of the same owner: the candidate is not the field that entry resolved
     to. It narrows a cross-type fact (which matches every field at the offsets its method touches, in
     any object) by fields already told apart, so an entry may name each field it knows the method also
     reads (`GhostAI.interactionBoost`, whose one writer is a coroutine no entry can name). While the
     named entry is unresolved this one is `Blocked`, and a new field the method comes to read leaves it
     `Ambiguous`, so it fails closed.
   - `negate` turns a fact around. A negated **field** fact removes a candidate only for an access
     through `this`; an access through any other register leaves it undecided. Negated `Calls` and
     `CalledBy` facts are not supported yet (the reader does not see indirect or virtual calls), and
     an entry with one is `Missing`.
   - Facts **fail closed**: when the code a fact needs cannot be fully read (an instruction it cannot
     decode, a desynchronised stream, data it does not recognise), the candidate stays and cannot be
     accepted, so the entry is `Ambiguous`, never a wrong pick.
   - A field fact on a struct owner is unsupported (a struct's methods address fields without the
     object header, so the displacement would be wrong). A field of a struct embedded in a class is
     mapped with `within` instead (below), and its facts read the class's code.
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
- **A method of a generic instance** (`Kind::Inflated`): a method of a generic class made for type
  arguments (`InputControl<Vector2>.ReadValue`), or, flagged `kGenericMethod`, a generic method made for
  them (`Sensor.GetDetectedComponents<PhotonObjectInteract>`). Use it for any member declared on a generic
  definition, whose own code does not exist at runtime: a plain `Method` entry there is `Missing` ("has no
  code of its own"). `owner` is the definition's type entry (a name the non-generic type shares is told
  apart by a `fieldTypes` of its parameter: `InputControl`1`), `name`, `type` and `params` are the method as
  declared, with the type parameters by name (`"TValue"`, `"List<T>"`), and `typeArgs` gives one type for
  each parameter, `"@Key"` or a name exactly one model type has. A wrapper is a plain `pm::api::Method`,
  called and hooked as any other: the host publishes it as a method with the inflated `MethodInfo`.
  The instance's code exists only in the running game. The host makes it on the main thread when it
  activates, after the icalls and before any mod starts, through the game's reflection
  (`RuntimeType.MakeGenericType`, `RuntimeMethodInfo.MakeGenericMethod`), so the offline check finds the
  definition and its arguments, lists the entry as checked at startup only, and **never claims it resolved**.
  A build with no code for the instance fails closed: the runtime still answers with a stub (one that
  forwards to the `MethodInfo`'s invoker, which raises and never returns), so the host reads that invoker
  and leaves the entry unresolved when it never returns, and a mod that requires it does not start. An invoker
  the exception table lists comes back through a `ret` or a jump out of it; one it does not list must reach a `ret`
  in straight-line code, since a jump there cannot be followed: such an entry stays unresolved. For a
  reference-type argument any such instance works: the runtime shares one body between them, so a hook on
  it sees the call for every reference-type `T`. For a value type, map an instance the game itself uses;
  check that a game method calls it, since no offline check will. The dump's `[Address]` on a generic
  definition is some shared instantiation's and proves nothing about yours.
- **An export** (`Kind::Export`), a function GameAssembly.dll exports (`"il2cpp_init"`), from the
  module's export table, in the game and offline alike. A name exported twice, or forwarded to
  another module, is `Missing`. Only the host may use an export: the host contract never gives one to
a mod.

A **native site** (`Kind::NativeSite`) is a place in native code, which has no metadata. `assembly` says
which module: `"UnityPlayer"`, or `"GameAssembly"` for the IL2CPP runtime's own functions. `pattern` is
hex bytes with `??` as a wildcard, and `within` is the name of an export of that module whose function
bounds the search, or `".text"` for the whole code section, where the pattern needs at least 16 bytes
that are not wildcards. It must match exactly once
there, or the entry is `Missing` or `Ambiguous`, and `patternOffset` must keep the site inside the
bytes it matched. The host resolves native sites over the loaded modules at startup, and the
offline check over copies of them. UnityPlayer exports only `UnityMain`, so every native
site today, all of them in UnityPlayer, searches `.text`: its pattern runs through the instructions its users rely on, so a build
that changes them leaves it unresolved. They are the most fragile entries. Only the host acts on one: the host
contract's `game_entry` never gives one to a mod, which asks the game API for the effect and requires none of the
entries behind it. The host lists its own (`pm::map::kSteeringEntries`), logs the unresolved ones at startup, and
the offline check prints them on its `HOST steering` line.

Four kinds are facts of the game's scene and asset files, not of its code (see
[scene and asset facts](../systems/scene.md)). The resolver only checks that one is well formed and
leaves it as checked in the scene files; the offline check verifies it with `tools/assetcheck.py`,
and the running game where it is used:

- **A Scene entry** (`Kind::Scene`): `scene` (a scene's name as the build lists it, or
  `"Resources/<path>"` for a prefab `Resources.Load` loads), `component` (the script or engine
  component on the object a lookup starts from) and `path` (from that object, as Transform.Find
  takes it). The offline check requires exactly one object at the path under every object that
  carries the component there.
- **A Layer entry** (`Kind::Layer`): its `name`. Resolved by name on the main thread when the host
  activates.
- **An AnimatorParam entry** (`Kind::AnimatorParam`): the parameter's `name`, the controller's name in
  `component` and the place that uses it in `scene`.
- **A ShaderProperty entry** (`Kind::ShaderProperty`): the property's `name` (`"_TintColour"`), the
  shader's name in `component` (`"Candle Flame"`) and in `scene` a scene whose assets hold the shader.
  The offline check requires every shader of that name in the scene's shared assets file, and in the
  shared assets files that one refers to, to have the property. The host reads its id through
  Shader.PropertyToID on the main thread when it activates; whether a material has it is asked where
  a mod uses it (Material.HasProperty).

A struct's own instance field (a `Field` entry whose `owner` is a struct, with no `within`) resolves to
its place in the struct itself: the offset both models report less the 0x10 boxed header. That is how
code holding the struct by address reads it, a method receiving a `PhotonMessageInfo` say, so its
wrapper takes the struct's address as `obj`. A boxed struct (an `object` holding one) keeps the 0x10
header in front of the value, so pass the box's address plus 0x10, the unboxed value, as `obj`. A
struct's static field keeps its offset.

A field of a struct embedded in a class (`GhostInfo` holds a `GhostTraits` value whose first fields
are the ghost's types) is a `Field` entry whose `owner` is the class and whose `within` is the key of
the class's instance `Field` entry that holds the struct. Its candidates are the struct's instance
fields of the entry's shape. It resolves to its place in the class, the holding field's offset plus
the struct field's offset less the 0x10 object header both models report for a value type's fields,
so its wrapper reads the class's object directly; it has no FieldInfo of its own. Its facts are judged
in the class's code at that place, with the usual rules for `this`. A holding field that is not of a
struct type the model knows, or is static, or is itself within a struct, leaves the entry `Missing`.

The same struct type is often embedded in more than one class (`GhostInfo` and `GhostController` both
hold `GhostTraits`). Where a fact decides a field within one of them, `sameFieldAs` gives the other the
very same struct field: a `within` entry with `sameFieldAs = "<key>"` keeps only the struct member that
`<key>`, another `within` entry, resolved to. It depends on that entry, so it is `Blocked` while that one
is unresolved, and it is `Missing` when the two hold different struct types or either is not a field
within a struct. `GhostInfo.identityType` follows `GhostController.ghostType` this way.

A static field is a `Field` entry flagged `kStatic`, found like any field. The resolver only reads the
metadata; the field's value is read when a mod asks for it, never at startup.

Identity. A type string `"@Key"` means the very type the entry `Key` resolved to, not just its name,
and the same holds in `params` and `fieldTypes`. Where the model says which type a member uses, it
must be that type; where it does not, a name that several types share makes the entry `Ambiguous`.
An `"@Key"` may also stand inside a generic's arguments (`"List<@GhostType>"`,
`"Nullable<@GhostType>"`): it means the name of the type that entry resolved to, and the entry depends
on it. The model gives no index for a generic's arguments, so there the type is known by name only, and
a name several types share makes the entry `Ambiguous`. The runtime spells types as
`System.Collections.Generic.List<UnityEngine.Vector3>`, `GhostAI/<an obfuscated nested class>` and `Player&`, and
the offline dump as `List<Vector3>`, `<that obfuscated class>` and `out Player`; both are canonicalised alike, so write
the short form.

Twins and aliases. A method entry resolves to its canonical body (the lowest address) plus every
other body that is the same behaviour:

- **Aliases**: another method of the owner with the same signature folded onto the very same address.
  They need no extra hook.
- **Twins**: another method with the same signature whose body, read in full, is the same once placed:
  the same length and the same normalised instructions, with rip-relative operands compared by
  address. **Only fully read code counts**: incomplete code is never a twin, not even byte for byte.
  A copy that references other metadata is not a twin (see the decoy in `systems/ghost-ai.md`).

Several candidates that are all one behaviour count as one. The result is `Resolved` with the twins
and aliases listed. A method with no code of its own (abstract, extern, a generic definition's, or code
outside GameAssembly) has no address, so its entry is `Missing`. A generic definition's method is a
generic method or any method of a generic type: the runtime gives it no code, since only its
instantiations have any. The dump writes a shared instantiation's address for it, so the offline check
takes every method's address where the runtime does, from the method pointer tables of GameAssembly's
code-gen modules, by the dump's `Token`. It refuses such an entry as the game would, however little of
the method's declaration shows that it is generic, and it stops when it cannot find those tables.

## Confidence and the note

`Confidence` is `Live` (seen in the game; the note gives the date), `Decompiled` (read in decompiled
code) or `Inferred` (from a name or Unity's documented API). A note never claims more than its
confidence, and an unverified note is never upgraded silently. A note says what the thing does in the
game, from the point of view of a mod author. State the build for facts read from code.

## Commands

Set `PM_GAME_DIR` (the Phasmophobia folder) and `PM_CPP2IL` (a Cpp2IL executable), or pass `-GameDir`
and `-Cpp2Il`. The game and Cpp2IL paths never come from the repository.

```powershell
.\tools\gamecheck.ps1                    # dump the game, resolve every entry, write re\dump, re\names.tsv
.\tools\gamecheck.ps1 -IfConfigured      # the same, or exit 0 quietly when the variables are not set
```

`gamecheck.ps1` copies four files out of the game folder (`GameAssembly.dll`, `Phasmophobia.exe`,
`globalgamemanagers`, `global-metadata.dat`), runs Cpp2IL with `--use-processor attributeinjector
--output-as diffable-cs` only when their hashes changed, copies `UnityPlayer.dll` when its size or
time changed, and runs `build\gamecheck.exe --dump <dir> --image <GameAssembly.dll> --unity-player
<UnityPlayer.dll> --addresses re\map-addresses.tsv --scene-entries re\dump\scene-entries.tsv
[--mods <dir>]`. It prints one line per entry (`OK`, `MISSING`, `AMBIGUOUS` or `BLOCKED`, the key, the
value, then twins, aliases and `renamed from`; `STARTUP`, the key and the name for an icall or a generic instance's method,
checked at startup only; `SCENE`, the key and its path or name for a scene fact, checked in the scene
files), then a summary. Then it copies the scene and asset files the scene facts need and runs
`tools/assetcheck.py` on them, which prints `OK` or `MISSING` for each (see
[scene and asset facts](../systems/scene.md)). The first configured run makes a Python venv in
`re\dump\venv` and installs the pinned UnityPy there, so it needs Python 3 (uv's Python 3.14, the
`py` launcher or `python`, in that order); an unconfigured run needs no Python. It never runs game code and never writes in the game
folder. Exit 0 when every required entry it can check resolved and no mod would be switched off, 1
when one did not or one would, 2 on bad arguments or input it cannot read.

`-Mods <dir>` adds `--mods <dir>`: gamecheck loads each mod DLL under `<dir>`, reads its
`pmmod_requires` and prints `would switch off <mod id>: <keys>` for each mod the host would not start
on that build, and `would not load <mod id> (contract <version>)` for a mod built against a host
contract the host does not run (`pm_api_compatible`), which the host refuses. It prints `would switch off <mod id>: it
records voice, and its manifest does not say so` for a DLL whose flag disagrees with the `manifest.json` beside it. `validate.ps1`
runs `.\tools\gamecheck.ps1 -IfConfigured -Mods dist\PhasmoMods\mods`, the staged bundled mods, so
it fails when a mod would be switched off or would not load.

## Game builds

When every check above passes, `gamecheck.ps1` writes a record of the build into `sdk\builds`, one small text file per
build (`core/build_record.h`): the SHA-256 of `GameAssembly.dll` and of `UnityPlayer.dll`, the digest of the map it was
checked with (`sdk/resolver/map_digest.h`), and nothing else. Commit it. The release ships the folder as `builds`.

The record never decides what runs. The host resolves the map on any build, and each mod's required entries decide
whether it runs. A record decides only whether the consent notice adds its update paragraph:

- Both hashes in a record made with this map: a known build, no paragraph.
- Anything else: an unseen build, and the paragraph. That covers a new game build, a record made with another map, since a new
  or changed entry was not checked against that build, a hash that cannot be computed, and a record that cannot be read.

The notice's fingerprint covers the two hashes as well as its text, so each new build asks once. A build whose hash
cannot be read is asked every time, since it could be any build.

So after changing the map, run the check on the supported build and commit what it writes, or every player's notice says
the game updated. `tools/release.ps1` and `package.ps1`, which CI's release runs, refuse a map no record was made with
(`tools/check_build_record.ps1`). The check cannot run in CI, because it needs the game's files. A record for an older game
build stays stale after a map change and does no harm; nobody plays that build.

The loader never looks at the anti-cheat to judge an update, and neither the map nor a record covers it.

Reading a function's code needs Ghidra, `GHIDRA_INSTALL_DIR` and a one-time import into `re/ghidra`.
`re/README.md` describes the import.

```powershell
node re\decomp.mjs <out.c> <rva|key> [<rva|key> ...]   # decompile by RVA or map key (GhostAI.Hunting)
node re\slimpe.mjs <GameAssembly.dll> <out.dll> <rvaFrom>-<rvaTo>|<key> ...   # a slim PE for REA
```

`decomp.mjs` annotates known callees from `re\names.tsv`. `slimpe.mjs` builds a PE that keeps the real
image base and carries only the chosen ranges, so a tool that cannot analyse the 113 MB original can
show real addresses. Both need the tables `gamecheck.ps1` writes.

Derived data is never committed: `re/dump/`, `re/names.tsv`, `re/map-addresses.tsv` and `re/out/` are
gitignored. Map entries (names, shapes, facts, hints, notes) are committed.

## Adding an entry, in order

1. Read the game's code: `node re\decomp.mjs`, and the dump under `re\dump\cs`.
2. Write the entry and its `///` note in `sdk/map/<Type>.h`, with the first strategy in the list above
   that resolves it by itself. Add its key to `kAllEntries` in `sdk/map/all.h`, and the `phasmo::` wrapper.
3. Run `.\tools\gamecheck.ps1`. The entry must be `OK` with the value you expect. To prove the facts
   matter, drop one and confirm the entry turns `AMBIGUOUS` or `MISSING`.
4. Put anything you learned that is not in the entry's note into `sdk/systems/<system>.md`.
5. `.\validate.ps1`.

The system overviews: [ghost AI](../systems/ghost-ai.md), [ghost types and the Mimic](../systems/ghost-types.md),
[scene and asset facts](../systems/scene.md), [the journal](../systems/journal.md), [the custom
difficulty](../systems/difficulty.md), [evidence](../systems/evidence.md), [flames](../systems/flames.md),
[players: death and revival](../systems/players.md),
[the contract](../systems/contract.md), [Photon and refused RPCs](../systems/photon.md), [doors, exits and
keys](../systems/doors.md). An
overview cites map keys, never addresses or offsets: the map entries are the only source of a value,
and `tools/gamecheck.ps1` prints them for the build in hand.

## Moving code onto the SDK

The mods and the host were moved off raw addresses, offsets and obfuscated names, onto map entries and
content functions, in stage 2b. Two checks keep it that way.

**The raw scan.** `tests/raw_scan_test.cpp` fails on any raw site in a `.cpp`, `.h` or `.asm` under
`mods/`, `core/` and `host/`; there is no baseline. A raw site is a hex literal of 0x100 or more, a <!-- pm:not-game-data: the scan's own rule -->
non-ASCII identifier or string, a call into `il2cpp::` or a `p_il2cpp_` pointer, `il2cpp::field<` or
`static_get`, `MH_CreateHook` outside `host/hooks.cpp`, or a string literal naming `FindObjectOfType`,
`NameToLayer` or `Find`. `build\raw_scan_test.exe --print` lists every file that has any.
A line that holds a value that is not game data (a colour, a protocol byte, a mask) ends in
`// pm:not-game-data: <why>` and is not counted. The runtime's own files (`host/il2cpp.*`,
`host/game_map.*`, `host/hooks.cpp`, `host/code_memory.h`, `host/scene_engine.cpp`,
`host/runtime_services.*`) are exempt. The host's other code reaches the game as a mod does, through the
map's wrappers, and a file under `host/` that names core's `game::` fails (it has `host/host_game.h`).

The same test keeps native code out of the mods, with no baseline: a file under `mods/` fails when it is
assembly, has `stub` as a word of its name (`steer_stub`, not `stubborn`), or names a native site's entry
(`UnityPlayer_...`) or a function that writes or allocates code (`VirtualProtect`, `VirtualAlloc`,
`WriteProcessMemory`, `FlushInstructionCache` and their `Nt`/`Zw` forms). It is a name rule against accidents. The
host owns native code and gives a mod no native site's address, and a mod asks the game API for the effect, as
`phasmo::NavMeshAgent::steer` does.

**The value expectations.** Before replacing a raw value with an entry, write the value to
`re\dump\expect\<name>.txt` (gitignored, never committed), one `<key> <hex>` per line: a method's or a
native site's RVA, a field's offset, an enum value's number. `tools\gamecheck.ps1 -Expect` (or
`gamecheck --expect <file>`, repeatable) checks that each key still resolves to that number. A
difference prints `EXPECT <key>: map 0x.. != expected 0x..` and exits 1; a key the map lacks exits 2.
The expectation files are never read by the game, only by this check.
