#pragma once

// Rolls of the game's own Random.RandomRangeInt that a tell answers instead, each recognised by a moment
// the tell marked itself (a flag set around the game's call), never by the return address: other mods'
// detours chain on this function, so the caller seen here may be one of them. Any client. Main thread.
namespace pg::tells::rolls {

/** Asked for every roll: true replaces the game's `result` (already rolled) with the one written there. */
using Roller = bool (*)(int min, int max, int& result);
void add_roller(Roller roller);  // once, at start

bool create_hooks();  // before hooks::enable_all

}  // namespace pg::tells::rolls
