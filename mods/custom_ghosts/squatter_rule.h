#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

// The Squatter's rules, kept free of game calls so they can be tested offline. It lurks in its room until
// it first sees a player, chases at just under the players' sprint speed until every living player is out of
// the house, then locks the way out. On a friendly ghost's difficulty it never chases: it mopes until it
// sees someone, then follows them. Where the game reads the facts below is the mod's business, not this file's.
namespace pm::custom::squatter {

enum class Phase : uint8_t {
  Lurking,        // in its room, until it first sees a player
  Chasing,        // after a player it can see or still tracks
  Searching,      // lost them, looking
  AtHidingPlace,  // knows a target is in a hiding place and waits at it
  LockedOut,      // every living player is out of the house: the doors are locked, for good
  Moping,         // friendly: seated and depressed until it sees someone
  Excited,        // friendly: getting up
  Following,      // friendly: follows the nearest player around inside
};

// Walk and Stand are the user's walking and standing clips (2026-11-09): it walks to search, follow and find a
// seat, and stands still rather than running on the spot. Neither is in the chase's cycle.
enum class Move : uint8_t { Run, Duck, Weave, Roll, Parkour, Stumble, Walk, Stand, Sneak };

/**
 * Which set of recorded lines the Squatter speaks from; the mod holds the recordings. Steps is no line: its footsteps,
 * the user's own recordings (user, 2026-11-09), one played per stride.
 */
enum class Lines : uint8_t { Lurk, Chase, Search, Hiding, Catch, Door, Tired, Mope, Excited, Friendly, Steps };

/** What the mod has read from the game this frame. */
struct Facts {
  bool friendly;      // Difficulty.friendlyGhost
  bool seesPlayer;    // a target is in sight now
  bool tracksPlayer;  // it still knows where a target is
  bool targetHiding;  // it knows its target is in a hiding place
  int livingInside;   // living players inside the house; negative when unknown (never locks, keeps the phase)
  bool actFinished;   // the get-up clip has ended (Excited)
};

// All the numbers below are starting values, to be tuned with the user in game.

// How far under the players' sprint speed the Squatter's running pace sits.
constexpr float kSprintMargin = 0.95f;
// Seconds each entry of kMoveCycle lasts at least (move_hold_seconds).
constexpr float kMoveSlotSeconds = 1.0f;
// Seconds between two taunts about a player's lapsed sprint.
constexpr float kTauntCooldown = 20.0f;

// The moves it runs through, in order, over and over: Run most of the time, broken up by the clumsy ones.
constexpr Move kMoveCycle[] = {Move::Run, Move::Run,  Move::Duck, Move::Run,     Move::Weave,  Move::Run,
                               Move::Run, Move::Roll, Move::Run,  Move::Parkour, Move::Run,    Move::Stumble};
constexpr int kMoveCount = static_cast<int>(sizeof(kMoveCycle) / sizeof(kMoveCycle[0]));

/** The phase a friendly or hostile Squatter begins a contract in: Lurking, or Moping on a friendly ghost's difficulty. */
inline Phase first_phase(bool friendly) { return friendly ? Phase::Moping : Phase::Lurking; }

/** The GhostState numbers the Squatter may still change to, as the running game numbers them (-1: unread). */
struct QuietStates {
  int idle = -1, wander = -1, favouriteRoom = -1;
};

/**
 * Whether the Squatter refuses GhostAI.ChangeState to `state`, forced or not: every state but idling, wandering and
 * going to its favourite room, so no interaction, event, ability, cursed pull, evidence state or hunt begins, and a
 * state the game adds later is refused too. There is no paranormal activity (the spec, "What happens in a contract").
 * While the mod steers the ghost's agent (`steering`, see steers) wandering and the favourite room are refused as
 * well, since each sets the agent's destination and would take it back; idling never touches the agent but its
 * speed (sdk/systems/ghost-ai.md, "Moving the ghost outside a hunt").
 */
inline bool keeps_quiet(const QuietStates& s, int state, bool steering) {
  if (state < 0) return true;
  if (steering) return state != s.idle;
  return state != s.idle && state != s.wander && state != s.favouriteRoom;
}

/**
 * Whether the mod steers the ghost's agent (GhostAI.agent) in phase `p`: in every phase but Lurking, where the
 * ghost's own idling, wandering and favourite room move it as they move any ghost (user, 2026-11-09: holding it still
 * left it in whatever room it spawned in, a closed pantry among them). In the others the mod moves it, or holds it:
 * seated, getting up, at a hiding place, at the exits.
 */
inline bool steers(Phase p) {
  switch (p) {
    case Phase::Lurking: return false;
    case Phase::Chasing:
    case Phase::Searching:
    case Phase::AtHidingPlace:
    case Phase::LockedOut:
    case Phase::Moping:
    case Phase::Excited:
    case Phase::Following: return true;
  }
  return false;
}

/**
 * Whether steering begins this frame: `now` steers and the last frame (`steeredBefore`) did not. The master then
 * changes the ghost to idling once, leaving wandering or the favourite room. Idling's start sets the agent's speed
 * to GhostAI.baseSpeed, so the mod sets its own speed after that change.
 */
inline bool steering_begins(bool steeredBefore, std::optional<Phase> now) {
  return !steeredBefore && now && steers(*now);
}

/**
 * The phase after `now` given this frame's facts. Pure and stateless: the caller keeps the phase. LockedOut
 * is final under every facts. A friendly ghost only moves Moping, Excited, Following (any other phase it is
 * handed restarts at Moping); it never chases. A hostile one goes to LockedOut from a chase phase the frame
 * exactly 0 living players are inside; a negative count (unknown) keeps the phase. In a chase phase a hiding
 * target wins over knowing where it is (hiding is safe), then knowing where a target is (sight or tracking)
 * chases, and otherwise a ghost that lost them searches.
 */
inline Phase next_phase(Phase now, const Facts& f) {
  if (now == Phase::LockedOut) return Phase::LockedOut;
  if (f.friendly) {
    switch (now) {
      case Phase::Excited: return f.actFinished ? Phase::Following : Phase::Excited;
      case Phase::Following: return Phase::Following;
      case Phase::Moping: return f.seesPlayer ? Phase::Excited : Phase::Moping;
      default: return Phase::Moping;
    }
  }
  switch (now) {
    case Phase::Lurking: return f.seesPlayer ? Phase::Chasing : Phase::Lurking;
    case Phase::Chasing:
    case Phase::Searching:
    case Phase::AtHidingPlace:
      if (f.livingInside < 0) return now;
      if (f.livingInside == 0) return Phase::LockedOut;
      if (f.targetHiding) return Phase::AtHidingPlace;
      return f.seesPlayer || f.tracksPlayer ? Phase::Chasing : Phase::Searching;
    default: return Phase::Lurking;  // a friendly phase handed to a hostile ghost
  }
}

/**
 * Whether this frame is the one that locks the doors: a hostile Squatter in a chase phase with no living
 * player left inside. Once it has gone to LockedOut this is false, so they lock once.
 */
inline bool doors_lock(Phase now, const Facts& f) {
  if (f.friendly) return false;
  const bool chase = now == Phase::Chasing || now == Phase::Searching || now == Phase::AtHidingPlace;
  return chase && f.livingInside == 0;
}

/** Whether the Squatter may kick its target now: hostile, chasing, the target alive and within `reach` (metres). */
inline bool may_kick(Phase now, bool friendly, bool targetAlive, float distance, float reach) {
  return !friendly && now == Phase::Chasing && targetAlive && distance <= reach;
}

/** Metres per second of one move, for a target sprint speed of `sprintSpeed`. Stumble stands still. */
inline float move_speed(float sprintSpeed, Move m) {
  float scale = 1.0f;  // starting values: Run 1.0, Duck 0.7, Weave 0.9, Roll 1.3, Parkour 1.1, Stumble 0
  switch (m) {
    case Move::Run: scale = 1.0f; break;
    case Move::Duck: scale = 0.7f; break;
    case Move::Weave: scale = 0.9f; break;
    case Move::Roll: scale = 1.3f; break;
    case Move::Parkour: scale = 1.1f; break;
    case Move::Stumble: scale = 0.0f; break;
    case Move::Walk: scale = 0.6f; break;
    case Move::Stand: scale = 0.0f; break;
    case Move::Sneak: scale = 0.6f; break;
  }
  return sprintSpeed * kSprintMargin * scale;
}

/** The mean of move_speed over one kMoveCycle, which stays just under `sprintSpeed`. */
inline float average_chase_speed(float sprintSpeed) {
  float sum = 0.0f;
  for (const Move m : kMoveCycle) sum += move_speed(sprintSpeed, m);
  return sum / kMoveCount;
}

/** Entry `index` of kMoveCycle, counting round it either way. */
inline Move move_of(int index) { return kMoveCycle[((index % kMoveCount) + kMoveCount) % kMoveCount]; }

/** Whether move `m` plays its clip once, as against Run, Walk and Stand, whose clips loop. */
inline bool plays_once(Move m) { return m != Move::Run && m != Move::Walk && m != Move::Stand && m != Move::Sneak; }

/**
 * Whether move `m` carries the Squatter by its own clip (root motion) rather than by a pace: the rolls, the trip and
 * the jump and landing, whose clips travel as far as they show, so the body never slides under them (user,
 * 2026-11-09: it slid along the floor after tripping).
 */
inline bool moves_by_clip(Move m) { return m == Move::Roll || m == Move::Stumble || m == Move::Parkour; }

/**
 * How long the chase keeps move `m` before the cycle moves on, in seconds, when its clip lasts `clipSeconds`: a slot of
 * kMoveSlotSeconds, or for a move that plays once its whole clip when that is longer, so a roll or a fall is never cut
 * off by the next move (user, 2026-11-09).
 */
inline float move_hold_seconds(Move m, float clipSeconds) {
  return plays_once(m) && std::isfinite(clipSeconds) ? std::max(kMoveSlotSeconds, clipSeconds) : kMoveSlotSeconds;
}

/** The set of lines spoken in a phase. Catch and Tired are triggered by events instead, not by a phase. */
inline Lines lines_for(Phase now) {
  switch (now) {
    case Phase::Lurking: return Lines::Lurk;
    case Phase::Chasing: return Lines::Chase;
    case Phase::Searching: return Lines::Search;
    case Phase::AtHidingPlace: return Lines::Hiding;
    case Phase::LockedOut: return Lines::Door;
    case Phase::Moping: return Lines::Mope;
    case Phase::Excited: return Lines::Excited;
    case Phase::Following: return Lines::Friendly;
  }
  return Lines::Lurk;
}

/** Whether to taunt now: the target's sprint has just lapsed and `secondsSinceTaunt` has passed the cooldown. */
inline bool tired_taunt_due(bool targetCanSprint, bool targetCouldSprintBefore, float secondsSinceTaunt) {
  return !targetCanSprint && targetCouldSprintBefore && secondsSinceTaunt >= kTauntCooldown;
}

/** An index below `count` picked by `roll`, never `last` when there is another to pick; -1 when `count` is 0. */
inline int pick_line(int count, int last, uint32_t roll) {
  if (count <= 0) return -1;
  if (count == 1) return 0;
  if (last < 0 || last >= count) return static_cast<int>(roll % static_cast<uint32_t>(count));
  const int pick = static_cast<int>(roll % static_cast<uint32_t>(count - 1));
  return pick >= last ? pick + 1 : pick;
}

}  // namespace pm::custom::squatter
