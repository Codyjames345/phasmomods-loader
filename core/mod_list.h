#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "text_files.h"

// The plain-text files that say which mods exist and which are switched on. Shared by the launcher
// (tools/launcher.cpp) and the host (host/mods.cpp); kept free of Windows calls (only standard file
// streams) so it can be tested offline. All text is UTF-8 with LF or CRLF line ends.
//
//  mods/<folder>/mod.ini              one mod: "id=", "name=", "dll=", "description=" lines, and an
//                                     optional "requires=" naming the one mod it cannot run without
//  %LOCALAPPDATA%\PhasmoMods\launcher.ini   "enabled=" followed by comma-separated mod ids: the mods
//                                     that start (the launcher's ticks and the in-game Mods panel)
//  %LOCALAPPDATA%\PhasmoMods\bin\session-<pid>.txt   one absolute mod DLL path per line, every
//                                     installed mod, written by the launcher for the game process it
//                                     loads the host into; each copy's mod.ini sits beside it as
//                                     <copy>.ini
namespace pg::modlist {

struct Manifest {
  std::string id, name, dll, description;
  std::string required;  // the id of the mod it needs running, or empty
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

/** A mod id: 1 to 40 characters of a-z, 0-9 and '-'. */
inline bool valid_id(std::string_view id) {
  if (id.empty() || id.size() > 40) return false;
  for (char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  return true;
}

/** A DLL file name with no folder in it, so a manifest can only name a file beside itself. */
inline bool valid_dll(std::string_view name) {
  if (name.size() < 5 || name.size() > 80 || name.find_first_of("/\\:") != std::string_view::npos) return false;
  std::string_view ext = name.substr(name.size() - 4);
  return (ext[0] == '.') && (ext[1] == 'd' || ext[1] == 'D') && (ext[2] == 'l' || ext[2] == 'L') &&
         (ext[3] == 'l' || ext[3] == 'L');
}

/** Read a mod.ini. On failure `error` says, in a sentence, what is wrong with it. */
inline bool parse_manifest(std::string_view text, Manifest& out, std::string& error) {
  out = {};
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
    else if (key == "requires") out.required = value;
  });
  if (!valid_id(out.id)) error = "Its id is missing or uses characters other than a-z, 0-9 and '-'.";
  else if (out.name.empty()) error = "It has no name.";
  else if (!valid_dll(out.dll)) error = "Its dll is not the name of a .dll file beside it.";
  else if (!out.required.empty() && (!valid_id(out.required) || out.required == out.id))
    error = "Its requires is not the id of another mod.";
  else return true;
  return false;
}

inline bool enabled(const std::vector<std::string>& ids, std::string_view id);

/** The mod a manifest list says `id` requires ("" for none or an unknown id). */
inline std::string_view requirement_of(const std::vector<Manifest>& all, std::string_view id) {
  for (const Manifest& m : all)
    if (m.id == id) return m.required;
  return {};
}

/**
 * The choice `ids` after switching `id` on or off: switching a mod on also switches on what it
 * requires (and what that requires), switching it off also switches off every mod that requires
 * it, directly or through another. Ids are added at the end, in the order they are reached.
 */
inline std::vector<std::string> switched(std::vector<std::string> ids, const std::vector<Manifest>& all,
                                         std::string_view id, bool on) {
  if (on) {
    // The whole chain, even past a mod already on (its requirement may be missing); at most one step
    // per installed mod, so a cycle ends.
    std::string_view next = id;
    for (size_t steps = 0; !next.empty() && steps <= all.size(); ++steps, next = requirement_of(all, next))
      if (!enabled(ids, next)) ids.emplace_back(next);
    return ids;
  }
  std::vector<std::string> off{std::string(id)};
  for (bool grew = true; grew;) {
    grew = false;
    for (const Manifest& m : all)
      if (!m.required.empty() && enabled(off, m.required) && !enabled(off, m.id)) {
        off.push_back(m.id);
        grew = true;
      }
  }
  std::vector<std::string> kept;
  for (const std::string& e : ids)
    if (!enabled(off, e)) kept.push_back(e);
  return kept;
}

/**
 * Why switching `id` on or off (from `before` to `after`) changed other mods too, for the player; ""
 * when it changed only `id`. Names come from the manifests.
 */
inline std::string switch_note(const std::vector<Manifest>& all, const std::vector<std::string>& before,
                               const std::vector<std::string>& after, std::string_view id, bool on) {
  std::string name(id), others;
  int count = 0;
  for (const Manifest& m : all) {
    if (m.id == id) name = m.name;
    else if (enabled(before, m.id) != enabled(after, m.id)) {
      others += (count ? ", " : "") + m.name;
      ++count;
    }
  }
  if (!count) return {};
  if (on) return name + " needs " + others + ", so they are switched on together.";
  return others + (count == 1 ? " needs " : " need ") + name + ", so they are switched off together.";
}

/** `ids` with what each of them requires switched on too: a saved choice older than a requirement. */
inline std::vector<std::string> with_requirements(std::vector<std::string> ids, const std::vector<Manifest>& all) {
  const std::vector<std::string> chosenIds = ids;
  for (const std::string& id : chosenIds) ids = switched(std::move(ids), all, id, true);
  return ids;
}

/** Whether a mod that requires `required` ("" for nothing) may start with `started` running. */
inline bool requirement_met(std::string_view required, const std::vector<std::string>& started) {
  return required.empty() || enabled(started, required);
}

/**
 * The order to start the mods in `ids`: each one after the mod it requires, otherwise in the
 * given order. A mod whose requirement is not among `ids` (or can never be met) is left out.
 */
inline std::vector<std::string> start_order(const std::vector<std::string>& ids, const std::vector<Manifest>& all) {
  std::vector<std::string> order;
  for (const std::string& id : ids) {
    // The chain id, its requirement, that one's requirement... until one already placed or none.
    std::vector<std::string> chain;
    bool met = true;
    for (std::string_view next = id; !next.empty() && !enabled(order, next); next = requirement_of(all, next)) {
      if (!enabled(ids, next) || enabled(chain, next)) {  // not chosen, or a cycle
        met = false;
        break;
      }
      chain.emplace_back(next);
    }
    if (met) order.insert(order.end(), chain.rbegin(), chain.rend());
  }
  return order;
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
      if (valid_id(id)) ids.emplace_back(id);
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

/** The mods to start: the saved choice in launcher.ini, or every installed mod when none is saved. */
inline std::vector<std::string> chosen(std::string_view launcherIni, const std::vector<std::string>& installed) {
  bool present = false;
  std::vector<std::string> ids = parse_enabled(launcherIni, present);
  return present ? ids : installed;
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
 * it and starts off until the player ticks it. With no mod installed nothing is saved.
 */
inline std::vector<std::string> load_choice(const std::filesystem::path& file, const std::vector<std::string>& installed) {
  const std::string text = text::read_file(file);
  bool present = false;
  parse_enabled(text, present);
  if (!present && !installed.empty()) save_choice(file, installed);  // unsaved, it is simply asked again
  return chosen(text, installed);
}

/** The DLL paths in a session file, in order. */
inline std::vector<std::string> parse_session(std::string_view text) {
  std::vector<std::string> paths;
  each_line(text, [&](std::string_view line) { paths.emplace_back(line); });
  return paths;
}

}  // namespace pg::modlist
