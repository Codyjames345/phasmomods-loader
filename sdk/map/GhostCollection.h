#pragma once

// GhostCollection and GhostData: the ScriptableObjects that list the ghost types, their names and
// their evidence. Every member keeps its real name. Notes read from decompiled code are from build
// 25811160. sdk/systems/ghost-types.md says where the game looks types up in them.

#include "entry.h"
#include "game_api.h"
#include "UnityPostProcessing.h"

namespace pm::map {

inline constexpr const char* GhostCollection_GetGhostOfType_params[] = {"@GhostType"};

/// A collection of ghost types (Assembly-CSharp, global namespace), a ScriptableObject. The contract
/// has one (GhostController.allGhostsCollection) and the difficulty menu another. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostCollection{.key = "GhostCollection",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "GhostCollection", .assembly = "Assembly-CSharp", .ns = "",
                                                    .base = &ScriptableObject};

/// The collection's ghosts, a List<GhostData>, alphabetical by name as the assets ship it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostCollection_ghosts{.key = "GhostCollection.ghosts",
                                                           .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostCollection", .name = "ghosts"};

/// The first GhostData in ghosts whose type is the one asked, or null. The roll's evidence pick and
/// GetFullEvidence use it without a null check, so a type with no GhostData here makes them throw.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostCollection_GetGhostOfType{
    .key = "GhostCollection.GetGhostOfType", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostCollection", .name = "GetGhostOfType",
    .params = GhostCollection_GetGhostOfType_params};

/// The type roll's weights: a new Dictionary<GhostType, int> holding a weight of 1 for the type of every GhostData in
/// ghosts, a custom ghost the host put there included. GhostController.Spawn takes some types out of it and lays
/// other weights over it before GhostController.PickType draws (sdk/systems/ghost-ai.md, "The ghost's setup").
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostCollection_GetDefaultWeightedGhostList{
    .key = "GhostCollection.GetDefaultWeightedGhostList", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostCollection", .name = "GetDefaultWeightedGhostList"};

/// One ghost type's data (Assembly-CSharp, global namespace), a ScriptableObject. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData{.key = "GhostData", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "GhostData", .assembly = "Assembly-CSharp", .ns = "",
                                              .base = &ScriptableObject};

/// The ghost type this data describes, a GhostType number. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_type{.key = "GhostData.type", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "GhostData", .name = "type"};

/// The localisation key of the ghost's name, a string ("Journal_<n>Title" for the vanilla ghosts).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_ghostNameLocalisationKey{
    .key = "GhostData.ghostNameLocalisationKey", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostData", .name = "ghostNameLocalisationKey"};

/// The first of the ghost's three evidence, an EvidenceType number. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_evidence1{.key = "GhostData.evidence1", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostData", .name = "evidence1"};

/// The second of the ghost's three evidence. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_evidence2{.key = "GhostData.evidence2", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostData", .name = "evidence2"};

/// The third of the ghost's three evidence. For the five types whose evidence is forced (Hantu,
/// Goryo, Obake, Moroi, Deogen), the roll never removes this one on a difficulty that gives fewer than
/// three. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_evidence3{.key = "GhostData.evidence3", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostData", .name = "evidence3"};

/// A new List<EvidenceType> of the three evidence fields, in order, an empty one (None) included.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_GetEvidenceList{.key = "GhostData.GetEvidenceList",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostData", .name = "GetEvidenceList"};

/// The Ghost value the journal's lists hold for this data (its type, name and evidence). The journal's
/// RebuildGhosts calls it for each entry of its collection. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostData_GetGhostData{.key = "GhostData.GetGhostData",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostData", .name = "GetGhostData"};

}  // namespace pm::map

/// @file
/// @brief The game API for GhostCollection and GhostData (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// A collection of ghost types. `collection` below is the GhostCollection.
struct GhostCollection {
  /// The GhostCollection type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved:
  /// null and false.
  static constexpr pm::api::Type<pm::map::GhostCollection> type{};
  /// The List<GhostData>, a managed reference: ghosts.get(collection). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostCollection_ghosts, pm::api::Object> ghosts{};
  /// The GhostData of a type, or null: GetGhostOfType(collection, type). Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostCollection_GetGhostOfType, pm::api::Object(pm::api::Ref<pm::map::GhostCollection>, int32_t)> GetGhostOfType{};
  /// The roll's default weights, a new Dictionary<GhostType, int> (a managed reference) with 1 for every type in the
  /// collection: GetDefaultWeightedGhostList(collection). Read its entries with pm::api::dictionary_entries. Main
  /// thread, any client. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostCollection_GetDefaultWeightedGhostList, pm::api::Object(pm::api::Ref<pm::map::GhostCollection>)>
      GetDefaultWeightedGhostList{};
};

/// One ghost type's data. `data` below is the GhostData.
struct GhostData {
  /// The GhostData type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null
  /// and false.
  static constexpr pm::api::Type<pm::map::GhostData> type{};
  /// The GhostType number (the entry GhostData.type): ghostType.get(data), ghostType.set(data, n).
  /// Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostData_type, int32_t> ghostType{};
  /// The name's localisation key, a managed string: ghostNameLocalisationKey.get(data). Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::GhostData_ghostNameLocalisationKey, pm::api::Object> ghostNameLocalisationKey{};
  /// The first evidence, an EvidenceType number. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostData_evidence1, int32_t> evidence1{};
  /// The second evidence. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostData_evidence2, int32_t> evidence2{};
  /// The third evidence. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostData_evidence3, int32_t> evidence3{};
  /// A new List<EvidenceType> of the three: GetEvidenceList(data). Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostData_GetEvidenceList, pm::api::Object(pm::api::Ref<pm::map::GhostData>)> GetEvidenceList{};
};

/// @}
}  // namespace phasmo
