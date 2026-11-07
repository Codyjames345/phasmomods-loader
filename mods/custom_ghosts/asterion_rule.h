#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// The Asterion's rules for one door, kept free of game calls so they can be tested offline. It never
// closes a door; a closed door in its way stops it while it batters the door for a few seconds, then
// bursts it: the leaf, and a double door's other leaf, swing fully open and are locked there for the contract.
namespace pm::custom {

constexpr float kAsterionReach = 1.1f;     // metres from the middle of a door's leaf: the door is in reach
constexpr float kAsterionBatter = 3.0f;    // seconds of battering before the door gives way
constexpr float kAsterionBang = 0.6f;      // seconds between bangs
constexpr float kAsterionClosedAngle = 12; // degrees from the door's closed angle still counted closed
constexpr float kAsterionAhead = 0.7f;     // cosine of the widest angle off its heading a door in its way lies at (about 45 degrees)
constexpr float kAsterionAcross = 0.7f;    // cosine of the narrowest angle its heading makes with the door's plane (about 45 degrees)
constexpr float kAsterionSwing = 0.3f;     // seconds a broken leaf takes to swing fully open (the user: a fast swing)
constexpr float kAsterionSettle = 0.5f;    // seconds it rests fully open before it is locked, so the game hushes its creak

/**
 * Whether a broken leaf `seconds` into its swing is locked: only once it has rested fully open for
 * kAsterionSettle. A hinge door sets its creak's volume from how fast its leaf turned, and only while it is
 * unlocked (Door.loopSource), so a leaf locked straight out of its swing creaked for the rest of the contract
 * (live, 2026-11-07). The game itself only locks a door that is still.
 */
inline bool asterion_locks(float seconds) { return seconds >= kAsterionSwing + kAsterionSettle; }

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

enum class DoorAction { None, Bang, Burst };

struct DoorTrack {
  enum class State { Free, Battering, Jammed } state = State::Free;
  float battered = 0;  // seconds of battering so far
  int bangs = 0;

  /**
   * One frame of a hunt (`hunting`), the ghost `distance` metres from the middle of the door's leaf, the door
   * `closed`, the ghost heading `towards` it. Returns what to do to the door now.
   */
  DoorAction step(bool hunting, float distance, bool closed, bool towards, float dt) {
    if (state == State::Jammed) return DoorAction::None;
    if (!hunting) {
      state = State::Free;
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
    }
    return DoorAction::None;
  }

  bool battering() const { return state == State::Battering; }

  /** Jammed open with no battering of its own: the other leaf of a double door whose first leaf burst. */
  void jam() {
    state = State::Jammed;
    battered = 0;
    bangs = 0;
  }
};

/**
 * The other leaf of `doors[i]`, as an index into `doors`, or -1 for a single door or a leaf not tracked.
 * `others[k]` and `paired[k]` are doors[k]'s Door.otherDoor and Door.pairedDoor (null for none): the level keeps a
 * double door's partner in either, and a leaf may name its partner without the partner naming it back.
 */
inline int door_partner(const std::vector<const void*>& doors, const std::vector<const void*>& others,
                        const std::vector<const void*>& paired, size_t i) {
  const auto names = [&](size_t a, const void* b) { return b && (others[a] == b || paired[a] == b); };
  for (size_t k = 0; k < doors.size(); ++k)
    if (k != i && doors[k] && (names(i, doors[k]) || names(k, doors[i]))) return static_cast<int>(k);
  return -1;
}

/**
 * The leaf's angle `seconds` into its swing from `from` to its open angle `to` (degrees on the hinge's axis):
 * the short way round, as Mathf.LerpAngle turns, reaching `to` after kAsterionSwing seconds and staying there.
 */
inline float swing_angle(float from, float to, float seconds) {
  const float t = seconds <= 0 ? 0.0f : seconds >= kAsterionSwing ? 1.0f : seconds / kAsterionSwing;
  return from + delta_angle(from, to) * t;
}

}  // namespace pm::custom
