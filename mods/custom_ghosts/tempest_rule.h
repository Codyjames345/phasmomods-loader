#pragma once

#include <algorithm>
#include <cstdint>

// The Tempest's rules, kept free of game calls so they can be tested offline. It hunts with the
// contract's own weather and never changes it: in a hunt it is invisible and slow, until lightning
// strikes; for the strike's moment it shows in full and runs very fast. Every hunt opens with a
// strike, the weather sets how often more follow (the game's own storm lightning counts too), and
// lightning flashes as each of its ghost events begins.
namespace pg::custom {

constexpr float kTempestDark = 0.6f;           // times the game's base speed between strikes (1.02 m/s)
constexpr float kTempestStrike = 2.25f;        // times the game's base speed during a strike (3.83 m/s)
constexpr float kTempestStrikeSeconds = 1.5f;  // how long a strike shows it and speeds it
constexpr int kWeatherRain = 2, kWeatherHeavyRain = 3;  // LevelValues' weather types (the class dump's enum)

enum class Sky { Clear, Rain, HeavyRain };

/** The sky that sets its strikes, from the contract's weather type; anything but rain is a clear night. */
inline Sky tempest_sky(int weatherType) {
  if (weatherType == kWeatherHeavyRain) return Sky::HeavyRain;
  if (weatherType == kWeatherRain) return Sky::Rain;
  return Sky::Clear;
}

/** Seconds from one strike to the next in a hunt under `sky`, for a random draw `unit` in 0..1. */
inline float tempest_next_strike(Sky sky, float unit) {
  float least = 9, most = 14;
  if (sky == Sky::Rain) least = 6, most = 10;
  if (sky == Sky::HeavyRain) least = 3, most = 6;
  return least + std::clamp(unit, 0.0f, 1.0f) * (most - least);
}

// When its hunts strike: as each hunt opens, then each time the gap drawn at the last strike runs out.
struct StrikeClock {
  bool hunting = false;  // in a hunt at the last step
  uint64_t next = 0;     // when the next strike is due, ms
};

/**
 * One step at `now` ms: whether lightning strikes now, `gapMs` being the gap to the strike after it.
 * The clock moves on before the caller strikes, so a strike that fails is never tried again.
 */
inline bool tempest_strike_now(StrikeClock& clock, bool hunting, uint64_t now, uint64_t gapMs) {
  const bool opens = hunting && !clock.hunting;
  clock.hunting = hunting;
  if (!hunting || (!opens && now < clock.next)) return false;
  clock.next = now + gapMs;
  return true;
}

/** Whether a strike `since` seconds ago still lights it up (negative: none yet). */
inline bool tempest_striking(float since) { return since >= 0 && since <= kTempestStrikeSeconds; }

/**
 * The Tempest's own hunt base (speed_rule.h flat_speed) for the game's base speed `base`: very fast
 * during a strike, slow in the dark between, and never the line-of-sight speed-up.
 */
inline float tempest_speed(float base, bool striking) { return base * (striking ? kTempestStrike : kTempestDark); }

}  // namespace pg::custom
