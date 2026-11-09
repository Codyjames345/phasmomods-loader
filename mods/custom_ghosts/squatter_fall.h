#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

#include "squatter_chase.h"
#include "quat.h"
#include "squatter_figure.h"

// The Squatter's falls on every client (user, 2026-11-09: every trip ends in a ragdoll before it gets up): the body
// falls under physics, settles, then gets up from the pose it landed in, with the get-up that matches how it lies.
// Pure rules, tested offline; squatter.cpp and squatter_figure.cpp do the game's side.
namespace pm::custom::squatter {

/** Where a fall is: not falling, falling under physics, or getting up from where it landed. */
enum class FallPhase { Up, Falling, GettingUp };

/** How slow, metres a second, the fastest of the ragdoll's bodies must be for it to count as lying still. */
inline constexpr float kSettleSpeed = 0.15f;
/** How long, milliseconds, it must lie still before it gets up. */
inline constexpr uint64_t kSettleMs = 300;
/** The longest a fall lasts, milliseconds, however the body moves: a body wedged in furniture or rocking on a stair
 * still gets up. */
inline constexpr uint64_t kFallMaxMs = 2500;
/** How long the landed pose takes to blend into the get-up clip, seconds (the inertialization's length). */
inline constexpr float kRecoverSeconds = 0.4f;
// Every value above is a starting value, judged with the user in game.

/** One fall's progress on this client. */
class FallState {
 public:
  /** A fall begins at `now`, milliseconds. */
  void begin(uint64_t now) {
    phase_ = FallPhase::Falling;
    since_ = now;
    still_.reset();
  }

  /**
   * Moves the fall on to `now`: falling turns to getting up once the fastest of its bodies (`fastest`, metres a second)
   * has stayed below kSettleSpeed for kSettleMs, or kFallMaxMs after it began; getting up turns to up once the get-up's
   * clip (`getUpMs` long) has played. Returns the phase it is in now.
   */
  FallPhase update(uint64_t now, float fastest, uint64_t getUpMs) {
    if (phase_ == FallPhase::Falling) {
      if (fastest < kSettleSpeed) {
        if (!still_) still_ = now;
      } else {
        still_.reset();
      }
      if ((still_ && now - *still_ >= kSettleMs) || now - since_ >= kFallMaxMs) {
        phase_ = FallPhase::GettingUp;
        since_ = now;
      }
    } else if (phase_ == FallPhase::GettingUp && now - since_ >= getUpMs) {
      phase_ = FallPhase::Up;
    }
    return phase_;
  }

  /** Where the fall is. */
  FallPhase phase() const { return phase_; }
  /** When its phase began, milliseconds. */
  uint64_t since() const { return since_; }
  /** Forgets the fall: up. */
  void reset() { *this = FallState{}; }

 private:
  FallPhase phase_ = FallPhase::Up;
  uint64_t since_ = 0;
  std::optional<uint64_t> still_;  // since when its bodies have been slow
};

/** How much further, metres, each point of the sheet but those the head carries may move from where the body carries it
 * while the body falls: enough for the hem to drop to the floor from a body on its side (a starting value). */
inline constexpr float kFallSlack = 2.0f;

/**
 * How much of kFallSlack the sheet has (0 to 1): all of it while the body falls, shrinking smoothly to none over the
 * get-up (`riseShare` of it gone, 0 to 1), none standing. The sheet is let go at once but drawn back gradually, so it
 * never snaps to the body.
 */
inline float sheet_slack(FallPhase phase, float riseShare) {
  if (phase == FallPhase::Falling) return 1.0f;
  if (phase == FallPhase::Up) return 0.0f;
  const float u = std::clamp(riseShare, 0.0f, 1.0f);
  return 1.0f - u * u * (3.0f - 2.0f * u);
}

/** How far a point the sheet lets move `base` metres from the body may move with `slack` of kFallSlack; a pinned point
 * (base 0) stays pinned. */
inline float slack_distance(float base, float slack) { return base > 0.0f ? base + slack * kFallSlack : 0.0f; }

/** How it gets up: from lying face down (Standing Up) or face up (Getting Up). */
enum class GetUp { FaceDown, FaceUp };

/** How near level, as the chest's front's height (a unit vector's y), a body counts as lying on its side. */
inline constexpr float kSideBand = 0.2f;

/**
 * Which get-up a body lying with its chest's front along `chestFront` (a unit vector) takes: face down when the front
 * points down, face up when it points up; a front within kSideBand of level (on its side) keeps `last`, so a body
 * rocking on its side never flips between the two (Review Focus 5).
 */
inline GetUp get_up_for(Point chestFront, GetUp last) {
  if (chestFront.y < -kSideBand) return GetUp::FaceDown;
  if (chestFront.y > kSideBand) return GetUp::FaceUp;
  return last;
}

/** The figure's state for a get-up: Standing Up from face down, Getting Up's rise from face up. */
inline const char* get_up_state(GetUp g) { return g == GetUp::FaceDown ? kStandUpState : kTripGetUpState; }

/**
 * Where each get-up clip's head lies at its start, degrees clockwise seen from above from the clip's own forward
 * (kStandUpState, then kTripGetUpState), read from the clips with the bundle's probe (2026-11-09).
 */
inline constexpr float kGetUpHeadYaw[] = {5.0f, 175.0f};

/** The heading, radians clockwise from +z seen from above, of a body turned by `rotation` (its forward on the flat). */
inline float body_heading(phasmo::Quaternion rotation) {
  const phasmo::Vector3 f = qrot(rotation, {0, 0, 1});
  return std::atan2(f.x, f.z);
}

/** The longest it turns on the spot to the player it is after once it has stood up from a fall, milliseconds, before it
 * runs on whichever way it faces (a starting value, judged with the user in game). */
inline constexpr uint64_t kRiseTurnMs = 2500;

/**
 * Whether its turn after getting up is done at `now` (it stood up to turn until `until`): its figure faces `facing`,
 * within kFaceAheadDegrees of the way to the player, `toTarget`, and no turn clip plays (`turning`), or time is up.
 */
inline bool rise_turn_done(uint64_t now, uint64_t until, float facing, float toTarget, bool turning) {
  return now >= until || (!turning && faces_ahead(facing, toTarget));
}

/** Where and which way the figure's root stands as it gets up. */
struct RootPose {
  Point at;   ///< at the hips, which the caller puts on the floor
  float yaw;  ///< radians clockwise from +z seen from above
};

/**
 * The root a get-up starts from for a body whose hips and head landed at `hips` and `head`: at the hips, turned so the
 * clip's own head direction (kGetUpHeadYaw) lies along hips to head on the flat, so the clip begins where the body lies.
 */
inline RootPose get_up_root(Point hips, Point head, GetUp g) {
  const float dx = head.x - hips.x, dz = head.z - hips.z;
  const float along = dx * dx + dz * dz > 1e-8f ? std::atan2(dx, dz) : 0.0f;
  const float clip = kGetUpHeadYaw[g == GetUp::FaceDown ? 0 : 1] * std::numbers::pi_v<float> / 180.0f;
  return {hips, wrap_angle(along - clip)};
}

}  // namespace pm::custom::squatter
