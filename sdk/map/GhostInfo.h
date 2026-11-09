#pragma once

// GhostInfo: the contract ghost's identity on every client, a component beside its GhostAI. The type
// and its RPCs keep their real names; its traits field is obfuscated and found by shape, and the two
// types inside the traits are found within it (sdk/internals/mapping.md, "within" and "sameFieldAs"). Notes read
// from decompiled code are from build 25811160. sdk/systems/ghost-types.md ties it to the Mimic.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* GhostInfo_SyncValues_params[] = {"@GhostTraits"};
inline constexpr const char* GhostInfo_SyncValuesNetworked_params[] = {"int", "int", "bool", "int", "int",
                                                                       "bool", "int", "int", "bool", "PhotonMessageInfo"};
// Both facts read GhostInfo through another object, so each matches an access at the offset through
// any register. ForceRespawnGhostOrb reads the behaviour type and nothing at the identity's offset, so
// it decides alone; the Mimic's copy stores the behaviour type but also stores at the identity's offset
// elsewhere, so alone it leaves both (Ambiguous). It stays as a check that the copy writes this field.
inline constexpr pm::sdk::Fact GhostInfo_behaviourType_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostController.MimicCopy"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "EvidenceController.ForceRespawnGhostOrb"}};
/// The ghost's info (Assembly-CSharp, global namespace), a MonoBehaviourPun. GhostAI holds it
/// (GhostAI.info). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo{.key = "GhostInfo", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "GhostInfo", .assembly = "Assembly-CSharp", .ns = ""};

/// The ghost's traits as this client knows them: the class's one GhostTraits field. Its first field
/// is the identity, the second the behaviour type; GhostAI.Init copies the host's traits in whole,
/// SyncValuesNetworked writes the one type it receives to both, and the Mimic's copy changes the
/// behaviour type here, on the host only. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_traits{.key = "GhostInfo.traits", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "GhostInfo",
                                                     .type = "@GhostTraits"};

/// The ghost's identity, a GhostType: what it is, as the journal guess is judged and the results name
/// it. The first GhostType of traits, placed in GhostInfo. Found as the same GhostTraits field as
/// GhostController.ghostType, whose fact decides it: no method an entry can name reads GhostInfo's
/// identity alone (the end of contract's check of the journal guess, in ExitLevel, is a
/// compiler-generated coroutine), and every other GhostInfo reader copies both types. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_identityType{.key = "GhostInfo.identityType",
                                                           .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostInfo",
                                                           .type = "@GhostType", .within = "GhostInfo.traits",
                                                           .sameFieldAs = "GhostController.ghostType"};

/// The ghost's behaviour type, a GhostType: whose type-specific rules its AI follows. The Mimic's copy
/// changes it, on the host; Custom Ghosts sets it to None for a custom ghost. The second GhostType of
/// traits, placed in GhostInfo. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_behaviourType{.key = "GhostInfo.behaviourType",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "GhostInfo",
                                                            .type = "@GhostType",
                                                            .facts = GhostInfo_behaviourType_facts,
                                                            .within = "GhostInfo.traits"};

// Found as the same GhostTraits field as GhostController.givenEvidence, whose fact decides it: SyncEvidence
// stores the list at that place, but not through a register the resolver can prove holds this.
/// The evidence the ghost gives in this contract, as the difficulty left it, on every client: a
/// List<EvidenceType>, the first of the traits' two evidence lists, placed in GhostInfo. SyncEvidence makes
/// it anew from the numbers it receives; the host's own copy comes with the traits from
/// GhostController.givenEvidence through GhostAI.Init first. Empty when the difficulty gives none.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_givenEvidence{.key = "GhostInfo.givenEvidence",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "GhostInfo",
                                                            .type = "List<@EvidenceType>",
                                                            .within = "GhostInfo.traits",
                                                            .sameFieldAs = "GhostController.givenEvidence"};

/// The host's sync of the traits (StartGhost calls it on the master): an RPC to every client of
/// SyncValuesNetworked with the traits' behaviour type as the ghost's type and six more of their
/// values, then an RPC of SyncEvidence with the given evidence: an int[] of LevelValues.difficulty's
/// evidenceGiven numbers, the first that many of the traits' givenEvidence. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_SyncValues{.key = "GhostInfo.SyncValues", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostInfo", .name = "SyncValues",
                                                         .params = GhostInfo_SyncValues_params};

/// The Photon RPC every client runs from SyncValues (it checks the sender first): it writes the type it
/// receives (its first argument) to both the identity and the behaviour type, sets the ghost's room,
/// and asks GhostController.GetFullEvidence for that type's three evidence. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_SyncValuesNetworked{
    .key = "GhostInfo.SyncValuesNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostInfo", .name = "SyncValuesNetworked",
    .params = GhostInfo_SyncValuesNetworked_params};

/// The Photon RPC every client runs, once, from SyncValues: it stores the given evidence and passes
/// the traits to EvidenceController.SpawnAllGhostTypeEvidence, which on the master spawns the ghost
/// orb when the evidence holds orbs or the behaviour type is the Mimic. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_SyncEvidence{.key = "GhostInfo.SyncEvidence",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostInfo", .name = "SyncEvidence"};

/// The ghost's name, as the contract gives it: the one string of traits, placed in GhostInfo.
/// PhraseListenerController.OnPhraseRecognized compares a recognised
/// phrase with it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInfo_name{.key = "GhostInfo.name", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "GhostInfo", .type = "string",
                                                   .within = "GhostInfo.traits"};

/// The ghost's favourite room, a LevelRoom: GhostInfo's one field of that type. Inferred: from the
/// field's type and the ghost's documented favourite room.
inline constexpr pm::sdk::EntryDesc GhostInfo_favouriteRoom{.key = "GhostInfo.favouriteRoom",
                                                            .kind = pm::sdk::Kind::Field, .owner = "GhostInfo",
                                                            .type = "@LevelRoom"};

}  // namespace pm::map

/// @file
/// @brief The game API for GhostInfo (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game. The traits field has no wrapper: it is a struct holding managed references.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The contract ghost's identity on every client. `info` below is the GhostInfo object.
struct GhostInfo {
  /// The GhostInfo type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null
  /// and false.
  static constexpr pm::api::Type<pm::map::GhostInfo> type{};
  /// The identity, a GhostType number: identityType.get(info), identityType.set(info, n). Unresolved:
  /// reads 0 (Spirit, so check use::resolved()), writes nothing.
  static constexpr pm::api::Field<pm::map::GhostInfo_identityType, int32_t> identityType{};
  /// The behaviour type, a GhostType number: behaviourType.get(info), behaviourType.set(info, n).
  /// Unresolved: reads 0 (Spirit, so check use::resolved()), writes nothing.
  static constexpr pm::api::Field<pm::map::GhostInfo_behaviourType, int32_t> behaviourType{};
  /// The given evidence, a List<EvidenceType>: givenEvidence.get(info). Read it as
  /// pm::api::Array<int32_t>{pm::api::List{list}.items()}, indexes below the List's size() only.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostInfo_givenEvidence, void*> givenEvidence{};
  /// The host's sync of the traits to every client: SyncValues(info, traits), `traits` pointing at a
  /// GhostTraits (passed by address, as the x64 convention passes a struct). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostInfo_SyncValues, void(void*, const void*)> SyncValues{};
  /// The RPC's body on this client: SyncValuesNetworked(info, type, ...seven more values..., message),
  /// `message` pointing at a PhotonMessageInfo. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostInfo_SyncValuesNetworked,
                                   void(void*, int32_t, int32_t, bool, int32_t, int32_t, bool, int32_t, int32_t,
                                        bool, void*)>
      SyncValuesNetworked{};
  /// The ghost's name, a System.String: name.get(info). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostInfo_name, void*> name{};
  /// Its favourite room, a LevelRoom: favouriteRoom.get(info). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostInfo_favouriteRoom, void*> favouriteRoom{};
};

/// @}
}  // namespace phasmo
