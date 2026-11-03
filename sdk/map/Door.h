#pragma once

// Door: a door of the level, HingeDoor (a door that swings on a hinge), and SmallMapVariation (one of
// LevelController's map variations, with the doors its hunts lock). The types and their methods keep
// their real names; Door's leaf body and closed angle are obfuscated and found by shape and facts.
// Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* Door_GhostForceDoor_params[] = {"bool", "float", "bool"};
inline constexpr const char* Door_ToggleDoorLock_params[] = {"bool", "bool"};
// HingeDoor.Awake reads one float of Door, the closed angle, to place its hinge's limit. It is the
// derived type's method, so the fact also matches that offset through another register.
inline constexpr pm::sdk::Fact Door_closedAngle_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "HingeDoor.Awake"}};

/// A door (Assembly-CSharp, global namespace), an abstract MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc Door{.key = "Door", .confidence = pm::sdk::Confidence::Decompiled,
                                         .name = "Door", .assembly = "Assembly-CSharp", .ns = ""};

/// The door leaf's Rigidbody: Door's one field of that type. HingeDoor.Awake takes the hinge's ConfigurableJoint
/// from it. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_body{.key = "Door.body", .kind = pm::sdk::Kind::Field,
                                              .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                              .type = "Rigidbody"};

/// The leaf's angle when the door is shut, in degrees: HingeDoor.Awake adds the hinge's limit to it for
/// the open angle, and HingeDoor.LevelControllerOpenDoor reads it too. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_closedAngle{.key = "Door.closedAngle", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                     .type = "float",
                                                     .facts = Door_closedAngle_facts};

/// The ghost forces the door open or shut (the overload with three arguments: open, strength, -1 for
/// the game's own, and a third flag). Inferred: from the method's name and its default arguments.
inline constexpr pm::sdk::EntryDesc Door_GhostForceDoor{.key = "Door.GhostForceDoor", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Door", .name = "GhostForceDoor",
                                                        .params = Door_GhostForceDoor_params};

/// Shuts and locks the door as a hunt does, on every client (an RPC), unless the contract's difficulty
/// keeps doors open in hunts (it tests Difficulty.huntDoorsStayOpen first). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_HuntingCloseDoor{.key = "Door.HuntingCloseDoor",
                                                          .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "Door", .name = "HuntingCloseDoor"};

/// Locks (false) or unlocks (true) the door, on every client. Inferred: from the method's name.
inline constexpr pm::sdk::EntryDesc Door_ToggleDoorLock{.key = "Door.ToggleDoorLock", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Door", .name = "ToggleDoorLock",
                                                        .params = Door_ToggleDoorLock_params};

inline constexpr const char* DoorKind_values[] = {"None", "Main", "Secondary", "PermanentLock"};

/// What kind of door a Door is (Assembly-CSharp, global namespace, name obfuscated): None, Main (the
/// front door), Secondary or PermanentLock. Found by its values. Inferred: from the values' names.
inline constexpr pm::sdk::EntryDesc DoorKind{.key = "DoorKind", .assembly = "Assembly-CSharp",
                                             .ns = "", .values = DoorKind_values};

/// The front door. GhostActivity.GetDoorToOpen never picks it, and the Asterion leaves it alone.
/// Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc DoorKind_Main{.key = "DoorKind.Main", .kind = pm::sdk::Kind::EnumValue,
                                                  .owner = "DoorKind", .name = "Main"};

/// The door's kind: Door's one field of the DoorKind type. Inferred: from the type's values.
inline constexpr pm::sdk::EntryDesc Door_kind{.key = "Door.kind", .kind = pm::sdk::Kind::Field, .owner = "Door",
                                              .type = "@DoorKind"};

/// The door's PhotonObjectInteract, the object a ghost's door interaction is given (GhostAI.ChangeState
/// with the door state): Door's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_interact{.key = "Door.interact", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                  .type = "@PhotonObjectInteract"};

/// Plays the door's slam noise on every client (an RPC). Its Door argument may be null. Inferred: from
/// the method's name; Custom Ghosts' Asterion has called it with null since 2026-10-20.
inline constexpr pm::sdk::EntryDesc Door_CreateDoorSlamNoise{.key = "Door.CreateDoorSlamNoise",
                                                             .kind = pm::sdk::Kind::Method, .owner = "Door",
                                                             .name = "CreateDoorSlamNoise"};

/// Switches the door on (true) or off (false) for players, on every client (an RPC): a door switched
/// off cannot be grabbed or closed. Inferred: from the method's name.
inline constexpr pm::sdk::EntryDesc Door_DisableOrEnableDoor{.key = "Door.DisableOrEnableDoor",
                                                             .kind = pm::sdk::Kind::Method, .owner = "Door",
                                                             .name = "DisableOrEnableDoor"};

/// What every client runs when a player grabs an unlocked door: the RPC Door.GrabbedDoor sends. Its only
/// callers are DragRigidbodyUse (the player's grab) and ClothDoor's override, which calls it first; no
/// ghost interaction sends it (read in decompiled code of build 25811160, 2026-10-22). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_NetworkedGrabbedDoor{.key = "Door.NetworkedGrabbedDoor",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "Door", .name = "NetworkedGrabbedDoor"};

/// A door that swings on a hinge (Assembly-CSharp, global namespace), a Door. Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor{.key = "HingeDoor", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "HingeDoor", .assembly = "Assembly-CSharp", .ns = ""};

/// Unity's Awake for the door: after Door.Awake it reads the hinge's joint limits from the leaf's body
/// and stores the open angle, the closed angle plus the limit on the hinge's side. Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor_Awake{.key = "HingeDoor.Awake", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "HingeDoor", .name = "Awake"};

/// The axis the leaf swings about, a Unity Axis value read as an int: Awake reads it to choose the
/// limit's side. Ghost Tells reads the leaf's local Euler angle on it (Axis.X, Axis.Z, else y).
/// Inferred: from the field's name and type.
inline constexpr pm::sdk::EntryDesc HingeDoor_hingeAxis{.key = "HingeDoor.hingeAxis", .kind = pm::sdk::Kind::Field,
                                                        .owner = "HingeDoor", .name = "hingeAxis"};

inline constexpr const char* Axis_values[] = {"None", "X", "Y", "Z"};

/// Unity's axis flags (UnityEngine.AnimationModule, namespace UnityEngine.Animations), the type of
/// HingeDoor.hingeAxis. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Axis{.key = "Axis", .name = "Axis", .assembly = "UnityEngine.AnimationModule",
                                         .ns = "UnityEngine.Animations", .values = Axis_values};

/// The x axis. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Axis_X{.key = "Axis.X", .kind = pm::sdk::Kind::EnumValue, .owner = "Axis",
                                           .name = "X"};

/// The z axis. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Axis_Z{.key = "Axis.Z", .kind = pm::sdk::Kind::EnumValue, .owner = "Axis",
                                           .name = "Z"};

/// One of LevelController's map variations, a class nested in LevelController. Inferred: from its
/// fields' names.
inline constexpr pm::sdk::EntryDesc SmallMapVariation{.key = "SmallMapVariation", .owner = "LevelController",
                                                      .name = "SmallMapVariation", .assembly = "Assembly-CSharp"};

/// The doors the variation's hunts lock, a Door[]: LevelController.LockHuntingDoors and
/// UnlockHuntingDoors walk the variations as well as their own list. Decompiled.
inline constexpr pm::sdk::EntryDesc SmallMapVariation_huntingDoorsToLock{
    .key = "SmallMapVariation.huntingDoorsToLock", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SmallMapVariation", .name = "huntingDoorsToLock"};

}  // namespace pm::map

// The game API for doors (sdk/api/game_api.h). Every member is called on Unity's main thread. One that
// did not resolve in the running game build logs "<key> is not available in this game build" once and
// returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// A door. `door` below is the Door.
struct Door {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Door> type{};
  /// The leaf's Rigidbody: body.get(door). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_body, void*> body{};
  /// The leaf's closed angle in degrees: closedAngle.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Door_closedAngle, float> closedAngle{};
  /// GhostForceDoor(door, open, strength, skipRattle). A mod may hook it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_GhostForceDoor, void(void*, bool, float, bool)> GhostForceDoor{};
  /// HuntingCloseDoor(door). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_HuntingCloseDoor, void(void*)> HuntingCloseDoor{};
  /// ToggleDoorLock(door, unlock, b). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_ToggleDoorLock, void(void*, bool, bool)> ToggleDoorLock{};
  /// Its kind, a DoorKind number: kind.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Door_kind, int32_t> kind{};
  /// Its PhotonObjectInteract: interact.get(door). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_interact, void*> interact{};
  /// CreateDoorSlamNoise(door, otherDoor): `otherDoor` may be null. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_CreateDoorSlamNoise, void(void*, void*)> CreateDoorSlamNoise{};
  /// DisableOrEnableDoor(door, enable, flag). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_DisableOrEnableDoor, void(void*, bool, bool)> DisableOrEnableDoor{};
  /// NetworkedGrabbedDoor(door). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_NetworkedGrabbedDoor, void(void*)> NetworkedGrabbedDoor{};
};

/// What kind of door a Door is.
namespace DoorKind {
/// Main(): the front door. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_Main> Main{};
}  // namespace DoorKind

/// A hinged door. `door` below is the HingeDoor.
struct HingeDoor {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::HingeDoor> type{};
  /// The axis its leaf swings about: hingeAxis.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::HingeDoor_hingeAxis, int32_t> hingeAxis{};
};

/// Unity's axis flags, the values HingeDoor::hingeAxis holds.
namespace Axis {
/// X(), Z(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::Axis_X> X{};
inline constexpr pm::api::EnumValue<pm::map::Axis_Z> Z{};
}  // namespace Axis

/// A map variation. `variation` below is the SmallMapVariation.
struct SmallMapVariation {
  /// Its hunts' doors, a Door[]: huntingDoorsToLock.get(variation), read as a pm::api::RefArray.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::SmallMapVariation_huntingDoorsToLock, void*> huntingDoorsToLock{};
};

/// @}
}  // namespace phasmo
