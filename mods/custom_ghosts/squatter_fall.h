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

/**
 * How far, seconds, the sheet takes back its own sway once it stands again, after its get-up's clip has played. A
 * cloth let go of a pinned body all at once snaps back to it, so the sway comes back as a fade instead (Unreal's cloth
 * blend weight and MagicaCloth's stabilizing time after a reset both fade in over a fraction of a second). A starting
 * value, judged with the user in game.
 */
inline constexpr float kClothEaseSeconds = 0.5f;

/**
 * How far the sheet's points may move from where the body carries them, as a share (0 to 1) of the limits the sheet was
 * made with (figure::set_cloth_blend): none while its body falls and gets up, so the sheet is worn and follows the body
 * exactly, and all of it `kClothEaseSeconds` after its get-up's clip has played.
 *
 * A cloth left free through a ragdoll is the one thing every cloth guide warns against: the falling body carries its
 * points at the ragdoll's speed, they take whatever freedom they are given, and the sheet spreads across the room and
 * blows up (Unity's Cloth manual, and the ragdoll guides that stop cloth before a fall and fade it back in after).
 * `sinceRise` is seconds since the get-up began, and `clipSeconds` is that clip's length: negative `sinceRise` is a
 * body still falling.
 */
inline float cloth_blend(float sinceRise, float clipSeconds) {
  const float free = sinceRise - clipSeconds;
  if (free <= 0.0f) return 0.0f;
  const float u = std::min(1.0f, free / kClothEaseSeconds);
  return u * u * (3.0f - 2.0f * u);
}

/** How far a point that may move `base` metres from the body may move at `blend`: 0 pins it to the body, 1 leaves the
 * sheet as it was made. A point the head carries (base 0) stays pinned whatever the blend. */
inline float blend_distance(float base, float blend) { return base * std::clamp(blend, 0.0f, 1.0f); }

/** `blend` brought to the nearest eighth, 0 to 1: the steps it reaches the cloth in, since each change re-sends every
 * point's constraint to the cloth. A whole get-up and its ease send a handful of times, never once a frame. */
inline float blend_step(float blend) { return std::round(std::clamp(blend, 0.0f, 1.0f) * 8.0f) / 8.0f; }

/** How far the root may move, metres, in one frame and still be walking (root_jumped). A starting value: it sits above
 * the furthest the Squatter's fastest clip carries it in a frame at twenty frames a second, and far below the
 * NavMesh warps and get-up placements that move it by metres. */
inline constexpr float kFrameJumpMetres = 0.5f;

/**
 * Whether the figure's root moved `metres` this frame: far enough that it jumped rather than stepped, so the sheet's
 * anchor went under it. Unity's answer for a cloth whose transform is moved a long way is to clear the motion it has
 * accumulated right after moving it (Cloth.ClearTransformMotion), which is what the figure then does; left to itself
 * the cloth is dragged after the body and arrives stretched across the room.
 */
inline bool root_jumped(float metres) { return metres >= kFrameJumpMetres; }

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
 * Whether its turn after getting up is done at `now` (it stood up to turn until `until`): no turn clip plays
 * (`turning`) and none would start from here (turn_for plays one only kTurnStartDegrees off or further), or time is up.
 * Waiting for the figure to face within kFaceAheadDegrees instead left it standing with nothing playing between those
 * two angles, frozen for the whole kRiseTurnMs (user, 2026-11-10: getting up after a trip left it still); once no clip
 * is left to play, its run's own turning finishes lining up on the way.
 */
inline bool rise_turn_done(uint64_t now, uint64_t until, float facing, float toTarget, bool turning) {
  return now >= until || (!turning && turn_for(wrap_angle(toTarget - facing)) == Turn::None);
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
