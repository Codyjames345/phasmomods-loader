#pragma once

// GhostWanderState: the ghost's wander, the state object GhostAI.ChangeState makes for GhostState.wander
// (on the master, which runs the AI). Its class and every member are obfuscated, so they are found by an
// exact shape and machine-code facts. Notes read from decompiled code are from build 25811160;
// sdk/internals/ghost-ai.md, "The wander and the favourite room", ties them together.

#include "Transform.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

// The wander declares exactly these eight fields. GhostDotsState declares the same and one more float, so the list
// is exact (kExactFields).
inline constexpr const char* GhostWanderState_fieldTypes[] = {"@GhostAI", "NavMeshAgent", "float", "int",
                                                              "bool",     "Vector3",      "bool",  "bool"};

/// The ghost's wander (Assembly-CSharp, global namespace, name obfuscated), one of the state objects
/// that implement the ghost's state interface. ChangeState's case for GhostState.wander makes a new one
/// (its constructor sets its radius to 3); its interface methods are an enter, a step and the exit
/// (GhostWanderState.Exit), which changes the favourite room only after a long roam. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState{.key = "GhostWanderState",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .flags = pm::sdk::kExactFields,
                                                     .assembly = "Assembly-CSharp", .ns = "",
                                                     .fieldTypes = GhostWanderState_fieldTypes};

// Ten overrides of the class call the favourite room's change, and only they do: the exit its interface
// runs and nine look-alikes with other constants, which nothing runs. The exit's chance of a move is 33, 66 or
// 100 by difficulty, and no look-alike holds its 33. That is a balance number: a tuning change of it leaves this
// entry Missing, and Playable Ghost off, until the map changes. A stronger fact is the one the state machine
// rests on: the exit's name is that of the third method of the state interface, the one the change of state
// runs on the old state. A fact for it (implements an interface's method) is for when the map is next reworked.
inline constexpr pm::sdk::Fact GhostWanderState_Exit_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostController.ChangeFavouriteRoomTo"},
    {.is = pm::sdk::Fact::Is::MovesConst, .value = 33}};

/// The wander's exit, a void(): after a long enough roam it may move the ghost's favourite room through
/// GhostController.ChangeFavouriteRoomTo, the one way the game's own rules move it. Tracked by the host
/// (pm::api::inside), whichever of its copies runs. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_Exit{
    .key = "GhostWanderState.Exit", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostWanderState", .type = "void", .facts = GhostWanderState_Exit_facts};

/// The wandering ghost's GhostAI: the class's one GhostAI field. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_ghost{.key = "GhostWanderState.ghost",
                                                           .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostWanderState",
                                                           .type = "@GhostAI"};

inline constexpr const char* GhostWanderState_PickDestination_params[] = {"out Vector3"};
// Seventeen bool(out Vector3) methods of the class pick a point; only this one compares the behaviour type
// with the Goryo's (the others compare it with other numbers).
inline constexpr pm::sdk::Fact GhostWanderState_PickDestination_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Goryo"}};

/// Picks the wander's next point (a wrapper, which the enter and the step call, calls it). For a ghost
/// standing in its favourite room whose behaviour type is not the Goryo, it rolls
/// RandomRangeInt(0, 100) against the difficulty's setting (90, 80 or 70) and past it widens the radius from 3: the long
/// roam, after which the exit may change the favourite room. Then it samples a point within the radius
/// and, only for a radius other than 3, tests the path to it unless the type is the Goryo's. Its
/// callees are Unity's Random, Transform, NavMesh, NavMeshPath and Object functions, LevelValues' and
/// LevelStats' instances and one static path-length helper; none reads GhostInfo. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_PickDestination{
    .key = "GhostWanderState.PickDestination", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostWanderState",
    .type = "bool", .params = GhostWanderState_PickDestination_params,
    .facts = GhostWanderState_PickDestination_facts};

// Of the class's void() methods two call PickDestination; the other compares the behaviour type with another number.
inline constexpr pm::sdk::Fact GhostWanderState_GoToPoint_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostWanderState.PickDestination"},
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Goryo"}};

/// Sends the wander to its next point, from its enter and its step: it asks PickDestination for destination, sends
/// the agent there and sets hasDestination; or ends the wander. Mapped for hasDestination's fact. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_GoToPoint{
    .key = "GhostWanderState.GoToPoint", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostWanderState", .type = "void", .facts = GhostWanderState_GoToPoint_facts};

/// How far this wander may roam from the ghost, in metres: the class's one int. Its constructor sets 3, and only
/// PickDestination widens it, for a long roam. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_radius{.key = "GhostWanderState.radius",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "GhostWanderState", .type = "int"};

/// The point this wander walks to: the class's one Vector3, which PickDestination writes. Holds a point only while
/// hasDestination is set. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_destination{
    .key = "GhostWanderState.destination", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostWanderState", .type = "Vector3"};

// Of the class's three bools GoToPoint sets only this one to 1.
inline constexpr pm::sdk::Fact GhostWanderState_hasDestination_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostWanderState.GoToPoint", .value = 1}};

/// Whether this wander has a point it walks to: GoToPoint sets it once it has sent the agent, and the step clears
/// it when it picks another. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostWanderState_hasDestination{
    .key = "GhostWanderState.hasDestination", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostWanderState", .type = "bool",
    .facts = GhostWanderState_hasDestination_facts};

// The state declares exactly these nine fields: the wander's eight and one more float, so the wander's exact list
// never matches it, nor this one the wander.
inline constexpr const char* GhostDotsState_fieldTypes[] = {"@GhostAI", "NavMeshAgent", "float", "int", "bool",
                                                            "Vector3",  "float",        "bool",  "bool"};

/// The ghost's walk for a D.O.T.S. sighting, the state object GhostAI.ChangeState makes for GhostState.dots: a
/// wander's walk with a radius of its own, which its constructor sets to 2. Its class and every member are
/// obfuscated, so they are found by an exact shape and machine-code facts. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostDotsState{.key = "GhostDotsState",
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .flags = pm::sdk::kExactFields,
                                                   .assembly = "Assembly-CSharp", .ns = "",
                                                   .fieldTypes = GhostDotsState_fieldTypes};

// Of the class's void() methods those that call GetNearestPlayer are told apart by the type they compare with.
inline constexpr pm::sdk::Fact GhostDotsState_GoToNearest_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.GetNearestPlayer"},
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Goryo"}};

/// Sends a Goryo's D.O.T.S. walk to the nearest player and sets hasDestination. Mapped for hasDestination's fact.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostDotsState_GoToNearest{
    .key = "GhostDotsState.GoToNearest", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostDotsState", .type = "void", .facts = GhostDotsState_GoToNearest_facts};

/// How far this walk may go from the ghost, in metres: the class's one int. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostDotsState_radius{.key = "GhostDotsState.radius", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "GhostDotsState", .type = "int"};

/// The point this walk goes to: the class's one Vector3. Holds a point only while hasDestination is set. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostDotsState_destination{
    .key = "GhostDotsState.destination", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostDotsState", .type = "Vector3"};

// Of the class's three bools GoToNearest stores only this one.
inline constexpr pm::sdk::Fact GhostDotsState_hasDestination_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostDotsState.GoToNearest", .value = 1}};

/// Whether this walk has a point it goes to, as GhostWanderState.hasDestination. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostDotsState_hasDestination{
    .key = "GhostDotsState.hasDestination", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostDotsState", .type = "bool", .facts = GhostDotsState_hasDestination_facts};

}  // namespace pm::map

// The game API for the wander (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build" once
// and returns its type's default (null, false) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The ghost's wander. `wander` below is one.
struct GhostWanderState {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostWanderState> type{};
  /// The wandering ghost's GhostAI: ghost.get(wander). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostWanderState_ghost, void*> ghost{};
  /// The exit of a wander: a mod asks pm::api::inside whether one is running, the host tracking it. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::GhostWanderState_Exit, void(void*)> Exit{};
  /// PickDestination(wander, &point): picks its next point. A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostWanderState_PickDestination, bool(void*, Vector3*)> PickDestination{};
  /// How far it may roam, in metres: radius.get(wander). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostWanderState_radius, int32_t> radius{};
  /// Whether it has a point it walks to: hasDestination.get(wander). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostWanderState_hasDestination, bool> hasDestination{};
  /// The point it walks to, while hasDestination: destination.get(wander). Unresolved: the zero vector.
  static constexpr pm::api::Field<pm::map::GhostWanderState_destination, Vector3> destination{};
};

/// The ghost's walk for a D.O.T.S. sighting. `walk` below is one.
struct GhostDotsState {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostDotsState> type{};
  /// How far it may go, in metres: radius.get(walk). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostDotsState_radius, int32_t> radius{};
  /// Whether it has a point it goes to: hasDestination.get(walk). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostDotsState_hasDestination, bool> hasDestination{};
  /// The point it goes to, while hasDestination: destination.get(walk). Unresolved: the zero vector.
  static constexpr pm::api::Field<pm::map::GhostDotsState_destination, Vector3> destination{};
};

/// @}
}  // namespace phasmo
