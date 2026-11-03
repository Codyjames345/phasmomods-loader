#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "build_record.h"
#include "text_table.h"

// The notice every player answers before any mod starts, and the fingerprint that remembers the
// answer. Changing a word of the notice, or the game build, changes the fingerprint, so players are
// asked again. Kept free of game calls so it can be tested offline.
namespace pm::consent {

struct Notice {
  std::string title, body;  // TextMeshPro rich text
};

struct Selected {  // an installed mod: any of them can be switched on from the Mods panel
  std::string name;   // as the launcher shows it
  bool recordsVoice;  // what its manifest says (a DLL that disagrees stays off, host/mods.cpp)
};

// The warning about community mods (the user's words, 2026-10-30), shown whatever mods are installed.
inline std::string programs_paragraph() { return i18n::text("consent.programs") + "\n\n"; }

// What the notice adds on a game build the offline check has not passed.
inline std::string update_paragraph() { return i18n::text("consent.update") + "\n\n"; }

inline Notice notice(const std::vector<Selected>& mods, bool buildChecked = true) {
  Notice n;
  n.title = i18n::text("consent.title");
  std::string list, voiceMods;
  for (const Selected& m : mods) {
    list += (list.empty() ? "" : i18n::text("list.separator")) + m.name;
    if (m.recordsVoice) voiceMods += (voiceMods.empty() ? "" : i18n::text("list.separator")) + m.name;
  }
  n.body = i18n::text("consent.intro") + "\n\n" + i18n::text("consent.account") + "\n\n";
  if (!buildChecked) n.body += update_paragraph();  // straight after the account's risk (Task 13c)
  n.body += programs_paragraph();
  if (!voiceMods.empty()) n.body += i18n::text("consent.voices", {{"mods", voiceMods}}) + "\n\n";
  n.body += i18n::text("consent.progress") + "\n\n" +
            i18n::text("consent.installed", {{"mods", list.empty() ? i18n::text("consent.installed-none") : list}});
  return n;
}

/**
 * The first text the notice reads that `table` lacks, or empty when it has them all. A player can only agree to words they could
 * read: a table that is missing or broken would show keys in their place, and "I agree" would be remembered all the same. So a
 * notice with any of them missing is never asked, and no mod starts.
 */
inline std::string missing_notice_text(const i18n::Table& table) {
  for (const char* key : {i18n::key("consent.title"), i18n::key("consent.intro"), i18n::key("consent.account"),
                          i18n::key("consent.update"), i18n::key("consent.programs"), i18n::key("consent.voices"),
                          i18n::key("consent.progress"), i18n::key("consent.installed"), i18n::key("consent.installed-none"),
                          i18n::key("consent.agree"), i18n::key("consent.decline"), i18n::key("list.separator")})
    if (!table.has(key)) return key;
  return {};
}

/**
 * FNV-1a over the notice and the game build, as 16 hex digits: what an "I agree" is remembered against, so each
 * new build asks once. Only the last agreement is kept, so going back to an earlier build asks again.
 */
inline std::string fingerprint(const Notice& n, const build::Hashes& game) {
  uint64_t h = 1469598103934665603ull;
  for (const std::string* s : {&n.title, &n.body, &game.gameAssembly, &game.unityPlayer}) {
    for (unsigned char c : *s) {
      h ^= c;
      h *= 1099511628211ull;
    }
    h *= 1099511628211ull;  // where one ends: the same characters split another way are another fingerprint
  }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
  return hex;
}

/** Whether an answer can be remembered against this build: one whose hash could not be read could be any build. */
inline bool can_remember(const build::Hashes& game) { return !game.gameAssembly.empty() && !game.unityPlayer.empty(); }

/** Whether `path` holds an agreement to the notice with this fingerprint. */
inline bool remembered(const std::wstring& path, const std::string& print) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
  char stored[64] = {};
  const size_t n = fread(stored, 1, sizeof stored - 1, f);
  fclose(f);
  return std::string(stored, n) == print;
}

/** Store an agreement to the notice with this fingerprint; false when it cannot be written. */
inline bool remember(const std::wstring& path, const std::string& print) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return false;
  const bool ok = fputs(print.c_str(), f) >= 0;
  return fclose(f) == 0 && ok;
}

}  // namespace pm::consent
