#pragma once

// Door: a door of the level, HingeDoor (a door that swings on a hinge), SmallMapVariation (one of
// LevelController's map variations, with the doors its hunts lock), and the keys that open locked doors.
// The types and their methods keep their real names; Door's leaf body, closed angle and lock are
// obfuscated and found by shape and facts. How exits lock and open: sdk/systems/doors.md.
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
/// the game's own, and a third flag; the one-argument overload is the same with strength 3). Both start
/// one coroutine, unless one is running: it sends Door.ForceDropDoorNetworked(true, true) to every client
/// at its start, moves the leaf, and at its end sends Door.ForceDropDoorNetworked(false, true), which sets
/// Door.grabbable and PhotonObjectInteract.grabbable again on every client, whatever Door.locked or the
/// door's kind. Callers: ghost states and the hunt state, directly or through Door's GhostOpenDoorAnimated
/// (the state GhostState.door enters uses it), ClosetZone for its closet doors, and the training rooms. The
/// states GhostState.idle, wander and favouriteRoom enter call neither.
/// It is a ghost interaction and leaves a ghost's traces. Both overloads mark the push as the ghost's and as one
/// that gives fingerprints, so before the leaf moves the door's ghost-use step runs (unless the door's
/// PhotonObjectInteract is off or in use): on the master it tries for fingerprints
/// (Fingerprint.AttemptToSpawnFingerprints, for a ghost with that evidence); unless LevelValues.eventsBlocked is
/// set, it plays the door's rattle by RPC most times (the three-argument overload's last flag skips it), then calls
/// Door.CreateDoorNoise, which plays the ghost's door sound by RPC and leaves a door-moved EMF reading at the door
/// (GhostInteraction.CreateInteractionEMF); either way it adds one to LevelStats.doorsMoved, which the results show
/// and count among the ghost's interactions. So no mod can use it to move a door with no paranormal trace
/// (sdk/systems/doors.md, "A ghost's force"). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_GhostForceDoor{.key = "Door.GhostForceDoor", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Door", .name = "GhostForceDoor",
                                                        .params = Door_GhostForceDoor_params};

/// Shuts and locks the door as a hunt does: LevelController.LockHuntingDoors calls it for every exit,
/// on the master, as a hunt starts. It does nothing on a friendly ghost's difficulty (it tests the
/// contract's Difficulty.friendlyGhost first, the flag HuntingCloseDoorNetworked refuses on). Otherwise
/// it switches the door off for players (DisableOrEnableDoor(false), by RPC) and sends
/// HuntingCloseDoorNetworked to every client, whose body swings the door shut on its owner's client and
/// then locks it (LockDoor(true)). sdk/systems/doors.md has the whole path. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_HuntingCloseDoor{.key = "Door.HuntingCloseDoor",
                                                          .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "Door", .name = "HuntingCloseDoor"};

inline constexpr const char* Door_HuntingCloseDoorNetworked_params[] = {"PhotonMessageInfo"};

/// The RPC HuntingCloseDoor sends to every client. It refuses a sender that is not the room's first
/// actor, and refuses outright on a friendly ghost's difficulty (Difficulty.friendlyGhost); otherwise it
/// clears Door.grabbable on every client, drops the door from the local player's hand, and on the client
/// that owns the door's PhotonView starts the coroutine that swings it shut, sets it to its closed angle
/// once and calls LockDoor(true). That owner is often the last player who grabbed the door, and then the
/// lock reaches only that client (LockDoor), while the cleared grab switch keeps it shut everywhere. A mod
/// hooks it to see a hunt's lock; never call it (it is an RPC's body). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_HuntingCloseDoorNetworked{
    .key = "Door.HuntingCloseDoorNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door", .name = "HuntingCloseDoorNetworked",
    .params = Door_HuntingCloseDoorNetworked_params};

/// Switches the door off (false) or back on (true) for players, as a hunt's end and the Monkey Paw do:
/// off, it switches the door off, drops it from the local player's hand and, on the client that owns its
/// PhotonView, swings it shut and locks it (the coroutine HuntingCloseDoorNetworked starts); on, it
/// switches the door and its collider back on. Either way it then clears the door's lock (`locked`) by
/// SyncLockState, so `true` unlocks it. Callers: LevelController.UnlockHuntingDoors, Door.LockDoorForTime,
/// and MonkeyPaw.OpenExitsWish (on each exit's owner). Its RPCs check no sender. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_ToggleDoorLock{.key = "Door.ToggleDoorLock", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Door", .name = "ToggleDoorLock",
                                                        .params = Door_ToggleDoorLock_params};

inline constexpr const char* Door_bool[] = {"bool"};

/// Locks the door. It returns at once on a locked door. With `force`, or when the leaf is already
/// within a small angle of shut, it sets the leaf to its closed angle once and marks it closed; a door
/// that is open and not forced stays unlocked. A closed door then sends NetworkedSyncCloseDoor to every
/// client and is marked locked here; on the master it also sends SyncLockState(true) and
/// NetworkedPlayLockSound; called on another client it sends neither, so the lock stays on that client
/// and no click plays.
/// HingeDoor has its own override of the same steps. The hunt's close coroutine calls it with `force`
/// set, on the door's owner. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_LockDoor{.key = "Door.LockDoor", .kind = pm::sdk::Kind::Method,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                  .name = "LockDoor", .params = Door_bool};

/// A hunting ghost's push of the door: while the door keeps no coroutine of this push yet, it starts the one
/// that moves the leaf, with one of two strengths (the argument, true by default, picks which), and keeps it in
/// a field of the door (what clears that field was not traced). The hunt's closet search reaches it through
/// ClosetZone.CheckCloset, on a closet's left door. It is the ghost moving a door, the coroutine
/// Door.GhostForceDoor starts, with the same flags and so the same traces: the noise, the EMF reading, the count
/// and the fingerprints. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_CheckDoorDuringHunt{.key = "Door.CheckDoorDuringHunt",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Door", .name = "CheckDoorDuringHunt",
                                                             .params = Door_bool};

/// Unlocks a locked door: it clears `locked`, sends SyncHasBeenUnlocked the first time, sends
/// SyncLockState(false) to every client and, with its argument set, NetworkedPlayUnlockSound. It does
/// nothing on an unlocked door. Door.AttemptToUnlockDoor calls it when the trying player holds the key.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_UnlockDoor{.key = "Door.UnlockDoor", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                    .name = "UnlockDoor", .params = Door_bool};

inline constexpr const char* Door_SyncLockState_params[] = {"bool", "PhotonMessageInfo"};

/// The RPC that sets the door's lock on every client: it refuses a lock (true) from a sender that is
/// not the room's first actor, but takes an unlock from anyone. It stores `locked`, freezes the leaf's
/// Rigidbody while locked and frees it otherwise, and for a DoorKind.Main door tells
/// SetupPhaseController.UnlockMainDoor and switches the door's hunt collider off. LockDoor sends it
/// from the master only. A mod hooks it to see a lock change; never call it (it is an RPC's body).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_SyncLockState{.key = "Door.SyncLockState", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "Door", .name = "SyncLockState",
                                                       .params = Door_SyncLockState_params};

// Of Door's bools SyncLockState stores only this one.
inline constexpr pm::sdk::Fact Door_locked_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Door.SyncLockState"}};

/// Whether the door is locked, on this client: SyncLockState stores it, LockDoor and UnlockDoor set
/// and clear it. Every exit (DoorKind.Main) starts a contract locked in the map's scene; a player opens
/// it with the house key (sdk/systems/doors.md). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_locked{.key = "Door.locked", .kind = pm::sdk::Kind::Field,
                                                .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                .type = "bool", .facts = Door_locked_facts,
                                                .access = pm::sdk::Visibility::Public};

/// A player's try at a locked door, on that player's own client: the interact input
/// (DragRigidbodyUse.OnInteract) calls it for the door under the player's aim. Door.GrabbedDoor runs the
/// same steps for a locked door, but the PC player's grab never reaches it for one (DragRigidbodyUse.FixedUpdate
/// refuses a locked door first). On an unlocked door it does nothing. Outside a hunt (GhostAI.isHunting) a key
/// of the door's kind in the local player's keys (Player.keys) unlocks it (UnlockDoor). Otherwise a
/// DoorKind.None door unlocks (unless LockDoorForTime holds it), and any other gets the locked sound:
/// NetworkedPlayLockSound to every client in a room, the same clip played here offline. A mod hooks it
/// to see the try. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_AttemptToUnlockDoor{.key = "Door.AttemptToUnlockDoor",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Door", .name = "AttemptToUnlockDoor"};

/// A player grabbed the door, on that player's client: an unlocked door sends NetworkedGrabbedDoor to
/// every client (runs it here offline); a locked one runs AttemptToUnlockDoor's steps instead. The PC
/// player's grab (DragRigidbodyUse.FixedUpdate) calls it only for a door that is unlocked and grabbable
/// (Door.grabbable). Door.Start also adds it to the door's PhotonObjectInteract grab event, under a
/// static test of another class that was not traced. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_GrabbedDoor{.key = "Door.GrabbedDoor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Door", .name = "GrabbedDoor"};

inline constexpr const char* Door_CreateDoorNoise_params[] = {"Door"};

/// The sound and EMF a ghost leaves at a door it moves, on the client that moves it: it sends the door's
/// PlayDoorNoise RPC to every client (whose body checks its sender, then plays one of the ghost's door clips
/// through the door's Noise) and leaves a door-moved EMF reading at the door's handle, or the door itself, through
/// the contract ghost's GhostInteraction.CreateInteractionEMF. Door.GhostForceDoor's ghost-use step calls it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_CreateDoorNoise{.key = "Door.CreateDoorNoise", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "Door", .name = "CreateDoorNoise",
                                                         .params = Door_CreateDoorNoise_params};

inline constexpr const char* Door_ForceDropDoorNetworked_params[] = {"bool", "bool"};

/// The RPC Door.DisableOrEnableDoor sends to every client with `off` the opposite of its argument. It sets
/// Door.grabbable to !off (and a flag of the door's PhotonObjectInteract with it); switching off, it drops
/// the door from the local player's hand and, with its second argument on the master, requests the door's
/// PhotonView. It checks no sender. A mod hooks it; never calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_ForceDropDoorNetworked{
    .key = "Door.ForceDropDoorNetworked", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Door", .name = "ForceDropDoorNetworked", .params = Door_ForceDropDoorNetworked_params};

// Of Door's bools ForceDropDoorNetworked stores only this one.
inline constexpr pm::sdk::Fact Door_grabbable_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Door.ForceDropDoorNetworked"}};

/// Whether players may grab the door, on this client: the PC player's grab (DragRigidbodyUse.FixedUpdate)
/// refuses a door with it clear, as it refuses a locked one. Door.ForceDropDoorNetworked sets it on every
/// client; a hunt's lock (Door.HuntingCloseDoor, Door.HuntingCloseDoorNetworked) and ToggleDoorLock(false)
/// clear it. ToggleDoorLock(true) sets it again, and so does the end of a ghost's force (Door.GhostForceDoor),
/// whatever the lock. Unlocking (UnlockDoor, SyncLockState) leaves it alone, so a switched-off door stays shut
/// when its lock clears. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_grabbable{.key = "Door.grabbable", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                   .type = "bool", .facts = Door_grabbable_facts,
                                                   .access = pm::sdk::Visibility::Public};

/// The RPC of the locked sound: it plays one of the door's lock clips through its Noise, for a
/// DoorKind.Main door only, while the door's object is active. It checks no sender. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_NetworkedPlayLockSound{.key = "Door.NetworkedPlayLockSound",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "Door", .name = "NetworkedPlayLockSound"};

inline constexpr const char* Door_LockDoorForTime_params[] = {"float", "bool"};

/// Locks the door for `seconds`, the Monkey Paw's timed lock. Its coroutine, with `takeOwnership` first
/// requesting the door's PhotonView and waiting until this client owns it, calls ToggleDoorLock(false),
/// which clears `locked` and switches the door off, then the close coroutine on the owner shuts and
/// re-locks it; after `seconds` it calls ToggleDoorLock(true), which switches the door back on and
/// unlocks it with no sound, whoever locked it. A DoorKind.Main door is skipped at the start, and left
/// locked at the end, while the ghost hunts (GhostAI.isHunting), so that hunt's end
/// (LevelController.UnlockHuntingDoors) opens it. Two wishes call it on every exit and one on the
/// favourite room's doors (sdk/systems/doors.md, "The Monkey Paw"). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_LockDoorForTime{.key = "Door.LockDoorForTime", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "Door", .name = "LockDoorForTime",
                                                         .params = Door_LockDoorForTime_params};

/// Switches the door's hunt collider (the box a hunt switches on across a DoorKind.Main doorway) on or off,
/// on this client only. MonkeyPaw.OpenExitsWish switches it off on every exit. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_ForceToggleHuntCollider{
    .key = "Door.ForceToggleHuntCollider", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door", .name = "ForceToggleHuntCollider",
    .params = Door_bool};

inline constexpr const char* DoorKind_values[] = {"None", "Main", "Secondary", "PermanentLock"};

/// What kind of door a Door is (Assembly-CSharp, global namespace, name obfuscated): None, Main (an
/// exit), Secondary or PermanentLock; a key (KeyInfo.keyType) opens the doors of its own kind. Found by
/// its values. Decompiled.
inline constexpr pm::sdk::EntryDesc DoorKind{.key = "DoorKind", .confidence = pm::sdk::Confidence::Decompiled,
                                             .assembly = "Assembly-CSharp", .ns = "", .values = DoorKind_values};

/// An exit: a door between the map's rooms and its outdoors (RoomKind.outsideMap). On every map of
/// build 25811160 the Main doors are exactly LevelController.huntingDoors, one to six of them, and each
/// starts the contract locked (scene read). Only a Main door plays the locked sound
/// (NetworkedPlayLockSound) and calls SetupPhaseController.UnlockMainDoor when its lock changes.
/// GhostActivity.GetDoorToOpen never picks it, and the Asterion leaves it alone. Decompiled.
inline constexpr pm::sdk::EntryDesc DoorKind_Main{.key = "DoorKind.Main", .kind = pm::sdk::Kind::EnumValue,
                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                  .owner = "DoorKind", .name = "Main"};

/// A plain door: no exit, no key's door, not kept locked. LevelController's contract-start opener opens only doors
/// of this kind (HingeDoor.LevelControllerOpenDoor), and an unlocked try at one unlocks it
/// (Door.AttemptToUnlockDoor). Decompiled.
inline constexpr pm::sdk::EntryDesc DoorKind_None{.key = "DoorKind.None", .kind = pm::sdk::Kind::EnumValue,
                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                  .owner = "DoorKind", .name = "None"};

/// A locked door inside the map, which a key of its kind opens (AttemptToUnlockDoor): Maple Lodge
/// Campsite's cabin door and Point Hope's machine housing in build 25811160 (scene read). Optional.
/// Inferred: from the value's name and the scenes.
inline constexpr pm::sdk::EntryDesc DoorKind_Secondary{.key = "DoorKind.Secondary", .kind = pm::sdk::Kind::EnumValue,
                                                       .flags = pm::sdk::kOptional, .owner = "DoorKind",
                                                       .name = "Secondary"};

/// A door that starts locked and is not an exit: wardrobes, toilet cubicles, a locked shed or safe, in
/// build 25811160 (scene read). Optional. Inferred: from the value's name and the scenes.
inline constexpr pm::sdk::EntryDesc DoorKind_PermanentLock{.key = "DoorKind.PermanentLock",
                                                           .kind = pm::sdk::Kind::EnumValue,
                                                           .flags = pm::sdk::kOptional, .owner = "DoorKind",
                                                           .name = "PermanentLock"};

/// A key's description (Assembly-CSharp, global namespace), a journal collectable's info. Decompiled.
inline constexpr pm::sdk::EntryDesc KeyInfo{.key = "KeyInfo", .confidence = pm::sdk::Confidence::Decompiled,
                                            .name = "KeyInfo", .assembly = "Assembly-CSharp", .ns = ""};

/// The kind of door the key opens, a DoorKind number: Door.AttemptToUnlockDoor compares it with the
/// door's kind. Decompiled.
inline constexpr pm::sdk::EntryDesc KeyInfo_keyType{.key = "KeyInfo.keyType", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "KeyInfo", .name = "keyType"};

/// A key lying in the level (Assembly-CSharp, global namespace): every map has a house key, and some a
/// car, garage, cabin or cabinet key too (scene read). Its Start hides a DoorKind.Main key while
/// GameController.allPlayersLoaded is false, outside the training; what shows it again was not traced.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Key{.key = "Key", .confidence = pm::sdk::Confidence::Decompiled, .name = "Key",
                                        .assembly = "Assembly-CSharp", .ns = ""};

/// A player picked the key up, on that player's client (Key.Start gives it to the key's
/// PhotonObjectInteract as a grab event): it adds the key's KeyInfo to the local player's keys
/// (Player.keys), so only that client's player gains it. Decompiled.
inline constexpr pm::sdk::EntryDesc Key_GrabbedKey{.key = "Key.GrabbedKey", .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Key",
                                                   .name = "GrabbedKey"};

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

/// The RPC that marks the door open on every client: it opens the door's OcclusionPortal, when it has one, and
/// clears Door.closed. It takes no PhotonMessageInfo, so it checks no sender. HingeDoor.LevelControllerOpenDoor
/// sends it, buffered for players who join later. A mod hooks it; never calls it (it is an RPC's body).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Door_SetDoorAndOcclusionOpen{
    .key = "Door.SetDoorAndOcclusionOpen", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door", .name = "SetDoorAndOcclusionOpen"};

// Of Door's bools SetDoorAndOcclusionOpen stores only this one.
inline constexpr pm::sdk::Fact Door_closed_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Door.SetDoorAndOcclusionOpen"}};

/// Whether the door reads shut, on this client: NetworkedSyncCloseDoor, the RPC Door.LockDoor sends, sets it on
/// every client, and Door.NetworkedGrabbedDoor and
/// Door.SetDoorAndOcclusionOpen clear it on every client. LevelController's contract-start opener opens only
/// doors that read shut (HingeDoor.LevelControllerOpenDoor). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_closed{.key = "Door.closed", .kind = pm::sdk::Kind::Field,
                                                .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                                .type = "bool", .facts = Door_closed_facts,
                                                .access = pm::sdk::Visibility::Public};

/// The door's PhotonView: Door's one field of that type. In every map scene of build 25811160 that was read,
/// the view is set to OwnershipOption.Takeover and observes the leaf's PhotonTransformView, which sends the
/// leaf's position and rotation from the view's owner and, on every other client, moves the leaf to the
/// owner's each frame (sdk/systems/doors.md, "Opening a door quietly"). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_view{.key = "Door.view", .kind = pm::sdk::Kind::Field,
                                              .confidence = pm::sdk::Confidence::Decompiled, .owner = "Door",
                                              .type = "PhotonView"};

/// The door is let go: it sends Door.NetworkedUngrabbedDoor to every client in a room (runs it here offline);
/// HingeDoor's override first does a step of its own when its argument is set. Door.ResetRigidbody's wait, three
/// seconds after HingeDoor.LevelControllerOpenDoor, calls it with `false` unless a player grabbed the door meanwhile.
/// Its other callers were not traced. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_UnGrabbedDoor{.key = "Door.UnGrabbedDoor", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "Door", .name = "UnGrabbedDoor",
                                                       .params = Door_bool};

/// Starts the door's wait after a move: it clears the door's note of a player's grab, waits three seconds and,
/// unless a player grabbed the door meanwhile (Door.NetworkedGrabbedDoor sets that note), calls
/// Door.UnGrabbedDoor with its argument. Only while the door's object is active. HingeDoor.LevelControllerOpenDoor
/// calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc Door_ResetRigidbody{.key = "Door.ResetRigidbody", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Door", .name = "ResetRigidbody",
                                                        .params = Door_bool};

/// What every client runs when the door is let go (Door.UnGrabbedDoor's RPC): it clears the door's
/// PhotonObjectInteract.inUse, stops the leaf's creak on a client that does not own the door's view, puts the
/// view's syncing back to on-change after a wait, and checks the door's OcclusionPortal. It takes no
/// PhotonMessageInfo, so it checks no sender. A mod hooks it; never calls it (it is an RPC's body). Decompiled.
inline constexpr pm::sdk::EntryDesc Door_NetworkedUngrabbedDoor{.key = "Door.NetworkedUngrabbedDoor",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "Door", .name = "NetworkedUngrabbedDoor"};

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

inline constexpr const char* HingeDoor_LevelControllerOpenDoor_params[] = {"bool", "float"};

/// Opens the door part of the way with no ghost's trace, the way the game opens doors as a contract starts. Its one
/// caller is LevelController's contract-start coroutine, on the master, which opens a number of random doors set by
/// the difficulty and the map's size, each one of kind DoorKind.None that reads shut (Door.closed) and has a
/// PhotonObjectInteract. On the client that calls it: it sets the leaf's Rigidbody's mass and gravity, makes it
/// kinematic, turns the leaf to a random angle a tenth to most of the way from shut to its hinge's open limit (the
/// bool swaps which end the fraction counts from; the float is unused), starts Door.ResetRigidbody's wait, makes the
/// Rigidbody free again and sends Door.SetDoorAndOcclusionOpen to every client, buffered (runs it here offline).
/// The leaf opens one way only, its hinge's, by an amount the call cannot choose.
/// It makes no sound and leaves no EMF reading, LevelStats count or fingerprint; Door.locked, Door.grabbable and the
/// door's kind are left as they were. The angle reaches other clients only through the door's PhotonTransformView,
/// from the view's owner (Door.view): called on a client that does not own the view, the leaf is pulled back to the
/// owner's angle the next frame and nobody else sees it open. So take the view first (PhotonView.RequestOwnership,
/// then wait for PhotonView.get_IsMine), as the ghost's own door step does (sdk/systems/doors.md, "Opening a door
/// quietly"). Its RPCs check no sender. Decompiled.
inline constexpr pm::sdk::EntryDesc HingeDoor_LevelControllerOpenDoor{
    .key = "HingeDoor.LevelControllerOpenDoor", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "HingeDoor", .name = "LevelControllerOpenDoor",
    .params = HingeDoor_LevelControllerOpenDoor_params};

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

/// The variation's rooms, a LevelRoom[]: LevelController.LockHuntingDoors locks the variation's doors
/// only when the ghost's room is one of them and a player is in one of them. Decompiled.
inline constexpr pm::sdk::EntryDesc SmallMapVariation_huntingRooms{
    .key = "SmallMapVariation.huntingRooms", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SmallMapVariation", .name = "huntingRooms"};

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
  /// HuntingCloseDoor(door): shuts, switches off and locks the door on every client, as a hunt locks its
  /// exits. Call it on the master only, as LevelController.LockHuntingDoors does: its RPC refuses any other
  /// sender, and refuses on a friendly ghost's difficulty, which this method itself skips. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Door_HuntingCloseDoor, void(void*)> HuntingCloseDoor{};
  /// HuntingCloseDoorNetworked(door, &info), an RPC. A mod hooks it; never calls it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Door_HuntingCloseDoorNetworked, void(void*, void*)>
      HuntingCloseDoorNetworked{};
  /// ToggleDoorLock(door, on, b): `on` false switches the door off and shuts it, true switches it back on;
  /// either way it ends unlocked. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_ToggleDoorLock, void(void*, bool, bool)> ToggleDoorLock{};
  /// LockDoor(door, force): locks the door, shutting it first with `force`; the lock reaches every client
  /// only when the master calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_LockDoor, void(void*, bool)> LockDoor{};
  /// UnlockDoor(door, sound): unlocks it on every client, with its unlock sound when `sound`. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::Door_UnlockDoor, void(void*, bool)> UnlockDoor{};
  /// SyncLockState(door, locked, &info), an RPC. A mod hooks it; never calls it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Door_SyncLockState, void(void*, bool, void*)> SyncLockState{};
  /// Whether it is locked on this client: locked.get(door). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Door_locked, bool> locked{};
  /// Whether players may grab it on this client: grabbable.get(door). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Door_grabbable, bool> grabbable{};
  /// ForceDropDoorNetworked(door, off, b), an RPC. A mod hooks it; never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_ForceDropDoorNetworked, void(void*, bool, bool)>
      ForceDropDoorNetworked{};
  /// AttemptToUnlockDoor(door): the local player tries the door. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Door_AttemptToUnlockDoor, void(void*)> AttemptToUnlockDoor{};
  /// GrabbedDoor(door): the local player grabbed the door. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Door_GrabbedDoor, void(void*)> GrabbedDoor{};
  /// NetworkedPlayLockSound(door), an RPC. A mod hooks it; never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_NetworkedPlayLockSound, void(void*)> NetworkedPlayLockSound{};
  /// ForceToggleHuntCollider(door, on): its hunt collider, on this client. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_ForceToggleHuntCollider, void(void*, bool)>
      ForceToggleHuntCollider{};
  /// LockDoorForTime(door, seconds, takeOwnership). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_LockDoorForTime, void(void*, float, bool)> LockDoorForTime{};
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
  /// SetDoorAndOcclusionOpen(door), an RPC. A mod hooks it; never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_SetDoorAndOcclusionOpen, void(void*)> SetDoorAndOcclusionOpen{};
  /// Whether it reads shut on this client: closed.get(door). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Door_closed, bool> closed{};
  /// Its PhotonView: view.get(door). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Door_view, void*> view{};
  /// ResetRigidbody(door, flag): the wait after a move, then UnGrabbedDoor(door, flag). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_ResetRigidbody, void(void*, bool)> ResetRigidbody{};
  /// UnGrabbedDoor(door, flag): lets the door go on every client. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_UnGrabbedDoor, void(void*, bool)> UnGrabbedDoor{};
  /// NetworkedUngrabbedDoor(door), an RPC. A mod hooks it; never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Door_NetworkedUngrabbedDoor, void(void*)> NetworkedUngrabbedDoor{};
};

/// What kind of door a Door is.
namespace DoorKind {
/// None(): a plain door, the kind the game's contract-start opener opens. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_None> None{};
/// Main(): an exit. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_Main> Main{};
/// Secondary(): a locked door inside the map that its own key opens. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_Secondary> Secondary{};
/// PermanentLock(): a door that stays locked. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoorKind_PermanentLock> PermanentLock{};
}  // namespace DoorKind

/// A key's description. `info` below is the KeyInfo.
struct KeyInfo {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::KeyInfo> type{};
  /// The kind of door it opens, a DoorKind number: keyType.get(info). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::KeyInfo_keyType, int32_t> keyType{};
};

/// A key lying in the level. `key` below is the Key.
struct Key {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Key> type{};
  /// GrabbedKey(key): the local player picked it up. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Key_GrabbedKey, void(void*)> GrabbedKey{};
};

/// A hinged door. `door` below is the HingeDoor.
struct HingeDoor {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::HingeDoor> type{};
  /// The axis its leaf swings about: hingeAxis.get(door). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::HingeDoor_hingeAxis, int32_t> hingeAxis{};
  /// LevelControllerOpenDoor(door, flip, unused): opens it part of the way with no ghost's trace, on this client;
  /// other clients see it only when this client owns the door's view (Door::view). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HingeDoor_LevelControllerOpenDoor, void(void*, bool, float)>
      LevelControllerOpenDoor{};
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
  /// Its hunts' rooms, a LevelRoom[]: huntingRooms.get(variation), read as a pm::api::RefArray.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::SmallMapVariation_huntingRooms, void*> huntingRooms{};
};

/// @}
}  // namespace phasmo
