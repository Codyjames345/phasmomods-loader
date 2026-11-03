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

/// A throwable thing (Assembly-CSharp, global namespace), the base class of Equipment. Decompiled.
inline constexpr pm::sdk::EntryDesc Prop{.key = "Prop", .confidence = pm::sdk::Confidence::Decompiled, .name = "Prop",
                                         .assembly = "Assembly-CSharp", .ns = ""};

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

// The game API for the world's props (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this game
// build" once and returns its type's default (0, false, null) without touching the game.
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
};

/// A throwable. `prop` below is one.
struct Prop {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Prop> type{};
  /// Its PhotonObjectInteract: interact.get(prop). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Prop_interact, void*> interact{};
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
