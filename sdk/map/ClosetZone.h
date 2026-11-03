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

/// The closet check the hunt's closet search runs on a closet it reaches. Inferred: from the method's
/// name; Custom Ghosts' Bogeyman has called it since 2026-10-20.
inline constexpr pm::sdk::EntryDesc ClosetZone_CheckCloset{.key = "ClosetZone.CheckCloset",
                                                           .kind = pm::sdk::Kind::Method, .owner = "ClosetZone",
                                                           .name = "CheckCloset"};

/// A hiding place that is not a closet (Assembly-CSharp, global namespace): under a bed, behind a
/// curtain. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HidingSpot{.key = "HidingSpot", .name = "HidingSpot", .assembly = "Assembly-CSharp",
                                               .ns = ""};

}  // namespace pm::map

// The game API for the hiding places (sdk/api/game_api.h). Every member is called on Unity's main thread.
// One that did not resolve in the running game build logs "<key> is not available in this game build"
// once and returns its type's default (null) without touching the game.
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
  /// CheckCloset(closet). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ClosetZone_CheckCloset, void(void*)> CheckCloset{};
};

/// A hiding place that is not a closet.
struct HidingSpot {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::HidingSpot> type{};
};

/// @}
}  // namespace phasmo
