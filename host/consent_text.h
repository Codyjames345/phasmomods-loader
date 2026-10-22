#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// The notice every player answers before any mod starts, and the fingerprint that remembers the
// answer. Changing a word of the notice changes the fingerprint, so players are asked again. Kept
// free of game calls so it can be tested offline.
namespace pg::consent {

struct Notice {
  std::string title, body;  // TextMeshPro rich text
};

struct Selected {  // an installed mod: any of them can be switched on from the Mods panel
  std::string name;   // as the launcher shows it
  bool recordsVoice;  // PG_MOD_RECORDS_VOICE
};

inline Notice notice(const std::vector<Selected>& mods) {
  Notice n;
  n.title = "Before you play with mods";
  std::string list, voiceMods;
  for (const Selected& m : mods) {
    list += (list.empty() ? "" : ", ") + m.name;
    if (m.recordsVoice) voiceMods += (voiceMods.empty() ? "" : ", ") + m.name;
  }
  n.body =
      "These mods change Phasmophobia in ways Kinetic Games does not support. Read this before you "
      "agree.\n\n"
      "<b>Your account is at risk.</b> The game's anti-cheat can report a modified game to Kinetic "
      "Games, which may ban the account. Play with mods only in private lobbies, and only with "
      "friends who also agreed to this.\n\n";
  if (!voiceMods.empty())
    n.body +=
        "<b>Voices are recorded.</b> When it is on, " + voiceMods +
        " keeps what other players say in memory during a contract and can play it back to the "
        "players in your lobby. Nothing is written to disk, and it is forgotten when the contract "
        "ends. Only players who agreed to this notice are recorded.\n\n";
  n.body +=
      "<b>Your progress is at your own risk.</b> Mods can break saves, progress, money and items. "
      "Nobody who made these mods is responsible for anything you lose.\n\n"
      "Mods installed: <b>" + (list.empty() ? std::string("none") : list) +
      "</b>. Choose which run from the Mods button on the main menu.";
  return n;
}

/** FNV-1a over the notice, as 16 hex digits: what an "I agree" is remembered against. */
inline std::string fingerprint(const Notice& n) {
  uint64_t h = 1469598103934665603ull;
  for (const std::string* s : {&n.title, &n.body})
    for (unsigned char c : *s) {
      h ^= c;
      h *= 1099511628211ull;
    }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
  return hex;
}

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

}  // namespace pg::consent
