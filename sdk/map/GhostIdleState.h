#pragma once

// GhostIdleState: the ghost's idle state, the state object GhostAI's state machine runs while the
// ghost idles (on the master, which runs the AI). Each time it decides what to do next it works out
// the insanity its hunt check uses. Its class and every member are obfuscated, so they are found by
// shape and machine-code facts. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LocalisationSystem.h"
#include "Player.h"

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

inline constexpr const char* GhostIdleState_ctor_params[] = {"@GhostAI"};

/// The idle state's constructor, (GhostAI): GhostAI.ChangeState makes a new idle state with it each time the ghost
/// idles. Decompiled: it sets the step's timer to 2 seconds, `rollRange` to 100 and the chance factor to 1, makes the
/// state's FrameSkipFilter and keeps the GhostAI in `ghost`. The controllers come later, in the state's enter.
inline constexpr pm::sdk::EntryDesc GhostIdleState_ctor{.key = "GhostIdleState.ctor", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostIdleState", .name = ".ctor", .type = "void",
                                                        .params = GhostIdleState_ctor_params};

/// The scene's LevelController, LevelController.instance: the state's enter copies it here. The class's one field of
/// that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_levelController{
    .key = "GhostIdleState.levelController", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "LevelController"};

/// The scene's GameController, GameController.instance: the state's enter copies it here, and HuntInsanity asks it
/// for the players' average insanity. The class's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_gameController{
    .key = "GhostIdleState.gameController", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "GameController"};

/// The scene's SoundController, SoundController.instance: the state's enter copies it here, and the Onryo's rule in
/// TypeHuntRules asks it which floor a flame is on. The class's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_soundController{
    .key = "GhostIdleState.soundController", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .type = "SoundController"};

// Twenty void() methods of the class ask for an insanity the same way and differ in the types they
// compare; only the hunt insanity compares the behaviour type with the Thaye's, and it asks
// GameController.GetAveragePlayerInsanity, as some look-alikes do.
inline constexpr pm::sdk::Fact GhostIdleState_HuntInsanity_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Thaye"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GameController.GetAveragePlayerInsanity"}};

/// Works out the insanity the idle state's hunt check uses: the players' average insanity (or,
/// while the ghost has a player of its own, that player's), plus an offset of the state's, written
/// to `insanity`, and a chance beside it, with branches for the Thaye and one more type. In order: it
/// writes the whole-number part of that insanity to `insanity`, runs TypeHuntRules, works out the chance,
/// then adds the type's offset to `insanity`. So `insanity` less the insanity it started from is the
/// type's offset, and the hunt line in sanity is 50 plus that offset. Its only caller is the state's
/// Execute, which then tests for a hunt. It writes `rollRange` through TypeHuntRules, and sends nothing.
/// Decompiled.
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

// The look-alikes compare the same types; only the real one is called by the hunt insanity, and it asks
// the ghost's room whether players talk in it (the Yokai) and the activity to refresh its electronics
// (the Raiju).
inline constexpr pm::sdk::Fact GhostIdleState_TypeHuntRules_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostIdleState.HuntInsanity"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "LevelRoom.ArePlayersTalkingInRoom"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostActivity.UpdateElectronics"}};

/// The type's part of the idle hunt check, run by HuntInsanity, which then adds the offset it wrote to
/// `insanity`. It first clears that offset (an int field of the state's own; no fact the offline reader can
/// prove tells it from a chance bonus beside it, so it has no entry) and the chance bonus, sets a chance
/// factor to 1, then branches on the behaviour type (GhostInfo.behaviourType, through `ghost`).
/// A state made with the constructor and given the three controllers the enter copies (LevelController.instance,
/// GameController.instance, SoundController.instance) answers HuntInsanity and this as the running one would; the
/// enter's other work (the timer, the ghost's animation and audio, `rollRange` by difficulty) is not read here.
/// Decompiled, the hunt line it gives each type, as the sanity at or below which the check may start a
/// hunt (50 plus the offset):
/// - the Mare (6): 40 while a light switch of LevelController.currentGhostRoom is on
///   (LightSwitch.get_isOn; a switch with LightSwitch.ghostUseRefused set is skipped), 60 otherwise;
/// - the Shade (8): 35, and its activity roll's range is made half as large again (`rollRange`);
/// - the Demon (9): 70;
/// - the Yokai (12): 80 while players talk in LevelController.currentGhostRoom (LevelRoom.ArePlayersTalkingInRoom),
///   with a chance bonus as well;
/// - the Onryo (16): 40 while one of the level's fire sources is lit (FireSource.isOn) on the ghost's
///   floor type and near it, 60 otherwise;
/// - the Raiju (18): 65 while electronics are on near it (GhostActivity.UpdateElectronics, run here);
/// - the Deogen (22): 40;
/// - the Gallu (25): 60 enraged, 40 weakened, 50 otherwise (a state of its own);
/// - the Dayan (26): 65 in one state, 45 in the other, 50 before either (a state of its own);
/// - the Obambo (27): 65 aggressive (GhostAI.obamboAggressive), 10 calm;
/// - the Kormos (28): 70 while a condition on a player in the ghost's room holds, 50 otherwise;
/// - every other type, "no type" among them: 50. The Oni (11) and the Wraith (1) get a chance bonus
///   only, and the Goryo (14) a doubled chance factor.
/// The Thaye (23) is not here: its check compares with GhostController.thayeThreshold instead. The
/// method reads the state's room and sound controller, which the state's enter sets, so it is only
/// meaningful on the idle state the machine runs now (GhostStateMachine.current), and it throws on a
/// state that has never entered. Master only (the idle state runs there). It sends nothing.
inline constexpr pm::sdk::EntryDesc GhostIdleState_TypeHuntRules{
    .key = "GhostIdleState.TypeHuntRules", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostIdleState", .hint = "TypeHuntRules", .type = "void",
    .facts = GhostIdleState_TypeHuntRules_facts};

// The step reads two int fields: the hunt insanity and this.
inline constexpr pm::sdk::Fact GhostIdleState_rollRange_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostIdleState.Execute"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostIdleState.insanity"}};

/// The top of the idle state's activity roll (Random.RandomRangeInt from 0): the constructor sets 100,
/// and TypeHuntRules makes it half as large again for the Shade, each time it runs, without resetting
/// it. A mod that runs TypeHuntRules itself puts this back afterwards. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostIdleState_rollRange{
    .key = "GhostIdleState.rollRange", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostIdleState", .hint = "rollRange", .type = "int", .facts = GhostIdleState_rollRange_facts};

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
  /// ctor(idle, ghostAI): constructs an object from type.make(); see the entry's note. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_ctor, void(pm::api::Ref<pm::map::GhostIdleState>, pm::api::Ref<pm::map::GhostAI>)> ctor{};
  /// The scene's LevelController its enter copies: levelController.get(idle), .set(idle, controller).
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_levelController, pm::api::Ref<pm::map::LevelController>> levelController{};
  /// The scene's GameController its enter copies: gameController.get(idle), .set(idle, controller).
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_gameController, pm::api::Ref<pm::map::GameController>> gameController{};
  /// The scene's SoundController its enter copies: soundController.get(idle), .set(idle, controller).
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_soundController, pm::api::Ref<pm::map::SoundController>> soundController{};
  /// The idling ghost's GhostAI: ghost.get(idle). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostIdleState_ghost, pm::api::Ref<pm::map::GhostAI>> ghost{};
  /// The insanity its hunt check uses: insanity.get(idle), insanity.set(idle, value). Unresolved: 0,
  /// writes nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_insanity, int32_t> insanity{};
  /// HuntInsanity(idle). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_HuntInsanity, void(pm::api::Ref<pm::map::GhostIdleState>)> HuntInsanity{};
  /// Execute(idle): its step, the idle roll. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_Execute, void(pm::api::Ref<pm::map::GhostIdleState>)> Execute{};
  /// TypeHuntRules(idle): writes the type's offset to the hunt insanity (and grows the Shade's
  /// `rollRange`) from the contract as it stands now. A mod usually hooks it. Only on the running idle
  /// state, on the master; it throws on a state that has not entered. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostIdleState_TypeHuntRules, void(pm::api::Ref<pm::map::GhostIdleState>)> TypeHuntRules{};
  /// The top of the activity roll: rollRange.get(idle), rollRange.set(idle, n). Unresolved: 0, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::GhostIdleState_rollRange, int32_t> rollRange{};
};

/// @}
}  // namespace phasmo
