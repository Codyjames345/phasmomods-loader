#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Transform.h"
#include "squatter_rule.h"

// The Squatter's recorded lines. The user records every line; the mod plays them. Each set of
// `squatter::Lines` is one folder of 16-bit PCM WAV files (wav.h), played in the world through
// sound::play_at. Files within a set are sorted by name, so every client gives the same line the same
// index. Main thread.
namespace pm::custom::squatter::voice {

/** The folder name of a set under the voice folder: the `Lines` member in lower case ("lurk", "chase", ...). */
inline const char* folder_name(Lines set) {
  switch (set) {
    case Lines::Lurk: return "lurk";
    case Lines::Chase: return "chase";
    case Lines::Search: return "search";
    case Lines::Hiding: return "hiding";
    case Lines::Catch: return "catch";
    case Lines::Door: return "door";
    case Lines::Tired: return "tired";
    case Lines::Mope: return "mope";
    case Lines::Excited: return "excited";
    case Lines::Friendly: return "friendly";
    case Lines::Trip: return "trip";
  }
  return "";
}

/** The WAV files of one folder, sorted by file name; empty when the folder is missing or unreadable. The case of ".wav" is ignored. */
std::vector<std::filesystem::path> list_wavs(const std::filesystem::path& folder);

/**
 * Reads every set from `folder` (its subfolders are folder_name of each set) and builds a clip for each
 * file, replacing what was loaded before. A file that cannot be read, decoded or built is logged with its
 * reason and keeps its slot as a silent line, so the other lines keep their indices.
 * @param folder the voice folder; a missing set is an empty set.
 * @return true when at least one line was built. Main thread; once, at contract start.
 */
bool load(const std::filesystem::path& folder);

/** How many lines `set` holds (silent ones included); 0 before load or for an empty set. */
int count(Lines set);

/**
 * Picks a line of `set` with pick_line (never the one it last picked there when another exists) and plays
 * it at `at`.
 * @param roll a random number from the caller.
 * @return the index picked, or -1 for an empty set. A line that failed to build is picked but silent.
 *         Does nothing, and does not crash, for an empty set.
 */
int play(Lines set, phasmo::Vector3 at, uint32_t roll);

/** Plays line `index` of `set` at `at`; does nothing for an empty set, an index out of range or a line that failed to build. */
/** How long line `index` of `set` lasts, seconds; 0 for none, or one gone. */
float seconds(Lines set, int index);
// With `follow` (a Transform, such as the figure's head), the line moves with it; else it stays at `at`.
// mixAt: where the game's mixer group is read for the line (sound.h); null for where it is said.
void play_index(Lines set, int index, phasmo::Vector3 at, float fullVolumeWithin = 0.0f, void* follow = nullptr,
                float silentBeyond = 0.0f,
                const phasmo::Vector3* mixAt = nullptr);

}  // namespace pm::custom::squatter::voice
