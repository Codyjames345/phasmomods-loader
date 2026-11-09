#pragma once

// GhostIdleState: the ghost's idle state, the state object GhostAI's state machine runs while the
// ghost idles (on the master, which runs the AI). Each time it decides what to do next it works out
// the insanity its hunt check uses. Its class and every member are obfuscated, so they are found by
// shape and machine-code facts. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

// The idle state's class holds a GhostAI, the level's, the game's and the sound's controllers, and a
// FrameSkipFilter; no other class has all of them.
inline constexpr const char* GhostIdleState_fieldTypes[] = {"@GhostAI", "LevelController", "GameController",
                                                            "SoundController", "FrameSkipFilter"};

/// The ghost's idle state (Assembly-CSharp, global namespace, name obfuscated), one of the state
/// objects that implement the ghost's state interface. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState{.key = "GhostIdleState",
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .assembly = "Assembly-CSharp", .ns = "",
                                                   .fieldTypes = GhostIdleState_fieldTypes};

/// The idling ghost's GhostAI: the class's one GhostAI field. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_ghost{.key = "GhostIdleState.ghost", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostIdleState",
                                                         .type = "@GhostAI"};

// Twenty void() methods of the class ask for an insanity the same way and differ in the types they
// compare; only the hunt insanity compares the behaviour type with the Thaye's, and it asks
// GameController.GetAveragePlayerInsanity, as some look-alikes do.
inline constexpr pm::sdk::Fact GhostIdleState_HuntInsanity_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Thaye"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GameController.GetAveragePlayerInsanity"}};

/// Works out the insanity the idle state's hunt check uses: the players' average insanity (or,
/// while the ghost has a player of its own, that player's), plus an offset of the state's, written
/// to `insanity`, and a chance beside it, with branches for the Thaye and one more type. Its only
/// caller is the state's Execute, which then tests for a hunt. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_HuntInsanity{
    .key = "GhostIdleState.HuntInsanity", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "void", .facts = GhostIdleState_HuntInsanity_facts};

// HuntInsanity's only caller.
inline constexpr pm::sdk::Fact GhostIdleState_Execute_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostIdleState.HuntInsanity"}};

/// The idle state's step, its override of the state interface's update, run each frame. For the Dayan
/// (26) it runs GhostAI.DayanPlayerChecks on the frames its FrameSkipFilter lets through. When its timer
/// runs out it works out the hunt insanity (HuntInsanity), and the roll begins.
/// The Onryo's hunt comes first. If GhostAI.flamesBlownOut is above 2, and none of
/// LevelValues.eventsBlocked, GhostAI.postHuntCooldown and GhostAI.huntsHeldOff is set, it clears the
/// count, calls GhostAI.ChangeState with GhostState.hunting and adds one to a LevelStats count (not
/// mapped). That hunt draws no random number and skips the sanity check. Only the Onryo (16) adds to
/// the count (FireSource.CountGhostBlowOut).
/// Otherwise its own Random.RandomRangeInt calls run, in order: a 0-to-2 coin; on 1 the hunt branch,
/// which needs the same three flags clear and, after the type's, sanity and setup checks (the Thaye's against GhostController.thayeThreshold) draws its final 1-in-3, 4
/// or 5 chance; then the activity roll. The callees it runs before that final chance (DayanPlayerChecks,
/// HuntInsanity, LevelValues' getters) draw no random numbers. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_Execute{
    .key = "GhostIdleState.Execute", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "void", .facts = GhostIdleState_Execute_facts};

// Of the class's four int fields the hunt insanity writes only this.
inline constexpr pm::sdk::Fact GhostIdleState_insanity_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostIdleState.HuntInsanity"}};

/// The insanity the idle state's hunt check uses, a whole number: HuntInsanity writes it, and the
/// hunt check right after compares it with the type's threshold. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_insanity{
    .key = "GhostIdleState.insanity", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "int", .facts = GhostIdleState_insanity_facts};

}  // namespace pm::map

/// @file
/// @brief The game API for the idle state (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The ghost's idle state. `idle` below is one.
struct GhostIdleState {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostIdleState> type{};
  /// The idling ghost's GhostAI: ghost.get(idle). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostIdleState_ghost, void*> ghost{};
  /// The insanity its hunt check uses: insanity.get(idle), insanity.set(idle, value). Unresolved: 0,
  /// writes nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_insanity, int32_t> insanity{};
  /// HuntInsanity(idle). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_HuntInsanity, void(void*)> HuntInsanity{};
  /// Execute(idle): its step, the idle roll. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_Execute, void(void*)> Execute{};
};

/// @}
}  // namespace phasmo
