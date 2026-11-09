#pragma once

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Transform.h"
#include "squatter_rule.h"

// What players see of the Squatter: a local copy of the investigator's body under a cloth bedsheet with eye holes
// (sheet_mesh.h), playing the user's chosen clips from the mod's own AssetBundle. Built on the SDK's figure helpers
// (sdk/api/bundles.h, sdk/api/models.h). Every client builds its own copy, never a networked object; what it plays
// comes from the master (a later task), so every client plays the same state. Main thread.
//
// The bundle is assets/squatter.bundle in the mod's assets folder, gitignored like every bundle (the clips are
// Mixamo's and may not be redistributed raw). Its controller is assets/squatter.controller, whose first layer has
// every state the table below names, spelt exactly so. figure::load checks all of that when the mod starts, and the
// mod does not start without it (a mod with an error is off, user 2026-11-06).
namespace pm::custom::squatter {

/** A one-off action the figure plays, as against a Move of the chase. */
enum class Act : uint8_t { Sit, GetUp, Kick, Bang, Bump, OpenDoorOutwards, OpenDoorInwards };

inline constexpr Act kAllActs[] = {Act::Sit, Act::GetUp, Act::Kick, Act::Bang, Act::Bump, Act::OpenDoorOutwards,
                                   Act::OpenDoorInwards};
inline constexpr Move kAllMoves[] = {Move::Run,     Move::Duck,    Move::Weave, Move::Roll, Move::Parkour,
                                     Move::Stumble, Move::Walk,    Move::Stand, Move::Sneak};

// The controller's state names, the one table the figure and the start check use (the user's clip choice,
// 2026-11-07): a move with two clips has two states, and each time it plays one of them is picked.
inline constexpr const char* kRunStates[] = {"Run"};      // Jogging into Running by the ground speed (Speed)
inline constexpr const char* kWalkStates[] = {"Walk"};    // Walking
inline constexpr const char* kSneakStates[] = {"Sneak"};  // Crouched Walking
inline constexpr const char* kStandStates[] = {"Stand"};  // Standing Idle
inline constexpr const char* kDuckStates[] = {"Duck_A", "Duck_B"};
inline constexpr const char* kWeaveStates[] = {"Weave_A", "Weave_B"};
inline constexpr const char* kRollStates[] = {"Roll_A", "Roll_B"};
inline constexpr const char* kParkourStates[] = {"Parkour_A"};  // jumping up, which runs on into a forward roll
// A trip from a run, one of two at random (user, 2026-11-09): Soccer Trip, or Tripping.
inline constexpr const char* kStumbleStates[] = {"Stumble", "Trip"};
// Clips each client plays of its own around the master's moves: turning on the spot, and setting off from standing.
inline constexpr const char* kTurnLeftState = "TurnLeft";
inline constexpr const char* kTurnRightState = "TurnRight";
inline constexpr const char* kTurnAroundState = "Turn180";
inline constexpr const char* kStartWalkState = "StartWalk";
inline constexpr const char* kLocalStates[] = {kTurnLeftState, kTurnRightState, kTurnAroundState, kStartWalkState};
// The user's Standing Up clip, which the bundle runs the trip (Stumble) on into, from where it lies face down as the
// trip lands to standing (user, 2026-11-09: getting up from the trip was too quick). Nothing plays it by name.
inline constexpr const char* kStandUpState = "StandUp";
// The trip's own clip length in seconds, before the get-up.
inline constexpr float kTripSeconds = 1.6000f;
// The get-up is imported turned this far, degrees clockwise seen from above, so it begins lying the way the trip lands:
// the turn about the vertical that leaves the least of the two clips' body rotations (their RootQ curves: the trip lands on
// its side, Standing Up begins face down) to blend. It stands up facing this far round too.
inline constexpr float kStandUpTurnDegrees = 119.0f;
// The second trip's get-up: the user's Getting Up, imported again as its own state, trimmed to its rise and turned -143
// degrees to begin lying as Tripping lands (the bundle's build says how). Tripping lasts kTrippingSeconds.
inline constexpr const char* kTripGetUpState = "TripGetUp";
inline constexpr float kTrippingSeconds = 2.1000f;
// How far round from its root each trip's get-up stands it (kStumbleStates' order): Standing Up stands facing its own
// turn; Getting Up ends 7 degrees short of its clip's forward, so its -143 turn stands it at -150.
inline constexpr float kRiseTurnDegrees[] = {kStandUpTurnDegrees, -150.0f};

/** The states a move may play, one or two. */
inline std::span<const char* const> states_of(Move m) {
  switch (m) {
    case Move::Run: return kRunStates;
    case Move::Duck: return kDuckStates;
    case Move::Weave: return kWeaveStates;
    case Move::Roll: return kRollStates;
    case Move::Parkour: return kParkourStates;
    case Move::Stumble: return kStumbleStates;
    case Move::Walk: return kWalkStates;
    case Move::Stand: return kStandStates;
    case Move::Sneak: return kSneakStates;
  }
  return kRunStates;
}

/** The state an act plays. Bang plays the kick's clip, which the bundle's Bang state holds (user, 2026-11-07). */
inline const char* state_of(Act a) {
  switch (a) {
    case Act::Sit: return "Sit";
    case Act::GetUp: return "GetUp";
    case Act::Kick: return "Kick";
    case Act::Bang: return "Bang";
    case Act::Bump: return "Bump";
    case Act::OpenDoorOutwards: return "OpenDoorOutwards";
    case Act::OpenDoorInwards: return "OpenDoorInwards";
  }
  return "Kick";
}

/**
 * The state move `m` plays for `variant`: variant modulo the move's state count, so the master picks a variant once
 * and every client that is sent it plays the same clip.
 */
inline const char* state_for(Move m, uint32_t variant) {
  const std::span<const char* const> states = states_of(m);
  return states[variant % states.size()];
}

/** A state's clip and how long it lasts. */
struct StateClip {
  const char* state;
  float seconds;
};

// Each state's clip length in seconds, as the bundle's build reads the user's clips: how long a move that plays once
// holds the chase (move_hold_seconds), and what a cross-fade's length is measured against. They change with the clips.
inline constexpr StateClip kStateClips[] = {
    {"Run", 0.7000f},     {"Walk", 1.0333f},      {"Stand", 6.0000f},     {"Sneak", 1.0333f}, {"Duck_A", 1.2667f},
    {"Duck_B", 1.1000f},  {"Weave_A", 1.1333f},   {"Weave_B", 1.1333f},   {"Roll_A", 1.1667f},
    {"Roll_B", 2.3667f},  {"Parkour_A", 1.4000f}, {"Parkour_B", 2.0000f}, {"Stumble", 6.1000f}, {"Trip", 4.3000f},
    {"Sit", 10.2667f},    {"GetUp", 2.7000f},     {"Kick", 1.3000f},      {"Bang", 1.3000f},
    {"Bump", 1.1667f},    {"OpenDoorOutwards", 4.1000f}, {"OpenDoorInwards", 6.3667f},
    {"TurnLeft", 1.0333f}, {"TurnRight", 1.0333f},  {"Turn180", 1.0000f},   {"StartWalk", 2.9333f},
    {"StandUp", 4.5000f}, {"TripGetUp", 2.2000f}};

// How fast each paced move's clips travel on their own, metres a second, as the bundle's build reads them: the Run
// blend goes from Jogging at the first to Running at the second by the ground speed, and Walk and Sneak are one clip.
inline constexpr float kJogSpeed = 1.8825f, kRunSpeed = 4.5229f, kWalkSpeed = 1.7006f, kSneakSpeed = 1.3565f;
// The slowest and fastest a paced clip plays, as a share of its own speed: starting values, judged with the user.
inline constexpr float kMinPlayRate = 0.25f, kMaxPlayRate = 2.0f;

/**
 * How fast move `m`'s clip plays (the controller's PlayRate) for a body moving at `speed` metres a second, so its feet
 * keep pace with the ground, as games match a stride to the speed: the ground speed over the speed its clips travel
 * at, which for Run is the blend's own between Jogging and Running. 1 for a move that is not paced, or not moving.
 */
inline float play_rate(Move m, float speed) {
  float own = 0.0f;
  switch (m) {
    case Move::Run: own = std::clamp(speed, kJogSpeed, kRunSpeed); break;
    case Move::Walk: own = kWalkSpeed; break;
    case Move::Sneak: own = kSneakSpeed; break;
    default: return 1.0f;
  }
  if (!(speed > 0.0f)) return 1.0f;
  return std::clamp(speed / own, kMinPlayRate, kMaxPlayRate);
}

/** How long `state`'s clip lasts in seconds; 0 for a state the table does not name. */
inline float seconds_of(const char* state) {
  for (const StateClip& c : kStateClips)
    if (state && std::string_view(c.state) == state) return c.seconds;
  return 0.0f;
}

// How long one clip blends into the next, in seconds (Animator.CrossFade): Unity's own default transition length, so
// the figure never snaps from one pose to another (user, 2026-11-09).
inline constexpr float kBlendSeconds = 0.25f;

/** Every state name of the table, each once: what the bundle's controller must have. */
inline std::vector<const char*> all_states() {
  std::vector<const char*> out;
  for (const Move m : kAllMoves)
    for (const char* s : states_of(m)) out.push_back(s);
  for (const Act a : kAllActs) out.push_back(state_of(a));
  for (const char* s : kLocalStates) out.push_back(s);
  out.push_back(kStandUpState);
  out.push_back(kTripGetUpState);
  return out;
}

namespace figure {

/**
 * Loads the bundle from `assets` (the mod's assets folder) and checks its controller has every state of the table.
 * False with `why` set, keeping nothing, when the file is missing, the engine refuses it, the controller is missing or
 * lacks a state, or an entry it needs (Cloth among them) did not resolve. From pmmod_start, before anything is
 * registered or hooked.
 */
bool load(const std::filesystem::path& assets, std::string& why);
/** Destroys the figure and unloads the bundle, as the mod stops. */
void unload();
/** Builds the figure at `at`, facing +z, when it is not built. A failure is logged once and leaves ready() false until destroy(). */
void create(phasmo::Vector3 at);
/** Destroys the figure, its sheet and what it made; a later create() builds it again. */
void destroy();
/** Moves the figure's feet to `at`, facing `yaw` radians about the vertical from +z. */
void place(phasmo::Vector3 at, float yaw);
/** Plays move `m`'s state for `variant` (state_for). */
void play(Move m, uint32_t variant);
/** Plays act `a`'s state. */
void play_act(Act a);
/**
 * Paces the figure's walking and running for a body moving at `speed` metres a second while it plays move `m`: the
 * controller's Speed (the Run blend) and PlayRate (play_rate). Every frame, on every client.
 */
void set_pace(Move m, float speed);
/** How far the figure's clip carried it last frame (root motion), metres in the world; zero when it is not built. */
phasmo::Vector3 clip_step();

/**
 * Plays `state` outright, with no blend, and poses the body in it this same frame (Animator.Update with no time), so a
 * turn of the figure's root set with place() in the same frame shows with it and nothing between. This client alone.
 */
void snap_to(const char* state);
/** Plays one of the figure's own clips (kLocalStates) on this client alone, blending into it. */
void play_clip(const char* state);
/** How far the figure's clip turned it last frame (root rotation about the vertical), radians, clockwise seen from above. */
float clip_turn();

/** A hand of the figure. */
enum class Hand : uint8_t { Left, Right };
/** The hand whose shoulder is nearer `at`, metres in the world. Right when the figure is not built. */
Hand nearer_hand(phasmo::Vector3 at);
/**
 * Reaches `hand` for `at` (world space) by `weight`, 0 (the clip's own arm) to 1 (the hand on the point), with Final IK's
 * full-body solver after the Animator has posed the body, so the shoulder and body lean into the reach as games put a
 * hand on a door handle. The other hand is let go. Every frame while it reaches; a weight of 0 lets both go.
 */
void reach(Hand hand, phasmo::Vector3 at, float weight);
/**
 * Turns the figure's head, and a little of its spine, toward `at` (world space) by `weight`, 0 to 1, with Final IK's
 * look-at solver, as games turn a character's head to what it attends to. Every frame; 0 lets it go.
 */
void look_at(phasmo::Vector3 at, float weight);
/** The figure's head bone, a Transform, which its voice follows; null when the figure is not built. */
void* head();
/** Whether the figure is built and still in the scene (a scene load destroys it, and its own assets with it). */
bool ready();

}  // namespace figure
}  // namespace pm::custom::squatter
