#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
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
inline constexpr uint64_t kBumpGapMinMs = 12000;
inline constexpr uint64_t kBumpGapMaxMs = 20000;
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

// Its footsteps (user, 2026-11-09: the user's recordings, Lines::Steps): one per stride of this length it moves. A move
// of more than kMaxStepMetres in one frame is a jump of the figure (a warp, a new master's first word), not a walk.
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
