#pragma once

// The hiding places players hide in during a hunt: closets and lockers (ClosetZone, with their doors and
// the spot where a searcher stands) and the other spots (HidingSpot). Notes read from decompiled code are
// from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// A closet or locker a player can hide in (Assembly-CSharp, global namespace). The hunt's closet
/// search goes to its nav spot, checks it with CheckCloset, then kills whoever hides there. Decompiled.
inline constexpr pm::sdk::EntryDesc ClosetZone{.key = "ClosetZone", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "ClosetZone", .assembly = "Assembly-CSharp", .ns = ""};

/// The closet's left door, a Door. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc ClosetZone_leftClosetDoor{.key = "ClosetZone.leftClosetDoor",
                                                              .kind = pm::sdk::Kind::Field, .owner = "ClosetZone",
                                                              .name = "leftClosetDoor"};

/// The closet's right door, a Door. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc ClosetZone_rightClosetDoor{.key = "ClosetZone.rightClosetDoor",
                                                               .kind = pm::sdk::Kind::Field, .owner = "ClosetZone",
                                                               .name = "rightClosetDoor"};

/// The Transform where a searcher stands to enter the closet (its tooltip says "A position used to enter
/// the Closet"). Inferred: from the field's name and tooltip.
inline constexpr pm::sdk::EntryDesc ClosetZone_navSpot{.key = "ClosetZone.navSpot", .kind = pm::sdk::Kind::Field,
                                                       .owner = "ClosetZone", .name = "navSpot"};

/// The closet check the hunt's closet search runs on a closet it reaches: it marks the closet searched and
/// moves its doors as a hunting ghost does (Door.CheckDoorDuringHunt on the left door, the door's force
/// coroutine on the right), the closet's open or close kind deciding which way. It is the ghost moving
/// doors. Custom Ghosts' Bogeyman has called it since 2026-10-20. Decompiled.
inline constexpr pm::sdk::EntryDesc ClosetZone_CheckCloset{.key = "ClosetZone.CheckCloset",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "ClosetZone", .name = "CheckCloset"};

/// Where a ghost stands outside the closet's entrance, a Transform (its tooltip says "usually centred and
/// about a foot away from the door"): the place to wait at a closet without opening it. Inferred: from the
/// field's name and tooltip.
inline constexpr pm::sdk::EntryDesc ClosetZone_standOutsideEntrancePoint{
    .key = "ClosetZone.standOutsideEntrancePoint", .kind = pm::sdk::Kind::Field, .owner = "ClosetZone",
    .name = "standOutsideEntrancePoint"};

/// The players hiding in the closet, a List<Player>: the closet's one list of them. Its trigger adds a living
/// player whose body enters and sets their Player.closet, and removes them and clears it as they leave, on
/// each client its physics runs on. Decompiled.
inline constexpr pm::sdk::EntryDesc ClosetZone_occupants{.key = "ClosetZone.occupants", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "ClosetZone", .type = "List<Player>"};

/// Whether anyone hides in the closet: true while ClosetZone.occupants is not empty. Decompiled.
inline constexpr pm::sdk::EntryDesc ClosetZone_ArePlayersInside{.key = "ClosetZone.ArePlayersInside",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "ClosetZone", .name = "ArePlayersInside"};

/// Forces the closet's doors as the ghost does (Door.GhostForceDoor on the left door, the force coroutine on
/// the right): a ghost's interaction with the doors, which Door.GhostForceDoor sends to every client.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ClosetZone_OpenCloseClosetDoors{
    .key = "ClosetZone.OpenCloseClosetDoors", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ClosetZone", .name = "OpenCloseClosetDoors"};

/// A NavMeshObstacle the game switches on and off by a wish of its own (Assembly-CSharp, global namespace). Each closet
/// has one over its inside (ClosetZone.internalAreaObstacle), which keeps a ghost's route out of the closet while it is
/// on. Decompiled.
inline constexpr pm::sdk::EntryDesc DynamicNavMeshObstacle{.key = "DynamicNavMeshObstacle",
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .name = "DynamicNavMeshObstacle",
                                                           .assembly = "Assembly-CSharp", .ns = ""};

/// The obstacle's NavMeshObstacle, which Update switches on and off. Decompiled.
inline constexpr pm::sdk::EntryDesc DynamicNavMeshObstacle_navMeshObstacle{
    .key = "DynamicNavMeshObstacle.navMeshObstacle", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DynamicNavMeshObstacle", .name = "navMeshObstacle"};

/// Whether the obstacle is wanted on: Update follows it, on every client. Cleared, Update switches the NavMeshObstacle
/// off at once, so routes may cross where it stood; set again, Update switches it back on only after a short wait (a
/// DebouncerFilter of the game's), and never while a non-trigger collider with a particular tag stands inside the
/// obstacle's own trigger. The obstacle's OnTriggerEnter and OnTriggerExit note that collider on the master only. The
/// property TargetActivity reads and writes it. Decompiled.
inline constexpr pm::sdk::EntryDesc DynamicNavMeshObstacle_targetActivity{
    .key = "DynamicNavMeshObstacle.targetActivity", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DynamicNavMeshObstacle", .name = "targetActivity"};

/// Each frame: switches the NavMeshObstacle to follow DynamicNavMeshObstacle.targetActivity, as that entry says. A mod
/// hooks it; never calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc DynamicNavMeshObstacle_Update{
    .key = "DynamicNavMeshObstacle.Update", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DynamicNavMeshObstacle", .name = "Update"};

/// The obstacle over the closet's inside, a DynamicNavMeshObstacle: while it is on, a ghost's route stops at the
/// closet's doorway, open doors or not (a mod's own walk into a closet on Willow Street's pantry, 2026-11-09). Which game
/// code switches it was not traced. Inferred: from the class dump (the field's name and type).
inline constexpr pm::sdk::EntryDesc ClosetZone_internalAreaObstacle{
    .key = "ClosetZone.internalAreaObstacle", .kind = pm::sdk::Kind::Field, .owner = "ClosetZone",
    .name = "internalAreaObstacle", .type = "@DynamicNavMeshObstacle"};

/// A hiding place that is not a closet (Assembly-CSharp, global namespace): under a bed, behind a
/// curtain. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HidingSpot{.key = "HidingSpot", .name = "HidingSpot", .assembly = "Assembly-CSharp",
                                               .ns = ""};

}  // namespace pm::map

/// @file
/// @brief The game API for the hiding places (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// A closet or locker. `closet` below is one.
struct ClosetZone {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::ClosetZone> type{};
  /// Its left door, or null: leftClosetDoor.get(closet). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ClosetZone_leftClosetDoor, void*> leftClosetDoor{};
  /// Its right door, or null: rightClosetDoor.get(closet). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ClosetZone_rightClosetDoor, void*> rightClosetDoor{};
  /// Where a searcher stands, a Transform: navSpot.get(closet). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ClosetZone_navSpot, void*> navSpot{};
  /// Where a ghost stands outside its entrance, a Transform: standOutsideEntrancePoint.get(closet).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::ClosetZone_standOutsideEntrancePoint, void*> standOutsideEntrancePoint{};
  /// The players hiding in it, a List<Player>: occupants.get(closet), read as a pm::api::List. Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::ClosetZone_occupants, void*> occupants{};
  /// CheckCloset(closet): the hunt's check, which moves its doors as a ghost does. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ClosetZone_CheckCloset, void(void*)> CheckCloset{};
  /// ArePlayersInside(closet): whether anyone hides in it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::ClosetZone_ArePlayersInside, bool(void*)> ArePlayersInside{};
  /// OpenCloseClosetDoors(closet): forces its doors as a ghost does. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::ClosetZone_OpenCloseClosetDoors, void(void*)> OpenCloseClosetDoors{};
  /// The obstacle over its inside, a DynamicNavMeshObstacle: internalAreaObstacle.get(closet). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ClosetZone_internalAreaObstacle, void*> internalAreaObstacle{};
};

/// A NavMeshObstacle the game switches on and off. `obstacle` below is one.
struct DynamicNavMeshObstacle {
  /// Whether it is wanted on: targetActivity.get(obstacle), targetActivity.set(obstacle, on). Cleared, its Update
  /// switches the obstacle off on this client; set, back on after a short wait. Unresolved: false, and a set does
  /// nothing.
  static constexpr pm::api::Field<pm::map::DynamicNavMeshObstacle_targetActivity, bool> targetActivity{};
  /// Its NavMeshObstacle: navMeshObstacle.get(obstacle). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DynamicNavMeshObstacle_navMeshObstacle, void*> navMeshObstacle{};
};

/// A hiding place that is not a closet.
struct HidingSpot {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::HidingSpot> type{};
};

/// @}
}  // namespace phasmo
