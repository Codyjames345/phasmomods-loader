#pragma once

#include <string>
#include <string_view>

#include "mod_list.h"

// A checked-build record: one small text file per game build the offline check has passed
// (tools/gamecheck.ps1 writes it, sdk/builds holds it, the release ships it). It holds hashes and nothing
// else: the SHA-256 of GameAssembly.dll and of UnityPlayer.dll, and the digest of the map it was checked
// with (sdk/resolver/map_digest.h). Pure logic, shared by the host and the offline check.
namespace pm::build {

// The running game's libraries: their SHA-256 in lower-case hex, empty when one could not be hashed.
struct Hashes {
  std::string gameAssembly, unityPlayer;
};

struct Record {
  std::string gameAssembly, unityPlayer;  // 64 lower-case hex digits
  std::string map;                        // 16 lower-case hex digits
};

constexpr size_t kMaxRecordBytes = 4096;  // a record that says more is not one

inline bool lower_hex(std::string_view text, size_t digits) {
  if (text.size() != digits) return false;
  for (const char c : text)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

/** Reads a record. False when a hash is missing or malformed; a line it does not know is ignored. */
inline bool parse_record(std::string_view text, Record& out) {
  out = {};
  modlist::each_line(text, [&](std::string_view line) {
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) return;
    const std::string_view key = line.substr(0, eq), value = line.substr(eq + 1);
    if (key == "gameassembly") out.gameAssembly = value;
    else if (key == "unityplayer") out.unityPlayer = value;
    else if (key == "map") out.map = value;
  });
  return lower_hex(out.gameAssembly, 64) && lower_hex(out.unityPlayer, 64) && lower_hex(out.map, 16);
}

inline std::string write_record(const Record& r) {
  return "# A game build the offline check passed. tools/gamecheck.ps1 writes this file.\ngameassembly=" + r.gameAssembly +
         "\nunityplayer=" + r.unityPlayer + "\nmap=" + r.map + "\n";
}

/** The record's file name under sdk/builds: one per build, whatever map it was last checked with. */
inline std::string record_file_name(const Record& r) {
  return r.gameAssembly.substr(0, 16) + "-" + r.unityPlayer.substr(0, 16) + ".txt";
}

}  // namespace pm::build
