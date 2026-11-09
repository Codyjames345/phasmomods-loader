#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <string_view>
#include <span>

#include "squatter_figure.h"
#include "squatter_net.h"
#include "squatter_rule.h"

// The Squatter's chase as rules, free of game calls so they can be tested offline: whom it goes for, how long it
// keeps track of them, what it hears, when it knows someone hides, when it speaks, bumps and opens a door, and what
// each client plays of the master's Shown. squatter.cpp reads the game and feeds them. Times are milliseconds on any
// steady clock; distances are metres. Main thread.
namespace pm::custom::squatter {

/** A point in the world, as the rules see one. */
struct Point {
  float x = 0, y = 0, z = 0;
};

// Game values, from the hunt's voice test (GhostHuntState.Hears, sdk/map/GhostHuntState.h): how far the ghost hears
// a player talk, and a player a Monkey Paw wish marks (Player.wished).
inline constexpr float kHearRange = 9.0f;
inline constexpr float kWishedHearRange = 1000.0f;

// Starting values, to be tuned with the user in game.

// How long it still knows where a target is after it last saw or heard them.
inline constexpr uint64_t kTrackMs = 4000;
// The gap between two of its lines in a phase that talks, picked at random in this range.
inline constexpr uint64_t kLineGapMinMs = 4000;
inline constexpr uint64_t kLineGapMaxMs = 9000;
// It counts as stuck after moving at under kStuckFraction of the pace it wants for kStuckMs: it bumps into something.
inline constexpr uint64_t kStuckMs = 700;
inline constexpr float kStuckFraction = 0.25f;
// A pace under this is standing still on purpose, never a stall.
inline constexpr float kStillPace = 0.5f;
// How far from where it lost a target it looks for them.
inline constexpr float kSearchRadius = 6.0f;
// How near a door's hinge it must be to open it, and how far above or below still counts as the same floor.
inline constexpr float kDoorReach = 1.3f;
inline constexpr float kDoorRise = 1.5f;
// Searching, it runs at this share of its chase pace.
inline constexpr float kSearchPace = 0.6f;
// Besides stalling, it bumps into something at a random gap in this range while it chases, and each bump stops it.
inline constexpr uint64_t kBumpPauseMs = 1200;
// At a hiding place it bangs on the door this often.
inline constexpr uint64_t kBangGapMs = 1600;
// Opening a door, and getting up, stop it this long.
inline constexpr uint64_t kDoorPauseMs = 900;
// How long it waits at a door it pushed for the gap to open wide enough to pass (user, 2026-11-09: it never passes
// through doors). A starting value.
inline constexpr uint64_t kPassWaitMs = 3000;
inline constexpr uint64_t kGetUpMs = 1800;
// How often the host pulses the ghost's sight copies and asks who is talking: each pulse is the sensor's window.
inline constexpr uint64_t kSightMs = 250;
// How often the master resends an unchanged Shown, for a client that missed it (squatter.cpp).
inline constexpr uint64_t kShownResendMs = 2000;
// Where it walks to is renewed this often while the place moves.
inline constexpr uint64_t kSteerMs = 200;
// How near a hiding place's stand point counts as there.
inline constexpr float kStandReach = 1.0f;
// The kick stops it this long, its clip's length or so.
inline constexpr uint64_t kKickMs = 1400;

// A game value: how near a hunting ghost must be to kill (GhostHuntState.killReach, which the hunt state's constructor
// sets to 1.1 m). It lives on the hunt's state object, which exists only in a hunt, so it cannot be read here.
inline constexpr float kKickReach = 1.1f;

/**
 * Whether a player is within its hearing, as the hunt's voice test bounds it before it asks the player's loudness
 * (GhostHuntState.Hears): never outdoors (the player's room is RoomKind.outsideMap); a wished player within
 * kWishedHearRange on any floor; any other within kHearRange on the ghost's own floor. The caller then asks
 * PlayerAudio.IsTalkingLoudly(0.1), which clears the loudness it answers true for, so it asks only past this test.
 * `metres` is from the ghost's head to the player's body; a value that is not a number is out of hearing.
 */
inline bool within_hearing(float metres, bool wished, bool sameFloor, bool outdoors) {
  if (outdoors || !(metres >= 0.0f)) return false;
  if (wished) return metres <= kWishedHearRange;
  return sameFloor && metres <= kHearRange;
}

/** A player as the target choice sees them. */
struct Candidate {
  int id;        // whatever the caller names players by
  float metres;  // from the ghost
  bool living;   // not dead
  bool inside;   // in the house: their room is not RoomKind.outsideMap
  bool sensed;   // seen or heard this frame
  bool hiding;   // in a hiding place it knows of (knows_hiding)
};

/**
 * Its target: the nearest living player inside it senses now; with nobody sensed, the `current` target while it
 * still tracks them (`currentTracked`) or knows their hiding place, as long as they are living and inside. -1 for
 * none. It never chooses a player outside, so it never follows anyone past the exits.
 */
inline int pick_target(std::span<const Candidate> players, int current, bool currentTracked) {
  int best = -1;
  float nearest = 0.0f;
  for (const Candidate& c : players) {
    if (!c.living || !c.inside || !c.sensed) continue;
    if (best < 0 || c.metres < nearest) {
      best = c.id;
      nearest = c.metres;
    }
  }
  if (best >= 0) return best;
  for (const Candidate& c : players)
    if (c.id == current && c.living && c.inside && (currentTracked || c.hiding)) return current;
  return -1;
}

/** Where its target was when it last sensed them, and whether it still knows (kTrackMs). */
class Tracker {
 public:
  /** It sensed `target` at `at` at `now`. */
  void sensed(int target, Point at, uint64_t now) {
    target_ = target;
    at_ = at;
    when_ = now;
  }
  /** Whether it still knows where its target is at `now`: within kTrackMs of the last sensing. */
  bool tracks(uint64_t now) const { return at_ && now - when_ <= kTrackMs; }
  /** The target it last sensed, -1 for none. */
  int target() const { return target_; }
  /** Where it last sensed its target, none before the first. */
  std::optional<Point> last_at() const { return at_; }
  /** Forgets the target, as when it is gone or the chase ends. */
  void forget() { *this = Tracker{}; }

 private:
  int target_ = -1;
  std::optional<Point> at_;
  uint64_t when_ = 0;
};

/**
 * Whether it knows its target is in a hiding place: they are in one (Player.closet) and it tracked them as they went
 * in, or senses them there now, or already knew. The game's own closet search learns a closet the same way, from the
 * player it goes for (sdk/systems/ghost-ai.md). False once they step out.
 */
inline bool knows_hiding(bool inHidingPlace, bool trackedOrSensed, bool knewBefore) {
  return inHidingPlace && (trackedOrSensed || knewBefore);
}

/** One door of a hiding place, as its way in reads it. */
struct HidingDoor {
  bool shut = true;      // reads shut (door::shut)
  bool through = false;  // lets it through (door::lets_through)
  bool mayOpen = false;  // the master may open it (door::may_open)
  float metres = 0.0f;   // from the Squatter
};

/** How it gets at a player in a hiding place. */
enum class WayIn : uint8_t {
  Kick,  // no way in: it waits at the doors, kicking them
  Open,  // it opens a door with the opening clip first
  Walk,  // a door already lets it through
};

/** Its way in, and for Open the index of the door it opens. */
struct Entry {
  WayIn how = WayIn::Kick;
  int door = -1;
};

/**
 * Its way into a hiding place by its `doors` (user, 2026-11-09: it never passes through a door, and an open hiding place
 * hides nobody): Walk through a door that is not shut and lets it through; else Open the nearest door that is not shut
 * and the master may open; else Kick. A shut door is never opened, so a hiding place with every door shut is kicked at.
 */
inline Entry way_in(std::span<const HidingDoor> doors) {
  Entry e;
  float nearest = 0.0f;
  for (size_t i = 0; i < doors.size(); ++i) {
    const HidingDoor& d = doors[i];
    if (d.shut) continue;
    if (d.through) return {WayIn::Walk, static_cast<int>(i)};
    if (d.mayOpen && (e.door < 0 || d.metres < nearest)) {
      e = {WayIn::Open, static_cast<int>(i)};
      nearest = d.metres;
    }
  }
  return e;
}

/** Whether it waits at its target's hiding place, kicking the doors: it knows they hide there and has no way in. */
inline bool waits_at_hiding_place(bool hidingKnown, WayIn way) { return hidingKnown && way == WayIn::Kick; }

// How long it kicks at a hiding place before it loses interest and lurks again, so the player has a chance to get away
// (user, 2026-11-09: 10 seconds).
inline constexpr uint64_t kKickInterestMs = 10000;

/** How long it has kicked at one hiding place: from its first kick there, until reset (a new phase). */
class Interest {
 public:
  /** It kicked at `now`; only the first kick starts the count. */
  void kicked(uint64_t now) {
    if (!since_) since_ = now;
  }
  /** Whether it has kicked there kKickInterestMs by `now`. False before the first kick or for a time before it. */
  bool lost(uint64_t now) const { return since_ && now >= *since_ && now - *since_ >= kKickInterestMs; }
  /** Forgets the count. */
  void reset() { since_.reset(); }

 private:
  std::optional<uint64_t> since_;
};

// Its kick at a hiding place sounds as the door being slammed (user, 2026-11-09), the sound the door's own slam RPC
// plays (Door.PlayDoorSlamNoise): at this volume, a constant in that method's code (a game value)...
inline constexpr float kSlamVolume = 0.15f;
// ...this long after the kick's clip starts, when the foot lands (a starting value, judged with the user in game).
inline constexpr uint64_t kSlamDelayMs = 500;

/** Which list a slam's clip comes from and its index there. */
struct SlamPick {
  bool own = false;  // the door's own slam clips (Door.slamClips), else the game's shared ones
  size_t index = 0;
};

/**
 * The slam's clip as Door.PlayDoorSlamNoise picks it, `roll` choosing: one of the door's own `own` clips, or of the
 * `shared` ones (SoundController.doorSlamClips) when it has none. None when both lists are empty.
 */
inline std::optional<SlamPick> slam_pick(size_t own, size_t shared, uint32_t roll) {
  if (own > 0) return SlamPick{true, roll % own};
  if (shared > 0) return SlamPick{false, roll % shared};
  return std::nullopt;
}

/** When its next line is due: at once at first, then a random gap after each. */
class LineClock {
 public:
  bool due(uint64_t now) const { return !next_ || now >= *next_; }
  /** It spoke at `now`; `roll` picks the gap. */
  void spoke(uint64_t now, uint32_t roll) {
    next_ = now + kLineGapMinMs + roll % (kLineGapMaxMs - kLineGapMinMs + 1);
  }
  void reset() { next_.reset(); }

 private:
  std::optional<uint64_t> next_;
};

/** Whether it has stalled against something: true once per stall of kStuckMs. */
class StuckClock {
 public:
  /** This frame's `pace` (metres a second it moves) against the `wanted` pace, at `now`. */
  bool update(float pace, float wanted, uint64_t now) {
    if (wanted < kStillPace || pace >= wanted * kStuckFraction) {
      since_.reset();
      fired_ = false;
      return false;
    }
    if (!since_) since_ = now;
    if (fired_ || now - *since_ < kStuckMs) return false;
    fired_ = true;
    return true;
  }
  void reset() { *this = StuckClock{}; }

 private:
  std::optional<uint64_t> since_;
  bool fired_ = false;
};

/** A place to look for a lost target: between 1 m and kSearchRadius from where it last sensed them, `roll` picking it. */
inline Point search_point(Point last, uint32_t roll) {
  const float angle = static_cast<float>(roll % 3600u) * (2.0f * std::numbers::pi_v<float> / 3600.0f);
  const float metres = 1.0f + static_cast<float>((roll / 3600u) % 1000u) * ((kSearchRadius - 1.0f) / 999.0f);
  return {last.x + std::cos(angle) * metres, last.y, last.z + std::sin(angle) * metres};
}

/**
 * The opening-door clip for a leaf that swings toward the Squatter as it opens (door::swings_toward), or away: it pulls
 * one that comes toward it (Opening Door Inwards) and pushes one that goes away (Open Door Outwards).
 */
inline Act door_act(bool swingsToward) { return swingsToward ? Act::OpenDoorInwards : Act::OpenDoorOutwards; }

/**
 * Whether the Squatter at `ghost`, moving at `velocity` (metres a second), walks into the door at `door`: it moves, at
 * kStillPace or more, and its way lies within 45 degrees of the door, so a door it passes beside or leaves is not one.
 */
inline bool heading_into(Point ghost, Point door, Point velocity) {
  const float dx = door.x - ghost.x, dz = door.z - ghost.z;
  const float speed = std::hypot(velocity.x, velocity.z), to = std::hypot(dx, dz);
  if (speed < kStillPace || to <= 0.0f) return false;
  return (dx * velocity.x + dz * velocity.z) / (speed * to) >= std::sqrt(0.5f);
}

/** Whether a door at `door` is within its reach from `ghost`: kDoorReach on the flat, kDoorRise up or down. */
inline bool at_door(Point ghost, Point door) {
  return std::hypot(door.x - ghost.x, door.z - ghost.z) <= kDoorReach && std::fabs(door.y - ghost.y) <= kDoorRise;
}

/**
 * The sprint speed it chases a target at (move_speed): the target's own as their client reports it, else the
 * master's own player's, else `last`. A tired target's report still carries the speed they would sprint at, so the
 * Squatter's pace never changes when a player cannot sprint (the spec).
 */
inline float chase_sprint(std::optional<Sprint> target, std::optional<Sprint> own, float last) {
  if (target && target->speed > 0.0f) return target->speed;
  if (own && own->speed > 0.0f) return own->speed;
  return last;
}

/**
 * When to taunt a target about their sprint (tired_taunt_due), by target: a target who cannot sprint as the chase
 * first turns to them counts as one whose sprint has just lapsed, so a difficulty without sprinting gets the taunt too.
 */
class TauntBook {
 public:
  /** Whether to taunt `target`, who `canSprint` or not at `now`; true also records the taunt. */
  bool due(int target, bool canSprint, uint64_t now) {
    auto [it, fresh] = could_.try_emplace(target, true);
    const float since = last_ ? static_cast<float>(now - *last_) / 1000.0f : kTauntCooldown;
    const bool taunt = tired_taunt_due(canSprint, it->second, since);
    it->second = canSprint;
    if (taunt) last_ = now;
    return taunt;
  }
  void reset() { *this = TauntBook{}; }

 private:
  std::map<int, bool> could_;
  std::optional<uint64_t> last_;
};

// Friendly mode (the spec's builder's calls): it follows at this share of its chase pace, stopping this far from the
// player it follows.
inline constexpr float kFollowPace = 0.5f;
inline constexpr float kFollowGap = 1.5f;

/**
 * The act a phase holds for as long as it lasts: it sits while it mopes, as the spec has it. Every other phase holds
 * none; standing still is a move (Move::Stand, rest_move).
 */
inline std::optional<Act> stance_for(Phase p) {
  if (p == Phase::Moping) return Act::Sit;  // it lurks and waits at the exits standing (Move::Stand)
  return std::nullopt;
}

/**
 * Whether the Squatter, steered at `before` metres a second and now at `now`, must stop where it stands rather than
 * slow to a halt: it was moving and is now held still (a fall, a bump, a kick, a door, an arrival).
 */
inline bool stops_dead(float before, float now) { return before > 0.0f && now <= 0.0f; }

/**
 * Whom its head turns to (Shown.lookAt): the player it is after, `target` by actor number, while it chases, waits at
 * their hiding place, follows them or is excited to see them; 0 (nobody) in any other phase, with no target, or for an
 * actor number a byte cannot hold.
 */
inline uint8_t look_target(Phase p, int target) {
  const bool attends = p == Phase::Chasing || p == Phase::AtHidingPlace || p == Phase::Following || p == Phase::Excited;
  return attends && target > 0 && target <= 255 ? static_cast<uint8_t>(target) : 0;
}

/** `now` moved toward `goal` by at most `step`, never past it: how an IK weight blends in and out. */
inline float approach(float now, float goal, float step) {
  if (!(step > 0.0f)) return now;
  return now < goal ? std::min(goal, now + step) : std::max(goal, now - step);
}

// How the head and the door hand blend in and out, seconds from none to all, and how long into a door clip the hand
// waits before it reaches: starting values, judged with the user in game.
inline constexpr float kLookBlendSeconds = 0.4f, kReachBlendSeconds = 0.3f, kReachDelaySeconds = 0.15f;
// Where a door's handle is taken to be: this share of the way from the hinge to the leaf's far edge, this high above
// the floor, and this far out from the leaf on the Squatter's side, metres. Starting values; the game's doors have no
// handle Transform.
inline constexpr float kHandleAlong = 0.9f, kHandleHeight = 0.95f, kHandleOut = 0.06f;
// How near a door must be for the hand to reach for it, metres, and how high a player's eyes are above their feet.
inline constexpr float kReachDoorMetres = 1.8f, kEyeHeight = 1.5f;

/** `radians` brought into -pi to pi, the same turn the shorter way round. */
inline float wrap_angle(float radians) {
  constexpr float pi = std::numbers::pi_v<float>;
  if (!std::isfinite(radians)) return 0.0f;
  float a = std::fmod(radians + pi, 2.0f * pi);
  if (a < 0) a += 2.0f * pi;
  return a - pi;
}

/** A turn on the spot the figure plays (the user's Left Turn, Right Turn and Walking Turn 180 clips). */
enum class Turn : uint8_t { None, Left, Right, Around };

// Turning on the spot as games do it: the figure keeps facing where it faces while it stands, and once where it should
// face (the ghost's own heading) has pulled further than kTurnStartDegrees away it plays a turn clip, which turns it by
// the clip's own root rotation; further than kTurnAroundDegrees, the half turn. Moving, it turns toward its heading at
// up to kTurnRateDegrees a second. Starting values, judged with the user in game.
inline constexpr float kTurnStartDegrees = 60.0f, kTurnAroundDegrees = 135.0f, kTurnRateDegrees = 360.0f;
// How long the Start Walking clip plays as it sets off from standing before its walk takes over, seconds: a starting
// value, judged with the user.
inline constexpr float kStartWalkSeconds = 1.0f;

/** The turn the figure plays standing, facing `delta` radians (wrap_angle) from where it should; clockwise seen from above is positive, a right turn. */
inline Turn turn_for(float delta) {
  const float degrees = std::fabs(wrap_angle(delta)) * 180.0f / std::numbers::pi_v<float>;
  if (degrees >= kTurnAroundDegrees) return Turn::Around;
  if (degrees >= kTurnStartDegrees) return wrap_angle(delta) > 0 ? Turn::Right : Turn::Left;
  return Turn::None;
}

/** The facing `from` radians turned toward `to` by at most `step` radians, the shorter way round. */
inline float turn_toward(float from, float to, float step) {
  const float d = wrap_angle(to - from);
  if (!(step > 0.0f)) return from;
  return wrap_angle(from + std::clamp(d, -step, step));
}

/**
 * What the chasing Squatter reads of the way ahead (squatter.cpp surroundings): the clumsy moves come from these, as
 * games pick a vault, a duck or a squeeze from the world around a character (user, 2026-11-09: not on a timer).
 */
struct Surroundings {
  bool dropAhead = false;  // the floor ahead falls away: stairs down, a step off
  bool kneeProp = false;   // a knee-high prop in its way, clear above
  bool ankleProp = false;  // an ankle-high prop in its way, clear at the knee
  bool gap = false;        // walls or furniture close on both sides: a doorway, a narrow gap
  int furniture = 0;       // furniture close on one side: -1 on its left, +1 on its right, 0 none
  bool diveAt = false;     // the player it chases is straight ahead, in diving range
};

/** A move and its clip (variant), as the Shown carries them. */
struct Pick {
  Move move = Move::Run;
  uint8_t variant = 0;
};

/**
 * The move the way ahead calls for, the user's choice of 2026-11-09: a dive roll at a close target; a jump down a drop
 * or over a knee-high prop (it lands into a roll); a trip over an ankle-high prop; a weave sideways through a doorway
 * or gap; a duck past furniture beside it, stepping away from it (Duck_A goes left, Duck_B right). Run when nothing
 * calls for one.
 */
inline Pick move_for(const Surroundings& s) {
  if (s.diveAt) return {Move::Roll, 0};  // Sprinting Forward Roll
  if (s.dropAhead || s.kneeProp) return {Move::Parkour, 0};
  if (s.ankleProp) return {Move::Stumble, 0};
  if (s.gap) return {Move::Weave, 0};
  if (s.furniture != 0) return {Move::Duck, static_cast<uint8_t>(s.furniture > 0 ? 0 : 1)};
  return {};
}

/**
 * The way the figure faces, radians clockwise from +z, for move `m` and clip `variant` while it heads `heading`: a
 * duck or a weave goes side-on, so its sidestep carries it along its way, as one squeezes through a door (the A clips
 * step left, so it faces a quarter turn right of its way; the B clips the other way); any other move faces its way.
 */
inline float move_facing(Move m, uint32_t variant, float heading) {
  if (m != Move::Duck && m != Move::Weave) return heading;
  constexpr float quarter = std::numbers::pi_v<float> / 2;
  return wrap_angle(heading + (variant % 2 == 0 ? quarter : -quarter));
}

// How it reads the way ahead, metres and milliseconds: starting values, judged with the user in game. It looks every
// kSenseMs while it runs, and after a clumsy move it runs kClumsyGapMs before another.
inline constexpr uint64_t kSenseMs = 150, kClumsyGapMs = 1500, kDiveGapMs = 8000;
inline constexpr float kAnkleHeight = 0.15f, kKneeHeight = 0.45f, kChestHeight = 1.1f, kWaistHeight = 0.9f;
inline constexpr float kPropLook = 1.2f;              // how far ahead a prop in its way counts
inline constexpr float kDropAhead = 1.5f, kDropDepth = 0.5f;  // where it looks for the floor, and how far down is a drop
inline constexpr float kGapHalf = 0.5f;               // either side within this: a doorway or gap
inline constexpr float kBesideReach = 0.6f;           // furniture this near on one side
inline constexpr float kDiveMin = 2.5f, kDiveMax = 5.0f, kDiveConeDegrees = 25.0f;  // a target this far, this straight
// How near the way it runs its figure must face for a clumsy move to begin (faces_ahead). A starting value.
inline constexpr float kFaceAheadDegrees = 20.0f;

/**
 * Whether a figure facing `facing` faces the way `way` it runs, both radians clockwise from +z: within
 * kFaceAheadDegrees, the shorter way round. A roll, a jump, a trip, a duck or a weave carries the figure by its clip the
 * way the figure faces (moves_by_clip), so one begins only then; one begun while the figure still turned took it off its
 * way, a roll away from the player it then came back for (user, 2026-11-09). False for an angle that is not a number.
 */
inline bool faces_ahead(float facing, float way) {
  if (!std::isfinite(facing) || !std::isfinite(way)) return false;
  return std::fabs(wrap_angle(way - facing)) <= kFaceAheadDegrees * std::numbers::pi_v<float> / 180.0f;
}

/** The move shown at `speed`: standing for a walk, sneak or run that is not moving, any other move as it is. */
inline Move rest_move(Move m, float speed) {
  return (m == Move::Run || m == Move::Walk || m == Move::Sneak) && speed <= 0.0f ? Move::Stand : m;
}

/** The act that starts a phase: getting up, out of moping into excitement. None for any other change. */
inline std::optional<Act> entry_act(std::optional<Phase> was, Phase next) {
  if (!was) return std::nullopt;
  return *was == Phase::Moping && next == Phase::Excited ? std::optional<Act>(Act::GetUp) : std::nullopt;
}

/** Where to walk and at what share of its pace; `bang` asks it to kick a hiding place's door. */
struct Step {
  std::optional<Point> to;  // none: stay where it is
  float pace = 0.0f;        // a share of its chase pace (move_speed of Run)
  bool bang = false;
};

/**
 * A friendly Squatter's step while it follows (the spec's builder's calls): to the hiding place's stand point when it
 * knows its target hides (`hidingStand`), waiting there to talk, never banging; else after its target (`targetAt`),
 * stopping kFollowGap from them; else to where it last knew them (`lastAt`); else nowhere.
 */
inline Step follow_step(Point ghost, std::optional<Point> targetAt, std::optional<Point> hidingStand,
                        std::optional<Point> lastAt) {
  const auto flat = [&](Point p) { return std::hypot(p.x - ghost.x, p.z - ghost.z); };
  if (hidingStand) {
    if (flat(*hidingStand) < kStandReach) return {std::nullopt, 0.0f, false};
    return {hidingStand, kFollowPace, false};
  }
  if (targetAt) {
    if (flat(*targetAt) < kFollowGap) return {std::nullopt, 0.0f, false};
    return {targetAt, kFollowPace, false};
  }
  if (lastAt && flat(*lastAt) >= kStandReach) return {lastAt, kFollowPace, false};
  return {};
}

/**
 * Whether a target at `target` is within the kick's reach of the Squatter at `ghost`: kKickReach in three dimensions, as
 * the hunt's kill measures its reach in all three (GhostHuntState.Kill), so never through a floor or a ceiling.
 */
inline bool within_kick(Point ghost, Point target) {
  const float d = std::hypot(target.x - ghost.x, target.y - ghost.y, target.z - ghost.z);
  return d <= kKickReach;  // false for a distance that is not a number
}

/** Whether the Squatter at `ghost` stands at `spot`: within kStandReach on the flat, on the same floor (kDoorRise). */
inline bool at_spot(Point ghost, std::optional<Point> spot) {
  return spot && std::hypot(spot->x - ghost.x, spot->z - ghost.z) < kStandReach &&
         std::fabs(spot->y - ghost.y) <= kDoorRise;
}

/**
 * Whether the Squatter at `ghost` reaches a player at `hider` in a hiding place it has a way into: at the closet's spot
 * (`spot`, ClosetZone.navSpot on the mesh), where the game's hunt goes and its closet search kills whoever hides there
 * (sdk/systems/ghost-ai.md, "Hiding places"), or within the kick's reach of them (within_kick).
 */
inline bool reaches_hider(Point ghost, Point hider, std::optional<Point> spot) {
  return at_spot(ghost, spot) || within_kick(ghost, hider);
}

/** Where a search centres: where it last sensed its target, else where it stands, so it never just stands still. */
inline Point search_origin(std::optional<Point> lastAt, Point ghost) { return lastAt ? *lastAt : ghost; }

/**
 * Facts.seesPlayer for phase `p`: in a seated phase (Lurking, Moping) whether it sees anyone inside (`anySeen`), else
 * whether it senses its target (`targetSensed`); always false while its figure cannot be shown (`figureShown`), so it
 * never starts a chase nobody can see.
 */
inline bool sees_for_phase(Phase p, bool anySeen, bool targetSensed, bool figureShown) {
  if (!figureShown) return false;
  return (p == Phase::Lurking || p == Phase::Moping) ? anySeen : targetSensed;
}

// Its footsteps (user, 2026-11-09: the players' own step sounds, by the surface under it): one per stride of this length
// it moves. A move of more than kMaxStepMetres in one frame is a jump of the figure (a warp, a new master's first word),
// not a walk.

// The surfaces a player's footsteps know, by the tag of the collider under the player (FootstepController's note in the
// map): each has its own clips, Stairs one list for walking and running. A tag that is none of these plays nothing.
inline constexpr const char* kStepSurfaces[] = {"Carpet", "Concrete", "Wood",  "OldWood", "Grass",         "Snow",  "Truck",
                                                "Wet",    "Glass",    "Metal", "Gravel",  "Vinyl", "Heavyconcrete", "Stairs"};
inline constexpr int kStairsSurface = 13;

/** The surface of kStepSurfaces a collider's `tag` names, or -1 for none (case counts, as the game compares it). */
inline int surface_of(std::string_view tag) {
  for (int i = 0; i < static_cast<int>(std::size(kStepSurfaces)); ++i)
    if (tag == kStepSurfaces[i]) return i;
  return -1;
}
inline constexpr float kStrideMetres = 1.4f;
inline constexpr float kMaxStepMetres = 3.0f;

/** When each client plays a footstep: every kStrideMetres the Squatter moves, counted on that client. */
class StepCounter {
 public:
  /** It moved `metres` since the last frame; true when that completes a stride. */
  bool moved(float metres) {
    if (!(metres >= 0.0f)) return false;
    if (metres > kMaxStepMetres) {
      walked_ = 0.0f;
      return false;
    }
    walked_ += metres;
    if (walked_ < kStrideMetres) return false;
    walked_ -= kStrideMetres;
    return true;
  }
  void reset() { walked_ = 0.0f; }

 private:
  float walked_ = 0.0f;
};

// How far past the side of a hiding place the difficulty closed (BlockedZone.blocked) a moping Squatter sits.
inline constexpr float kOutsideMargin = 0.5f;

/** Whether `p` lies in the box at `centre` with half sizes `extents` (a Collider's bounds, Collider.get_bounds). */
inline bool inside_box(Point p, Point centre, Point extents) {
  return std::fabs(p.x - centre.x) <= extents.x && std::fabs(p.y - centre.y) <= extents.y &&
         std::fabs(p.z - centre.z) <= extents.z;
}

/**
 * The closest places outside the box at `centre` with half sizes `extents` (a closed hiding place's collider) from `p`
 * inside it: straight out through each of its four sides, kOutsideMargin past it, at `p`'s height, nearest first (user,
 * 2026-11-09: a moping Squatter moves to the closest valid position). The caller takes the first that is valid.
 */
inline std::vector<Point> ways_out(Point p, Point centre, Point extents) {
  std::vector<Point> out = {{centre.x + extents.x + kOutsideMargin, p.y, p.z},
                            {centre.x - extents.x - kOutsideMargin, p.y, p.z},
                            {p.x, p.y, centre.z + extents.z + kOutsideMargin},
                            {p.x, p.y, centre.z - extents.z - kOutsideMargin}};
  std::sort(out.begin(), out.end(), [&](const Point& a, const Point& b) {
    return std::hypot(a.x - p.x, a.z - p.z) < std::hypot(b.x - p.x, b.z - p.z);
  });
  return out;
}

/** What a client starts playing for a Shown. */
struct Cues {
  bool move = false;  // play the move (figure::play), its act being over or none
  bool line = false;  // play the line (voice::play_index)
  bool act = false;   // play the act (figure::play_act)
};

/**
 * What each client plays of the master's Shown: a line or an act once per new cue, and the move when it changes or an
 * act ends. The first Shown after a reset plays the act it carries (one still under way, such as sitting) or else its
 * move, and never its line, so a client that takes it late (or the master's resend) never replays an old line.
 */
class CuePlayer {
 public:
  Cues take(const Shown& s) {
    Cues c;
    if (!last_) {
      // An act still set is under way (the master clears a one-off as it ends): join it; never an old line.
      c.act = s.act >= 0;
      c.move = !c.act;
    } else {
      c.line = s.lineCue != last_->lineCue && s.lineSet >= 0 && s.lineIndex >= 0;
      c.act = s.actCue != last_->actCue && s.act >= 0;
      const bool actEnded = last_->act >= 0 && s.act < 0;
      c.move = s.act < 0 && (actEnded || s.move != last_->move || s.variant != last_->variant);
    }
    last_ = s;
    return c;
  }
  void reset() { last_.reset(); }

 private:
  std::optional<Shown> last_;
};

}  // namespace pm::custom::squatter
