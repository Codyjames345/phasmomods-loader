#pragma once

#include <cmath>

// The Brownie's rules, kept free of game calls so they can be tested offline. It works only unseen: in
// a hunt it is very fast while nobody looks at it and slows to less than a walk while anyone does,
// it never speeds up on sight of its prey, its footsteps carry only a short way (the game's own Myling rule),
// and nothing it does happens in view of a player or a video camera.
namespace pg::custom {

constexpr float kBrowniePlayerCone = 50.0f;  // degrees either side of where a player faces
constexpr float kBrowniePlayerRange = 15.0f; // metres
constexpr float kBrownieCameraCone = 35.0f;  // degrees either side of where a video camera points
constexpr float kBrownieCameraRange = 12.0f; // metres
constexpr float kBrownieUnseen = 2.25f;      // times the game's base speed while unseen (3.83 m/s)
constexpr float kBrownieWatched = 0.6f;      // times the game's base speed while seen (1.02 m/s): slower than a walk
constexpr float kMylingStepQuiet = 8.0f;     // metres a hunting Myling's footsteps lose (the game's rule, 0x243E380)

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

}  // namespace pg::custom
