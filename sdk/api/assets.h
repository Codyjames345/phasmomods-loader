#pragma once

#include <filesystem>

// A mod's own data files (voice recordings, textures, tables): where a mod finds them while it runs.
//
// A mod ships its files in an `assets` folder beside its DLL and manifest.json. When the launcher starts a game it makes a
// fresh copy of each mod for that game process, and it copies the `assets` folder, with every file and nested folder, beside
// the DLL copy. The mod reads its files from there with assets_folder(). Because the copy is per process, an update of the
// installed mod never changes a file under a running game, and a mod never reads a path of the install folder.
//
// A mod whose assets folder cannot be copied whole is left off, with a reason in the Mods panel.
namespace pm::api {

/**
 * @brief The folder holding the calling mod's own files for this game process: the copy the launcher made of the
 *        `assets` folder beside the mod's DLL.
 * @return The folder's full path, found from the path of the running mod DLL itself. An empty path when the mod has no
 *         such folder: it shipped no `assets` folder, it was not started by the launcher, or the folder cannot be found.
 *         A mod that needs a file checks for an empty path and fails its start with a logged reason.
 * @note Any thread. Reads the file system on each call and keeps nothing.
 * @note The files are a private copy for this process and must be treated as read-only: a mod never writes there. Files a
 *       mod needs to keep between games belong in its save data, not in this folder, which is gone with the process.
 * @note The folder is the one of the DLL the call is compiled into, so it is the calling mod's own.
 * @since Contract 2.3 (a helper in the mod's own code; the host contract itself does not change)
 */
std::filesystem::path assets_folder();

}  // namespace pm::api
