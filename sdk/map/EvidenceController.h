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
};

/// @}
}  // namespace phasmo
