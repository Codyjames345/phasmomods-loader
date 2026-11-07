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
inline constexpr const char* Door_GhostOpenDoorEvent_params[] = {"bool", "bool", "float"};
inline constexpr const char* Door_SyncLockState_params[] = {"bool", "PhotonMessageInfo"};
inline constexpr const char* HingeDoor_LevelControllerOpenDoor_params[] = {"bool", "float"};
inline constexpr const char* HingeDoor_LockDoor_params[] = {"bool"};
// Door has two fields of its own type; the other one is not Door.otherDoor.
inline constexpr pm::sdk::Fact Door_pairedDoor_facts[] = {
    {.is = pm::sdk::Fact::Is::IsNot, .method = "Door.otherDoor"}};
// Door.SyncLockState stores one of Door's bools, the locked flag, and nothing else of Door.
inline constexpr pm::sdk::Fact Door_locked_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Door.SyncLockState"}};
// HingeDoor has two floats of its own; HingeDoor.Awake stores only the open angle.
inline constexpr pm::sdk::Fact HingeDoor_openAngle_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "HingeDoor.Awake"}};
// Door has two fields of its own type. Of Door's methods only GhostOpenDoorEvent reads the other leaf, and
// Door.Convert copies both, so the load singles it out.
inline constexpr pm::sdk::Fact Door_otherDoor_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "Door.GhostOpenDoorEvent"}};

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

/// The ghost pushes the door open or shut (the overload with three arguments: open, strength, -1 for the
/// game's own, and whether to skip the rattle). It does nothing while Door.forceRoutine is set; otherwise
/// it starts the push there. The push switches the door off for players (Door.DisableOrEnableDoor's RPC,
/// taking the door over on the master), waits until no player holds it and this client owns it, pushes
/// the leaf with the hinge's physics, waits, switches the door back on and clears Door.forceRoutine. The
/// hunt pushes doors between the ghost and a player this way. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_GhostForceDoor{.key = "Door.GhostForceDoor", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Door", .name = "GhostForceDoor",
                                                        .params = Door_GhostForceDoor_params};

/// The door's lock as every client applies it, an RPC only the game sends (from HingeDoor.LockDoor, which
/// locks a shut door only, snapping a nearly shut one to its closed angle first). It sets Door.locked, freezes
/// the leaf's Rigidbody where it stands (every position and rotation constraint while locked, none while
/// unlocked) and ends with Door.CheckOcclusionPortal. A mod never sends or calls it; Custom Ghosts' Asterion
/// leaves the same state on each client itself. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_SyncLockState{.key = "Door.SyncLockState", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "Door", .name = "SyncLockState",
                                                       .params = Door_SyncLockState_params};

/// Whether the door is locked on this client: Door's bool that Door.SyncLockState sets. While it is set the
/// crosshair shows the padlock on the door (PCPropGrab.Update). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_locked{.key = "Door.locked", .kind = pm::sdk::Kind::Field,
                                                .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                .type = "bool", .facts = Door_locked_facts};

/// A double door's other leaf, kept in Door's second field of its own type: the level sets one of
/// Door.otherDoor and Door.pairedDoor on each leaf of a double door, not always the same one (read from the
/// scene files, 2026-11-06), so check both. Null otherwise. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_pairedDoor{.key = "Door.pairedDoor", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                    .type = "@Door", .facts = Door_pairedDoor_facts};

/// What a player's grab on a locked door does (Door.GrabbedDoor runs the same test). Outside a hunt, a key a
/// player holds of the door's kind (Door.kind) unlocks it first. Then an ordinary interior door (kind None) is
/// unlocked (Door.UnlockDoor, which tells every client) unless the game holds it (a flag of the door its own
/// timed hold sets); any other door not unlocked by a key rattles instead. So a locked interior door opens to
/// the first grab, and a DoorKind.PermanentLock door never does. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_AttemptToUnlockDoor{.key = "Door.AttemptToUnlockDoor",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Door", .name = "AttemptToUnlockDoor"};

/// What a player's grab on a door runs on their client: an unlocked door is grabbed on every client (it sends
/// the RPC behind Door.NetworkedGrabbedDoor); a locked one gets Door.AttemptToUnlockDoor's test. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_GrabbedDoor{.key = "Door.GrabbedDoor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Door", .name = "GrabbedDoor"};

/// The door's PhotonView: Door's one field of that type. Its ViewID names the same door on every client.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_view{.key = "Door.view", .kind = pm::sdk::Kind::Field,
                                              .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                              .type = "PhotonView"};

/// Starts the door's update of its occlusion portal, which decides whether the camera sees through the
/// doorway, when the door has a portal and is active. Door.SyncLockState ends with it. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_CheckOcclusionPortal{.key = "Door.CheckOcclusionPortal",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "Door", .name = "CheckOcclusionPortal"};

/// A ghost event's door push: pushes this door open or shut (its second argument; the third is a strength)
/// and, when its first argument is true, one time in three the double door's other leaf (Door.otherDoor)
/// too. Unlike Door.GhostForceDoor it starts without checking Door.forceRoutine, and its push clears
/// Door.forceRoutine as it ends. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_GhostOpenDoorEvent{.key = "Door.GhostOpenDoorEvent",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "Door", .name = "GhostOpenDoorEvent",
                                                            .params = Door_GhostOpenDoorEvent_params};

/// A double door's other leaf, null for a single door: the Door field Door.GhostOpenDoorEvent reads to
/// push both leaves. Some double doors keep their partner in Door.pairedDoor instead, so check both.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_otherDoor{.key = "Door.otherDoor", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                   .type = "@Door", .facts = Door_otherDoor_facts};

/// The door's looping creak, an AudioSource set in the scene. A HingeDoor measures how fast its leaf turns
/// on each physics step, in degrees a second from the change of its angle on the hinge's axis, so a leaf
/// turned by setting its angle counts too. Every third frame, while the door is unlocked (Door.locked
/// clear), HingeDoor.Update sets this source's volume from that speed (silent below a threshold set in the
/// scene) and plays it if it has stopped. While the door is locked it leaves the source alone, so a leaf
/// locked while it still turned creaks on at that volume until it is unlocked (seen in the game,
/// 2026-11-07). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_loopSource{.key = "Door.loopSource", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                    .name = "loopSource", .type = "AudioSource"};

/// Whether the ghost moves this door with physics rather than an animation, set in the scene:
/// Door.GhostOpenDoorAnimated reads it and starts the physics push when it is set, else the animated move.
/// Door.GhostForceDoor never reads it and always pushes with physics. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_useForceInsteadOfAnimations{
    .key = "Door.useForceInsteadOfAnimations", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door", .name = "useForceInsteadOfAnimations",
    .type = "bool"};

/// The door's running push, a Coroutine: Door's one field of that type. Door.GhostForceDoor and the ghost's
/// animated door moves start only while it is null and store their coroutine here, and the push clears it
/// as it ends, so while it is set a ghost push does nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_forceRoutine{.key = "Door.forceRoutine", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                      .type = "Coroutine"};

/// Shuts and locks the door as a hunt does, on every client (an RPC), unless the contract's difficulty
/// keeps doors open in hunts (it tests Difficulty.huntDoorsStayOpen first). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_HuntingCloseDoor{.key = "Door.HuntingCloseDoor",
                                                          .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "Door", .name = "HuntingCloseDoor"};

/// Switches the door on (true) or off (false) for players: grabbing it (Door.DisableOrEnableDoor), its colliders
/// and, the other way round, its hunt collider; switched off it is dropped from any hand. A door that is locked is
/// unlocked as well, on every client. It never locks a door (HingeDoor.LockDoor does). Ghost Tells' Yurei unlocks a
/// door it locked with (true, true). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_ToggleDoorLock{.key = "Door.ToggleDoorLock", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
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

/// A door that never opens: a player's grab on it while locked rattles it (Door.AttemptToUnlockDoor), and no
/// key unlocks it, as keys match a door's kind. The door code tests only for None and Main otherwise, so a
/// door of this kind is still pushed by hunts and picked by ghost events. Decompiled.
inline constexpr pm::sdk::EntryDesc DoorKind_PermanentLock{.key = "DoorKind.PermanentLock",
                                                           .kind = pm::sdk::Kind::EnumValue,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "DoorKind", .name = "PermanentLock"};

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

/// Switches the door on (true) or off (false) for players, on every client (an RPC): switched off, it is
/// dropped from any player's hand and cannot be grabbed, so no player can move it. It does not hold the
/// leaf still. Door.GhostForceDoor's push switches the door back on as it ends. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_DisableOrEnableDoor{.key = "Door.DisableOrEnableDoor",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Door", .name = "DisableOrEnableDoor"};

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

/// Sets the leaf straight to an angle between shut and open, as the level does when it opens doors: a
/// random amount open (its first argument false) or shut, on the hinge's axis, then the body reset and the
/// door's open state sent to every client. It moves the leaf only on the client that calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor_LevelControllerOpenDoor{
    .key = "HingeDoor.LevelControllerOpenDoor", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "HingeDoor", .name = "LevelControllerOpenDoor",
    .params = HingeDoor_LevelControllerOpenDoor_params};

/// Locks the door the game's way, a shut door only: when it is not locked yet and either `force` is true or its
/// leaf is nearly shut, the leaf snaps to its closed angle; then a shut (or forced) door is marked locked here,
/// its door sound stopped, and on the master Door.SyncLockState is sent to every client. An open door, unforced,
/// is left as it is. Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor_LockDoor{.key = "HingeDoor.LockDoor", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "HingeDoor", .name = "LockDoor",
                                                       .params = HingeDoor_LockDoor_params};

/// The leaf's angle when the door is fully open, in degrees on the hinge's axis: HingeDoor.Awake stores the
/// closed angle plus the hinge's limit, and HingeDoor.LevelControllerOpenDoor turns the leaf toward it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor_openAngle{.key = "HingeDoor.openAngle", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "HingeDoor", .type = "float",
                                                        .facts = HingeDoor_openAngle_facts};

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
  /// Whether the ghost moves it with physics rather than an animation: useForceInsteadOfAnimations.get(door).
  /// Unresolved: false.
  static constexpr pm::api::Field<pm::map::Door_useForceInsteadOfAnimations, bool> useForceInsteadOfAnimations{};
  /// Its looping creak, an AudioSource: loopSource.get(door). Its hinge door sets the volume from how fast the
  /// leaf turns, only while unlocked. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_loopSource, void*> loopSource{};
  /// Whether it is locked on this client (the padlock): locked.get(door), .set(door, v). Unresolved: false,
  /// writes nothing.
  static constexpr pm::api::Field<pm::map::Door_locked, bool> locked{};
  /// Its PhotonView: view.get(door). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_view, void*> view{};
  /// CheckOcclusionPortal(door): updates whether the camera sees through the doorway. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_CheckOcclusionPortal, void(void*)> CheckOcclusionPortal{};
  /// A double door's other leaf, when the level keeps it here rather than in otherDoor: pairedDoor.get(door).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_pairedDoor, void*> pairedDoor{};
  /// GrabbedDoor(door): a player's grab on it, from this client. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_GrabbedDoor, void(void*)> GrabbedDoor{};
  /// AttemptToUnlockDoor(door): what a player's grab on a locked door does. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_AttemptToUnlockDoor, void(void*)> AttemptToUnlockDoor{};
  /// A double door's other leaf: otherDoor.get(door), null for a single door. It may be set on one leaf
  /// only. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_otherDoor, void*> otherDoor{};
  /// The door's running push: forceRoutine.get(door), null when none runs. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_forceRoutine, void*> forceRoutine{};
  /// GhostForceDoor(door, open, strength, skipRattle). A mod may hook it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_GhostForceDoor, void(void*, bool, float, bool)> GhostForceDoor{};
  /// HuntingCloseDoor(door). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_HuntingCloseDoor, void(void*)> HuntingCloseDoor{};
  /// ToggleDoorLock(door, on, b): switches it on or off for players, unlocking a locked door. Unresolved: does nothing.
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
/// PermanentLock(): a door that never opens to a grab. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_PermanentLock> PermanentLock{};
}  // namespace DoorKind

/// A hinged door. `door` below is the HingeDoor.
struct HingeDoor {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::HingeDoor> type{};
  /// The axis its leaf swings about: hingeAxis.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::HingeDoor_hingeAxis, int32_t> hingeAxis{};
  /// The leaf's fully open angle in degrees on the hinge's axis: openAngle.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::HingeDoor_openAngle, float> openAngle{};
  /// LockDoor(door, force): locks a shut door (force: snaps it shut first), on every client. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::HingeDoor_LockDoor, void(void*, bool)> LockDoor{};
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
