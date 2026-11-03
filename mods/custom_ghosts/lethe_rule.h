#pragma once

#include <cstdint>
#include <initializer_list>

// The Lethe's rules, kept free of game calls so they can be tested offline. It makes the living forget:
// media of it fades from the journal unless someone looks at it first; it forgets its prey once out of
// sight; and each of its appearances comes back once, the same, a few minutes later (deja vu).
namespace pm::custom {

constexpr float kLetheFade = 180.0f;       // seconds a piece of media lasts unless someone looks at it
constexpr float kLetheGiveUp = 1.0f;       // seconds out of sight before it forgets its prey
constexpr float kLetheReplayMin = 120.0f;  // seconds after an appearance its replay comes, at least
constexpr float kLetheReplayMax = 240.0f;  // and at most

/** Whether media `age` seconds old, `viewed` or not, fades now. */
inline bool lethe_fades(float age, bool viewed) { return !viewed && age >= kLetheFade; }

/** Whether prey out of sight for `unseen` seconds is forgotten. */
inline bool lethe_forgets(float unseen) { return unseen >= kLetheGiveUp; }

/**
 * The time the player its hunt still looks for has been out of the game's sight. The count is that player's
 * own: it starts over for another player, for nobody, while they are in sight, and once they are forgotten.
 * `Keep` holds the player between frames: pm::api::Held in the game; a plain pointer in the offline tests.
 */
template <class Keep = void*>
class LetheMemory {
 public:
  /** `lost` (null for nobody) was `seen` or not for `dt` seconds; true when it forgets them now. */
  bool forgets(void* lost, bool seen, float dt) {
    if (lost != static_cast<void*>(prey_)) {
      prey_ = lost;
      unseen_ = 0;
    }
    if (!lost || seen) {
      unseen_ = 0;
      return false;
    }
    unseen_ += dt;
    if (!lethe_forgets(unseen_)) return false;
    prey_ = nullptr;
    unseen_ = 0;
    return true;
  }

 private:
  Keep prey_{};
  float unseen_ = 0;
};

/** When an appearance at `at` (seconds) replays, for a roll `r` in 0..1. */
inline float lethe_replay_at(float at, float r) { return at + kLetheReplayMin + (kLetheReplayMax - kLetheReplayMin) * r; }

// The GhostAI states that take the ghost's model over from a replay, as the game numbers them
// (GhostState, read from the game: 2, 8 and 14 in build 25811160).
struct ShowingStates {
  int hunting, appear, randomEvent;
};

/**
 * Whether the AI's new state takes the ghost's model over from a replay (a hunt, an appearance or a
 * random event), so the replay must not hide it; any other state does not hide it itself.
 */
inline bool lethe_state_shows_itself(int state, const ShowingStates& s) {
  return state == s.hunting || state == s.appear || state == s.randomEvent;
}

// The media subjects that record the ghost's activity, as the game numbers them (MediaSubject, the enum
// MediaValues.evidenceType holds, read from the game by each value's name).
struct ActivitySubjects {
  int emfSpot, fingerprint, footstep, ghost, dirtyWater, ghostWriting, usedCrucifix,
      dotsGhost,  // a D.O.T.S. ghost
      ghostOrb,
      salt,  // salt stepped in
      freezingTemperature, ghostSmoke,
      camoGhost,  // a camouflaged ghost
      paranormalSound,
      spiritBox,  // a spirit box answer
      ghostHunt, bansheeWail, spiritBoxBreath,
      motionSensor,  // a motion sensor tripped
      shadowGhost, ghostGroan, ghostLaugh, ghostTalk, ghostWhisper, obakeFingerprint,
      burningChapelCrucifix,  // a crucifix burnt in the chapel
      obakeShapeshift, emf5;
};

/**
 * Whether this client counts as looking at the journal's media: its journal is on the media pages and its own investigator is in
 * the game's player list. A ghost player's parked body is out of that list: they play no investigator, so what their journal
 * shows keeps nothing from fading.
 */
inline bool lethe_looking(bool onMediaPages, bool inPlayerList) { return onMediaPages && inPlayerList; }

/**
 * Whether journal media of this subject (MediaValues' evidence type) records the ghost's activity,
 * which fades; bones, bodies, cursed objects and the team's own gear stay.
 */
inline bool lethe_activity(int subject, const ActivitySubjects& s) {
  for (int activity : {s.emfSpot, s.fingerprint, s.footstep, s.ghost, s.dirtyWater, s.ghostWriting, s.usedCrucifix,
                       s.dotsGhost, s.ghostOrb, s.salt, s.freezingTemperature, s.ghostSmoke, s.camoGhost,
                       s.paranormalSound, s.spiritBox, s.ghostHunt, s.bansheeWail, s.spiritBoxBreath, s.motionSensor,
                       s.shadowGhost, s.ghostGroan, s.ghostLaugh, s.ghostTalk, s.ghostWhisper, s.obakeFingerprint,
                       s.burningChapelCrucifix, s.obakeShapeshift, s.emf5})
    if (subject == activity) return true;
  return false;
}

}  // namespace pm::custom
