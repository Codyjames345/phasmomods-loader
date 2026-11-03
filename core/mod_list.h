#pragma once

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "mod_ids.h"
#include "mod_manifest.h"
#include "text_files.h"
#include "text_table.h"

// The files that say which mods exist and which are switched on. Shared by the launcher
// (tools/launcher.cpp) and the host (host/mods.cpp); it uses only standard file streams and touches no
// game, so it can be tested offline. All text is UTF-8 with LF or CRLF line ends.
//
//  mods/<folder>/manifest.json        one mod (core/mod_manifest.h)
//  mods/<folder>/mod.ini              an old-format mod, in a folder with no manifest.json: "id=",
//                                     "name=", "dll=", "description=" lines, and an optional "requires="
//                                     naming the one mod it cannot run without
//  %LOCALAPPDATA%\PhasmoMods\launcher.ini   "enabled=" followed by comma-separated mod ids: the mods
//                                     that start (the launcher's ticks and the in-game Mods panel)
//  %LOCALAPPDATA%\PhasmoMods\bin\session-<pid>.txt   written by the launcher for the game process it
//                                     loads the host into: one absolute mod DLL path per line, every
//                                     installed mod; for each mod it refused a line "?<name><tab><why>"; and
//                                     a line "!builds<tab><folder>" naming the package's checked-build
//                                     records (core/build_record.h), which a host copied into bin has no
//                                     other way to find. Each copy's manifest sits beside it as <copy>.json
//                                     (<copy>.ini for an old-format mod)
namespace pm::modlist {

/** A mod that stays off whatever the player chooses: its folder or name, and a sentence saying why. */
struct Refused {
  std::string name, why;
};

/** Visit each "key=value" line, trimming spaces; blank lines and '#' comments are skipped. */
template <class Visit>
void each_line(std::string_view text, Visit visit) {
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);
    if (!line.empty() && line.front() != '#') visit(line);
    start = end + 1;
  }
}

/**
 * Read an old-format mod's mod.ini. On failure `error` says, in a sentence, what is wrong with it. A
 * requirement of a bundled mod by its old id means that mod under its new one.
 */
inline bool parse_mod_ini(std::string_view text, Manifest& out, std::string& error) {
  out = {};
  std::string required;
  each_line(text, [&](std::string_view line) {
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) return;
    std::string_view key = line.substr(0, eq), value = line.substr(eq + 1);
    while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
    while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    if (key == "id") out.id = value;
    else if (key == "name") out.name = value;
    else if (key == "dll") out.dll = value;
    else if (key == "description") out.description = value;
    else if (key == "requires") required = value;
  });
  if (!modid::valid_old_id(out.id)) error = i18n::text("manifest.old-bad-id");
  else if (out.id == modid::kReservedAuthor) error = i18n::text("manifest.old-vanilla");
  else if (modid::old_bundled_id(out.id)) error = i18n::text("manifest.old-bundled-id");
  else if (out.name.empty()) error = i18n::text("manifest.no-name");
  else if (!modid::display_safe(out.name, modid::kMaxNameBytes))
    error = i18n::text("manifest.old-bad-name");
  else if (!out.description.empty() && !modid::display_safe(out.description, kMaxDescriptionBytes))
    error = i18n::text("manifest.old-bad-description");
  else if (!valid_dll(out.dll)) error = i18n::text("manifest.bad-dll");
  else if (!required.empty() && (!modid::valid_any_id(required) || required == out.id))
    error = i18n::text("manifest.old-bad-requires");
  else {
    out.oldFormat = true;
    if (!required.empty()) out.dependencies.push_back({modid::migrated_id(required), "", true});
    return true;
  }
  return false;
}

/** What a folder or a copy's manifest said. */
enum class Reading {
  None,      // a folder with neither manifest nor DLL: not a mod
  Readable,  // `out` is the mod's manifest
  Refused    // `why` says, in a sentence, what is wrong
};

namespace detail {

inline Reading read_ini(const std::filesystem::path& file, Manifest& out, std::string& why) {
  std::string text;
  if (!read_manifest_file(file, text, why)) return Reading::Refused;
  return parse_mod_ini(text, out, why) ? Reading::Readable : Reading::Refused;
}

inline Reading read_json(const std::filesystem::path& file, Manifest& out, std::string& why) {
  std::string text;
  if (!read_manifest_file(file, text, why)) return Reading::Refused;
  return parse_manifest_json(text, out, why) ? Reading::Readable : Reading::Refused;
}

}  // namespace detail

/** Where a mod's folder was found, as whoever read it knows: the loader's install folder, or anywhere else. */
enum class Place {
  InstallFolder,  // the mods folder beside the launcher, or beside a host that was not copied into the data folder
  Elsewhere       // any other mods folder, such as one in %LOCALAPPDATA%\PhasmoMods
};

/**
 * Why a mod found at `place` may not run as who its manifest says it is, in a sentence for the player; empty
 * when it may. The id author "phasmomods" and the manifest author "PhasmoMods" (core/mod_ids.h
 * names_project_author) belong to the mods that ship in the install folder. This stops honest mistakes and
 * casual impersonation. It proves nothing about where a file came from: nothing is signed, and anyone who can
 * write to the install folder can replace a mod there.
 */
inline std::string reserved_author_refusal(const Manifest& m, Place place) {
  if (place == Place::InstallFolder) return {};
  if (modid::names_project_author(std::string_view(m.id).substr(0, m.id.find('.'))))
    return i18n::text("manifest.reserved-id");
  if (modid::names_project_author(m.author))
    return i18n::text("manifest.reserved-author");
  return {};
}

namespace detail {

inline Reading read_folder_manifest(const std::filesystem::path& folder, Manifest& out, std::string& why) {
  std::error_code error;
  if (std::filesystem::exists(folder / L"manifest.json", error)) return detail::read_json(folder / L"manifest.json", out, why);
  if (std::filesystem::exists(folder / L"mod.ini", error)) return detail::read_ini(folder / L"mod.ini", out, why);
  for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
    std::wstring ext = entry.path().extension().wstring();
    for (wchar_t& c : ext) c = static_cast<wchar_t>(std::towlower(c));
    if (ext == L".dll") {
      why = i18n::text("manifest.dll-without-manifest");
      return Reading::Refused;
    }
  }
  return Reading::None;
}

}  // namespace detail

/**
 * The manifest in a mod's folder: manifest.json, or for an old-format mod a mod.ini when it has no
 * manifest.json. A bad manifest.json refuses the mod, whatever else is there. A folder with a DLL and
 * neither file is refused, and one with neither is not a mod. A mod found at `place` that claims the
 * project's author is refused (reserved_author_refusal): the launcher and the host both read every mods
 * folder through here, so it holds whichever folder sorts first, and refuses nothing else.
 */
inline Reading read_folder(const std::filesystem::path& folder, Manifest& out, std::string& why, Place place) {
  const Reading reading = detail::read_folder_manifest(folder, out, why);
  if (reading != Reading::Readable) return reading;
  why = reserved_author_refusal(out, place);
  if (why.empty()) return reading;
  out = {};
  return Reading::Refused;
}

/**
 * The manifest beside a launcher's copy of a mod's DLL: <copy>.json, or <copy>.ini for an old-format mod.
 * The launcher read the mod's own folder, and refused a reserved-author claim there, before copying it. The developer
 * tool pminject is the exception: it copies whatever DLL paths it is given, so nothing judged those.
 */
inline Reading read_copy(const std::filesystem::path& dll, Manifest& out, std::string& why) {
  std::error_code error;
  const std::filesystem::path json = std::filesystem::path(dll).replace_extension(L".json");
  if (std::filesystem::exists(json, error)) return detail::read_json(json, out, why);
  const std::filesystem::path ini = std::filesystem::path(dll).replace_extension(L".ini");
  if (std::filesystem::exists(ini, error)) return detail::read_ini(ini, out, why);
  why = i18n::text("manifest.missing");
  return Reading::Refused;
}

/** The ids on the launcher.ini "enabled=" line; `present` is false when there is no such line. */
inline std::vector<std::string> parse_enabled(std::string_view text, bool& present) {
  std::vector<std::string> ids;
  present = false;
  each_line(text, [&](std::string_view line) {
    if (line.substr(0, 8) != "enabled=") return;
    present = true;
    ids.clear();
    std::string_view rest = line.substr(8);
    while (!rest.empty()) {
      size_t comma = rest.find(',');
      std::string_view id = rest.substr(0, comma);
      while (!id.empty() && id.front() == ' ') id.remove_prefix(1);
      while (!id.empty() && id.back() == ' ') id.remove_suffix(1);
      if (modid::valid_any_id(id)) ids.emplace_back(id);
      if (comma == std::string_view::npos) break;
      rest.remove_prefix(comma + 1);
    }
  });
  return ids;
}

inline std::string write_enabled(const std::vector<std::string>& ids) {
  std::string out = "# Mods the launcher starts. Written by the launcher.\nenabled=";
  for (size_t i = 0; i < ids.size(); ++i) out += (i ? "," : "") + ids[i];
  return out + "\n";
}

inline bool enabled(const std::vector<std::string>& ids, std::string_view id) {
  for (const std::string& e : ids)
    if (e == id) return true;
  return false;
}

/** `ids` with each old bundled id moved to its new one (core/mod_ids.h), no id twice, in the same order. */
inline std::vector<std::string> migrated_ids(const std::vector<std::string>& ids) {
  std::vector<std::string> moved;
  for (const std::string& id : ids) {
    std::string now = modid::migrated_id(id);
    if (!enabled(moved, now)) moved.push_back(std::move(now));
  }
  return moved;
}

/** The mods to start: the saved choice in launcher.ini, or every installed mod when none is saved. */
inline std::vector<std::string> chosen(std::string_view launcherIni, const std::vector<std::string>& installed) {
  bool present = false;
  std::vector<std::string> ids = parse_enabled(launcherIni, present);
  return present ? migrated_ids(ids) : installed;
}

/** Save `ids` as the choice in the launcher.ini at `file`, creating its folder. */
inline bool save_choice(const std::filesystem::path& file, const std::vector<std::string>& ids) {
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << write_enabled(ids);
  return static_cast<bool>(out);
}

/**
 * The mods to start, from the launcher.ini at `file` (chosen()). On the first run, with no choice
 * saved, every installed mod starts and that set is saved, so a mod a later update brings is not in
 * it and starts off until the player ticks it. With no mod installed nothing is saved. A choice
 * holding old bundled ids is saved again with the new ones, once.
 */
inline std::vector<std::string> load_choice(const std::filesystem::path& file, const std::vector<std::string>& installed) {
  const std::string text = text::read_file(file);
  bool present = false;
  const std::vector<std::string> saved = parse_enabled(text, present);
  if (!present && !installed.empty()) save_choice(file, installed);  // unsaved, it is simply asked again
  std::vector<std::string> ids = chosen(text, installed);
  if (present && ids != saved) save_choice(file, ids);               // unsaved, it moves again next time
  return ids;
}

/** A session file's line for a mod the launcher refused. Neither a folder's name nor a reason holds a tab or a line end. */
inline std::string session_refused_line(const Refused& r) { return "?" + r.name + "\t" + r.why + "\n"; }

/** The DLL paths in a session file, in order. */
inline std::vector<std::string> parse_session(std::string_view text) {
  std::vector<std::string> paths;
  each_line(text, [&](std::string_view line) {
    if (line.front() != '?' && line.front() != '!') paths.emplace_back(line);
  });
  return paths;
}

/** A session file's line naming the folder of checked-build records. */
inline std::string session_builds_line(const std::string& folder) { return "!builds\t" + folder + "\n"; }

/** The folder of checked-build records a session file names; empty when it names none. */
inline std::string parse_session_builds(std::string_view text) {
  std::string folder;
  each_line(text, [&](std::string_view line) {
    constexpr std::string_view kKey = "!builds\t";
    if (line.substr(0, kKey.size()) == kKey) folder = line.substr(kKey.size());
  });
  return folder;
}

/** The mods the launcher refused, in a session file. */
inline std::vector<Refused> parse_session_refused(std::string_view text) {
  std::vector<Refused> refused;
  each_line(text, [&](std::string_view line) {
    const size_t tab = line.find('\t');
    if (line.front() == '?' && tab != std::string_view::npos)
      refused.push_back({std::string(line.substr(1, tab - 1)), std::string(line.substr(tab + 1))});
  });
  return refused;
}

}  // namespace pm::modlist
