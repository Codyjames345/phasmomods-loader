#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "quat.h"

// Inertialization, as Gears of War does it (David Bollo, "Inertialization: High-Performance Animation Transitions",
// GDC 2018): a new clip plays at once, and each bone keeps the difference between the pose it had and the new clip's
// first pose, which dies away over a moment. The body keeps its momentum instead of floating between two poses as a
// cross-fade does. Pure arithmetic, so it is tested offline.
namespace pm::custom {

/** How long the old pose takes to die away, seconds: a starting value, judged with the user in game. */
inline constexpr float kInertiaSeconds = 0.25f;

/**
 * What is left of the difference, from 1 at the start to 0 at the end (`u` the share of kInertiaSeconds gone): Bollo's
 * quintic with no starting velocity, 1 - 10u^2 + 20u^3 - 15u^4 + 4u^5, which is (1 - u)^4 (1 + 4u). It starts and ends
 * with zero speed and only ever falls.
 */
inline float inertia_left(float u) {
  u = std::clamp(u, 0.0f, 1.0f);
  const float rest = 1.0f - u;
  return rest * rest * rest * rest * (1.0f + 4.0f * u);
}

/** The pose difference of a body's bones after a change of clip, dying away over kInertiaSeconds. */
class PoseInertia {
 public:
  /**
   * Starts from a change of clip at `now` (seconds): `from` is each bone's rotation before it, `to` the new clip's first
   * rotation of the same bone. Spans of different lengths are not one body's poses, and start nothing.
   */
  void begin(std::span<const phasmo::Quaternion> from, std::span<const phasmo::Quaternion> to, float now) {
    clear();
    if (from.size() != to.size() || from.empty()) return;
    bones_.reserve(from.size());
    for (size_t i = 0; i < from.size(); ++i) {
      phasmo::Quaternion d = qmul(from[i], qinv(to[i]));
      if (d.w < 0.0f) d = {-d.x, -d.y, -d.z, -d.w};  // the short way round
      const float s = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      Bone b;
      if (s > 1e-6f) {
        b.axis = {d.x / s, d.y / s, d.z / s};
        b.angle = 2.0f * std::atan2(s, d.w);
      }
      bones_.push_back(b);
    }
    start_ = now;
  }

  /** The rotation to put before bone `bone`'s animated one at `now`: identity once it has died away, or for a bone it
   * was never given. */
  phasmo::Quaternion offset(size_t bone, float now) const {
    if (!active(now) || bone >= bones_.size()) return {0, 0, 0, 1};
    const Bone& b = bones_[bone];
    const float half = 0.5f * b.angle * inertia_left((now - start_) / kInertiaSeconds);
    const float s = std::sin(half);
    return {b.axis.x * s, b.axis.y * s, b.axis.z * s, std::cos(half)};
  }

  /** Whether any of the difference is left at `now`. */
  bool active(float now) const { return !bones_.empty() && now - start_ < kInertiaSeconds; }

  /** Forgets the difference. */
  void clear() { bones_.clear(); }

 private:
  struct Bone {
    phasmo::Vector3 axis{0, 1, 0};
    float angle = 0.0f;
  };
  std::vector<Bone> bones_;
  float start_ = 0.0f;
};

}  // namespace pm::custom
