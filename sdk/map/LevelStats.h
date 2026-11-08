#pragma once

// LevelStats: the contract's statistics for the results screen. The type and its instance getter keep
// their real names. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The contract's statistics (Assembly-CSharp, global namespace), a plain class. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats{.key = "LevelStats", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "LevelStats", .assembly = "Assembly-CSharp", .ns = ""};

/// The statistics' instance, the Instance property's getter. The Mimic's copy calls it to count the
/// copy, and SyncValuesNetworked to store the ghost's type. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_get_Instance{.key = "LevelStats.get_Instance",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .flags = pm::sdk::kStatic, .owner = "LevelStats",
                                                            .name = "get_Instance"};

/// How many times the favourite room changed: GhostController.ChangeFavouriteRoomTo counts each move in it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_roomsChanged{.key = "LevelStats.roomsChanged",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "LevelStats", .name = "roomsChanged",
                                                            .type = "int"};

/// How many objectives the contract completed: ExitLevel.Exit writes it after the mission and challenge checks. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_objectivesCompleted{
    .key = "LevelStats.objectivesCompleted", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelStats", .name = "objectivesCompleted", .type = "int"};

/// The other objectives: ExitLevel.Exit writes the remainder plus one, beside objectivesCompleted. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_objectivesFailed{
    .key = "LevelStats.objectivesFailed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelStats", .name = "objectivesFailed", .type = "int"};

/// The average of the samples within two bounds in a float list the same block keeps (the others are left out):
inline constexpr pm::sdk::EntryDesc LevelStats_averageFPS{
    .key = "LevelStats.averageFPS", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelStats", .name = "averageFPS", .type = "float"};

/// How many doors the ghost moved: a ghost's push of a door (Door.GhostForceDoor, Door.CheckDoorDuringHunt) adds
/// one before the leaf moves. The results' statistics show it on a line of its own as they open, and add it into
/// amountOfGhostInteractions. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_doorsMoved{
    .key = "LevelStats.doorsMoved", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelStats", .name = "doorsMoved", .type = "int"};

/// The ghost's interactions in all, as the results show them: as they open, the results' statistics set it to
/// the sum of LevelStats' itemsThrown, objectsUsed, doorsMoved, lightsSwitched and fuseboxToggles, then show it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LevelStats_amountOfGhostInteractions{
    .key = "LevelStats.amountOfGhostInteractions", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelStats", .name = "amountOfGhostInteractions",
    .type = "int"};

}  // namespace pm::map

// The game API for LevelStats (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build"
// once and returns null without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// The contract's statistics.
struct LevelStats {
  /// The LevelStats type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null
  /// and false.
  static constexpr pm::api::Type<pm::map::LevelStats> type{};
  /// The statistics' instance: get_Instance(), or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::LevelStats_get_Instance, void*()> get_Instance{};
  /// How many times the favourite room changed: roomsChanged.get(stats), .set(stats, n). Unresolved: 0,
  /// writes nothing.
  static constexpr pm::api::Field<pm::map::LevelStats_roomsChanged, int32_t> roomsChanged{};
};

/// @}
}  // namespace phasmo
