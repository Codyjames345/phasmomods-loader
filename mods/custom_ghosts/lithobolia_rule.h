#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// The Lithobolia's rules, kept free of game calls so they can be tested offline. It throws at people:
// a thrown object leaves on a lobbed line that lands on the target's chest, and a hit while the object
// still flies fast costs that person extra sanity.
namespace pg::custom {

constexpr float kLithoboliaRange = 8.0f;     // metres: the farthest investigator it aims at
constexpr float kLithoboliaSpeed = 9.0f;     // metres per second along the line to the target
constexpr float kLithoboliaChest = 1.2f;     // metres above a player's feet it aims at
constexpr float kLithoboliaHitRadius = 0.7f; // metres from the chest that count as a hit
constexpr float kLithoboliaHitSpeed = 2.0f;  // metres per second the object must still have
constexpr float kLithoboliaHit = 5.0f;       // sanity a hit costs (percent)
constexpr float kLithoboliaFlight = 2.0f;    // seconds a throw is watched for a hit
constexpr float kGravity = 9.81f;

struct Vec {
  float x, y, z;
};

inline float length(Vec v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

/**
 * The velocity that carries an object from `from` to `to`: `speed` along the line between them, plus
 * the upward part that cancels gravity over the flight, so it lands where it was aimed.
 */
inline Vec aim_velocity(Vec from, Vec to, float speed = kLithoboliaSpeed) {
  const Vec d{to.x - from.x, to.y - from.y, to.z - from.z};
  const float distance = length(d);
  if (distance < 0.01f || speed <= 0) return {0, 0, 0};
  const float t = distance / speed;
  return {d.x / t, d.y / t + kGravity * t / 2, d.z / t};
}

/**
 * The index of the smallest of `distances` that is not above `range`, or -1 when none is: the person
 * it throws at, and no throw at all when nobody is in sight.
 */
inline int nearest_in_range(const std::vector<float>& distances, float range) {
  int best = -1;
  for (size_t i = 0; i < distances.size(); ++i)
    if (distances[i] <= range && (best < 0 || distances[i] < distances[best])) best = static_cast<int>(i);
  return best;
}

/** Where an object launched at `from` with `v` is after `t` seconds (no drag). */
inline Vec position_after(Vec from, Vec v, float t) {
  return {from.x + v.x * t, from.y + v.y * t - kGravity * t * t / 2, from.z + v.z * t};
}

/** Whether an object at `at` moving at `speed` hits someone whose chest is at `chest`. */
inline bool lithobolia_hit(Vec at, float speed, Vec chest) {
  const Vec d{at.x - chest.x, at.y - chest.y, at.z - chest.z};
  return speed >= kLithoboliaHitSpeed && length(d) <= kLithoboliaHitRadius;
}

}  // namespace pg::custom
