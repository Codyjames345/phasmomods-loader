#pragma once

#include <filesystem>

// Where a mod's own files are at run time, in one place for both sides that must agree. The launcher
// (tools/inject.cpp) copies a mod's assets folder beside the copy of its DLL it makes for each game
// process, named after that copy as its manifest and string table are (<copy>.json, <copy>.i18n.json);
// sdk/api/assets.h finds it again from the running DLL's own path.
namespace pm::modassets {

/** The folder holding the assets of the mod whose DLL copy is `dllCopy`: the copy's path with ".assets" for its extension. */
inline std::filesystem::path beside_copy(const std::filesystem::path& dllCopy) {
  std::filesystem::path folder = dllCopy;
  folder.replace_extension(L".assets");
  return folder;
}

}  // namespace pm::modassets
