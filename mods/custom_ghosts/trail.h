#pragma once

#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

// The Black Shuck's scent trails: where each investigator walked in the last kTrailMs, and the next
// step a hound on the trail takes. Salt, running water and rain break a trail; scent does not reach
// across a break. Pure arithmetic, kept free of game calls so it can be tested offline.
namespace pg::custom::trail {

constexpr uint64_t kTrailMs = 30000;
constexpr float kSaltReach = 0.6f;   // metres: salt this close to the path between two steps breaks it
constexpr float kWaterReach = 2.0f;  // metres: running water washes out scent this close
constexpr float kPickUp = 8.0f;      // metres: the hound picks up a trail this close to it
constexpr float kArrived = 1.0f;     // metres: a step this close counts as reached

struct Point {
  float x = 0, y = 0, z = 0;
};

inline float distance(Point a, Point b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/** Distance from p to the segment ab. */
inline float distance_to_segment(Point p, Point a, Point b) {
  const float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
  const float len2 = abx * abx + aby * aby + abz * abz;
  float t = len2 > 0 ? ((p.x - a.x) * abx + (p.y - a.y) * aby + (p.z - a.z) * abz) / len2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return distance(p, {a.x + abx * t, a.y + aby * t, a.z + abz * t});
}

/** What breaks scent: salt piles, and running taps, sinks, baths and showers. */
struct Breaks {
  std::vector<Point> salt;
  std::vector<Point> water;
};

/** False when the segment from..to passes within kSaltReach of salt or kWaterReach of water. */
inline bool scent_reaches(Point from, Point to, const Breaks& breaks) {
  for (const Point& s : breaks.salt)
    if (distance_to_segment(s, from, to) <= kSaltReach) return false;
  for (const Point& w : breaks.water)
    if (distance_to_segment(w, from, to) <= kWaterReach) return false;
  return true;
}

class Trail {
 public:
  struct Step {
    Point at;
    uint64_t time;
  };

  /**
   * Add where the player is now. Outdoors in rain, or within kWaterReach of running water, the trail
   * breaks and no step is laid. Salt or water on the way from the last step breaks the trail there.
   */
  void add(Point p, uint64_t now, const Breaks& breaks, bool rainedOut) {
    if (rainedOut || !scent_reaches(p, p, {{}, breaks.water})) {
      steps_.clear();
      return;
    }
    if (!steps_.empty() && !scent_reaches(steps_.back().at, p, breaks)) steps_.clear();
    steps_.push_back({p, now});
  }

  /** Water has started running: every step up to and including the freshest one within reach goes. */
  void wash(const std::vector<Point>& water) {
    size_t washed = 0;
    for (size_t i = 0; i < steps_.size(); ++i)
      if (!scent_reaches(steps_[i].at, steps_[i].at, {{}, water})) washed = i + 1;
    steps_.erase(steps_.begin(), steps_.begin() + washed);
  }

  void forget_older_than(uint64_t now) {
    while (!steps_.empty() && now - steps_.front().time > kTrailMs) steps_.pop_front();
  }

  const std::deque<Step>& steps() const { return steps_; }

 private:
  std::deque<Step> steps_;  // oldest first
};

/** The step nearest to `at` within kPickUp that scent reaches, over all trails; false when none. */
inline bool nearest_step(const std::vector<const Trail*>& trails, Point at, const Breaks& breaks, const Trail*& best,
                         size_t& bestIndex) {
  best = nullptr;
  float bestDistance = kPickUp;
  for (const Trail* t : trails) {
    const auto& steps = t->steps();
    for (size_t i = 0; i < steps.size(); ++i) {
      const float d = distance(steps[i].at, at);
      if (!scent_reaches(at, steps[i].at, breaks)) continue;
      // Nearest wins; on a tie the fresher step (the later one) does.
      if (d < bestDistance || (best && d == bestDistance && steps[i].time > best->steps()[bestIndex].time)) {
        best = t;
        bestIndex = i;
        bestDistance = d;
      }
    }
  }
  return best != nullptr;
}

/** The step after `index` towards the player: on from steps already reached, never across a break. */
inline Point advance(const Trail& t, size_t index, Point at, const Breaks& breaks) {
  const auto& steps = t.steps();
  size_t i = index;
  while (i + 1 < steps.size() && distance(steps[i].at, at) <= kArrived &&
         scent_reaches(steps[i].at, steps[i + 1].at, breaks))
    ++i;
  return steps[i].at;
}

/**
 * The next point for a hound at `at`: on the trail with a step nearest to it (within kPickUp), the
 * step after the nearest one, towards where the player is now. Scent does not reach across a break,
 * so a step behind salt or water does not count. False when no trail is close.
 */
inline bool next_step(const std::vector<const Trail*>& trails, Point at, const Breaks& breaks, Point& out) {
  const Trail* best;
  size_t index = 0;
  if (!nearest_step(trails, at, breaks, best, index)) return false;
  out = advance(*best, index, at, breaks);
  return true;
}

/**
 * The next point towards one particular player. False means the hound has lost them: no scent of
 * theirs within reach, or salt or water now lies across their trail between there and where they are.
 */
inline bool follow(const Trail& prey, Point at, const Breaks& breaks, Point& out) {
  const Trail* best;
  size_t index = 0;
  if (!nearest_step({&prey}, at, breaks, best, index)) return false;
  const auto& steps = prey.steps();
  for (size_t i = index; i + 1 < steps.size(); ++i)
    if (!scent_reaches(steps[i].at, steps[i + 1].at, breaks)) return false;
  out = advance(prey, index, at, breaks);
  return true;
}

/**
 * The trail a hound at `at` picks up: of the trails it can follow to their player (follow), the one
 * with a step nearest to it. -1 when it can follow none.
 */
inline int pick_up(const std::vector<const Trail*>& trails, Point at, const Breaks& breaks) {
  int best = -1;
  float bestDistance = 0;
  for (size_t i = 0; i < trails.size(); ++i) {
    const Trail* nearest;
    size_t index = 0;
    Point step;
    if (!follow(*trails[i], at, breaks, step) || !nearest_step({trails[i]}, at, breaks, nearest, index)) continue;
    const float d = distance(trails[i]->steps()[index].at, at);
    if (best < 0 || d < bestDistance) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  return best;
}

}  // namespace pg::custom::trail
