#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// The route a ghost drags a thing along, kept free of game calls so it can be tested offline (carry.cpp
// follows it). The thing keeps its height above the floor along the navigation mesh's corners, then goes
// onto the spot it came from. With no path over the mesh it goes straight there.
namespace pm::custom::drag {

struct Point {
  float x = 0, y = 0, z = 0;
};

struct Route {
  std::vector<Point> points;
  float length = 0;  // metres
};

inline float distance(Point a, Point b) {
  const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * The route from `start` to `end` through `corners` (NavMeshPath.get_corners, on the floor), each raised by
 * `lift` (the thing's height above the floor where it starts; never below the floor). No corners: straight.
 */
inline Route route(Point start, const std::vector<Point>& corners, Point end, float lift) {
  const float up = std::max(0.0f, lift);
  Route r;
  r.points.push_back(start);
  for (const Point& c : corners) r.points.push_back({c.x, c.y + up, c.z});
  r.points.push_back(end);
  for (size_t i = 1; i < r.points.size(); ++i) r.length += distance(r.points[i - 1], r.points[i]);
  return r;
}

/** Where the thing is `metres` along the route: its start before it, its end from the route's length on. */
inline Point along(const Route& r, float metres) {
  if (r.points.empty()) return {};
  if (metres <= 0) return r.points.front();
  for (size_t i = 1; i < r.points.size(); ++i) {
    const float leg = distance(r.points[i - 1], r.points[i]);
    if (metres <= leg && leg > 0) {
      const float t = metres / leg;
      const Point a = r.points[i - 1], b = r.points[i];
      return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
    metres -= leg;
  }
  return r.points.back();
}

// The drag with physics: the ghost holds the thing by a corner and pulls that corner along the route, so the
// thing swings, trails and bumps on the floor as it comes. The numbers are the drag's own feel.
constexpr float kStiffness = 40.0f;  // per second squared: the pull per metre the corner lags the hand
constexpr float kDamping = 8.0f;     // per second: how much the corner's own motion is held back
constexpr float kMaxPull = 30.0f;    // metres a second squared: never a violent yank
constexpr float kLead = 0.8f;        // metres: the hand waits while the corner trails it by more
constexpr float kArrived = 0.5f;     // metres: the thing near enough its spot to be set on it
constexpr float kMinHold = 0.25f;    // metres above the floor the corner is held, at least
constexpr unsigned kKnockMs = 450;   // the steady rhythm of the gentle knocks while it moves
constexpr float kKnockMoving = 0.2f; // metres a second: slower than this, it is not moving

/** The top corner of a box (centre `c`, half sizes `e`) nearest the way the ghost goes (`towards`, flat). */
inline Point grab_corner(Point c, Point e, Point towards) {
  return {c.x + (towards.x < 0 ? -e.x : e.x), c.y + e.y, c.z + (towards.z < 0 ? -e.z : e.z)};
}

/**
 * The pull on the held corner at `at`, moving at `velocity`, toward the hand at `hand`: an acceleration in
 * metres a second squared (a spring, damped by the corner's motion), never more than kMaxPull.
 */
inline Point pull(Point at, Point hand, Point velocity) {
  Point a{kStiffness * (hand.x - at.x) - kDamping * velocity.x, kStiffness * (hand.y - at.y) - kDamping * velocity.y,
          kStiffness * (hand.z - at.z) - kDamping * velocity.z};
  const float n = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  if (n > kMaxPull) a = {a.x * kMaxPull / n, a.y * kMaxPull / n, a.z * kMaxPull / n};
  return a;
}

/**
 * How far along the route the hand is after `dt` seconds: on at `pace` metres a second while the corner is within
 * kLead of it (`lag` metres), else it waits; never past the route's `length`.
 */
inline float advance(float travelled, float length, float lag, float pace, float dt) {
  if (lag > kLead) return travelled;
  return std::min(length, travelled + pace * dt);
}

/** Whether the drag is over: the hand at the route's end and the thing at `at` within kArrived of its spot. */
inline bool arrived(float travelled, float length, Point at, Point home) {
  return travelled >= length && distance(at, home) <= kArrived;
}

/** The speed to knock at, metres a second: the thing's own, kept below the prop's gentle-clip cutoff. */
inline float knock_speed(float speed, float cutoff) { return cutoff > 0 ? std::min(speed, cutoff * 0.9f) : speed; }

/** The steady knocks of a dragged thing on one client. */
struct Knocks {
  unsigned next = 0;  // ms, the next knock; 0 before the first
  /** Whether to knock at `now` ms for the thing moving at `speed` metres a second. */
  bool due(unsigned now, float speed) {
    if (!next) next = now + kKnockMs;
    if (speed < kKnockMoving || now < next) return false;
    next = now + kKnockMs;
    return true;
  }
};

}  // namespace pm::custom::drag
