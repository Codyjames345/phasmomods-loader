#pragma once

// Address-based hooks are only valid for the exact binaries they were recovered from
// (Steam build 25811160).
namespace pg::build {
/** Hash a complete readable file; a read error never produces a successful partial digest. */
bool sha256_file(const wchar_t* path, char (&hex)[65]);
bool verify_game_assembly(void* module);
bool verify_unity_player(void* module);
}  // namespace pg::build
