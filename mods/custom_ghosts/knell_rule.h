#pragma once

#include <algorithm>
#include <cmath>

// The Knell's rules, kept free of game calls so they can be tested offline. In a hunt it moves on a
// strict beat: a surge of speed for one second, then a crawl for one second, over and over. Its steps are the
// game's own, so they quicken in a surge and slow in a crawl with its speed.
namespace pg::custom {

constexpr float kKnellCycle = 2.0f;      // seconds: one surge and one crawl
constexpr float kKnellFast = 1.75f;      // times the game's base speed during a surge: 2.98 m/s
constexpr float kKnellSlow = 0.35f;      // times the game's base speed during a crawl: 0.6 m/s

/** Whether the Knell surges `seconds` into a hunt (the first half of each cycle). */
inline bool knell_surging(float seconds) { return std::fmod(std::max(seconds, 0.0f), kKnellCycle) < kKnellCycle / 2; }

/** The Knell's hunt base for the game's base speed `base` (speed_rule.h) `seconds` into a hunt. Not capped. */
inline float knell_speed(float base, float seconds) { return base * (knell_surging(seconds) ? kKnellFast : kKnellSlow); }

}  // namespace pg::custom
