#pragma once

// EvidenceController: the contract's evidence in the world (the ghost orb, fingerprints, bone DNA). The
// type and the members mapped here keep their real names. Notes read from decompiled code are from
// build 25811160. sdk/systems/ghost-types.md says how it gives the Mimic's orbs.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The evidence controller (Assembly-CSharp, global namespace), a MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController{.key = "EvidenceController",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "EvidenceController", .assembly = "Assembly-CSharp",
                                                       .ns = ""};

/// Moves the ghost orb to the favourite room (ChangeFavouriteRoomTo calls it), by an RPC to every
/// client, but only when the ghost's given evidence holds Ghost Orbs or its GhostInfo's behaviour type
/// is the Mimic. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController_ForceRespawnGhostOrb{
    .key = "EvidenceController.ForceRespawnGhostOrb", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "EvidenceController", .name = "ForceRespawnGhostOrb"};

/// Sets up the evidence the ghost gives in the world, given its traits (a GhostTraits, passed by address): on the
/// master, when the given evidence holds Ghost Orbs or the behaviour type is the Mimic, it places the ghost orb; on
/// every client, when it holds Freezing Temperatures, it sets a flag of the controller. Each test is one evidence's
/// membership of the given list, so an evidence it does not name changes nothing. GhostInfo.SyncEvidence calls it on
/// every client once the given evidence arrives. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController_SpawnAllGhostTypeEvidence{
    .key = "EvidenceController.SpawnAllGhostTypeEvidence", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "EvidenceController", .name = "SpawnAllGhostTypeEvidence"};

/// Whether the contract's ghost gives Ultraviolet (fingerprints): true when its GhostInfo's given evidence holds it,
/// false when the list is empty. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController_IsFingerPrintEvidence{
    .key = "EvidenceController.IsFingerPrintEvidence", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "EvidenceController", .name = "IsFingerPrintEvidence"};

/// The ghost orb, a Transform: the controller's one field of that name. The orb's RPC switches its GameObject on and
/// places it; nothing in the game switches it off again. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController_ghostOrb{.key = "EvidenceController.ghostOrb", .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "EvidenceController", .name = "ghostOrb"};

// SpawnAllGhostTypeEvidence stores one bool of the controller: the freezing flag.
inline constexpr pm::sdk::Fact EvidenceController_freezing_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "EvidenceController.SpawnAllGhostTypeEvidence"}};

/// Whether the ghost gives Freezing Temperatures, as the evidence setup left it: SpawnAllGhostTypeEvidence sets it on
/// every client when the given evidence holds Temperature, and nothing in the game clears it. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceController_freezing{.key = "EvidenceController.freezing", .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "EvidenceController", .type = "bool",
                                                                .facts = EvidenceController_freezing_facts};

/// The fingerprints a ghost can leave on one object, such as a door (Assembly-CSharp, global namespace), a
/// MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc Fingerprint{.key = "Fingerprint", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "Fingerprint", .assembly = "Assembly-CSharp", .ns = ""};

/// Tries to leave fingerprints on the object, on the master only: nothing unless the contract's ghost gives
/// Ultraviolet (EvidenceController.IsFingerPrintEvidence); otherwise a roll against a chance the difficulty sets
/// (higher for one ghost type) picks whether, and an RPC shows them on every client. A ghost's push of a door
/// (Door.GhostForceDoor) calls it for the door's prints. Decompiled.
inline constexpr pm::sdk::EntryDesc Fingerprint_AttemptToSpawnFingerprints{
    .key = "Fingerprint.AttemptToSpawnFingerprints", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Fingerprint", .name = "AttemptToSpawnFingerprints"};

}  // namespace pm::map

/// @file
/// @brief The game API for EvidenceController (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and does nothing.
namespace phasmo {
/// @addtogroup equipment
/// @{

/// The contract's evidence in the world. `controller` below is the EvidenceController.
struct EvidenceController {
  /// The EvidenceController type: klass(), is(obj), type_object(), make() and make_array(n).
  /// Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::EvidenceController> type{};
  /// Moves the ghost orb to the favourite room when the ghost gives orbs or is acting as the Mimic:
  /// ForceRespawnGhostOrb(controller). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EvidenceController_ForceRespawnGhostOrb, void(void*)> ForceRespawnGhostOrb{};
  /// The ghost orb's Transform: ghostOrb.get(controller). Unresolved: null.
  static constexpr pm::api::Field<pm::map::EvidenceController_ghostOrb, void*> ghostOrb{};
  /// Whether the ghost gives Freezing Temperatures, as the setup left it: freezing.get(controller). Unresolved: false.
  static constexpr pm::api::Field<pm::map::EvidenceController_freezing, bool> freezing{};
  /// Sets up the ghost's evidence in the world from its traits (a GhostTraits by address):
  /// SpawnAllGhostTypeEvidence(controller, traits). The game calls it once per contract on every client; a mod hooks
  /// it to follow that moment. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EvidenceController_SpawnAllGhostTypeEvidence, void(void*, const void*)>
      SpawnAllGhostTypeEvidence{};
  /// Whether the contract's ghost gives fingerprints (Ultraviolet): IsFingerPrintEvidence(controller). Main thread,
  /// any client once the given evidence has arrived. Unresolved: false.
  static constexpr pm::api::Method<pm::map::EvidenceController_IsFingerPrintEvidence, bool(void*)> IsFingerPrintEvidence{};
};

/// @}
}  // namespace phasmo
