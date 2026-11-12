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
/// pixels: a large neutral zone in the middle, so a pick is easy to let go of by moving back toward the centre (the
/// user, 2026-11-10, twice; it was 40, then 120). A starting value.
inline constexpr float kPickDeadZone = 240.0f;
/// How far from the centre the mouse's movement is held, in the same pixels (the user, 2026-11-10): past it the movement
/// stops growing, so after any flick the way back to the neutral zone is only kPickReach - kPickDeadZone. A starting
/// value.
inline constexpr float kPickReach = 360.0f;
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

/// Adds a mouse movement to the movement since the wheel opened, held within kPickReach of the centre: a point past it
/// is brought back along its own direction, so the pick it points at stays the same.
/// @param x,y The movement so far, changed here (right and up positive).
/// @param dx,dy This frame's movement.
inline void add_capped(float& x, float& y, float dx, float dy) {
  x += dx;
  y += dy;
  const float length = std::hypot(x, y);
  if (length > kPickReach) {
    x *= kPickReach / length;
    y *= kPickReach / length;
  }
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

/// How fast the player must move along the floor for the body to turn to the way it goes, in metres a second: slower is a
/// sway, not walking. A starting value, judged in game.
inline constexpr float kWalkingSpeed = 0.5f;
/// How fast the body turns, in degrees a second: to the way it walks, or to keep the view within its arc. A starting
/// value, judged in game.
inline constexpr float kTurnDegreesPerSecond = 540.0f;

/// `from` turned toward `to` by at most `maxStep` degrees, the short way round.
/// @return The new yaw, in (-180, 180].
inline float turn_toward(float from, float to, float maxStep) {
  const float diff = wrap_degrees(to - from);
  if (std::fabs(diff) <= maxStep) return wrap_degrees(to);
  return wrap_degrees(from + (diff > 0.0f ? maxStep : -maxStep));
}

/// Whether the player moved far enough from where the emote started to end it: more than kCancelDistance along the
/// floor. A climb or a drop alone is no move.
inline bool moved_too_far(Point start, Point now) {
  return std::hypot(now.x - start.x, now.z - start.z) > kCancelDistance;
}

/// How far the move input must point forward, 0 to 1. A starting value.
inline constexpr float kForwardInput = 0.5f;
/// How long the forward input must be let go before a walking emote stands again, in seconds. A starting value.
inline constexpr float kStopHoldSeconds = 0.2f;
/// How long the switch to or from a walking clip blends, in seconds. A starting value.
inline constexpr float kMoveBlendSeconds = 0.25f;
/// How long the intro clip blends into the loop clip, in seconds. A starting value.
inline constexpr float kIntroBlendSeconds = 0.1f;

/// Whether this move input is walking forward: the game's move input (x right, y forward, at most length 1), W alone
/// or the stick pushed roughly up, not a diagonal (the user's ruling, 2026-11-12: forward only).
inline bool walking_forward(float moveX, float moveY) {
  return moveY >= kForwardInput && std::fabs(moveX) < kForwardInput;
}

/// How much faster than its walk speed the investigator must move to count as sprinting. The game's sprint mode can
/// toggle Run, so the mod reads a sprint from the speed (LocalPCPlayer.UpdatePlayerSpeed moves the rig faster than its
/// walk speed only while it sprints). A starting value.
inline constexpr float kSprintMargin = 1.15f;
/// How long the speed is measured over, in seconds. A starting value.
inline constexpr float kSpeedWindowSeconds = 0.3f;

/// Whether the investigator sprints, from its travel over kSpeedWindowSeconds rather than one frame: the frame clock
/// ticks in 10 to 16 ms steps, so one frame's travel over its dt read a walk as a sprint at 33 to 54 frames a second,
/// and frames with a dt of 0 were never measured.
class SprintGauge {
 public:
  /// Each frame: where the investigator is, the seconds since the last frame (0 is fine), and its walk speed in metres a
  /// second (0 or less: never a sprint). Returns whether it sprints, as the last whole window measured.
  bool feed(Point pos, float dt, float walkSpeed) {
    if (!started_) {
      from_ = pos;
      started_ = true;
      return sprinting_;
    }
    seconds_ += dt;
    if (seconds_ >= kSpeedWindowSeconds) {
      const float speed = std::hypot(pos.x - from_.x, pos.z - from_.z) / seconds_;
      sprinting_ = walkSpeed > 0.0f && speed > walkSpeed * kSprintMargin;
      from_ = pos;
      seconds_ = 0.0f;
    }
    return sprinting_;
  }

  /// Forgets the window, as after a scene change or a jump in position.
  void reset() { *this = SprintGauge{}; }

 private:
  Point from_{};
  float seconds_ = 0.0f;
  bool started_ = false;
  bool sprinting_ = false;
};

/// A rotation (a unit quaternion, x y z w, as Unity's).
struct Quat {
  float x, y, z, w;
};

/// A place and a turn: a Transform's world pose, or one relative to another.
struct Pose {
  Point p;
  Quat q;
};

inline Quat mul(Quat a, Quat b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat inverse(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Point rotate(Quat q, Point v) {
  const Quat r = mul(mul(q, Quat{v.x, v.y, v.z, 0.0f}), inverse(q));
  return {r.x, r.y, r.z};
}

/// Where `child` sits in `parent`'s frame.
inline Pose relative_to(const Pose& parent, const Pose& child) {
  const Quat back = inverse(parent.q);
  return {rotate(back, {child.p.x - parent.p.x, child.p.y - parent.p.y, child.p.z - parent.p.z}), mul(back, child.q)};
}

/// The world pose of `local`, a pose in `parent`'s frame.
inline Pose placed_on(const Pose& parent, const Pose& local) {
  const Point d = rotate(parent.q, local.p);
  return {{parent.p.x + d.x, parent.p.y + d.y, parent.p.z + d.z}, mul(parent.q, local.q)};
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

/// Where a song loops: from `start` seconds for `length` seconds, after playing once from its own start. A length of 0
/// or less is no loop: the song plays on.
struct SongLoop {
  float start = 0.0f;
  float length = 0.0f;
};

/// Where in its song an emote is, and which of its two songs that is: an intro song plays first when the emote has
/// one, then the loop song plays from its own start until the end of its loop and repeats the loop. The songs loop in
/// the game (AudioSource.loop, on a song cut to its loop); this is for their loudness.
struct SongAt {
  bool intro;     ///< still in the intro song
  double seconds; ///< seconds into whichever song plays now
};

/// @param elapsed Seconds since the emote began (its own clock, not a clip's time).
/// @param introSongSeconds The intro song's length; 0 or less means the emote has none.
/// @param loop The loop song's loop.
inline SongAt song_position(double elapsed, float introSongSeconds, SongLoop loop) {
  if (introSongSeconds > 0.0f && elapsed < static_cast<double>(introSongSeconds)) return {true, elapsed};
  const double t = elapsed - (introSongSeconds > 0.0f ? static_cast<double>(introSongSeconds) : 0.0);
  const double end = static_cast<double>(loop.start) + loop.length;
  if (loop.length <= 0.0f || t < end) return {false, t};
  return {false, loop.start + std::fmod(t - loop.start, static_cast<double>(loop.length))};
}

/// Whether a one-shot emote has played to its end.
/// @param time The clip's time, seconds.
/// @param clipSeconds Its length.
inline bool one_shot_done(double time, float clipSeconds) { return clipSeconds > 0.0f && time >= clipSeconds; }

}  // namespace pm::emotes
