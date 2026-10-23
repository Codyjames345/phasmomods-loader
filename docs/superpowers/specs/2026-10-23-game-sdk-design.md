# Game SDK: design

Status: approved by the user on 2026-10-23 ("The plan is approved"). Game build 25811160 (GameAssembly.dll sha256
`e300eda9...`). Where a decision is Claude's call rather than the user's, it says so.

## Intent

Reverse engineering is the expensive part of every change to this suite, and today it is paid
again and again. What has been learned lives in three places that a new session cannot rely on:
raw addresses and offsets in mod source with a comment beside them, Claude's memory notes, and a
class dump that is not in the repository and has to be regenerated and re-read. The latest
reviewer of the Tempest work had to read stored class dumps to recover behaviour; earlier agents
spent a long time looking for dumps that no longer existed.

The loader takes over all contact with the game. The reverse engineering is done once, by the
loader's maintainers, and recorded in one place: a map of the game's classes and members under
readable names, each with what is known about how it behaves. Mods call the game only through that
map, and never see an address, an offset or an obfuscated name. When the game updates, only the
loader is fixed, and where it can, the loader fixes itself by finding things by their names and
shapes rather than by where they were in the last build.

## Goals

- Every interaction between the suite and the game (mods, host and shared code) goes through the
  loader's map. Mods hold no addresses, offsets, obfuscated names or direct IL2CPP calls.
- The interface stays low-level and close to the game: the game's own classes, methods, fields,
  enums and events, de-obfuscated, not a set of higher-level mod concepts (decided by the user).
- Each mapped member carries what is known about its behaviour and how sure we are of it, so an
  agent reads the map instead of decompiling.
- The game's main systems are mapped completely, not only what the current mods use (decided by
  the user): ghost AI and states, hunts, evidence and equipment, weather and the environment,
  players, the level and its rooms, the journal, and the lobby and menus. The full list is in
  "What gets mapped".
- After a game update, mod DLLs keep working unchanged. Whatever the loader cannot find, it says
  at startup by name, and it switches off only the mods that need it rather than crashing.
- One command checks a new game build without launching the game. It reports what still resolves
  and what has to be found again, in the code and in the scene files. A build runs modded only
  after it passes this check (decided by the user).
- Every existing mod, the host and `core/` move onto the map (decided by the user: a complete job,
  not a partial one).

## Non-goals

- ~~A public modding API frozen for outside modders.~~ Reversed by the user on 2026-10-23: other
  people build their own mods with the SDK. See "Third-party mods" at the end.
- New mod features or behaviour changes. The migration keeps every mod's behaviour exactly as it is.
- A new loading method. Loading stays as it is: the launcher injects into a running game.
- Delivering updates to players. The user asked for an automatic updater on 2026-10-23. It is its
  own design, and it is how a new package, with its checked-build records, reaches every player.

## Anti-goals

- Anything that touches the anti-cheat. The anti-cheat class, `CloudEvents.SelfReport`, the
  moderation bindings and the crash path they use are never mapped, hooked or called, and nothing
  in the loader hides it from them (the standing rule in the project's anti-cheat notes).
- Raw addresses or offsets surviving in a mod, or a mod reaching the game around the map "just
  this once". A second way in recreates the scattered knowledge this exists to remove.
- A guess written down as a fact. Every behaviour note says whether it was seen in the game, read
  in decompiled code, or inferred from a name, and an unverified note is never upgraded silently.
- A wrong match that looks like a right one. An entry that matches zero or several candidates is
  reported unresolved; the loader never picks one and hopes.
- Any change to the migrated mods' behaviour, network messages or saved data.

## What reverse engineering already shows (and why it shapes the design)

Measured on a fresh Cpp2IL dump of build 25811160, 2026-10-23:

- Assembly-CSharp has 1,619 top-level types; 391 have obfuscated names (Malayalam letters), among
  them types the mods need, such as the ghost-state enum `ദശസറദബഹജഭ`.
- About 47% of fields are obfuscated. In `GhostAI` nearly every field is: the post-hunt cooldown
  flag the mods use is `ടഴദബടഠഹടന` at +0xD8, one of 13 unnamed bools in the class.
- Public methods, Unity messages (`Awake`, `Update`) and Photon RPCs keep their real names
  (`GhostAI.ChangeState`, `LightningController.PlayLocalLightning`,
  `LightningController.PlayLightningNetworked`). Other private methods and every parameter are
  obfuscated.
- The obfuscator adds same-signature copies of real methods. `GhostAI.Init(x)` has an alias
  `ഞയറജഭബഠഴമ(x)`; the linker folded both to one address (0x2022A70). `GhostAI.SetGhostModel` has a
  same-signature look-alike `ഞതപശഝമരജഡ` at a different address (0x2023DB0 vs 0x2024EA0); their lengths
  differ, so it is not a twin by the strict rule. So a signature alone does not identify a method, and
  a hook must know which bodies are the same behaviour.
- We have one game build, so whether obfuscated names survive an update is unknown. The design must
  work whether they do or not.
- Some behaviour exists only as code inlined into another method (the sensor pulse inside
  `GhostActivity.TryInteract`), and one hook is in UnityPlayer.dll, which has no IL2CPP metadata
  (the steering stub at UnityPlayer+0xA4FB7A).

Today the five mods hold about 145 distinct code addresses, about 295 hard-coded field offsets,
about 146 name lookups and about 145 hooks, in roughly 20,000 lines. `core/game.h` adds more
offsets (`LevelController+0x40`, `Network+0x28`, ...), and the host refuses any game build whose
hash differs from the audited one.

## The design

Everything in this section is Claude's proposal unless marked as the user's decision. The user has
approved the outline in conversation: three ways of finding things, readable names only, behaviour
notes in the map, a test that rejects raw addresses, hooks by name, and an offline check.

### Parts

Terms (agreed with the user): **the game SDK** is the whole kit, this project and the `sdk/`
directory: the game API, the behaviour notes and overviews, the fake backend for tests, the
offline check and the RE tooling that reads the map. **The game API** is the part mods call.
**The host contract** stays the name of `core/mod_api.h`, the existing contract between the host
and each mod (logging, hooks, messages, services).

1. **The map**: the declaration of every mapped game type and member. For each: its readable name,
   how to find it, its type, and its behaviour note. One source; everything else is derived from it.
2. **The resolver**: given a description of the game (its metadata and its code bytes), finds each
   map entry and checks it. The same resolver runs in two places: inside the game at startup, and
   offline against a copy of the game's files.
3. **The game API**: the typed C++ surface mods compile against. `GhostAI::ChangeState(ghost, state)`,
   `GhostAI::postHuntCooldown(ghost)`, `GhostState::Hunting`, `hook<GhostAI::ChangeState>(detour)`.
4. **The host side**: resolves the map once at startup, hands mods the result through the host
   contract, decides which mods can start, and owns all hooking.
5. **The offline check**: one command that runs the resolver against a game folder and reports.

### The map (Claude's call: declared in C++)

The map is written in C++ headers, one per game type, in a new top-level `sdk/` directory, using a
small declaration vocabulary (macros or constexpr tables; the mechanics are the builder's). It is
C++ because everything else here is: a typo fails to compile, there is no generator or new
toolchain, and the declarations and the typed surface mods use can be the same lines. If the
builder finds a single data file plus a generator inside the CMake build cleaner, that is
acceptable as long as there is still exactly one declaration and generation runs in the build.

Each entry records:

- **Readable name.** The game's own name where it has one. Where it is obfuscated, a name chosen
  from what the member does, in the game's naming style, marked as ours.
- **How to find it**, as one or more of the strategies below, tried in order.
- **Type information**: a method's parameters and return, a field's type, an enum's values, a static
  instance's type. Enum values are entries too: obfuscated enums may be reordered by an update.
- **Behaviour note** and **confidence**: `live` (seen working in the game, with the date),
  `decompiled` (read in decompiled code), or `inferred` (from the name or the type only). Notes
  include threading (main thread, host only), network effects (an RPC to every client), side
  effects, and known traps (for example that the game's `VoiceVolume.Update` throws every frame when
  the local player is cleared).
- **Requirements of use** where they exist: "call only on the host", "returns an IEnumerator; start
  it with a coroutine".

Each game system also has a short overview (how the hunt flows, how the idle roll chooses, how
evidence is given) that ties its classes together. These live beside the headers in `sdk/` and are
the first thing an agent reads about a system. The RE facts in Claude's memory notes move into the
entries and overviews; afterwards those memory notes only point at `sdk/`.

### Finding things: the strategies

Each entry names one or more of these, most robust first. The resolver tries them in order and
accepts an entry only when exactly one candidate survives every check the entry declares.

1. **By real name.** Namespace, class and member name, plus parameter count and types for methods
   (to separate overloads). Covers public methods, Unity messages, RPCs, and the readable fields and
   types.
2. **By shape**, for obfuscated members and types. The declaring type (itself found by name or
   shape), static or instance, return and parameter types, a field's type, and where needed its
   position among the type's fields of that type. The obfuscated name from the last mapped build is
   tried first, but only as a hint: it must still pass the shape checks.
3. **By code fingerprint**, to separate candidates that shape cannot (the twins above, the five
   bools beside the cooldown flag). Facts about a candidate's machine code: which mapped methods it
   calls, which mapped fields it reads or writes, which string literals or constants it uses, which
   mapped methods call it. Instructions are decoded with the HDE64 length disassembler already
   vendored inside MinHook, so this adds no dependency.
4. **By pattern inside a found function**, for things that exist only as inlined code: a short byte
   pattern with wildcards, searched only inside one resolved method's body, never across the whole
   binary. The UnityPlayer steering stub is found the same way, anchored on a function located
   through UnityPlayer's exports or a pattern unique within a bounded region. Pattern entries are
   the last resort, and each says why nothing better works.

Twins: a method entry resolves to its canonical body plus every twin that shape and fingerprint
show to be the same behaviour. Calls use the canonical body; a hook covers all of them, so a mod
never misses the calls that reach the other twin.

Checks: entries may declare assertions that must hold after a match (a field's type, a method
calling another mapped method, an enum value's number). They catch a match that is unique but
wrong after an update.

### What mods see

- Typed wrappers named after the game: classes, static instances (`LevelController::instance()`),
  methods, fields (read and write, with the GC write barrier for references handled inside),
  enums, coroutines and RPC names. The IL2CPP ABI traps already found (the reference-field
  set_value quirk, GC holding with `il2cpp::Held`, alive checks on destroyed objects, thread
  attach and detach) are handled inside the game API, not by each mod.
- Unity engine classes the mods use (`Transform`, `GameObject`, `Camera`, `NavMeshAgent`,
  `Physics`, `Animator`, `Renderer`, `Light`, the Input System, TextMeshPro, DOTween, Doozy) are
  mapped the same way. They are not obfuscated, so they resolve by name.
- **Hooks by entry**: a mod hooks a mapped method, not an address. The host's existing chained
  MinHook layer does the work (newest detour runs first, as today) and covers every twin.
- **Caller context**: a detour can ask whether it is running inside a given mapped method (for
  example "is this `RandomRangeInt` call coming from the weighted ghost-type roll"). The host tracks
  this for the mapped methods that need it by hooking them itself. It replaces return-address
  checks, which break when several mods chain detours on one function.
- **Required and optional entries**: everything a mod uses is known to the host before the mod
  starts. If a required entry did not resolve, the mod does not start, and the log and the in-game
  Mods panel name it and the missing entries. A mod may mark an entry optional and check it is
  available at run time.

Mod DLLs never contain resolved values. They receive them from the host when they start, so a game
update needs no mod rebuild. The host contract (`core/mod_api.h`) gains the map, and its version
goes up. The exact ABI is the builder's call, within the rules `mod_api.h` already states: plain
pointers, integers and C strings only, nothing thrown across it.

### Enforcement

- A source scan test, like the existing `held_scan_test`, fails the build if any mod, host or
  `core/` file outside the game API's own runtime contains a code address, a field offset, an
  obfuscated name, a direct IL2CPP export call, or a hook created on a raw pointer.
- Mods link only to the game API. The IL2CPP layer (`core/il2cpp.*`, `core/game.*`) becomes
  private to the loader.
- Mods find scene objects through scene-fact entries. The game API gives them no lookup by a raw path
  string and no setter that takes a raw layer number, so asset facts cannot drift back into mod
  code.
- Each check must be watched failing once: a deliberately raw offset in a mod, a deliberately
  broken entry, an ambiguous shape. Each must be reported as such.

### The offline check

- One command (a CMake-built tool plus a PowerShell wrapper, Claude's call) takes a game folder,
  copies the four files Cpp2IL needs into a working directory outside the game folder, runs Cpp2IL,
  and runs the same resolver over the result and over a copy of GameAssembly.dll's bytes. It never
  runs game code and never touches the game folder.
- It also reads the game's scene and asset files, with Python and UnityPy, to verify the scene and
  asset facts (object paths, layers, Animator parameters). The user approved this dependency on
  2026-10-23, for the offline check only. The build and the mods do not depend on it.
  - UnityPy's version is pinned.
  - Python runs isolated (`-I`), on copies of the asset files outside the game folder.
- The report lists, for every entry: resolved (and whether the obfuscated name or the address
  changed since the last mapped build), ambiguous (with its candidates), or missing. Then it lists
  which mods each failure would switch off. It exits non-zero when a required entry fails.
- When every required entry passes, the check records the build as checked. The record holds the
  GameAssembly.dll and UnityPlayer.dll hashes, and is what lets the host run on that build (see
  "Failure handling at startup").
- The game folder and the Cpp2IL executable come from arguments or environment variables, never
  from paths in the repository. Unconfigured, `validate.ps1` reports the check as not configured
  instead of failing, so a fresh checkout still builds and tests.
- The same command writes the dump, a table from every map entry to its address in this build,
  and `re/names.tsv` to fixed, gitignored paths. The RE scripts (`re/decomp.mjs`, `re/slimpe.mjs`)
  accept map names as well as addresses. An agent never has to look for a dump: it runs the command
  or reads the fixed path, and the tools say which command to run when the path is empty.

### What gets mapped (decided by the user: main systems up front)

Completely mapped means: every type in the system's inventory has every member in the map, found,
named and typed, with a behaviour note of at least `decompiled` confidence for methods with bodies
and fields the code reads or writes. The exceptions are explicit and give a reason: a twin folded
into its canonical entry, a compiler-generated class covered by its parent method's note, or the
anti-cheat. A coverage report, run as a test, counts unmapped members per type, and is zero when
the work is done.

The systems, with the root types each inventory starts from (Claude's list; the builder extends
each inventory to the game types its roots depend on):

- **Ghost AI and states**: `GhostAI`, its state machine and every state class, `GhostActivity`,
  `GhostInfo` and the ghost types, `GhostController`, `GhostInteraction`, `GhostAudio`,
  `GhostModel`, ghost events and abilities, the favourite room and wandering.
- **Hunts**: the hunt state, hunt speed and line-of-sight speed-up, targets, hiding, crucifix,
  smudging, salt, hunt telegraphing (flicker, audio).
- **Evidence and equipment**: every evidence type and the equipment that shows it (EMF, Spirit Box,
  ghost writing, freezing temperatures, ghost orbs, D.O.T.S., ultraviolet, candles, lighters and
  firelight), the equipment base classes and inventory, photos, cursed possessions.
- **Weather and environment**: `LightningController`, weather, the fuse box, lights and switches,
  doors and their locks, `PhotonObjectInteract` props, sound and footstep systems.
- **Players**: `Player`, `LocalPlayer` and the PC player, sanity, death and the dead state, voice
  (Vivox taps the mods use), input actions and bindings, the player camera.
- **Level and rooms**: `LevelController`, `LevelRoom`, `LevelValues`, `SetupPhaseController`,
  the truck and its map and monitors, `Network` and the player spots.
- **Journal**: `JournalController`, the evidence and ghost pages, the journal's networked
  selections.
- **Lobby and menus**: the main menu UI, the lobby board and ID cards, difficulty and custom
  options with their save and sync (ES3), the Doozy UI the suite reuses.
- **Networking**: PUN and Photon Realtime as the suite uses them: rooms, players, actors, RPCs,
  raised events.

Money, experience and collectable rewards are mapped like everything else so the suite's existing
behaviour keeps working; mapping them changes nothing about what the mods do with them.

Where an inventory stops (Claude's call):

- A game type (Assembly-CSharp or Assembly-CSharp-firstpass) belongs to a system when the system's
  roots use it for the system's behaviour. It is then mapped completely.
- A game type that only appears elsewhere as a field's or parameter's type is mapped as a type, so
  the field is typed. Its members are mapped when a system claims it.
- Unity engine classes and third-party libraries (PUN and Photon Realtime, ES3, Doozy, TextMeshPro,
  DOTween, SensorToolkit) keep their real names and have their own documentation. They are mapped
  member by member: everything the suite uses, plus any member a system note refers to. The
  coverage report does not count their other members. Adding one later is a name-only entry that
  needs no reverse engineering.
- Each system's inventory is written into its overview before its members are mapped, so its
  boundary can be read and reviewed.

### Game facts outside the metadata

Some of what the mods depend on is not in the IL2CPP metadata. These facts are map entries too,
owned by the loader like every other game fact (the user's rule that the loader does all the
interfacing):

- **Scene and asset facts**: GameObject paths and names (`Content/GhostHuntingBookContent` and about
  35 more in the journal code), layer numbers (PlayerView 11), Animator parameter names.
  - At run time, layers resolve by name (`LayerMask.NameToLayer`).
  - Animator parameters are checked against the Animator's own parameter list.
  - A path is checked when its scene loads. One that fails is reported by name like any other
    entry, and the feature that needs it stays off.
  - The offline check also verifies them from the game's asset files, before launch (decided by
    the user; see "The offline check").
- **Runtime internals**: icall names (`UnityEngine.Animator::SetBoolID`) resolve through the runtime
  by name. The IL2CPP exports the host hooks (`il2cpp_shutdown`) are entries of their own kind.
- **Native UnityPlayer patches**: the steering stub, its register-level assumptions (RBX,
  XMM14/15, [RSP+24h]), and the CrowdAgent and NavMeshAgent internals behind it move out of
  Playable Ghost into the loader, as a native-patch entry kind the host owns. The mod asks the game
  API for the effect (steer the ghost's agent this frame), not for the stub.
  - UnityPlayer has no metadata, so these resolve by exports and by patterns within bounded
    regions, with checks.
  - They are the most fragile entries, and the report lists them as such.

### Engine version (Claude's call)

The IL2CPP object layouts the suite relies on (the object header, array data at +0x20, `String`,
`List`, `MethodInfo`'s method pointer, the reference-field write quirk) belong to the host's game API
runtime, not to code compiled into mods. That keeps a Unity upgrade loader-only work as well.

Supporting a second Unity or metadata version is not part of this work. The host and the offline
check detect an engine or metadata version they do not know, and refuse it with a clear report.
The announced move to Unity 6, and any change of networking library, will need their own work
when they ship.

### Failure handling at startup

- Which builds the host runs on (decided by the user, 2026-10-23):
  - It runs mods only on a build the offline check has passed. This keeps today's rule that
    nothing runs modded on a game patch nobody has looked at, which also guards against a changed
    anti-cheat.
  - The passing check writes its record. On an unchecked build the host stays inactive, as today,
    and the log names the command to run.
  - The compiled-in hashes in `host/build_check.cpp` are replaced by these records.
- Where the record lives (Claude's call): a small text file per checked build. It is committed under
  `sdk/builds/` and shipped in the release package. Other players get it with the package (through
  the updater, `2026-10-23-updater-design.md`), and a game update that breaks nothing needs a new
  release, not a code change.
- What a record holds (Claude's call): the GameAssembly.dll and UnityPlayer.dll hashes, and a
  digest of the map it was checked with.
  - A record is stale when the map has changed since its check. A new or changed entry has then not
    been checked against that build.
  - The host treats a stale record as no record.
  - The release script and CI refuse to publish with a stale record for the build being supported.
- The host resolves the whole map once, before any mod starts, on its own thread where IL2CPP
  allows and on the main thread where it does not (builder's call), inside the existing 60-second
  startup deadline.
- Every unresolved or ambiguous entry is logged once, by readable name, with which strategy failed
  and the candidates seen.
- Mods whose required entries all resolved start. The rest stay off, and the Mods panel shows them
  as unavailable with the reason, in the game's own UI (the vanilla-UI-only rule).
- An entry that resolves but fails its declared checks counts as unresolved.

### Migration

- `core/game.*`, `core/ui.*`, `core/bindings.*`, the host and all five mods are rewritten against
  the game API. Every hard-coded address and offset is replaced by an entry, and every name lookup
  by an entry. Each mod's behaviour stays exactly as it is.
- The offline tests keep their coverage. Their fakes, which today lay out fake objects at
  production offsets, move to the SDK's fake backend: a test declares fake game objects and the
  game API resolves entries against them. The resolver itself is tested against small synthetic
  metadata: a renamed member, an ambiguous shape, a twin, a missing type, a failed check.
- The archived mods (`archive/replay`, `archive/bigger_lobbies`) are not built and stay as they
  are; they move to the game API only if they are brought back.
- Twins during the migration (Claude's call):
  - Every hook stays on exactly the body it hooks today. That is what lets the offline check prove
    no target moved.
  - The offline check lists every hooked method that has a separate twin.
  - After the migration, each such twin is reverse-engineered: does the game reach it, and from
    where. Widening a hook to cover it is a recorded change, reported to the user.
  - New hooks cover every twin by default.

### Verification

- Gates 1 and 2: the build, the source scan, the resolver's unit tests, every mod's offline tests
  on the fake backend, and the coverage report. All in `validate.ps1`, run through `harness-verify`.
- The offline check against the installed build: every entry resolves, and every address and
  offset agrees with the values the mods hard-code today. This is the proof that the migration
  changed no target.
- Gate 3, in the game: inject the rebuilt suite (no files in the game folder, the user launching
  the game), confirm the log reports the whole map resolved, and repeat each mod's existing live
  checks. Every live test follows the anti-cheat rules: the user is told the ban risk first and
  starts the game themselves.
- An update drill: run the offline check against a deliberately altered copy (a renamed
  obfuscated member, a shifted field) to show that the report names exactly those entries.

### The reverse-engineering method

The user asked for heavy use of reverse engineering. Each system is mapped by:

1. Inventory from the Cpp2IL dump: its types, members and the fields they share.
2. Batch decompilation of every method with a body (`re/decomp.mjs` with headless Ghidra, or REA on
   slim PE slices from `re/slimpe.mjs`), callers and callees resolved to names.
3. Naming the obfuscated members from what the code does, and writing each note.
4. Live checks for what the mods depend on, and for any note where decompiled code leaves doubt
   about runtime behaviour.

The work can be split across agents one system at a time, because the map has one header per type
and each system's inventory is independent. The plan decides the order. Claude's suggestion, in
stages:

1. Build the runtime, resolver, game API mechanics and offline check with a handful of entries.
2. Map and migrate everything the current mods use, which proves the design on code that already
   works.
3. Map the remaining systems.

Each stage leaves the suite building, tested and working, so the migrated suite is usable before
the last systems are mapped. The work is finished when the coverage report reaches zero, not
before.

## Constraints

- C++ with MSVC, CMake and Ninja; MinHook 1.3.4 vendored; each module links its own static C
  runtime. HDE64 is already vendored with MinHook. The only new dependency is Python with UnityPy,
  for the offline check (approved). Anything else needs the user's approval first.
- Every game call on Unity's main thread unless the IL2CPP runtime documents otherwise; worker
  threads detach as soon as they stop making managed calls (exit hang).
- The suite still loads by injection into a running game. No file is placed in the game folder.
- Nothing tied to this machine in the repository: game and tool paths come from arguments or the
  environment.
- Derived game data (the dump, the address table, `names.tsv`, decompiled output) stays out of the
  repository, as `.gitignore` already requires. The map itself (names, shapes, fingerprints,
  notes) is committed.

## Left to the builder

- The declaration mechanics in C++, the game API's exact naming conventions for invented names, and the
  ABI that carries resolved entries to mods.
- How each fingerprint is computed and stored, and how much of the resolver runs before versus on
  the main thread.
- The offline tool's internal model of the dump, and whether it reads Cpp2IL's output or another
  Cpp2IL output format.
- The order of work, and how the systems are split between agents.
- Whether system overviews are Markdown beside the headers or comments at the top of an index
  header, as long as there is one per system and nothing is said in two places.

## Third-party mods (added 2026-10-23)

The user, mid-build: "make sure that the SDK is fully documented and contains everything that someone
would need to know to make their own mod without reverse engineering the game. I will bundle my mods
with the mod loader, but I'd like for other people to be able to use the documented SDK to make their
own mods and be able to toggle them in-game the same way the included mods are."

### Decided by the user

- Players put other people's mods in `%LOCALAPPDATA%\PhasmoMods\mods\<mod folder>\` (a DLL and its
  `mod.ini`), beside the settings, so loader updates never touch them. The bundled mods stay in the
  install folder and update with the loader.
- Every GitHub release carries a `PhasmoMods-SDK-<version>.zip`: the headers, the libraries to link,
  the generated game API reference and the guides, a template mod that builds out of the box, and
  the fake game backend for offline tests. Modders never need the source tree.
- Compatibility is versioned like a library. Within a major version, releases only add: a mod built
  against an older minor version keeps loading without a rebuild. Game API entries are never removed
  or changed silently; they are marked deprecated first and removed only in a major release. A major
  release refuses older mods and the loader names the mods that need rebuilding.

### How it works (Claude's calls)

- **Same treatment as bundled mods.** A third-party mod appears in the launcher's list, the in-game
  Mods panel, the consent notice and the join guard exactly as a bundled one does; it is switched on
  and off the same way, and starts off when it first appears (the updater's rule).
- **Discovery.** The launcher and the host read both folders. A mod id found in both is taken from
  the install folder and the other copy is reported. `mod.ini` gains an optional `author=` line, and
  the Mods panel shows the author for third-party mods.
- **Network channels.** Bundled mods keep their fixed channels (1 to 63, reserved). A third-party mod
  declares channel 0, and the host gives it a channel from 64 to 255 derived from a stable hash of its
  mod id, so every client agrees without negotiation; two installed mods that land on the same channel
  are both refused with a message naming them.
- **Versioning.** `PGMOD_API_VERSION` becomes a major and a minor number. The host accepts a mod with
  the same major and a minor no newer than its own. `PgHostApi` and every published struct only ever
  grow at the end. Each game API entry and host contract function records the minor version that
  added it, and deprecation is an attribute that the build turns into a compiler warning.
- **Consent.** When any third-party mod is installed, the notice adds one line: mods from other
  authors run with the same access to the game and the computer as the game itself.
- **Documentation**, in the SDK zip and in `sdk/` in the repository:
  - A game API reference generated in the build from the map headers: every class, method, field,
    enum value and scene fact, with its behaviour note, confidence, threading and network notes, and
    the version that added it. Generated, so it can never drift from the map.
  - The system overviews (`sdk/systems/`).
  - Guides: getting started with the template; the mod lifecycle (`pgmod_info`, start, tick, stop,
    `pgmod_requires`); the host contract (logging, hooks by entry, caller context, network messages
    and channels, services between mods, peer checks); building UI from the game's own widgets (the
    vanilla-UI rule); testing with the fake backend; packaging and installing; what the consent
    notice and the join guard do with a mod; the anti-cheat and private-lobby rules.
  - A documentation test fails the build when any map entry, public header declaration or host
    contract function has no documentation.
- **When it ships.** The SDK zip is published once stage 2 (every mod on the game API) is done,
  because until then the bundled mods still use the IL2CPP layer directly and the API is not yet the
  only way in. The mods folder, the channel scheme and the versioning land with stage 2 as well; the
  versioning rule is applied to the host contract from its version 2 (foundation Task 9) onwards.

### Changes to the anti-goals for outside authors

- Nothing in the SDK or its documentation describes the anti-cheat, how it detects mods, or how to
  avoid it. The guides state the private-lobby rule and the ban risk, nothing more.
