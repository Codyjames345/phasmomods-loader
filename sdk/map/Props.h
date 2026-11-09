#pragma once

// The things in a contract's world a ghost or a player acts on: PhotonObjectInteract (anything that can
// be picked up, used or opened), Prop (a throwable, the base of Equipment), the sinks and the salt.
// Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// An object players and the ghost can interact with (Assembly-CSharp, global namespace): a carried
/// item has a Rigidbody, a door a Door. A ghost's interactions are given one (GhostAI.ChangeState).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract{.key = "PhotonObjectInteract",
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .name = "PhotonObjectInteract", .assembly = "Assembly-CSharp",
                                                         .ns = ""};

/// Its Rigidbody, set on things that can be carried: its one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_body{.key = "PhotonObjectInteract.body",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "PhotonObjectInteract",
                                                              .type = "Rigidbody"};

/// Its Door, set on a door (which is not carried): its one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_door{.key = "PhotonObjectInteract.door",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "PhotonObjectInteract",
                                                              .type = "@Door"};

/// Its PhotonView, through which ownership and its synchronisation go: its one field of that type.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_view{.key = "PhotonObjectInteract.view",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "PhotonObjectInteract",
                                                              .type = "PhotonView"};

/// Its Prop (Equipment is a Prop), set on things that can be thrown or used: its one field of that
/// type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_prop{.key = "PhotonObjectInteract.prop",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "PhotonObjectInteract",
                                                              .type = "@Prop"};

inline constexpr const char* PhotonObjectInteract_FreezeRigidbody_params[] = {"bool", "bool"};

/// Freezes or frees its Rigidbody. FreezeRigidbody(false, true) frees it as the game's prop throw
/// readies a thrown object: gravity on, not kinematic, continuous collisions; while its view's
/// IsMine is set it also sends the RPC that does the same on every client. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_FreezeRigidbody{
    .key = "PhotonObjectInteract.FreezeRigidbody", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhotonObjectInteract", .name = "FreezeRigidbody",
    .params = PhotonObjectInteract_FreezeRigidbody_params};

/// Unity's Start for it: among its set-up it reads `grabbable`. Found for that fact. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_Start{.key = "PhotonObjectInteract.Start",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "PhotonObjectInteract", .name = "Start"};

/// Unity's Reset for it (the editor's defaults): among two other bools it sets `grabbable`. Found for that
/// fact. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_Reset{.key = "PhotonObjectInteract.Reset",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "PhotonObjectInteract", .name = "Reset"};

// Of the class's bools Start reads three and Reset stores three; this one is the only bool in both.
inline constexpr pm::sdk::Fact PhotonObjectInteract_grabbable_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "PhotonObjectInteract.Start"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "PhotonObjectInteract.Reset"}};

/// Whether a VR player may take it, on this client: VRGrabbable.IsSelectableBy refuses its object while
/// this is clear, and never reads Door.locked. For a door, Door.ForceDropDoorNetworked sets it with
/// Door.grabbable on every client, so a hunt's lock (Door.HuntingCloseDoor), ToggleDoorLock(false) and a
/// ghost's force (Door.GhostForceDoor, while it runs) clear it; the PC grab reads Door.grabbable instead.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_grabbable{
    .key = "PhotonObjectInteract.grabbable", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhotonObjectInteract", .type = "bool",
    .facts = PhotonObjectInteract_grabbable_facts,
    .access = pm::sdk::Visibility::Public};

// Of the class's bools Door.NetworkedUngrabbedDoor stores only this one, through the door's interact.
inline constexpr pm::sdk::Fact PhotonObjectInteract_inUse_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Door.NetworkedUngrabbedDoor"}};

/// Whether something is using it now, on this client. For a door: Door.NetworkedGrabbedDoor sets it on every
/// client as a player grabs the door and Door.NetworkedUngrabbedDoor clears it as it is let go; a ghost's force
/// (Door.GhostForceDoor) sets it while its leaf moves, and refuses to start on a door that has it set. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonObjectInteract_inUse{
    .key = "PhotonObjectInteract.inUse", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonObjectInteract", .type = "bool", .facts = PhotonObjectInteract_inUse_facts,
    .access = pm::sdk::Visibility::Public};

/// The VR player's hold on an object (Assembly-CSharp, global namespace), an XR interactable. Decompiled.
inline constexpr pm::sdk::EntryDesc VRGrabbable{.key = "VRGrabbable", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "VRGrabbable", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* VRGrabbable_IsSelectableBy_params[] = {"IXRSelectInteractor"};

/// Whether a VR hand may take the object: the XR toolkit's own test first, then false while its
/// PhotonObjectInteract's `grabbable` is clear, and further tests of ownership, the local player and line
/// of sight. It never reads Door.locked, so a locked door holds for a VR player only through that flag.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc VRGrabbable_IsSelectableBy{
    .key = "VRGrabbable.IsSelectableBy", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "VRGrabbable", .name = "IsSelectableBy",
    .params = VRGrabbable_IsSelectableBy_params};

/// A throwable thing (Assembly-CSharp, global namespace), the base class of Equipment. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop{.key = "Prop", .confidence = pm::sdk::Confidence::Decompiled, .name = "Prop",
                                         .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* Prop_PlayImpactSound_params[] = {"float", "AudioClip[]"};
inline constexpr const char* ObjectPooler_SpawnFromPool_params[] = {"string", "Vector3", "Quaternion", "bool"};
// Prop has six methods of this shape; only the real one spawns a pooled sound object (it then plays its clip as
// its last act, a jump the Calls fact does not see).
inline constexpr pm::sdk::Fact Prop_PlayImpactSound_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "ObjectPooler.SpawnFromPool"}};

/// The game's pool of reusable objects (Assembly-CSharp, global namespace), such as the sound object a prop's
/// impact plays through. Decompiled.
inline constexpr pm::sdk::EntryDesc ObjectPooler{.key = "ObjectPooler", .confidence = pm::sdk::Confidence::Decompiled,
                                                 .name = "ObjectPooler", .assembly = "Assembly-CSharp", .ns = ""};

/// Takes an object of the named pool and puts it at a position and rotation, as Prop.PlayImpactSound does for
/// its sound object. Decompiled.
inline constexpr pm::sdk::EntryDesc ObjectPooler_SpawnFromPool{.key = "ObjectPooler.SpawnFromPool",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "ObjectPooler", .name = "SpawnFromPool",
                                                               .params = ObjectPooler_SpawnFromPool_params};

/// Plays the prop's impact sound on this client only, at the prop: a pooled sound object's Noise plays one of
/// the prop's gentle clips when `velocity` (metres a second) is no more than Prop.lowImpactVelocityCutoff and it
/// has some, else one of its hard ones (or the game's default impact clip when it has none), at a volume that
/// grows with `velocity`. `clips` (may be null) replaces the prop's own clips. The prop's collision handler calls
/// it on each client as the prop hits something. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop_PlayImpactSound{.key = "Prop.PlayImpactSound", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "Prop", .type = "void",
                                                         .params = Prop_PlayImpactSound_params,
                                                         .facts = Prop_PlayImpactSound_facts};

/// The impact speed, metres a second, at or below which the prop plays its gentle clips. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop_lowImpactVelocityCutoff{.key = "Prop.lowImpactVelocityCutoff",
                                                                 .kind = pm::sdk::Kind::Field,
                                                                 .confidence = pm::sdk::Confidence::Decompiled,
                                                                 .owner = "Prop", .name = "lowImpactVelocityCutoff"};

/// Its PhotonObjectInteract: Prop's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop_interact{.key = "Prop.interact", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Prop",
                                                  .type = "@PhotonObjectInteract"};

inline constexpr const char* Prop_TryThrow_params[] = {"bool", "bool"};

/// The ghost's throw of the prop (the overload with two flags). The hunt's tick is its only caller
/// while a ghost hunts; the ghost's throwing state never reaches it. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop_TryThrow{.key = "Prop.TryThrow", .kind = pm::sdk::Kind::Method,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Prop",
                                                  .name = "TryThrow", .params = Prop_TryThrow_params};

/// A sink (Assembly-CSharp, global namespace). Its first Use shows the water and starts the fill
/// tween on the water surface; each later Use pauses or resumes it (DOTween.TogglePause), and it ends
/// when the basin is full. Decompiled.
inline constexpr pm::sdk::EntryDesc Sink{.key = "Sink", .confidence = pm::sdk::Confidence::Decompiled, .name = "Sink",
                                         .assembly = "Assembly-CSharp", .ns = ""};

/// The water surface's Transform, which the fill tween moves. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc Sink_water{.key = "Sink.water", .kind = pm::sdk::Kind::Field, .owner = "Sink",
                                               .name = "water"};

/// Set by the sink's first Use: Sink's one bool. Decompiled.
inline constexpr pm::sdk::EntryDesc Sink_used{.key = "Sink.used", .kind = pm::sdk::Kind::Field,
                                              .confidence = pm::sdk::Confidence::Decompiled, .owner = "Sink",
                                              .type = "bool"};

/// A pile of salt a player has laid (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc SaltSpot{.key = "SaltSpot", .name = "SaltSpot", .assembly = "Assembly-CSharp",
                                             .ns = ""};

/// The flattened pile, a GameObject shown once the ghost has stepped in the salt. Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc SaltSpot_flatSalt{.key = "SaltSpot.flatSalt", .kind = pm::sdk::Kind::Field,
                                                      .owner = "SaltSpot", .name = "flatSalt"};

inline constexpr const char* SaltSpot_SyncSalt_params[] = {"PhotonMessageInfo"};

/// The ghost's step in the salt, an RPC on every client. Unless the ghost's type ignores salt, it flattens the pile and
/// resets the ghost's salt timer. Tier three salt then sets to a half a speed multiplier the hunt's speed update reads,
/// in a hunt, an appearance, an event, the summoning circle or the music box, and otherwise changes the ghost's state to
/// its salt state. In a hunt nothing in it stops the agent, and it does not touch the NavMesh. Mapped so its claims can
/// be read. Decompiled.
inline constexpr pm::sdk::EntryDesc SaltSpot_SyncSalt{.key = "SaltSpot.SyncSalt", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled, .owner = "SaltSpot",
                                                      .name = "SyncSalt", .params = SaltSpot_SyncSalt_params};

}  // namespace pm::map

/// @file
/// @brief The game API for the world's props (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game.
namespace phasmo {
/// @addtogroup equipment
/// @{

/// An object one can interact with. `interact` below is one.
struct PhotonObjectInteract {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PhotonObjectInteract> type{};
  /// Its Rigidbody, or null: body.get(interact). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_body, void*> body{};
  /// Its Door, or null: door.get(interact). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_door, void*> door{};
  /// Its PhotonView: view.get(interact). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_view, void*> view{};
  /// Its Prop, or null: prop.get(interact). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_prop, void*> prop{};
  /// FreezeRigidbody(interact, freeze, networked). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhotonObjectInteract_FreezeRigidbody, void(void*, bool, bool)>
      FreezeRigidbody{};
  /// Whether a VR player may take it: grabbable.get(interact). Unresolved: false.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_grabbable, bool> grabbable{};
  /// Whether a player's hand or a ghost's force is using it now: inUse.get(interact). Unresolved: false.
  static constexpr pm::api::Field<pm::map::PhotonObjectInteract_inUse, bool> inUse{};
};

/// The VR player's hold on an object. `grabbable` below is the VRGrabbable.
struct VRGrabbable {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::VRGrabbable> type{};
  /// IsSelectableBy(grabbable, interactor): whether a VR hand may take it. A mod hooks it. Unresolved:
  /// false.
  static constexpr pm::api::Method<pm::map::VRGrabbable_IsSelectableBy, bool(void*, void*)> IsSelectableBy{};
};

/// A throwable. `prop` below is one.
struct Prop {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Prop> type{};
  /// Its PhotonObjectInteract: interact.get(prop). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Prop_interact, void*> interact{};
  /// PlayImpactSound(prop, velocity, clips): its impact sound on this client; clips may be null. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::Prop_PlayImpactSound, void(void*, float, void*)> PlayImpactSound{};
  /// The speed at or below which it plays its gentle clips: lowImpactVelocityCutoff.get(prop). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Prop_lowImpactVelocityCutoff, float> lowImpactVelocityCutoff{};
  /// TryThrow(prop, a, b). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Prop_TryThrow, void(void*, bool, bool)> TryThrow{};
};

/// A sink. `sink` below is one.
struct Sink {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Sink> type{};
  /// The water surface's Transform: water.get(sink). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Sink_water, void*> water{};
  /// Whether it has been used: used.get(sink). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Sink_used, bool> used{};
};

/// A pile of salt.
struct SaltSpot {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SaltSpot> type{};
  /// SyncSalt(salt, &info): the ghost's step in it. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SaltSpot_SyncSalt, void(void*, void*)> SyncSalt{};
  /// The flattened pile, a GameObject: flatSalt.get(salt). Unresolved: null.
  static constexpr pm::api::Field<pm::map::SaltSpot_flatSalt, void*> flatSalt{};
};

/// @}
}  // namespace phasmo
