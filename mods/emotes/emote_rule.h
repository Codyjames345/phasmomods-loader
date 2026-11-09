#pragma once

// The Emotes mod's rules, free of game calls (docs/superpowers/specs/2026-11-10-emotes-design.md): which segment of the
// wheel a mouse movement or a stick picks, how the wheel's key plays the pick, how the body turns as the view turns,
// when a move ends an emote, and when a looping emote's sound starts again. The values marked as starting values are
// judged with the user in game.

#include <cmath>
#include <optional>

namespace pm::emotes {

/// The wheel's segments, the first straight up and the rest clockwise.
inline constexpr int kSegments = 8;
/// How far the mouse must move from where it was when the wheel opened before a segment is picked, in the pointer's
/// pixels. A starting value.
inline constexpr float kPickDeadZone = 40.0f;
/// How far a stick must be pushed before a segment is picked, 0 to 1. A starting value.
inline constexpr float kStickDeadZone = 0.5f;
/// How far the player may move from where an emote started before it ends, in metres along the floor (the user's).
inline constexpr float kCancelDistance = 0.5f;
/// How far the view may turn either side of the body before the body turns with it, in degrees (the user's).
inline constexpr float kFreeArcDegrees = 90.0f;

/// A point in the world, y up, as Unity's.
struct Point {
  float x, y, z;
};

/// The segment a direction picks: 0 straight up, then clockwise, each kSegments-th of the circle wide and centred on
/// its direction; a direction on a boundary picks the segment clockwise of it.
/// @param dx Rightward (mouse movement or stick, right positive).
/// @param dy Upward (up positive).
/// @param deadZone How long the direction must be to pick anything.
/// @return The segment, or -1 inside the dead zone.
inline int segment_for(float dx, float dy, float deadZone) {
  if (std::hypot(dx, dy) <= deadZone) return -1;
  float degrees = std::atan2(dx, dy) * 57.29577951f;  // 0 up, 90 right
  if (degrees < 0.0f) degrees += 360.0f;
  const float width = 360.0f / kSegments;
  return static_cast<int>(std::floor((degrees + width / 2.0f) / width)) % kSegments;
}

/// An angle in degrees brought into (-180, 180].
inline float wrap_degrees(float a) {
  a = std::fmod(a, 360.0f);
  if (a > 180.0f) a -= 360.0f;
  if (a <= -180.0f) a += 360.0f;
  return a;
}

/// The body's yaw after the view turned (the user's rule): while the view is within kFreeArcDegrees either side of the
/// body, the body stays; past that it turns to keep the view exactly kFreeArcDegrees from it, on the side the view
/// went. Called every frame, so the body follows the view continuously and never swings back on its own.
/// @param bodyYaw The body's yaw now, degrees.
/// @param viewYaw The view's yaw now, degrees.
/// @return The body's new yaw, in (-180, 180].
inline float body_yaw(float bodyYaw, float viewYaw) {
  const float diff = wrap_degrees(viewYaw - bodyYaw);
  if (diff > kFreeArcDegrees) return wrap_degrees(viewYaw - kFreeArcDegrees);
  if (diff < -kFreeArcDegrees) return wrap_degrees(viewYaw + kFreeArcDegrees);
  return wrap_degrees(bodyYaw);
}

/// Whether the player moved far enough from where the emote started to end it: more than kCancelDistance along the
/// floor. A climb or a drop alone is no move.
inline bool moved_too_far(Point start, Point now) {
  return std::hypot(now.x - start.x, now.z - start.z) > kCancelDistance;
}

/// The wheel as its key drives it.
struct WheelInput {
  bool open = false;  ///< the key is held and the wheel shows
  int picked = -1;    ///< the segment picked now, or -1
};

/// One frame of the wheel's key: it opens while the key is held, follows the pick, and on release plays what is
/// picked. Something that takes the keyboard (a menu, the pause screen, death) closes it with nothing played.
/// @param w The wheel's state, changed here.
/// @param held The key is held this frame.
/// @param released The key went up this frame.
/// @param interrupted Something closed the wheel this frame.
/// @param pickNow The segment picked this frame, or -1 (segment_for).
/// @return The segment to play when the key is released on one; nothing otherwise.
inline std::optional<int> wheel_step(WheelInput& w, bool held, bool released, bool interrupted, int pickNow) {
  if (interrupted) {
    w = WheelInput{};
    return std::nullopt;
  }
  if (!w.open) {
    if (held && !released) w = WheelInput{true, -1};
    return std::nullopt;
  }
  w.picked = pickNow >= 0 && pickNow < kSegments ? pickNow : -1;
  if (released || !held) {
    const int pick = released ? w.picked : -1;
    w = WheelInput{};
    if (pick >= 0) return pick;
  }
  return std::nullopt;
}

/// Whether a looping emote's sound starts again this frame (the user's rule: the sound's loop is exactly the clip's):
/// when the clip's own time crossed a whole number of clip lengths since the last frame.
/// @param before The clip's time last frame, seconds.
/// @param now The clip's time this frame, seconds.
/// @param clipSeconds The clip's length; 0 or less never starts again.
inline bool loop_restarted(double before, double now, float clipSeconds) {
  if (clipSeconds <= 0.0f || now <= before || before < 0.0) return false;
  return std::floor(now / clipSeconds) > std::floor(before / clipSeconds);
}

/// Whether a one-shot emote has played to its end.
/// @param time The clip's time, seconds.
/// @param clipSeconds Its length.
inline bool one_shot_done(double time, float clipSeconds) { return clipSeconds > 0.0f && time >= clipSeconds; }

}  // namespace pm::emotes
