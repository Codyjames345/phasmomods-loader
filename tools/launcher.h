#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mod_list.h"

namespace pg::launcher {
/** Quote an argument so Windows parsing preserves every character. */
std::wstring quote(std::wstring_view value);
/** Join arguments without involving a command shell. */
std::wstring command_line(const std::vector<std::wstring>& arguments);
/** Preserve an explicit Unity file log or append the reserved fresh log path. */
std::wstring configure_log(std::vector<std::wstring>& arguments,
                           const std::wstring& freshPath, std::wstring& logPath);
/** Consume bounded complete log lines, accepting only the game's menu marker. */
class MenuReadiness {
 public:
  /** Discard an initial partial line when reading from an existing log offset. */
  MenuReadiness() = default;
  /** Discard the remainder of a line that began before the observation offset. */
  explicit MenuReadiness(bool discardPartial) : discard_(discardPartial) {}
  /** Return true only after a complete exact menu marker was observed. */
  bool feed(std::string_view bytes);
 private:
  std::string pending_;
  bool discard_ = false;
  bool ready_ = false;
};

/** A mod found in the package's mods folder. */
struct ModEntry {
  modlist::Manifest manifest;
  std::wstring dll;  // absolute path of its DLL
};

/** What one load puts into the game: the host and the mods it is to run. */
struct Package {
  std::wstring host;
  std::vector<std::wstring> mods;
};

/** The mods in `folder` (one sub-folder each, with a mod.ini); `problems` names any that are broken. */
std::vector<ModEntry> discover_mods(const std::filesystem::path& folder, std::wstring& problems);
/** The ids ticked in the saved selection; every mod when nothing has been saved yet. */
std::vector<std::string> saved_selection(const std::filesystem::path& launcherIni, const std::vector<ModEntry>& mods);
bool save_selection(const std::filesystem::path& launcherIni, const std::vector<std::string>& ids);
/** Every installed mod's DLL, in the order they were found: the host loads them all and starts the saved choice. */
std::vector<std::wstring> all_dlls(const std::vector<ModEntry>& mods);
}
