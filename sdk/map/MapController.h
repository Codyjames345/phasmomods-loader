#pragma once

// The truck's map monitor: MapController, the camera that draws the level's floor plans into a
// texture, and its floors. Every entry is optional: a mod that shows the map checks them and does
// without it. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The truck's map monitor (Assembly-CSharp, global namespace), a MonoBehaviour. Optional. Inferred:
/// from the class dump.
inline constexpr pm::sdk::EntryDesc MapController{.key = "MapController", .flags = pm::sdk::kOptional,
                                                  .name = "MapController", .assembly = "Assembly-CSharp", .ns = ""};

/// The level's MapController: the class's one static field of its own type. Optional. Inferred: from
/// the class dump.
inline constexpr pm::sdk::EntryDesc MapController_instance{
    .key = "MapController.instance", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kStatic | pm::sdk::kOptional,
    .owner = "MapController", .type = "@MapController"};

/// The camera that draws the floor plans into a RenderTexture. Optional. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MapController_mapCamera{.key = "MapController.mapCamera",
                                                            .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kOptional,
                                                            .owner = "MapController", .name = "mapCamera"};

/// One floor of the map, a class nested in MapController. Optional. Inferred: from its fields' names.
inline constexpr pm::sdk::EntryDesc MapFloor{.key = "MapController.Floor", .flags = pm::sdk::kOptional,
                                             .owner = "MapController", .name = "Floor", .assembly = "Assembly-CSharp"};

/// The level's floors, a MapController.Floor[], in the monitor's order. Optional. Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc MapController_allFloors{.key = "MapController.allFloors",
                                                            .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kOptional,
                                                            .owner = "MapController", .name = "allFloors"};

/// The index in allFloors of the floor the monitor shows: MapController's one int field. Optional.
/// Inferred: from the class dump and Playable Ghost's use since it was built.
inline constexpr pm::sdk::EntryDesc MapController_floorIndex{
    .key = "MapController.floorIndex", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kOptional,
    .owner = "MapController", .type = "int"};

inline constexpr const char* MapController_bool[] = {"bool"};

/// Shows the next floor up (true) or down: the monitor's own buttons. Optional. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MapController_ChangeFloor{
    .key = "MapController.ChangeFloor", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
    .owner = "MapController", .name = "ChangeFloor", .params = MapController_bool};

/// Which floor it is, a FloorType (LevelRoom.h), as LevelRoom.floor numbers floors. Optional. Inferred:
/// from its name.
inline constexpr pm::sdk::EntryDesc MapFloor_floorType{.key = "MapController.Floor.floorType",
                                                       .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kOptional,
                                                       .owner = "MapController.Floor", .name = "floorType"};

/// The floor's Transform, at the floor's height. Optional. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MapFloor_floor{.key = "MapController.Floor.floor", .kind = pm::sdk::Kind::Field,
                                                   .flags = pm::sdk::kOptional, .owner = "MapController.Floor",
                                                   .name = "floor"};

}  // namespace pm::map

/// @file
/// @brief The game API for the truck's map (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// The truck's map monitor. `map` below is the MapController. Every member is optional.
struct MapController {
  /// The level's MapController: instance.get(), or null; check it with pm::api::alive. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::MapController_instance, void*> instance{};
  /// Its camera: mapCamera.get(map). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MapController_mapCamera, void*> mapCamera{};
  /// Its floors, a MapController.Floor[] (pm::api::RefArray): allFloors.get(map). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MapController_allFloors, void*> allFloors{};
  /// The index in allFloors of the shown floor: floorIndex.get(map). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::MapController_floorIndex, int32_t> floorIndex{};
  /// ChangeFloor(map, up): shows the next floor up or down. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MapController_ChangeFloor, void(void*, bool)> ChangeFloor{};
};

/// One floor of the map. `floor` below is a MapController.Floor.
struct MapFloor {
  /// Which floor, a FloorType number: floorType.get(floor). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::MapFloor_floorType, int32_t> floorType{};
  /// Its Transform: floor.get(floor). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MapFloor_floor, void*> floor{};
};

/// @}
}  // namespace phasmo
