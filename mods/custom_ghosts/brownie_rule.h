#pragma once

#include <cmath>

// The Brownie's rules, kept free of game calls so they can be tested offline. It works only unseen: in
// a hunt it is very fast while nobody looks at it and slows to less than a walk while anyone does, in
// person or on the truck's monitor (user, 2026-11-03), it never speeds up on sight of its prey, and nothing
// it does happens in view of a player or a running video camera. Its footsteps in a hunt make no sound while nobody looks at it, to players or to equipment, and
// carry only a short way while anyone does (the game's own Myling rule).
namespace pm::custom {

constexpr float kBrowniePlayerCone = 50.0f;  // degrees either side of where a player faces
constexpr float kBrowniePlayerRange = 15.0f; // metres
constexpr float kBrownieCameraCone = 35.0f;  // degrees either side of where a video camera points
constexpr float kBrownieCameraRange = 12.0f; // metres
constexpr float kBrownieUnseen = 2.25f;      // times the game's base speed while unseen (3.83 m/s)
constexpr float kBrownieWatched = 0.6f;      // times the game's base speed while seen (1.02 m/s): slower than a walk
constexpr float kMylingStepQuiet = 8.0f;     // metres a hunting Myling's footsteps lose (the game's rule, Footstep.Play)
constexpr unsigned long long kBrownieSeenResendMs = 1000;  // the host says again whether anyone looks, this often in a hunt

/**
 * Whether a viewer at (vx, vz) facing (fx, fz) (horizontal) has the point (tx, tz) within `cone`
 * degrees either side and `range` metres. Line of sight is the caller's.
 */
inline bool in_view(float vx, float vz, float fx, float fz, float tx, float tz, float cone, float range) {
  const float dx = tx - vx, dz = tz - vz;
  const float d = std::sqrt(dx * dx + dz * dz), f = std::sqrt(fx * fx + fz * fz);
  if (d > range) return false;
  if (d < 0.5f) return true;  // right beside them: seen whichever way they face
  if (f < 1e-4f) return false;
  const float cosAngle = (dx * fx + dz * fz) / (d * f);
  return cosAngle >= std::cos(cone * 3.14159265f / 180.0f);
}

/**
 * The Brownie's own hunt base (speed_rule.h flat_speed) for the game's base speed `base`: it never gains the
 * line-of-sight speed-up.
 */
inline float brownie_speed(float base, bool seen) { return base * (seen ? kBrownieWatched : kBrownieUnseen); }

/** How far a hunting Brownie's footstep carries, given the default distance the game saved for it (the Myling rule). */
inline float quiet_step_distance(float defaultDistance) { return defaultDistance - kMylingStepQuiet; }

/** What becomes of one footstep of the haunting Brownie. */
enum class BrownieStep {
  Vanilla,  // outside hunts: the game's own step
  Quiet,    // in a hunt while anyone looks at it: the game's Myling rule (quiet_step_distance)
  Silent,   // in a hunt while nobody looks at it: no sound, and nothing for the sound sensor or the parabolic microphone
};

/** The rule for a step of the Brownie, from whether it hunts and whether anyone looks at it (brownie::seen()). */
inline BrownieStep brownie_step(bool hunting, bool seen) {
  if (!hunting) return BrownieStep::Vanilla;
  return seen ? BrownieStep::Quiet : BrownieStep::Silent;
}

/**
 * The volume the game's own Footstep.Play is given for a step it was given `volume` for: Play multiplies only
 * the step's AudioSource by it, so 0 silences what players hear and nothing else.
 */
inline float brownie_step_volume(BrownieStep step, float volume) { return step == BrownieStep::Silent ? 0.0f : volume; }

/**
 * When the host says whether anyone looks at the Brownie (flame_net.h kSeen). Outside a hunt nothing reads the
 * word, so only the first goes out. Entering a hunt forces one, so a client has the current state from the first
 * hunting frame. In a hunt a change goes out at once, and an unchanged state repeats every kBrownieSeenResendMs
 * for a client that missed it. Every client plays footsteps, so every client needs it.
 */
struct SeenTeller {
  bool told = false, last = false, hunted = false;
  unsigned long long next = 0;

  /** Whether to say `seen` now, at `now` ms; when it answers true it counts as said. */
  bool due(bool seen, bool hunting, unsigned long long now) {
    const bool entering = hunting && !hunted;
    hunted = hunting;
    if (told && !entering && (!hunting || (seen == last && now < next))) return false;
    told = true;
    last = seen;
    next = now + kBrownieSeenResendMs;
    return true;
  }
};

}  // namespace pm::custom
