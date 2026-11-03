#pragma once

#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

// The Black Shuck's scent trails: where each investigator walked in the last kTrailMs, and the next
// step a hound on the trail takes. Salt, running water and rain break a trail; scent does not reach
// across a break. Pure arithmetic, kept free of game calls so it can be tested offline.
namespace pm::custom::trail {

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

/** Whether a hound at `at` may pick up the step: closer than kPickUp, with scent across the way to it. */
inline bool smells(Point at, Point step, const Breaks& breaks) {
  return distance(step, at) < kPickUp && scent_reaches(at, step, breaks);
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
      if (!smells(at, steps[i].at, breaks)) continue;
      const float d = distance(steps[i].at, at);
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

// A player controlling the Black Shuck in a hunt steers only within a corridor around the scent trails, the
// same trails its AI follows (user, 2026-10-31).
constexpr float kCorridor = 1.0f;           // pm:not-game-data: metres either side of a trail, the user's design width
constexpr float kCorridorLookAhead = 0.5f;  // pm:not-game-data: metres ahead a step is tested, so the agent's own
                                            // slowing stops it before the edge
constexpr float kFar = 1e9f;                // a distance standing for no corridor at all

/** Runs fn(steps, first, last) for each run of a trail's steps with scent across every stretch between them. */
template <class Fn>
void each_unbroken_run(const std::vector<const Trail*>& trails, const Breaks& breaks, Fn fn) {
  for (const Trail* t : trails) {
    const auto& steps = t->steps();
    size_t first = 0;
    for (size_t i = 0; i + 1 <= steps.size(); ++i)
      if (i + 1 == steps.size() || !scent_reaches(steps[i].at, steps[i + 1].at, breaks)) {
        if (i > first) fn(steps, first, i);
        first = i + 1;
      }
  }
}

/**
 * Distance from `p` to the spine of one unbroken run of steps: the nearest point on one of its stretches, its
 * foot within the stretch, or a step joining two of them on the outside of a bend. A run's ends are flat, so salt
 * or water across a trail leaves a gap no run spans. kFar when `p` falls beside none of it.
 */
inline float run_distance(const std::deque<Trail::Step>& steps, size_t first, size_t last, Point p) {
  // Where p falls along the stretch a..b: 0 at a, 1 at b; a stretch of no length counts p past both ends.
  const auto along = [&](Point a, Point b, float atZeroLength) {
    const float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    const float len2 = abx * abx + aby * aby + abz * abz;
    return len2 > 0 ? ((p.x - a.x) * abx + (p.y - a.y) * aby + (p.z - a.z) * abz) / len2 : atZeroLength;
  };
  float best = kFar;
  for (size_t i = first; i < last; ++i) {
    const Point a = steps[i].at, b = steps[i + 1].at;
    if (i > first && along(steps[i - 1].at, a, 1) >= 1 && along(a, b, 0) <= 0) best = std::fmin(best, distance(p, a));
    const float s = along(a, b, -1);
    if (s >= 0 && s <= 1)
      best = std::fmin(best, distance(p, {a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s, a.z + (b.z - a.z) * s}));
  }
  return best;
}

/** Distance from `p` to the corridor's spine, over every unbroken run of the trails (run_distance). */
inline float corridor_distance(const std::vector<const Trail*>& trails, Point p, const Breaks& breaks) {
  float best = kFar;
  each_unbroken_run(trails, breaks, [&](const std::deque<Trail::Step>& steps, size_t first, size_t last) {
    best = std::fmin(best, run_distance(steps, first, last, p));
  });
  return best;
}

/**
 * The trail a player's hound at `at` walks, its prey's: `current`'s (an index, or -1) while it can follow them and
 * stands in their trail's corridor (corridor_distance); else the one whose corridor it stands in, the nearest to
 * its spine; else `current`'s while it can follow them; else the one a hound picks up (pick_up). -1 when it can
 * follow none.
 */
inline int walked(const std::vector<const Trail*>& trails, int current, Point at, const Breaks& breaks) {
  int best = -1;
  float bestDistance = kCorridor;
  bool currentFollowable = false;
  for (size_t i = 0; i < trails.size(); ++i) {
    Point step;
    if (!follow(*trails[i], at, breaks, step)) continue;
    const float d = corridor_distance({trails[i]}, at, breaks);
    if (static_cast<int>(i) == current) {
      currentFollowable = true;
      if (d <= kCorridor) return current;
    }
    if (d <= bestDistance) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  if (best >= 0) return best;
  return currentFollowable ? current : pick_up(trails, at, breaks);
}

/**
 * How its agent would walk from where it stands to a point, as its AI's agent walks to the trail: known only for a
 * complete path, whose next corner is `next`; `straight` when the path has no corner between, so `next` is the point.
 */
struct Way {
  bool known = false;
  Point next{};
  bool straight = false;
};

constexpr float kWayMoved = 0.25f;      // pm:not-game-data: metres walked before the way is asked again
constexpr uint64_t kWayMaxAgeMs = 200;  // pm:not-game-data: longest an answer is kept, as a door may shut meanwhile

/**
 * The last answer to how its agent would walk to a step. The game works a path out on the main thread, so a frame
 * that would get the same answer does not ask: only another step, a walk of kWayMoved, a next corner within
 * kWayMoved (nearer that corner must go on to the one after it) or an answer kWayMaxAgeMs old ask again.
 */
class WayCache {
 public:
  template <class Ask>
  Way get(Point at, Point to, uint64_t now, Ask ask) {
    const bool kept = asked_ && to.x == to_.x && to.y == to_.y && to.z == to_.z && distance(at, from_) < kWayMoved &&
                      now - when_ < kWayMaxAgeMs && !(way_.known && distance(at, way_.next) < kWayMoved);
    if (kept) return way_;
    way_ = ask(to);
    asked_ = true;
    from_ = at;
    to_ = to;
    when_ = now;
    return way_;
  }

 private:
  bool asked_ = false;
  Point from_{}, to_{};
  uint64_t when_ = 0;
  Way way_{};
};

/**
 * Confines the direction (dx, dz) a player steers the hunting Black Shuck in, at `at`, to the corridor of
 * kCorridor around the trails (corridor_distance). The step tested is kCorridorLookAhead along the direction. Inside
 * the corridor a step that would leave it slides along the nearest stretch instead, or stops; outside it, only a step
 * "nearer" is kept. Nearer is along the way its agent would walk to the step its scent reaches (`way_to(step)`, a
 * Way): with a wall between, towards that way's next corner, and a trail just beyond the wall is no corridor to hold
 * it in; with nothing between, nearer the trail or that step by the straight line. With no complete path to ask about
 * it is free outside the corridor. Outside a hunt (`hunting` false) it moves freely, as its AI does. So it does
 * wherever its scent reaches no step (next_step false: none within kPickUp, all of them behind a break, or it stands
 * by running water or salt), as its AI then roams by the game's own hunt.
 *
 * Scent does not cross salt or water, so in the corridor and on open floor neither does a step (scent_reaches): its
 * AI's own way there never does. On a way round a wall nothing tests that: its AI's agent walks the game's path through
 * salt and water, which block no agent, until its scent gives out.
 */
template <class WayTo>
void confine(bool hunting, const std::vector<const Trail*>& trails, Point at, const Breaks& breaks, float& dx, float& dz,
             WayTo way_to) {
  const Trail* nearest = nullptr;
  size_t index = 0;
  const float length = std::sqrt(dx * dx + dz * dz);
  if (!hunting || length < 1e-4f || !nearest_step(trails, at, breaks, nearest, index)) return;
  const auto probe = [&](float x, float z, float ahead = kCorridorLookAhead) {
    const float n = std::sqrt(x * x + z * z);
    return Point{at.x + x / n * ahead, at.y, at.z + z / n * ahead};
  };
  const auto crossable = [&](Point p) { return scent_reaches(at, p, breaks); };
  // Whether a step to p stays in the corridor of a run the ghost stands in: never across a gap to the next run.
  const auto inside = [&](Point p) {
    bool found = false;
    each_unbroken_run(trails, breaks, [&](const std::deque<Trail::Step>& steps, size_t first, size_t last) {
      found = found ||
              (run_distance(steps, first, last, at) <= kCorridor && run_distance(steps, first, last, p) <= kCorridor);
    });
    return found && crossable(p);
  };
  const Point step = nearest->steps()[index].at;
  const Way way = way_to(step);
  const bool walled = way.known && !way.straight;  // its agent would walk round to the trail
  if (walled || corridor_distance(trails, at, breaks) > kCorridor) {  // outside: only nearer
    if (!way.known) return;  // no way to tell nearer: free
    if (walled) {
      const float toNext = distance(at, way.next);
      const Point p = probe(dx, dz, std::fmin(kCorridorLookAhead, toNext));
      if (!(distance(p, way.next) < toNext)) dx = dz = 0;
      return;
    }
    // How far a point is from the nearest stretch scent crosses, its ends included; kFar with none.
    const auto gauge = [&](Point p) {
      float d = kFar;
      each_unbroken_run(trails, breaks, [&](const std::deque<Trail::Step>& steps, size_t first, size_t last) {
        for (size_t i = first; i < last; ++i) d = std::fmin(d, distance_to_segment(p, steps[i].at, steps[i + 1].at));
      });
      return d;
    };
    // The step its scent reaches: straight towards it nothing can break, so there is always a way on. All that
    // is left of the scent being a lone step it has reached, nothing holds it there.
    const float toStep = distance(at, step);
    if (gauge(at) == kFar && toStep <= kArrived) return;
    const Point p = probe(dx, dz, std::fmin(kCorridorLookAhead, toStep));
    if (!((gauge(p) < gauge(at) || distance(p, step) < toStep) && crossable(p))) dx = dz = 0;
    return;
  }
  if (inside(probe(dx, dz))) return;
  // Slide along the stretch nearest the ghost: the part of the direction along it.
  float best = kFar, ux = 0, uz = 0;
  each_unbroken_run(trails, breaks, [&](const std::deque<Trail::Step>& steps, size_t first, size_t last) {
    for (size_t i = first; i < last; ++i) {
      const Point a = steps[i].at, b = steps[i + 1].at;
      const float sx = b.x - a.x, sz = b.z - a.z, n = std::sqrt(sx * sx + sz * sz);
      if (n < 1e-4f) continue;
      const float d = distance_to_segment(at, a, b);
      if (d < best) best = d, ux = sx / n, uz = sz / n;
    }
  });
  const float along = dx * ux + dz * uz;
  if (best < kFar && std::fabs(along) > 1e-4f && inside(probe(ux * along, uz * along))) {
    dx = ux * along;
    dz = uz * along;
    return;
  }
  dx = dz = 0;
}

}  // namespace pm::custom::trail
