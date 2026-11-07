#pragma once

// LevelController: the controller of a contract's level, one per contract. The type is found by name;
// its static instance field and its ghost are obfuscated, so they are found by shape. Notes read from
// decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "Transform.h"

namespace pm::map {

/// The contract's fuse box (Assembly-CSharp, global namespace). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc FuseBox{.key = "FuseBox", .name = "FuseBox", .assembly = "Assembly-CSharp", .ns = ""};

/// Breaks the box: it sets outOfOrder. Decompiled.
inline constexpr pm::sdk::EntryDesc FuseBox_BreakFuseBox{.key = "FuseBox.BreakFuseBox", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "FuseBox", .name = "BreakFuseBox"};

// FuseBox declares two public bools, this one and outOfOrder. UseNetworked flips this one, through a
// register the fact reader cannot prove holds this.
inline constexpr pm::sdk::Fact FuseBox_powerOn_facts[] = {{.is = pm::sdk::Fact::Is::IsNot, .method = "FuseBox.outOfOrder"}};
// BreakFuseBox stores the constant 1 in this one.
inline constexpr pm::sdk::Fact FuseBox_outOfOrder_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "FuseBox.BreakFuseBox", .value = 1}};

/// Whether the power is on. Decompiled.
inline constexpr pm::sdk::EntryDesc FuseBox_powerOn{.key = "FuseBox.powerOn", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled, .owner = "FuseBox",
                                                    .type = "bool",
                                                    .facts = FuseBox_powerOn_facts,
                                                    .access = pm::sdk::Visibility::Public};

/// Whether the box is broken, so that the ghost's fuse state does nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc FuseBox_outOfOrder{.key = "FuseBox.outOfOrder", .kind = pm::sdk::Kind::Field,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "FuseBox", .type = "bool",
                                                       .facts = FuseBox_outOfOrder_facts,
                                                       .access = pm::sdk::Visibility::Public};

/// The contract's fuse box: LevelController's one FuseBox field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LevelController_fuseBox{.key = "LevelController.fuseBox",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .owner = "LevelController",
                                                            .type = "@FuseBox"};

/// The contract's setup phase (Assembly-CSharp, global namespace). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc SetupPhaseController{.key = "SetupPhaseController", .name = "SetupPhaseController",
                                                         .assembly = "Assembly-CSharp", .ns = ""};

/// Unlocks the truck's main door; it sets mainDoorOpen, unless it is set already. Decompiled.
inline constexpr pm::sdk::EntryDesc SetupPhaseController_UnlockMainDoor{
    .key = "SetupPhaseController.UnlockMainDoor", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SetupPhaseController", .name = "UnlockMainDoor"};

// Of the class's three public bools UnlockMainDoor stores the constant 1 in this one.
inline constexpr pm::sdk::Fact SetupPhaseController_mainDoorOpen_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "SetupPhaseController.UnlockMainDoor", .value = 1}};

/// Whether the main door has been unlocked: until it has, the idle roll's TryInteract sends the ghost home.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc SetupPhaseController_mainDoorOpen{
    .key = "SetupPhaseController.mainDoorOpen", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SetupPhaseController", .type = "bool",
    .facts = SetupPhaseController_mainDoorOpen_facts};

/// The level's controller (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController{.key = "LevelController",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "LevelController", .assembly = "Assembly-CSharp", .ns = ""};

/// The current level's controller: the class's one static field of its own type. It outlives its
/// level: after a contract it still points at the destroyed controller, so check it with
/// pm::api::alive (core/game.cpp does). Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_instance{
    .key = "LevelController.instance", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "LevelController", .type = "@LevelController"};

/// The contract's ghost: the class's one GhostAI field, set when the ghost spawns, on every client.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_currentGhost{
    .key = "LevelController.currentGhost", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "@GhostAI"};

inline constexpr const char* LevelController_SyncCurrentGhostRoomNetworked_params[] = {"int", "PhotonMessageInfo"};

/// The RPC that sets the ghost's room on every client: it takes the room at the index it is given in
/// `rooms` and stores it in `currentGhostRoom`. The master sends it from SyncCurrentGhostRoom with
/// that room's index, when the room changes and other players are in the room. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_SyncCurrentGhostRoomNetworked{
    .key = "LevelController.SyncCurrentGhostRoomNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelController",
    .name = "SyncCurrentGhostRoomNetworked", .params = LevelController_SyncCurrentGhostRoomNetworked_params};

// Of the class's three LevelRoom fields, the room RPC stores only this one.
inline constexpr pm::sdk::Fact LevelController_currentGhostRoom_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "LevelController.SyncCurrentGhostRoomNetworked"}};

/// The room the ghost is in now, on every client: the master's SyncCurrentGhostRoom sets it and sends
/// it to the others through SyncCurrentGhostRoomNetworked. Live: Spectral Flame read it in the game on
/// 2026-10-21 (build 25811160).
inline constexpr pm::sdk::EntryDesc LevelController_currentGhostRoom{
    .key = "LevelController.currentGhostRoom", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Live,
    .owner = "LevelController", .type = "@LevelRoom", .facts = LevelController_currentGhostRoom_facts};

// Of the class's two LevelRoom[] fields, the room RPC reads only this one (GetRoomFromName reads the
// other).
inline constexpr pm::sdk::Fact LevelController_rooms_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LevelController.SyncCurrentGhostRoomNetworked"}};

/// The level's rooms, a LevelRoom[]: the rooms the ghost's room is one of, which SyncCurrentGhostRoom
/// searches for it and SyncCurrentGhostRoomNetworked indexes. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_rooms{
    .key = "LevelController.rooms", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "@LevelRoom[]", .facts = LevelController_rooms_facts};

/// The level's fire sources, a List<FireSource>: each FireSource adds itself in its Start, so it
/// holds every candle, lantern and other flame of the level once it has started. The class's one
/// field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_fireSources{
    .key = "LevelController.fireSources", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "List<@FireSource>"};

/// Locks the level's exits as a hunt starts, on the master (the hunt state calls it right after it sends
/// GhostAI.Hunting): Door.HuntingCloseDoor on every door of its own list (huntingDoors), then on a map
/// variation's doors (mapVariations) when the ghost's room is one of the variation's rooms and a player
/// is in one of them. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_LockHuntingDoors{
    .key = "LevelController.LockHuntingDoors", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelController", .name = "LockHuntingDoors"};

/// Opens the exits again as a hunt ends: Door.ToggleDoorLock(true, true) on every door of huntingDoors
/// and of every variation's huntingDoorsToLock. GameController.ResetHuntEffects calls it, and that runs
/// only when a hunt was on. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_UnlockHuntingDoors{
    .key = "LevelController.UnlockHuntingDoors", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelController", .name = "UnlockHuntingDoors"};

// Of the class's two Door[] fields, LockHuntingDoors reads only this one.
inline constexpr pm::sdk::Fact LevelController_huntingDoors_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LevelController.LockHuntingDoors"}};

/// The doors a hunt locks, a Door[]: the map's exits. LockHuntingDoors and UnlockHuntingDoors walk it.
/// On every map of build 25811160 it holds exactly the map's DoorKind.Main doors, and every variation's
/// huntingDoorsToLock is a part of it (scene read; sdk/systems/doors.md lists them). Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_huntingDoors{
    .key = "LevelController.huntingDoors", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "@Door[]", .facts = LevelController_huntingDoors_facts};

/// The level's map variations, a SmallMapVariation[]: the class's one field of that type. The hunt
/// door methods walk their doors too. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_mapVariations{
    .key = "LevelController.mapVariations", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "@SmallMapVariation[]"};

inline constexpr const char* LevelController_IsPointInOutsideRoom_params[] = {"Vector3"};

/// Whether a point lies outdoors: true when any of outsideRoom's roomColliders' bounds contains it.
/// It throws when outsideRoom or its colliders are null. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_IsPointInOutsideRoom{
    .key = "LevelController.IsPointInOutsideRoom", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelController", .name = "IsPointInOutsideRoom",
    .params = LevelController_IsPointInOutsideRoom_params};

/// Unity's Awake: among its set-up it copies outsideRoom into another LevelRoom field. Found only for
/// outsideRoom's fact. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_Awake{.key = "LevelController.Awake", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "LevelController", .name = "Awake"};

// Of the class's three LevelRoom fields IsPointInOutsideRoom reads only this, but it reuses the
// register that holds `this` (so the resolver cannot count its reads). Of the three, Awake's reads
// leave this one and currentGhostRoom, and the room RPC stores currentGhostRoom only.
inline constexpr pm::sdk::Fact LevelController_outsideRoom_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LevelController.Awake"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "LevelController.SyncCurrentGhostRoomNetworked", .negate = true}};

/// The level's outdoors, a LevelRoom, or null on a level without one: the room IsPointInOutsideRoom
/// tests a point against. On every map of build 25811160 it is the map's one room of kind
/// RoomKind.outsideMap, named Outside or Exterior (scene read). Decompiled.
inline constexpr pm::sdk::EntryDesc LevelController_outsideRoom{
    .key = "LevelController.outsideRoom", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelController", .type = "@LevelRoom", .facts = LevelController_outsideRoom_facts};

/// The ghost's use of the fuse box: in a room it sends the box's networked use by RPC, and outside one it runs it here.
inline constexpr pm::sdk::EntryDesc FuseBox_GhostUse{
    .key = "FuseBox.GhostUse", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FuseBox", .name = "GhostUse"};


/// The contract's setup phase: the class's one static field of its own type, set while a contract runs.
/// Like LevelController.instance it may still point at a destroyed one after the contract, so check it
/// with pm::api::alive. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc SetupPhaseController_instance{
    .key = "SetupPhaseController.instance", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kStatic,
    .owner = "SetupPhaseController", .type = "@SetupPhaseController"};

}  // namespace pm::map

// The game API for LevelController (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this
// game build" once and returns its type's default (null, false) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// The contract's fuse box.
struct FuseBox {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::FuseBox> type{};
  /// Whether the power is on: powerOn.get(box). Unresolved: false.
  static constexpr pm::api::Field<pm::map::FuseBox_powerOn, bool> powerOn{};
  /// Whether the box is broken: outOfOrder.get(box). Unresolved: false.
  static constexpr pm::api::Field<pm::map::FuseBox_outOfOrder, bool> outOfOrder{};
};

/// The contract's setup phase.
struct SetupPhaseController {
  /// Whether the main door has been unlocked: mainDoorOpen.get(setup). Unresolved: false.
  static constexpr pm::api::Field<pm::map::SetupPhaseController_mainDoorOpen, bool> mainDoorOpen{};
  /// The contract's setup phase, or null: instance.get(). Check it with pm::api::alive. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::SetupPhaseController_instance, void*> instance{};
};

/// The controller of a contract's level.
struct LevelController {
  /// The LevelController type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved:
  /// null and false.
  static constexpr pm::api::Type<pm::map::LevelController> type{};
  /// The current level's controller: instance.get(), or null. A destroyed one remains after a
  /// contract ends, so check it with pm::api::alive. Reading it may run the class's static
  /// constructor, so read it while the game plays, never at startup. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::LevelController_instance, void*> instance{};
  /// The contract's ghost, its GhostAI: currentGhost.get(level), or null before it spawns. Check it with
  /// pm::api::alive. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_currentGhost, void*> currentGhost{};
  /// The room the ghost is in, a LevelRoom: currentGhostRoom.get(level), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_currentGhostRoom, void*> currentGhostRoom{};
  /// The contract's fuse box, a FuseBox: fuseBox.get(level). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_fuseBox, void*> fuseBox{};
  /// The level's rooms, a LevelRoom[]: rooms.get(level), read as a pm::api::RefArray. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_rooms, void*> rooms{};
  /// The level's fire sources, a List<FireSource>: fireSources.get(level), read as a pm::api::List.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_fireSources, void*> fireSources{};
  /// The doors a hunt locks, a Door[]: huntingDoors.get(level), read as a pm::api::RefArray.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_huntingDoors, void*> huntingDoors{};
  /// The map variations, a SmallMapVariation[]: mapVariations.get(level), read as a pm::api::RefArray.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_mapVariations, void*> mapVariations{};
  /// The level's outdoors, a LevelRoom: outsideRoom.get(level), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelController_outsideRoom, void*> outsideRoom{};
  /// Whether `point` lies outdoors: IsPointInOutsideRoom(level, &point). It throws, as the game's code
  /// does, when outsideRoom or its roomColliders are null, so check both first. Unresolved: false.
  static constexpr pm::api::Method<pm::map::LevelController_IsPointInOutsideRoom, bool(void*, const Vector3*)>
      IsPointInOutsideRoom{};
};

/// @}
}  // namespace phasmo
