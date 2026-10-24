# Scene and asset facts

Some of what the mods depend on is not in the game's code: where a GameObject sits in a scene or a
prefab, which index a layer has, which parameters an Animator's controller has. These facts are map
entries of three kinds (`sdk/api/entry.h`): `Scene`, `Layer` and `AnimatorParam`. The entries are in
`sdk/map/Scene.h`, and their `///` notes hold what is known of each. This page says which of the game's
files hold such facts, how they are read, and how they are checked. It was written against Steam
build 25811160 and copies no game data: run the check to see it.

## How they are checked

- **Offline**, `tools/assetcheck.py` reads copies of the game's files with UnityPy. `tools/gamecheck.ps1`
  runs it after `gamecheck.exe`: gamecheck lists each scene fact as `SCENE <key>` and writes them to
  `re\dump\scene-entries.tsv` (`--scene-entries`); assetcheck names the data files it needs
  (`--needs`), the script copies them to `re\dump\game\Phasmophobia_Data` (a copy whose size and time
  match is kept), and assetcheck prints `OK` or `MISSING` for each entry. A required entry that is
  missing fails the check.
- **In the running game**, the host checks each fact where it is used, never on the startup worker
  thread, and only when the engine entries it needs resolved (Transform, Transform.Find and
  SceneManager.GetActiveScene for a path; LayerMask.NameToLayer for a layer; Animator, its
  `get_parameters` and `StringToHash`, and AnimatorControllerParameter's `m_Name` for a parameter).
  Otherwise the fact stays missing, naming the engine entry that did not resolve.
  - A layer resolves by its name through `LayerMask.NameToLayer`, on the main thread when the host
    activates.
  - A path is found when a mod looks it up (`SceneObject::find`), through `Transform.Find`, and kept
    until the active scene's handle changes (every scene load gives a new one), the object found is
    destroyed or another root is given.
  - An Animator parameter is checked against that Animator's own `parameters` when it is first asked,
    and again after a scene load.
  - A miss is logged once per scene load, saying what failed.

  The shared logic is `sdk/runtime/scene_facts.cpp`; the host gives it the running game
  (`host/scene_engine.cpp`) and the fake backend its own scene, so the tests run the host's code.

## The files

In `Phasmophobia_Data`:

- `globalgamemanagers`: the build settings (the scenes, in build order), the tag manager (the 32 layer
  names) and the Resources container (each path `Resources.Load` takes, lower-cased, with a pointer to
  its object). Engine objects here keep their type trees.
- `globalgamemanagers.assets`: the MonoScript objects, which name each script's class and namespace.
- `level<N>`: scene N of the build settings. Seasonal copies of some scenes are separate scenes with
  their own `level<N>`, so a fact that must hold in a variant needs that scene named too. A scene name
  can be listed more than once; the check reads every file it stands for.
- `resources.assets`: everything under a `Resources` folder, prefabs included.
- `sharedassets<N>.assets`: assets the scenes and prefabs refer to, Animator controllers among them.
  The check copies one only when an object it reads refers to it.
- `.resS` and `.resource` files hold texture, mesh and audio data, which the check never reads.

## Reading them

- Engine objects (GameObject, Transform, RectTransform, Animator, AnimatorController) read with
  UnityPy's own type trees for the build's Unity version (2022.3).
- A MonoBehaviour's type tree is stripped, so its fields cannot be read by name. Its script is the
  PPtr at byte 16 of its raw data (after `m_GameObject`, 12 bytes, and `m_Enabled`, aligned to 4): a
  file index into the reading file's externals (0 for the file itself) and a path id. The MonoScript
  there names the class.
- A path follows `Transform.Find`: each segment is a child's name, children in their Transform's
  `m_Children` order, inactive ones included. The game takes the first match, so the check requires
  exactly one. The hierarchy is walked with a stack and each GameObject once, so a malformed file
  whose hierarchy loops still ends.
- An AnimatorController's parameters are `m_Controller.m_Values` (each an id, the name's hash) with
  the names in `m_TOS`. The check follows each Animator's `m_Controller`, through an override
  controller to the controller it overrides.
- A reference into another file names that file by its path in the referring file's externals; the
  check flattens it to its base name and looks for that name in `Phasmophobia_Data`. So a file outside
  it (`Resources/unity_builtin_extra`, Unity's built-in resources) and a name holding a space or any
  character other than letters, digits, `_` and `.` (which `gamecheck.ps1` refuses to copy) are not
  reachable yet: an object that needs one fails closed, as missing or unreadable.

## The pinned packages

`tools/assetcheck-requirements.txt` pins UnityPy and every package it pulls in, each with its hashes,
and `gamecheck.ps1` installs it with `--require-hashes --only-binary=:all:`. It is generated from
`tools/assetcheck-requirements.in` with pip-tools, which is needed only for that, considering only
uploads at least two weeks old (put the date two weeks before the day it is run):

```powershell
py -3 -I -m venv <scratch venv>
<scratch venv>\Scripts\python -I -m pip install pip-tools
<scratch venv>\Scripts\python -I -m piptools compile --generate-hashes --allow-unsafe --strip-extras `
  --pip-args "--only-binary=:all: --uploaded-prior-to=<date>T00:00:00Z" `
  --output-file tools/assetcheck-requirements.txt tools/assetcheck-requirements.in
```

UnityPy 1.25.3 and later need `tpk_ar`, which publishes no wheel, so the input pins 1.25.2. When
the file changes, `gamecheck.ps1` makes its venv afresh.

## Adding a scene fact

1. Read where it lives: `assetcheck.py --data <copy> --entries <tsv> --record <json>` writes what the
   check reads of the places the entries name (keep the output out of the repository: it is game
   data).
2. Write the entry and its `///` note in `sdk/map/Scene.h`, add it to `kAllEntries`, and give it a
   `phasmo::` wrapper (`SceneObject`, `Layer` or `AnimatorParam`).
3. Run the configured `.\tools\gamecheck.ps1`. The entry must be `OK`.
