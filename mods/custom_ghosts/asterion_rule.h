#pragma once

#include <cmath>

// The Asterion's rules for one door, kept free of game calls so they can be tested offline. In a hunt
// every door it passes slams shut behind it; a closed door in its way stops it while it batters the
// door for a few seconds, then bursts it open, and that door stays jammed open for the contract.
namespace pg::custom {

constexpr float kAsterionReach = 1.1f;     // metres from the middle of a door's leaf: the door is in reach
constexpr float kAsterionPassed = 1.8f;    // metres: a door it was at and has left behind
constexpr float kAsterionBatter = 3.0f;    // seconds of battering before the door gives way
constexpr float kAsterionBang = 0.6f;      // seconds between bangs
constexpr float kAsterionClosedAngle = 12; // degrees from the door's closed angle still counted closed
constexpr float kAsterionAhead = 0.7f;     // cosine of the widest angle off its heading a door in its way lies at (about 45 degrees)
constexpr float kAsterionAcross = 0.7f;    // cosine of the narrowest angle its heading makes with the door's plane (about 45 degrees)

/** The shortest signed difference between two angles in degrees, as Mathf.DeltaAngle. */
inline float delta_angle(float from, float to) {
  float d = std::fmod(to - from, 360.0f);
  if (d > 180) d -= 360;
  if (d < -180) d += 360;
  return d;
}

inline bool door_closed(float angle, float closedAngle) {
  return std::fabs(delta_angle(angle, closedAngle)) <= kAsterionClosedAngle;
}

/**
 * Whether a door lies in the ghost's way as it moves along (hx, hz): the leaf's centre, (cx, cz) from
 * the ghost, close to straight ahead, and the heading crossing the leaf's plane, which runs along
 * (wx, wz) (hinge to centre), not running beside it as down a hallway past a side door.
 */
inline bool door_ahead(float hx, float hz, float cx, float cz, float wx, float wz) {
  const float speed = std::sqrt(hx * hx + hz * hz), distance = std::sqrt(cx * cx + cz * cz),
              width = std::sqrt(wx * wx + wz * wz);
  if (speed < 0.05f || distance < 1e-4f || width < 1e-4f) return false;
  return hx * cx + hz * cz > kAsterionAhead * speed * distance &&
         std::fabs(hx * wx + hz * wz) < kAsterionAcross * speed * width;
}

enum class DoorAction { None, Bang, Burst, Slam };

struct DoorTrack {
  enum class State { Free, Battering, Jammed } state = State::Free;
  bool reached = false;  // in this hunt it came within reach of the door
  float battered = 0;    // seconds of battering so far
  int bangs = 0;

  /**
   * One frame of a hunt (`hunting`), the ghost `distance` metres from the middle of the door's leaf, the door
   * `closed`, the ghost heading `towards` it. Returns what to do to the door now.
   */
  DoorAction step(bool hunting, float distance, bool closed, bool towards, float dt) {
    if (state == State::Jammed) return DoorAction::None;
    if (!hunting) {
      state = State::Free;
      reached = false;
      battered = 0;
      bangs = 0;
      return DoorAction::None;
    }
    if (state == State::Battering) {
      battered += dt;
      if (battered >= kAsterionBatter) {
        state = State::Jammed;
        return DoorAction::Burst;
      }
      const int due = static_cast<int>(battered / kAsterionBang) + 1;
      if (due > bangs) {
        bangs = due;
        return DoorAction::Bang;
      }
      return DoorAction::None;
    }
    if (distance <= kAsterionReach) {
      if (closed && towards) {
        state = State::Battering;
        battered = 0;
        bangs = 1;
        return DoorAction::Bang;
      }
      reached = true;
      return DoorAction::None;
    }
    if (reached && distance >= kAsterionPassed) {
      reached = false;
      return closed ? DoorAction::None : DoorAction::Slam;
    }
    return DoorAction::None;
  }

  bool battering() const { return state == State::Battering; }
};

}  // namespace pg::custom
