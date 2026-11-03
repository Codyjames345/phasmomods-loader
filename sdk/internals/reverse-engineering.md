# Mapping the game: tools and pitfalls

For whoever extends the SDK: how the game's code is read, and the mistakes earlier work made, so nobody makes them again.
The rules for an entry are in the [SDK guide](../README.md#mapping-the-game). This page names map keys and class names,
never addresses: the numbers belong to one build, and `re/decomp.mjs <key>` finds a key again on any build. Raw numbers go
only in the gitignored `re/dump/`.

**Work on copies.** Every tool reads copies of the game's files outside the game folder. Nothing is ever written in the game
folder, and nothing here touches or describes the game's anti-cheat.

## What the game is made of

- **GameAssembly.dll**: the game's C# compiled by IL2CPP. Every game method is native code here.
- **global-metadata.dat**: the names, types and layouts IL2CPP keeps. Many of the game's own class and member names are
  obfuscated (non-ASCII), while Unity's, Photon's and most libraries' are not.
- **UnityPlayer.dll**: the engine. It keeps every engine call (icall) registered by name, even where the managed method
  that called it was stripped.
- **The scene and asset files** under the game's data folder: scenes, prefabs, materials and shaders.

## The tools

`tools/gamecheck.ps1` makes the dumps everything else reads ([Commands](../README.md#commands)): a Cpp2IL DiffableCs dump in
`re/dump` (one C# file per class, with each field's offset as a comment), `re/names.tsv` (each method's address and
signature) and `re/map-addresses.tsv` (each map key's resolved value). Then:

| Script | What it does |
| --- | --- |
| `re/decomp.mjs` | Decompiles functions by map key (or address) with headless Ghidra (`re/Decomp.java`), naming the callees it knows. Needs a one-time import into `re/ghidra` and `GHIDRA_INSTALL_DIR`. |
| `re/callers.mjs` | Lists the direct call sites of a function, with the method each lies in: a fast stand-in for cross-references over the whole binary. |
| `re/slimpe.mjs` | Builds a slim copy of GameAssembly.dll that holds only the chosen functions, at their real addresses, with every known method as an export, for an interactive Ghidra session. |
| `re/strings.mjs` | Dumps IL2CPP's string literals from the metadata. |
| `re/names.mjs` | Builds `re/names.tsv` from a DiffableCs dump. |
| `tools/assetcheck.py` | Checks scene and asset facts with the pinned UnityPy ([Scene and asset facts](../systems/scene.md)). |

- **Full analysis of GameAssembly.dll does not finish** in an interactive Ghidra session's time limit, and reopening it
  analyses it again. Decompile by key with `re/decomp.mjs`, or open a slim PE of the few functions you need. Keep a slice to
  a few megabytes of code; a ten-megabyte slice timed out on reopening.
- **Scene files with UnityPy.** A MonoBehaviour's type tree is stripped from the scene files, so read its script reference
  raw; the script names live in `globalgamemanagers.assets`. The menus are in the scene the main menu loads.

## Reading obfuscated code

- **Decoys.** Obfuscated classes carry look-alike methods of the same size that do nothing the game uses. Never take the
  first match: find the real method by what it does (the state it changes, the call it makes), by its callers
  (`re/callers.mjs`), or by the metadata slot that names it, and write that as the entry's fact. The resolver never picks
  among candidates, so an entry whose facts fit two methods stays `Ambiguous` until a better fact is found.
- **Compiler-made methods.** `async` methods, coroutines and lambdas compile to classes of their own; an `async void`
  listener can inline a whole method, so a hook on the named method sees nothing (`ServerManager.StartGame` is one:
  [Photon](../systems/photon.md), "How a contract starts").
- **Watch effects, not names**, when a state class has many same-shaped methods: hook what they call
  (`GhostAI.ChangeState`, a door's force) rather than guessing which is the state's entry.

## Stripped methods and engine calls

- IL2CPP strips managed methods the game never calls, so a method in Unity's documentation may be missing here. Check
  `re/names.tsv` before writing an entry for one. Ones found missing: `NavMeshAgent.desiredVelocity` and `angularSpeed`,
  `AudioSource.PlayClipAtPoint`, `AnimatorControllerParameter.nameHash`, `CanvasRenderer.SetAlpha`, and the managed sides of
  `VideoPlayer` and `AsyncGPUReadback`.
- The engine call behind a stripped method usually survives in UnityPlayer.dll: search its strings for `Class::method`, and
  map it as an icall entry. In this engine version (Unity 2022.3) an icall takes the object (and an out pointer for a
  struct result) and no method info.

## The IL2CPP runtime's own pitfalls

- **Setting a reference field.** The runtime's field setter stores the value it is given itself for a reference-type
  field, with no dereference, while the getter writes through its out pointer. Passing the address of a pointer stores a
  stack address. Offline fakes cannot catch this: check an export's behaviour by disassembling it.
- **Native threads.** A thread attached to the runtime must detach as soon as it stops making managed calls: the runtime's
  shutdown waits for attached threads, so the game's quit hangs forever.
- **The collector does not see a DLL's memory**, and Unity keeps only weak handles on most engine objects' wrappers. Keep any
  managed object across frames in a `pm::api::Held` (the SDK guide's rules).
- **Static singletons outlive their scene.** `LevelController`, `GameController` and `GhostController` keep their static
  instance after their scene is gone, so "in a contract" read from them alone is true in the lobby. Check the object is alive.

## Unity and the game's scenes

- **Hidden objects.** `FindObjectOfType` without `includeInactive` skips inactive objects. The lobby's difficulty board is
  hidden unless the player is on it, so a search for its menu found nothing until the board was opened: use the overload that
  includes inactive objects (the map has `Object.FindObjectsOfType(Type,bool)`).
- **The journal is a 3D book.** Its content is rotated a quarter turn about X and each page a quarter turn back, so anything
  new placed under it needs the page's rotation or it renders edge-on. `JournalController` switches the book's Canvas off
  while it is closed, so a copy taken closed must switch it on.
- **What the player holds renders last.** Held items and the open journal render in URP's own passes for the PlayerView
  layer, after the normal transparent pass; the journal opens only because `LocalJournalController.OpenCloseJournal` moves
  its canvas to that layer. Copy the game's own steps from the player's rig; never invent a placement or a render order.
- **Cameras.** URP renders each camera through its camera stack; the engine's `Camera.FireOnPreCull` and the render
  pipeline's begin-camera event never fire here (seen in a live run). Moving the real player camera breaks gameplay (in one run the ghost
  then killed that player): move only what renders.
- **The picture.** The project renders in linear colour space with URP's ACES tonemapping. A material's HDR colour becomes
  very large in linear light, bright blues turn purple and bright greens lime under ACES, and the colour grading's lookup
  table clamps very bright values. [Spectral Flame](../systems/flames.md) models the whole chain.

## Hooks

- **Several detours can sit on one function.** The newest runs first and reaches the older one through `original`, and mods
  load in order, so a return address inside a detour may be another mod's detour. To know who called, hook the caller and
  raise a flag around it; never read the return address, unless yours is the only detour on that function.

## The project's own bugs, to recognise them again

Each of these looked at first like the game's fault, and each was ours. The unmodded game exits and leaves rooms fine, so
look for the cause in our code first.

- **Quitting hung** on a native thread left attached to the runtime; on probing a socket handle from inside a detour; and on
  a PhotonView kept alive outside `PhotonNetwork.Destroy`, which breaks Photon's room cleanup
  ([Photon](../systems/photon.md), "PhotonViews and the cleanup").
- **A collected wrapper** read as alive, and the next engine call crashed: a raw pointer kept across frames.
- **Errors every frame** after taking the local player out: game code that assumes a local Player, such as
  `VoiceVolume.Update` ([Voice](../systems/voice.md)). After a change like that, read the game's log for an
  `at <Class>.Update ()` line repeated many times.
- **A slower client never got the house key** when a mod destroyed a player's networked body: each client's all-loaded wait
  needs every `PlayerSpot.player` alive.
