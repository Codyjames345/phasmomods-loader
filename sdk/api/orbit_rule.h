#pragma once

/// @file
/// @brief The rules behind the third-person view (third_person.h), free of game calls: where the camera sits behind
/// the head, how it eases out and back, and how close a wall pulls it in.
/// @ingroup runtime
/// @since Contract 2.3

#include <algorithm>
#include <cmath>

namespace pm::api::view {

/// How far behind the head the camera sits, in metres. A starting value, judged with the user in game.
inline constexpr float kOrbitDistance = 2.2f;
/// How far above the head's height the camera sits at full distance, in metres. A starting value, judged in game.
inline constexpr float kOrbitHeight = 0.25f;
/// How long the camera takes to ease out, and back, in seconds. A starting value, judged in game.
inline constexpr float kEaseSeconds = 0.6f;
/// How far short of a wall the camera stops, in metres. A starting value, judged in game.
inline constexpr float kWallMargin = 0.2f;

/// A point or direction in the world (y up), as Unity's Vector3.
struct Point {
  float x, y, z;
};

/// Where the camera sits: `distance` straight back from the head along the look, raised by `height`.
/// @param head The head camera's position.
/// @param forward The head camera's forward direction, unit length.
/// @param distance Metres behind the head.
/// @param height Metres up.
/// @return The camera's position.
inline Point orbit_point(Point head, Point forward, float distance, float height) {
  return {head.x - forward.x * distance, head.y - forward.y * distance + height, head.z - forward.z * distance};
}

/// A smooth ease from 0 to 1 (smoothstep): slow at both ends.
/// @param t Progress, clamped to 0..1.
/// @return The eased progress, 0 at 0 and 1 at 1.
inline float ease(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/// How far back the camera may go when a ray from the head toward it hits something `hitDistance` metres away: short
/// of the hit by kWallMargin, never more than wanted and never less than 0.
/// @param wanted Metres wanted.
/// @param hitDistance Metres to the hit; anything above `wanted` plus the margin leaves it as wanted.
/// @return The distance to use.
inline float pulled_in(float wanted, float hitDistance) {
  return std::clamp(std::min(wanted, hitDistance - kWallMargin), 0.0f, std::max(wanted, 0.0f));
}

}  // namespace pm::api::view
