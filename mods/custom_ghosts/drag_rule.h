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

}  // namespace pm::custom::drag
