#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "inject.h"
#include "mod_list.h"

namespace pm::launcher {
#ifdef PM_SUITE_VERSION
#define PM_WIDEN_(text) L##text
#define PM_WIDEN(text) PM_WIDEN_(text)
/** The suite version from the VERSION file. */
inline constexpr wchar_t kVersion[] = PM_WIDEN(PM_SUITE_VERSION);
#endif
/** Quote an argument so Windows parsing preserves every character. */
std::wstring quote(std::wstring_view value);
/** The launch option to paste into Steam for the executable at `executable`. */
std::wstring steam_launch_option(const std::wstring& executable);
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

/** What one load puts into the game: the host, the mods it is to run, and the session lines for those refused and the records. */
struct Package {
  std::wstring host;
  std::vector<std::wstring> mods;
  std::vector<std::string> refused = {};
};

/** A folder of mods, and where it is: anywhere else, unless the caller says it is the install folder, beside the launcher. */
struct ModsFolder {
  std::filesystem::path path;
  modlist::Place place = modlist::Place::Elsewhere;
};

/**
 * The mods in `folders`, in that order, each in folder order: one sub-folder each with a manifest.json
 * (or a mod.ini for an old-format mod). `problems` names any that are broken, for the launcher's window,
 * and `refused` the same for the host's Mods panel. A broken one is left out, and never stops the others.
 * A mod that repeats an id found before it is refused; one outside the install folder that claims the
 * project's author is refused before that (modlist::read_folder), so it never takes the real mod's id.
 * A mod whose manifest says `developerOnly` is left out unless launcher.ini says developer=1: players
 * never see a mod author's tools (modlist::parse_developer).
 */
std::vector<ModEntry> discover_mods(const std::vector<ModsFolder>& folders, std::wstring& problems,
                                    std::vector<modlist::Refused>& refused);
/** The session file's lines for mods refused here, and for the package's folder of checked-build records when given. */
std::vector<std::string> session_lines(const std::vector<modlist::Refused>& refused, const std::filesystem::path& builds = {});
/** The ids ticked in the saved selection; every mod when nothing has been saved yet. */
std::vector<std::string> saved_selection(const std::filesystem::path& launcherIni, const std::vector<ModEntry>& mods);
bool save_selection(const std::filesystem::path& launcherIni, const std::vector<std::string>& ids);
/** %LOCALAPPDATA%\PhasmoMods\launcher.log, where `--steam` records update results and every load records how it ended; empty
 *  when the folder is unavailable. */
std::filesystem::path launcher_log();
/** One launcher.log line for how a load ended: `how` (attach or steam), the outcome's name, the detail the player was shown,
 *  the Windows error with Windows' own words for it when there is one, and the mod log's path. Never more than one line. */
std::wstring result_line(const loader::Result& result, std::wstring_view how);
/** Every installed mod's DLL, in the order they were found: the host loads those the player's choice starts, once the player agreed. */
std::vector<std::wstring> all_dlls(const std::vector<ModEntry>& mods);
}
