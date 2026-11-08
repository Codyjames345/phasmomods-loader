#pragma once

// The ghost-type and evidence enums, and the traits struct that carries a ghost's type. All three
// names are obfuscated, so they are found by shape; their value names are real. Notes read from
// decompiled code are from build 25811160. sdk/systems/ghost-types.md ties them together.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* EvidenceType_values[] = {"EMF", "SpiritBox", "ultraViolet", "GhostOrb",
                                                      "GhostWritingBook", "Temperature", "DotsProjector"};
// Two evidence lists, a string and three bools: JournalController, the only other type with two
// evidence lists, has two bools.
inline constexpr const char* GhostTraits_fieldTypes[] = {"List<@EvidenceType>", "List<@EvidenceType>", "string",
                                                         "bool", "bool", "bool"};
inline constexpr const char* GhostType_values[] = {"Spirit", "Mimic", "None"};

/// The evidence types (Assembly-CSharp, global namespace, name obfuscated): None 0, EMF 1, SpiritBox
/// 2, ultraViolet 3, GhostOrb 4, GhostWritingBook 5, Temperature 6, DotsProjector 7. GhostData's three
/// evidence fields and the lists in GhostTraits hold these numbers. The host reads the enum whole at
/// startup, as it does GhostType: custom evidence is numbered from one past its largest value.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceType{.key = "EvidenceType", .confidence = pm::sdk::Confidence::Decompiled,
                                                 .assembly = "Assembly-CSharp", .ns = "",
                                                 .values = EvidenceType_values};

/// No evidence: an empty evidence slot. GhostData.GetEvidenceList adds it like any other. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceType_None{.key = "EvidenceType.None", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "EvidenceType", .name = "None"};

/// EMF level 5. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EvidenceType_EMF{.key = "EvidenceType.EMF", .kind = pm::sdk::Kind::EnumValue,
                                                     .owner = "EvidenceType", .name = "EMF"};

/// Spirit Box. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EvidenceType_SpiritBox{.key = "EvidenceType.SpiritBox",
                                                           .kind = pm::sdk::Kind::EnumValue,
                                                           .owner = "EvidenceType", .name = "SpiritBox"};

/// Ultraviolet (fingerprints and footprints). Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EvidenceType_ultraViolet{.key = "EvidenceType.ultraViolet",
                                                             .kind = pm::sdk::Kind::EnumValue,
                                                             .owner = "EvidenceType", .name = "ultraViolet"};

/// Ghost Orbs. EvidenceController.SpawnAllGhostTypeEvidence spawns the orb when the given evidence
/// holds this value, or when the ghost's behaviour type is the Mimic. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceType_GhostOrb{.key = "EvidenceType.GhostOrb",
                                                          .kind = pm::sdk::Kind::EnumValue,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "EvidenceType", .name = "GhostOrb"};

/// Ghost Writing. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EvidenceType_GhostWritingBook{.key = "EvidenceType.GhostWritingBook",
                                                                  .kind = pm::sdk::Kind::EnumValue,
                                                                  .owner = "EvidenceType", .name = "GhostWritingBook"};

/// Freezing Temperatures. SpawnAllGhostTypeEvidence sets a flag of the EvidenceController when the
/// given evidence holds it. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceType_Temperature{.key = "EvidenceType.Temperature",
                                                             .kind = pm::sdk::Kind::EnumValue,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "EvidenceType", .name = "Temperature"};

/// D.O.T.S. Projector. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EvidenceType_DotsProjector{.key = "EvidenceType.DotsProjector",
                                                               .kind = pm::sdk::Kind::EnumValue,
                                                               .owner = "EvidenceType", .name = "DotsProjector"};

/// A ghost's traits (Assembly-CSharp, global namespace), a struct that GhostController (the host's
/// roll) and GhostInfo (every client's copy) each embed whole. It begins with
/// two GhostType fields: the ghost's identity (what it is) and its behaviour type (whose rules its AI
/// follows), then the given evidence list and the full evidence list. GhostAI.Init and
/// GhostInfo.SyncValues take it by value. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostTraits{.key = "GhostTraits", .confidence = pm::sdk::Confidence::Decompiled,
                                                .assembly = "Assembly-CSharp", .ns = "",
                                                .fieldTypes = GhostTraits_fieldTypes};

/// The ghost types, an enum nested in GhostTraits (name obfuscated, value names real): Spirit 0 to
/// Thaye 23, None 24, then Gallu 25 to Deildegast 30. Nothing in the game counts the types but the
/// Mimic's list (GhostController.BuildMimicCopies), which takes every number below the count of the
/// enum's names. The host reads the enum whole from this entry's type at startup, every value with its
/// name (sdk/runtime/game_enums.h): custom ghost types are numbered from one past its largest value
/// and the vanilla ghosts' content ids come from the names, so a type the game adds needs no change.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType{.key = "GhostType", .confidence = pm::sdk::Confidence::Decompiled,
                                              .owner = "GhostTraits", .values = GhostType_values};

/// No type. The roll keeps a Custom difficulty's ghost type unless it is None (Random); the Mimic's
/// list leaves it out; Custom Ghosts gives a custom ghost this behaviour type so that no type's own
/// rules run for it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_None{.key = "GhostType.None", .kind = pm::sdk::Kind::EnumValue,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "GhostType", .name = "None"};

/// The Mimic. GhostController.AttemptGhostTypeStateChange runs the Mimic's copy when the ghost's
/// identity is this type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Mimic{.key = "GhostType.Mimic", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostType", .name = "Mimic"};

/// The Yurei. The incense's Yurei trap (WhiteSage.YureiTrap) compares the ghost's behaviour type
/// with it before it stops the ghost wandering. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Yurei{.key = "GhostType.Yurei", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostType", .name = "Yurei"};

/// The Onryo (16). FireSource's count of a ghost's blow-outs (FireSource.CountGhostBlowOut) compares the
/// ghost's behaviour type with it before it adds to GhostAI.flamesBlownOut, and the idle state
/// (GhostIdleState.Execute) starts a hunt once that count passes 2. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Onryo{.key = "GhostType.Onryo", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostType", .name = "Onryo"};

/// The Mare's type number. The light chance (GhostActivity.lightChance) keeps a Mare from flickering.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Mare{.key = "GhostType.Mare", .kind = pm::sdk::Kind::EnumValue,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "GhostType", .name = "Mare"};

/// The Banshee's type number. The hunt's look for a player in sight (GhostHuntState.SightPick) finds
/// nobody for it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Banshee{.key = "GhostType.Banshee", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostType", .name = "Banshee"};

/// The Deogen. The hunt's sight test (GhostHuntState.Sees) passes for every player while the ghost's
/// behaviour type is this, unless asked to skip it; the hunt's voice and electronics tests compare with
/// it too. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Deogen{.key = "GhostType.Deogen", .kind = pm::sdk::Kind::EnumValue,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "GhostType", .name = "Deogen"};

/// The Thaye. The idle state's hunt insanity (GhostIdleState.HuntInsanity) scales the chance it
/// works out by a further factor while the behaviour type is this. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Thaye{.key = "GhostType.Thaye", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostType", .name = "Thaye"};

/// The Goryo. The wander's destination picker (GhostWanderState.PickDestination) skips the
/// long-roam roll for a ghost whose behaviour type is this, so a Goryo never roams far and its
/// wander never changes its favourite room. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Goryo{.key = "GhostType.Goryo", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostType", .name = "Goryo"};

/// The Poltergeist. The hunt's throw (the hunt state's update) holds back half its throws with a
/// roll, except for a hunt whose ghost type is this one. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostType_Poltergeist{.key = "GhostType.Poltergeist",
                                                          .kind = pm::sdk::Kind::EnumValue,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "GhostType", .name = "Poltergeist"};

}  // namespace pm::map

/// @file
/// @brief The game API for the ghost-type and evidence enums (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. An unresolved value logs "<key> is not available in this game
/// build" once and gives 0, which is a real value of both enums (Spirit, and no evidence): check `use::resolved()`
/// before relying on one.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The evidence types: the numbers GhostData's evidence fields and the evidence lists hold.
namespace EvidenceType {
/// The EvidenceType enum type: klass(), is(obj), type_object() and make_array(n). Unresolved: null
/// and false.
inline constexpr pm::api::Type<pm::map::EvidenceType> type{};
/// An empty slot: None(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_None> None{};
/// EMF level 5: EMF(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_EMF> EMF{};
/// Spirit Box: SpiritBox(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_SpiritBox> SpiritBox{};
/// Ultraviolet: ultraViolet(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_ultraViolet> ultraViolet{};
/// Ghost Orbs: GhostOrb(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_GhostOrb> GhostOrb{};
/// Ghost Writing: GhostWritingBook(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_GhostWritingBook> GhostWritingBook{};
/// Freezing Temperatures: Temperature(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_Temperature> Temperature{};
/// D.O.T.S. Projector: DotsProjector(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EvidenceType_DotsProjector> DotsProjector{};
}  // namespace EvidenceType

/// The ghost types: the numbers GhostData.type, the Mimic's list and the traits' two type fields hold.
namespace GhostType {
/// The GhostType enum type: klass(), is(obj), type_object() and make_array(n). Unresolved: null and
/// false.
inline constexpr pm::api::Type<pm::map::GhostType> type{};
/// No type: None(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_None> None{};
/// The Mimic: Mimic(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_Mimic> Mimic{};
/// The Yurei: Yurei(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_Yurei> Yurei{};
/// The Onryo: Onryo(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_Onryo> Onryo{};
/// The Goryo: Goryo(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_Goryo> Goryo{};
/// The Poltergeist: Poltergeist(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostType_Poltergeist> Poltergeist{};
}  // namespace GhostType

/// A ghost's traits, the struct GhostController and GhostInfo embed.
struct GhostTraits {
  /// The GhostTraits type: klass(), is(obj), type_object() and make_array(n). Unresolved: null and
  /// false.
  static constexpr pm::api::Type<pm::map::GhostTraits> type{};
};

/// @}
}  // namespace phasmo
