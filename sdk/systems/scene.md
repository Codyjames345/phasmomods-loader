# Scene and asset facts

Some of what the mods depend on is not in the game's code: where a GameObject sits in a scene or a
prefab, which index a layer has, which parameters an Animator's controller has. These facts are map
entries of three kinds (`sdk/api/entry.h`): `Scene`, `Layer` and `AnimatorParam`. The entries are in
`sdk/map/Scene.h`; their `///` notes are the authority, and this page says where the facts live in the
game's files, how they are read, and how they are checked. Facts were read from the files of Steam
build 25811160. This page copies no game data: run the check to see it.

## How they are checked

- **Offline**, `tools/assetcheck.py` reads copies of the game's files with UnityPy (pinned in
  `tools/assetcheck-requirements.txt`). `tools/gamecheck.ps1` runs it after `gamecheck.exe`: gamecheck
  lists each scene fact as `SCENE <key>` and writes them to `re\dump\scene-entries.tsv`
  (`--scene-entries`); assetcheck names the data files it needs (`--needs`), the script copies them to
  `re\dump\game\Phasmophobia_Data` (a copy whose size and time match is kept), and assetcheck prints
  `OK` or `MISSING` for each entry. A required entry that is missing fails the check.
- **In the running game**, the host checks each fact where it is used, never on the startup worker
  thread:
  - a layer by its name through `LayerMask.NameToLayer`, on the main thread when the host activates;
    a name no layer has leaves the entry unresolved, and `Layer::index()` answers -1;
  - a path when a mod first looks it up (`SceneObject::find`), through `Transform.Find`. The answer is
    kept until the active scene's handle changes (`SceneManager.GetActiveScene`; every scene load gives
    a new one), the object found is destroyed, or another root is given. A path that is not there is
    logged once per scene load and answers null;
  - an Animator parameter when a mod first asks for it with an Animator (`AnimatorParam::hash`),
    against that Animator's own `parameters`, and again after a scene load. One the Animator lacks is
    logged once per scene load and answers 0.

  The shared logic is `sdk/runtime/scene_facts.cpp`; the host gives it the running game
  (`host/scene_engine.cpp`) and the fake backend its own scene, so the tests run the host's code.

## Where the facts live

`Phasmophobia_Data` holds, among others:

- `globalgamemanagers`: the build settings (the scenes, in build order), the tag manager (the 32
  layer names) and the Resources container (each path `Resources.Load` takes, lower-cased, with a
  pointer to its object). Engine objects here keep their type trees.
- `globalgamemanagers.assets`: the MonoScript objects, which give each script's class and namespace.
- `level<N>`: scene N of the build settings, by its index. The main menu is `level2` (`Main_Menu`).
  Seasonal copies of some scenes (menus and maps with an event suffix in their names) are separate
  scenes with their own `level<N>`, so a fact that must hold in a seasonal variant needs that scene
  named as well. One scene name is listed twice in this build, so a scene name may stand for several
  files; the check reads them all.
- `resources.assets`: everything under a `Resources` folder, the player prefabs among them. The journal
  is not in any scene: it is part of each player prefab (`PCPlayer`, `VRPlayer (OpenXR)` and the older
  `OldPCPlayer` and `OldVRPlayer`), under the object that carries `JournalController`. Of the files
  without a `.resS` suffix, only `resources.assets` names `GhostHuntingBookContent`.
- `sharedassets<N>.assets`: assets the scenes and prefabs refer to, Animator controllers among them.
  The check copies one only when an object it reads refers to it.
- `.resS` and `.resource` files hold texture, mesh and audio data, which the check never reads.

Files to copy for the current entries: `globalgamemanagers`, `globalgamemanagers.assets`,
`resources.assets` (about 1 GB) and `sharedassets0.assets`.

## Reading them

- Engine objects (GameObject, Transform, RectTransform, Animator, AnimatorController) read with
  UnityPy's own type trees for the build's Unity version (2022.3).
- A MonoBehaviour's type tree is stripped, so its fields cannot be read by name. Its script is the
  PPtr at byte 16 of its raw data (after `m_GameObject`, 12 bytes, and `m_Enabled`, aligned to 4): a
  file index into the reading file's externals (0 for the file itself) and a path id. The MonoScript
  there names the class.
- A path follows `Transform.Find`: each segment is a child's name, children in their Transform's
  `m_Children` order, inactive ones included. The game takes the first match, so the check requires
  exactly one: a second object at the same path would make the game's choice silent.
- An AnimatorController's parameters are `m_Controller.m_Values` (each an id, the name's hash) with the
  names in `m_TOS`. The check follows each Animator's `m_Controller`, through an override controller to
  the controller it overrides.
- The main menu's lobby board, `Game Lobby` (the object carrying `ServerManager`), sits in `level2`
  under an object named `Main Menu UI Canvas - DISABLE FOR BAKE`, not `Main Menu UI Canvas`: name the
  canvas by a component, not by its name, when it is mapped.

## Adding a scene fact

1. Read where it lives: `assetcheck.py --data <copy> --entries <tsv> --record <json>` writes what the
   check reads of the places the entries name (keep the output out of the repository: it is game
   data).
2. Write the entry and its `///` note in `sdk/map/Scene.h`, add it to `kAllEntries`, and give it a
   `phasmo::` wrapper (`SceneObject`, `Layer` or `AnimatorParam`).
3. Run the configured `.\tools\gamecheck.ps1`. The entry must be `OK`.
