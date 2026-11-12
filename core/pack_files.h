#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "file_hash.h"
#include "mod_assets.h"

// A content pack's place in the launcher's and the host's lists of mods (core/mod_manifest.h, contentPackFor). A pack has
// no DLL, so it is named by a stand-in path where a mod's DLL would be. The launcher copies a pack for each game process
// as it copies a mod: an empty file at a fresh stand-in name, with the manifest (<copy>.json), the string table
// (<copy>.i18n.json) and the assets folder (<copy>.assets) beside it. Its build, which the lobby compares as it compares
// a mod's, is a hash of those files, so two players with the same pack have the same build whatever their copies are
// called. Link bcrypt.
namespace pm::packfiles {

/** The stand-in a content pack has for a DLL: <folder>\<folder's name>.pack. Nothing is at it in the install folder. */
inline std::filesystem::path stand_in(const std::filesystem::path& folder) {
  return folder / (folder.filename().wstring() + L".pack");
}

/** Whether `path` names a content pack's stand-in rather than a DLL. */
inline bool is_stand_in(const std::filesystem::path& path) {
  std::wstring ext = path.extension().wstring();
  for (wchar_t& c : ext) c = static_cast<wchar_t>(towlower(c));
  return ext == L".pack";
}

/** The files a pack copy's build covers, each with the name it has in the pack's own folder ("manifest.json",
 *  "i18n/default.json", "assets/<path>"), sorted by that name. A file that cannot be listed is left out; the hash of
 *  what remains still differs from the whole pack's. */
inline std::vector<std::pair<std::string, std::filesystem::path>> build_files(const std::filesystem::path& copy) {
  namespace fs = std::filesystem;
  std::vector<std::pair<std::string, fs::path>> files;
  std::error_code error;
  const fs::path manifest = fs::path(copy).replace_extension(L".json");
  if (fs::is_regular_file(manifest, error)) files.emplace_back("manifest.json", manifest);
  fs::path text = copy;
  text.replace_extension(L".i18n.json");
  if (fs::is_regular_file(text, error)) files.emplace_back("i18n/default.json", text);
  const fs::path assets = modassets::beside_copy(copy);
  if (fs::is_directory(assets, error))
    for (fs::recursive_directory_iterator it(assets, error), end; !error && it != end; it.increment(error))
      if (it->is_regular_file(error)) files.emplace_back("assets/" + fs::relative(it->path(), assets, error).generic_string(), it->path());
  std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return files;
}

/** A pack copy's build: the first 16 hex digits of the SHA-256 over each of its files' names and SHA-256s, in name
 *  order. Sixteen zeros when a file cannot be read, as a DLL that cannot be read gets. */
inline std::string build_of(const std::filesystem::path& copy) {
  std::string listing;
  for (const auto& [name, file] : build_files(copy)) {
    char hex[65];
    if (!build::sha256_file(file.c_str(), hex)) return std::string(16, '0');
    listing += name + '\n' + hex + '\n';
  }
  char hex[65];
  return build::sha256_bytes(listing.data(), listing.size(), hex) ? std::string(hex, 16) : std::string(16, '0');
}

}  // namespace pm::packfiles
