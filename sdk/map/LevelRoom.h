#pragma once

// LevelRoom: one room of a contract's level, and Unity's Collider, whose bounds make up a room. The
// room's colliders keep their real name. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "Transform.h"

namespace pm::map {

/// A room of the level (Assembly-CSharp, global namespace), a MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom{.key = "LevelRoom", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "LevelRoom", .assembly = "Assembly-CSharp", .ns = ""};

/// The room's colliders, a List<Collider>: the volumes that make up the room. Awake fills it with the
/// room object's own colliders (GetComponents<Collider>) when the level left it empty, and makes the
/// room's wander colliders from it, less roomCollidersToIgnoreForWander. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_roomColliders{
    .key = "LevelRoom.roomColliders", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelRoom", .name = "roomColliders"};

inline constexpr const char* LevelRoom_AreRoomLightsOn_params[] = {"out bool"};

/// The room's light switches, a List<LightSwitch>: the room's one list of them. Inferred: from the
/// field's type.
inline constexpr pm::sdk::EntryDesc LevelRoom_lightSwitches{.key = "LevelRoom.lightSwitches",
                                                            .kind = pm::sdk::Kind::Field, .owner = "LevelRoom",
                                                            .type = "List<@LightSwitch>"};

/// The room's doors, a Door[]: the room's one array of them. Inferred: from the field's type.
inline constexpr pm::sdk::EntryDesc LevelRoom_doors{.key = "LevelRoom.doors", .kind = pm::sdk::Kind::Field,
                                                    .owner = "LevelRoom", .type = "@Door[]"};

/// Whether the room's lights are on, with whether they flicker in the out argument. Inferred: from the
/// method's name.
inline constexpr pm::sdk::EntryDesc LevelRoom_AreRoomLightsOn{.key = "LevelRoom.AreRoomLightsOn",
                                                              .kind = pm::sdk::Kind::Method, .owner = "LevelRoom",
                                                              .name = "AreRoomLightsOn",
                                                              .params = LevelRoom_AreRoomLightsOn_params};

inline constexpr const char* LevelRoom_float[] = {"float"};

/// Whether a player in the room is talking above a loudness (0.1 by default). Inferred: from the
/// method's name; Custom Ghosts' Echo has called it for the host's own voice since it was built.
inline constexpr pm::sdk::EntryDesc LevelRoom_ArePlayersTalkingInRoom{.key = "LevelRoom.ArePlayersTalkingInRoom",
                                                                      .kind = pm::sdk::Kind::Method,
                                                                      .owner = "LevelRoom",
                                                                      .name = "ArePlayersTalkingInRoom",
                                                                      .params = LevelRoom_float};

inline constexpr const char* RoomKind_values[] = {"insideRoom", "outsideMap", "exterior", "exteriorRoom"};

// Found by its values.
/// What kind of room a LevelRoom is, an enum nested in LevelRoom (name obfuscated, value names real):
/// insideRoom, outsideMap, exterior, exteriorRoom. Decompiled.
inline constexpr pm::sdk::EntryDesc RoomKind{.key = "RoomKind", .confidence = pm::sdk::Confidence::Decompiled,
                                             .owner = "LevelRoom", .values = RoomKind_values};

// The test's look-alikes test otherwise: one passes only a room of kind 0, and another tests the room of the
// ghost's own player, not the candidate's, and passes when that room is of this kind.
/// The room beyond the map's exits: on every map of build 25811160 exactly one room is of this kind,
/// LevelController.outsideRoom, named Outside or Exterior (scene read). A player whose room is of this
/// kind is out of the investigation: PlayerSanity.Update counts that time apart and drains no sanity
/// for it, and the hunt's test of whom it may go for (a GhostHuntState method) never goes for them. Decompiled.
inline constexpr pm::sdk::EntryDesc RoomKind_outsideMap{.key = "RoomKind.outsideMap", .kind = pm::sdk::Kind::EnumValue,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "RoomKind", .name = "outsideMap"};

/// A room indoors. Most of every map's rooms are of this kind, but not all indoor rooms: Grafton
/// Farmhouse marks most of its rooms exteriorRoom (scene read, build 25811160). Optional. Inferred: from
/// the value's name and the scenes.
inline constexpr pm::sdk::EntryDesc RoomKind_insideRoom{.key = "RoomKind.insideRoom", .kind = pm::sdk::Kind::EnumValue,
                                                        .flags = pm::sdk::kOptional, .owner = "RoomKind",
                                                        .name = "insideRoom"};

/// An open area within the map's exits: Sunny Meadows' courtyard and the campsites' fires, pitches and
/// entrance (scene read, build 25811160). Optional. Inferred: from the value's name and the scenes.
inline constexpr pm::sdk::EntryDesc RoomKind_exterior{.key = "RoomKind.exterior", .kind = pm::sdk::Kind::EnumValue,
                                                      .flags = pm::sdk::kOptional, .owner = "RoomKind",
                                                      .name = "exterior"};

/// A room within the map's exits marked as outdoors or apart: tents, toilet blocks, Bleasdale's garden
/// and side path, Point Hope's balcony, and most of Grafton Farmhouse (scene read, build 25811160).
/// Optional. Inferred: from the value's name and the scenes.
inline constexpr pm::sdk::EntryDesc RoomKind_exteriorRoom{.key = "RoomKind.exteriorRoom",
                                                          .kind = pm::sdk::Kind::EnumValue,
                                                          .flags = pm::sdk::kOptional, .owner = "RoomKind",
                                                          .name = "exteriorRoom"};

inline constexpr const char* FloorType_values[] = {"basement", "firstFloor", "secondFloor"};

// Found by its values.
/// A floor, an enum nested in LevelRoom (name obfuscated, value names real): basement 0, firstFloor 1
/// and up. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc FloorType{.key = "FloorType", .owner = "LevelRoom",
                                              .values = FloorType_values};

/// The basement, the floor below the ground floor. Optional. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc FloorType_basement{.key = "FloorType.basement", .kind = pm::sdk::Kind::EnumValue,
                                                       .flags = pm::sdk::kOptional,
                                                       .owner = "FloorType", .name = "basement"};

/// The room's floor: LevelRoom's one FloorType field. The hunt's voice test hears a player only on
/// the floor of the ghost's room. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_floor{.key = "LevelRoom.floor", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "LevelRoom", .type = "@FloorType"};

/// The room's name, as the map shows it: LevelRoom's one string field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LevelRoom_name{.key = "LevelRoom.name", .kind = pm::sdk::Kind::Field,
                                                   .owner = "LevelRoom", .type = "string"};

inline constexpr const char* LevelRoom_OnTriggerEnter_params[] = {"Collider"};

/// Unity's trigger message for the room's colliders, on every client. When a player's camera collider
/// (tagged MainCamera, under a root tagged Player) enters, it sets that player's Player.currentRoom to
/// this room, and for the local player also LevelController's own room field; GameController hears of
/// the move. When the ghost's room collider (tagged GhostRoomCollider) enters another room not of kind
/// RoomKind.outsideMap, it calls LevelController.SyncCurrentGhostRoom with it, and during a hunt
/// (GhostAI.isHunting) starts the room's light switches blinking. A mod hooks it to see a player change
/// rooms. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_OnTriggerEnter{.key = "LevelRoom.OnTriggerEnter",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "LevelRoom", .name = "OnTriggerEnter",
                                                             .params = LevelRoom_OnTriggerEnter_params};

/// The room's kind, a RoomKind number: LevelRoom's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_kind{.key = "LevelRoom.kind", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelRoom",
                                                   .type = "@RoomKind"};

/// The room's temperature system, every frame on every client, a Unity message: it returns at once on
/// a few flags and for a room outside the map; otherwise it moves `temperature` by the frame's time
/// (down while the ghost is in the room, LevelController's currentGhostRoom; towards the room's own
/// levels while it is not, by the power and the room's kind), then clamps it between a floor and 20.
/// The floor is -10 while EvidenceController's flag for the ghost's Freezing Temperatures is set and 1
/// otherwise, so a room never drops below 1 for a ghost whose given evidence lacks Freezing. Apart
/// from `temperature` it writes only a time it adds the frame's time to. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_Update{.key = "LevelRoom.Update", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "LevelRoom", .name = "Update"};

/// Puts `temperature` back to the room's own starting level, or for one difficulty setting to 1 or
/// 20 by whether the power cools the room. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_ResetTemperature{.key = "LevelRoom.ResetTemperature",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "LevelRoom", .name = "ResetTemperature"};

// Of LevelRoom's floats ResetTemperature writes only the temperature.
inline constexpr pm::sdk::Fact LevelRoom_temperature_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "LevelRoom.ResetTemperature"}};

/// The room's temperature in degrees Celsius, on each client: Update drives it every frame, and every
/// reading reads it. The thermometer's reading is it (or a nearby heat source's own) plus
/// Random.Range(0, 3), or (0, 2) for one tier, and enables its photo evidence only below 1; a player's
/// breath shows while their room's is at or below 5. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelRoom_temperature{
    .key = "LevelRoom.temperature", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelRoom", .type = "float", .facts = LevelRoom_temperature_facts};

/// Unity's collider component (UnityEngine.PhysicsModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Collider{.key = "Collider", .name = "Collider",
                                             .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine"};

/// The collider's box in world space, a Bounds (its centre and its half size). Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc Collider_get_bounds{.key = "Collider.get_bounds", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Collider", .name = "get_bounds"};

inline constexpr const char* Collider_bool[] = {"bool"};

/// Switches the collider on or off: one switched off takes part in no contact, trigger or physics query,
/// so no sensor detects it. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Collider_set_enabled{.key = "Collider.set_enabled", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Collider", .name = "set_enabled",
                                                         .params = Collider_bool};

/// Whether any investigator is in the room now. Inferred: from its name (LevelRoom.AnyPlayersInRoom).
inline constexpr pm::sdk::EntryDesc LevelRoom_AnyPlayersInRoom{.key = "LevelRoom.AnyPlayersInRoom",
                                                               .kind = pm::sdk::Kind::Method, .owner = "LevelRoom",
                                                               .name = "AnyPlayersInRoom"};

/// Whether the collider takes part in physics. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Collider_get_enabled{.key = "Collider.get_enabled", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Collider", .name = "get_enabled"};

/// The Rigidbody the collider moves with: its own object's, or the nearest parent's; null when none. Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc Collider_get_attachedRigidbody{.key = "Collider.get_attachedRigidbody",
                                                                   .kind = pm::sdk::Kind::Method, .owner = "Collider",
                                                                   .name = "get_attachedRigidbody"};

}  // namespace pm::map

/// @file
/// @brief The game API for rooms (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null, zero) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// Unity's Bounds: a box by its centre and its extents (half its size on each axis).
struct Bounds {
  Vector3 center;   ///< the box's centre, in world space
  Vector3 extents;  ///< half the box's size on each axis
};

/// A room of the level. `room` below is the LevelRoom.
struct LevelRoom {
  /// Its floor, a FloorType number: floor.get(room). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LevelRoom_floor, int32_t> floor{};
  /// Its name, a managed string: name.get(room). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelRoom_name, void*> name{};
  /// The LevelRoom type. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::LevelRoom> type{};
  /// Its colliders, a List<Collider>: roomColliders.get(room), read as a pm::api::List. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelRoom_roomColliders, void*> roomColliders{};
  /// Its light switches, a List<LightSwitch>: lightSwitches.get(room), read as a pm::api::List.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelRoom_lightSwitches, void*> lightSwitches{};
  /// Its doors, a Door[]: doors.get(room), read as a pm::api::RefArray. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelRoom_doors, void*> doors{};
  /// AreRoomLightsOn(room, &flickering): whether its lights are on. Unresolved: false.
  static constexpr pm::api::Method<pm::map::LevelRoom_AreRoomLightsOn, bool(void*, bool*)> AreRoomLightsOn{};
  /// ArePlayersTalkingInRoom(room, loudness). Unresolved: false.
  static constexpr pm::api::Method<pm::map::LevelRoom_ArePlayersTalkingInRoom, bool(void*, float)>
      ArePlayersTalkingInRoom{};
  /// Its kind, a RoomKind number: kind.get(room). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LevelRoom_kind, int32_t> kind{};
  /// Update(room): one frame of its temperature system. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::LevelRoom_Update, void(void*)> Update{};
  /// Its temperature in degrees Celsius: temperature.get(room), temperature.set(room, t). Unresolved:
  /// reads 0, writes nothing.
  static constexpr pm::api::Field<pm::map::LevelRoom_temperature, float> temperature{};
  /// AnyPlayersInRoom(room): whether any investigator is in it now. Unresolved: false.
  static constexpr pm::api::Method<pm::map::LevelRoom_AnyPlayersInRoom, bool(void*)> AnyPlayersInRoom{};
};

/// What kind of room a LevelRoom is.
namespace FloorType {
/// basement(): the floor below the ground floor. Optional. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::FloorType_basement> basement{};
}  // namespace FloorType

/// What kind of room a LevelRoom is: a player whose room is outsideMap is beyond the map's exits.
namespace RoomKind {
/// outsideMap(): the room beyond the map's exits (LevelController.outsideRoom). Unresolved: 0, which is
/// insideRoom's number, so check the type's use::resolved() before comparing.
inline constexpr pm::api::EnumValue<pm::map::RoomKind_outsideMap> outsideMap{};
/// insideRoom(): a room indoors. Optional. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::RoomKind_insideRoom> insideRoom{};
/// exterior(): an open area within the map's exits. Optional. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::RoomKind_exterior> exterior{};
/// exteriorRoom(): a room within the map's exits marked as outdoors or apart. Optional. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::RoomKind_exteriorRoom> exteriorRoom{};
}  // namespace RoomKind

/// Unity's collider. `collider` below is one.
struct Collider {
  /// get_bounds(collider): its world-space box. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Collider_get_bounds, Bounds(void*)> get_bounds{};
  /// set_enabled(collider, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Collider_set_enabled, void(void*, bool)> set_enabled{};
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Collider> type{};
  /// get_enabled(collider): whether it takes part in physics. Unresolved: false.
  static constexpr pm::api::Method<pm::map::Collider_get_enabled, bool(void*)> get_enabled{};
  /// get_attachedRigidbody(collider): the Rigidbody it moves with. Unresolved or none: null.
  static constexpr pm::api::Method<pm::map::Collider_get_attachedRigidbody, void*(void*)> get_attachedRigidbody{};
};

/// @}
}  // namespace phasmo
