# Mapping the game: tools and pitfalls

For whoever extends the SDK: how the game's code is read, and the mistakes earlier work made, so nobody makes them again.
The rules for an entry are in the [mapping page](mapping.md). This page names map keys and class names,
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

`tools/gamecheck.ps1` makes the dumps everything else reads ([Commands](mapping.md#commands)): a Cpp2IL DiffableCs dump in
`re/dump` (one C# file per class, with each field's offset as a comment), `re/names.tsv` (each method's address and
signature) and `re/map-addresses.tsv` (each map key's resolved value). Then:

| Script | What it does |
| --- | --- |
| `re/decomp.mjs` | Decompiles functions by map key (or address) with headless Ghidra (`re/Decomp.java`), naming the callees it knows. Needs a one-time import into `re/ghidra` and Ghidra's `support` folder on PATH. |
| `re/callers.mjs` | Lists the direct call sites of a function, with the method each lies in: a fast stand-in for cross-references over the whole binary. |
| `re/slimpe.mjs` | Builds a slim copy of GameAssembly.dll that holds only the chosen functions, at their real addresses, with every known method as an export, for an interactive Ghidra session. |
| `re/strings.mjs` | Dumps IL2CPP's string literals from the metadata. |
| `re/names.mjs` | Builds `re/names.tsv` from a DiffableCs dump. |
| `tools/assetcheck.py` | Checks scene and asset facts with the pinned UnityPy ([Scene and asset facts](../systems/scene.md)). |

- **Ghidra refuses a project path with a folder whose name starts with a dot** ("Path element starting with '.' is not
  permitted"), so `re/decomp.mjs` fails inside a git worktree kept under a dotted folder. Copy `re/ghidra` to a path
  without one and point a copy of the script at it.
- **A scene's components without a type tree** still name their scripts: UnityPy reads the script reference raw, and the
  scene's GameObjects give their layer. The physics layer matrix (`PhysicsManager.m_LayerCollisionMatrix`) and the layer
  names (`TagManager`) are in `globalgamemanagers`; that is how the ghost was shown to pass through doors
  ([ghost AI](../systems/ghost-ai.md), "Moving the ghost outside a hunt").
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

## What the decompile leaves unnamed

- **RPC names and other string literals.** A decompiled `PhotonView.RPC` call names its method through a global that the
  runtime fills at startup. In the copied GameAssembly.dll that global still holds its metadata usage: the low 29 bits
  are the index shifted left by one, the top three the kind (5 for a string literal, 1 for a type). For a string, the
  index is the literal's number in `re/strings.mjs`'s list; that is how `Door`'s RPCs were told apart
  ([doors](../systems/doors.md)).
- **Which class a type global is.** For kind 1 the index is a type's index, not its definition's: find the definition
  in `global-metadata.dat` whose by-value type index is that number, and read its name. This named the coroutine class a
  `Door` method starts, and the static instances other methods read (`LevelController`, `GameController`).
- **Two methods folded onto one address.** The linker folds bodies that compile identical, and `re/names.tsv` then lists
  each original under that one address, so a decompile can name a call after the wrong one. `WaitForSeconds`' float
  constructor shares an address with `MeshCombineStudio.MeshCombiner/LODSettings`'s, which made `ExitLevel`'s leaving
  coroutine look like it yielded a LODSettings object. The type global the allocator just before takes is the class the
  object really is (previous bullet), so read the allocation, not the constructor name.
- **Virtual calls** show as a call through the object's class at a fixed offset. A class's new virtual methods take
  slots in the order of their metadata tokens, sixteen bytes each, so two known slots fix the rest; a byte search for
  indirect calls at that offset then finds a virtual method's callers, which `re/callers.mjs` cannot.
- **Cpp2IL's `override` and `virtual`** are not reliable for the game's own classes: it marked `Door.LockDoor` an
  override and `HingeDoor.LockDoor` virtual.

## A component's fields in a scene

UnityPy cannot parse a MonoBehaviour without its type tree, which the game's scenes strip. Read its raw data instead:
after the header (the GameObject's PPtr, the enabled byte padded to four, the script's PPtr and the name string) come the
serialized fields in the order the class declares them. A field is serialized when it is public or carries
`[SerializeField]` (Cpp2IL's dump shows the attribute; `[HideInInspector]` fields are still serialized), and not static,
const or `[NonSerialized]`. A PPtr is twelve bytes (a 32-bit file index and a 64-bit path id), each bool takes four with
its padding, a string or array is a 32-bit count then its items, padded to four, and a nested class's fields follow in
place. The script's name is the `MonoScript` in `globalgamemanagers.assets` the script PPtr points to. Check a parse
against what must hold (sane array counts, an enum in range) before trusting it: every map's exits and room kinds were
read this way ([doors](../systems/doors.md)).

## Stripped methods and engine calls

- IL2CPP strips managed methods the game never calls, so a method in Unity's documentation may be missing here. Check
  `re/names.tsv` before writing an entry for one. Ones found missing: `NavMeshAgent.desiredVelocity` and `angularSpeed`,
  `AudioSource.PlayClipAtPoint`, `AnimatorControllerParameter.nameHash`, `CanvasRenderer.SetAlpha`, and the managed sides of
  `VideoPlayer` and `AsyncGPUReadback`.
- The engine call behind a stripped method usually survives in UnityPlayer.dll: search its strings for `Class::method`, and
  map it as an icall entry. In this engine version (Unity 2022.3) an icall takes the object (and an out pointer for a
  struct result) and no method info.

- An in-game test of a body copy, clips loaded from an AssetBundle and runtime Cloth, with the bundle's build settings, is in
  [Engine experiments](engine-experiments.md).

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
- **Taking the local player out.** Playable Ghost once cleared `Network.localPlayer` on the ghost player's client. Hundreds of
  game methods read it, many with no check: thousands of errors a contract from `FootstepController.AttemptPlaySound`,
  `EVPRecorder.Update` and `VoiceVolume.Update`, the investigators' footsteps lost, and `SanityEffectsController.Update`
  returning before its timer reset. The fatal one was `GhostAudio.PlayOrStopAppearSourceSync`: an RPC to all runs inline on
  the sender, so its exception reached the ghost's state change, which logs `StateMachine ChangeState Error` and calls
  `GhostAI.ChangeState` again. That throws again, until the stack runs out. Keep the reference set and move what the player
  sees instead. After any change like that, read the game's log for an `at <Class>.Update ()` line repeated many times.
- **A slower client never got the house key** when a mod destroyed a player's networked body: each client's all-loaded wait
  needs every `PlayerSpot.player` alive.
- **Opening the dev menu's new scroll list faulted the game** (2026-11-12): a GameObject was asked for its transform through
  `Component.get_transform`. The list's viewport and content are GameObjects, and the layout helper asked every object the
  same question the same way; a GameObject is not a Component, so the engine read a layout that object does not have. The
  chain was read, not guessed. Our own log names the last line before the fault, so it brackets the call. Unity writes a
  crash folder with a `crash.dmp`; a debugger reads the exception record from it, which gives the address read and the
  instruction that read it. And a mod DLL relinked with the linker's map file, its `.text` compared byte for byte first so
  the map belongs to the code that actually ran, puts our own function names and offsets under the frames of that stack.
  What to look for is a member typed `RectTransform` or `Component` handed something else: `ScrollRect.content`,
  `ScrollRect.viewport`, the `RectTransformUtility` hit tests, every `RectTransform.set_*`. An object made at run time has no
  RectTransform until it takes a component whose class requires one, so laying it out before that is the same mistake one
  step on. The answer is to ask the object, never to assume: `pm::api::Type::is` on the class in question, as
  `pm::game::transform_of`, `game_object_of` and `rect_transform_of` now do.
