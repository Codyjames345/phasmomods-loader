#pragma once

// A contract's objectives and the player's payout: the objective entries LevelValues keeps, the
// ObjectiveValues each names (with its money reward), and SingleplayerProfile, whose AddXPAndFunds the
// reward screen pays the experience through. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// One objective's settings, a ScriptableObject (Assembly-CSharp, global namespace). Inferred: from its
/// name and its fields' names.
inline constexpr pm::sdk::EntryDesc ObjectiveValues{.key = "ObjectiveValues", .name = "ObjectiveValues",
                                                    .assembly = "Assembly-CSharp", .ns = ""};

/// The money the objective pays when completed. LevelValues.GetInvestigationBonusReward sums it over the
/// main objective and the side objectives. Decompiled.
inline constexpr pm::sdk::EntryDesc ObjectiveValues_moneyReward{.key = "ObjectiveValues.moneyReward",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "ObjectiveValues", .name = "moneyReward",
                                                                .type = "int"};

inline constexpr const char* Objective_fieldTypes[] = {"@ObjectiveValues", "string", "int"};

// Its name is obfuscated, so it is found by its fields.
/// One of the contract's objectives (Assembly-CSharp, global namespace): the ObjectiveValues it is, its
/// text and its progress. LevelValues keeps the main objective, a list of the side objectives and one more.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Objective{.key = "Objective", .confidence = pm::sdk::Confidence::Decompiled,
                                              .assembly = "Assembly-CSharp", .ns = "",
                                              .fieldTypes = Objective_fieldTypes};

/// The objective's settings: Objective's one ObjectiveValues field. Decompiled.
inline constexpr pm::sdk::EntryDesc Objective_values{.key = "Objective.values", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Objective",
                                                     .type = "@ObjectiveValues"};

/// The local player's saved progress in single player and in an online game alike (Assembly-CSharp,
/// global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc SingleplayerProfile{.key = "SingleplayerProfile", .name = "SingleplayerProfile",
                                                        .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* SingleplayerProfile_AddXPAndFunds_params[] = {"int", "int"};

/// The reward screen's experience pay (every call to it is RewardManager's): stores the experience and the
/// money on the profile card, then adds the experience to the player's level at once and saves it. The card's
/// money field is never read, so the money argument pays nothing; the screen's money is saved by
/// RewardManager.Results (sdk/systems/contract.md). Decompiled.
inline constexpr pm::sdk::EntryDesc SingleplayerProfile_AddXPAndFunds{
    .key = "SingleplayerProfile.AddXPAndFunds", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SingleplayerProfile", .name = "AddXPAndFunds",
    .params = SingleplayerProfile_AddXPAndFunds_params};


inline constexpr const char* ES3IO_CommitBackup_params[] = {"ES3Settings"};

/// EasySave3's file helpers (EasySave3 assembly, ES3Internal namespace; real names). Decompiled.
inline constexpr pm::sdk::EntryDesc ES3IO{.key = "ES3IO", .confidence = pm::sdk::Confidence::Decompiled, .name = "ES3IO",
                                          .assembly = "EasySave3", .ns = "ES3Internal"};

/// CommitBackup(settings): the one step of every EasySave3 write that replaces the real save. Decompiled: for a file
/// save it moves the freshly written temporary file (the save's path plus a suffix) over the save, deleting the old
/// one; for a PlayerPrefs save it copies the temporary key over the real one and saves PlayerPrefs. All 13 of its
/// callers are EasySave3's own writers, which write the temporary copy first. So while it is skipped, nothing the game
/// saves reaches the save: the temporary copy is all that changes, and the next write rewrites it from the real save.
inline constexpr pm::sdk::EntryDesc ES3IO_CommitBackup{
    .key = "ES3IO.CommitBackup", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "ES3IO", .name = "CommitBackup", .params = ES3IO_CommitBackup_params};

}  // namespace pm::map

/// @file
/// @brief The game API for the contract's rewards (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// One objective's settings. `values` below is the ObjectiveValues.
struct ObjectiveValues {
  /// The money it pays: moneyReward.get(values). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::ObjectiveValues_moneyReward, int32_t> moneyReward{};
};

/// One of the contract's objectives. `objective` below is the Objective.
struct Objective {
  /// Its ObjectiveValues: values.get(objective). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Objective_values, void*> values{};
};

/// EasySave3's file helpers.
struct ES3IO {
  /// CommitBackup(settings): replaces the real save with the copy just written. A mod hooks it to keep the save as
  /// it is (the dev menu's dev session); skipping `original` leaves the save untouched. Main thread. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::ES3IO_CommitBackup, void(void*)> CommitBackup{};
};

/// The local player's saved progress. `profile` below is the SingleplayerProfile.
struct SingleplayerProfile {
  /// AddXPAndFunds(profile, xp, money): adds xp to the player's level and saves it; money is stored on the
  /// card and pays nothing (see the entry). A mod hooks it to change the experience a contract pays.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SingleplayerProfile_AddXPAndFunds, void(void*, int32_t, int32_t)>
      AddXPAndFunds{};
};

/// @}
}  // namespace phasmo
