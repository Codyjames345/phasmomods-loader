#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
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
inline constexpr Move kAllMoves[] = {Move::Run, Move::Duck, Move::Weave, Move::Roll, Move::Parkour, Move::Stumble};

// The controller's state names, the one table the figure and the start check use (the user's clip choice,
// 2026-11-07): a move with two clips has two states, and each time it plays one of them is picked.
inline constexpr const char* kRunStates[] = {"Run"};
inline constexpr const char* kDuckStates[] = {"Duck_A", "Duck_B"};
inline constexpr const char* kWeaveStates[] = {"Weave_A", "Weave_B"};
inline constexpr const char* kRollStates[] = {"Roll_A", "Roll_B"};
inline constexpr const char* kParkourStates[] = {"Parkour_A", "Parkour_B"};
inline constexpr const char* kStumbleStates[] = {"Stumble"};

/** The states a move may play, one or two. */
inline std::span<const char* const> states_of(Move m) {
  switch (m) {
    case Move::Run: return kRunStates;
    case Move::Duck: return kDuckStates;
    case Move::Weave: return kWeaveStates;
    case Move::Roll: return kRollStates;
    case Move::Parkour: return kParkourStates;
    case Move::Stumble: return kStumbleStates;
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

/** Every state name of the table, each once: what the bundle's controller must have. */
inline std::vector<const char*> all_states() {
  std::vector<const char*> out;
  for (const Move m : kAllMoves)
    for (const char* s : states_of(m)) out.push_back(s);
  for (const Act a : kAllActs) out.push_back(state_of(a));
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
/** Whether the figure is built and still in the scene (a scene load destroys it, and its own assets with it). */
bool ready();

}  // namespace figure
}  // namespace pm::custom::squatter
