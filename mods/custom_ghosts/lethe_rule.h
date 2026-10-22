#pragma once

#include <cstdint>

// The Lethe's rules, kept free of game calls so they can be tested offline. It makes the living forget:
// media of it fades from the journal unless someone looks at it first; it forgets its prey once out of
// sight; and each of its appearances comes back once, the same, a few minutes later (deja vu).
namespace pg::custom {

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
 * `Keep` holds the player between frames: il2cpp::Held in the game; a plain pointer in the offline tests.
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

/**
 * Whether the AI's new state takes the ghost's model over from a replay (the GhostAI states: 2 hunt,
 * 8 appear, 14 random event), so the replay must not hide it; any other state does not hide it itself.
 */
inline bool lethe_state_shows_itself(int state) { return state == 2 || state == 8 || state == 14; }

/**
 * Whether journal media of this subject (MediaValues' evidence type, build 25811160) records the
 * ghost's activity, which fades; bones, bodies, cursed objects and the team's own gear stay.
 */
inline bool lethe_activity(int subject) {
  switch (subject) {
    case 0:   // EMF spot
    case 2:   // fingerprint
    case 3:   // footstep
    case 5:   // the ghost
    case 7:   // dirty water
    case 13:  // ghost writing
    case 14:  // used crucifix
    case 15:  // D.O.T.S. ghost
    case 18:  // ghost orb
    case 22:  // salt stepped in
    case 23:  // freezing temperature
    case 24:  // ghost smoke
    case 25:  // camouflaged ghost
    case 26:  // paranormal sound
    case 27:  // spirit box answer
    case 28:  // ghost hunt
    case 29:  // banshee wail
    case 31:  // spirit box breath
    case 32:  // motion sensor tripped
    case 33:  // shadow ghost
    case 34: case 35: case 36: case 37:  // groan, laugh, talk, whisper
    case 38:  // obake fingerprint
    case 39:  // crucifix burnt in the chapel
    case 40:  // obake shapeshift
    case 43:  // EMF 5
      return true;
    default:
      return false;
  }
}

}  // namespace pg::custom
