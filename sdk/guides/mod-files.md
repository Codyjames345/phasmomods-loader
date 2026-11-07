# A mod's own files

A mod that needs data files (voice recordings, tables, textures it builds from) ships them in an `assets` folder beside
its DLL and `manifest.json`, and reads them at run time with `pm::api::assets_folder()` (`sdk/api/assets.h`).

```
mods/<folder>/
  MyMod.dll
  manifest.json
  i18n/default.json
  assets/
    sounds/
      cough.wav
```

## What the launcher does with them

The launcher never runs a mod from its install folder. For each game it starts, it makes a fresh copy of each mod's DLL,
its `manifest.json` and its string table. It also copies the mod's whole `assets` folder, every file and every nested
folder, beside the DLL copy. A mod with no `assets` folder is copied exactly as before.

- **The files are copied for each launch.** An update of the installed mod never changes a file under a running game,
  and two games never share a copy.
- **The files are read-only.** The copy is private to this game process and is not kept. A mod never writes there; what
  it must keep between games belongs in its save data.
- **Nothing outside the mod's folder is copied.** The launcher never follows a link or a junction out of the `assets`
  folder.
- **A mod whose files cannot be copied is off.** If any file under `assets` cannot be copied, that mod does not start, its
  row in the Mods panel gives the reason, and the other mods start as usual. The same applies to a mod that needs a file
  and cannot find it: fail the start with a logged reason, as the bundled Custom Ghosts does when its Squatter voice
  folder is missing. A mod with an error is off; there is no partial workaround.

## Finding them

```cpp
#include "assets.h"

const std::filesystem::path assets = pm::api::assets_folder();  // empty when the mod has no assets folder
if (assets.empty()) {
  PM_LOG("MyMod inactive: its assets folder is missing");
  return false;
}
const std::filesystem::path cough = assets / "sounds" / "cough.wav";
```

`assets_folder()` finds the folder from the path of the running DLL itself, so it needs no setting and no host call. It is
empty when the mod shipped no `assets` folder or was not started by the launcher.

## Shipping them

The release lists every file of a mod in `tools/payload.psd1`, so a mod's asset files are payload entries like its DLL
(the release has no entry for a folder). An empty folder a mod needs holds a `.gitkeep` file that is listed too, since the
launcher can only copy what is in the release.
