#pragma once

// PlayerSanity: a player's sanity and its drain. Its Update keeps its real name; the test it applies
// to the candles near the player is a lambda of its compiler-generated closure class, found by shape
// there. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// A player's sanity (Assembly-CSharp, global namespace), a MonoBehaviour on each player. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity{.key = "PlayerSanity", .confidence = pm::sdk::Confidence::Decompiled,
                                                 .name = "PlayerSanity", .assembly = "Assembly-CSharp", .ns = ""};

/// Every frame, the sanity drain in the dark: it takes the first of the candles near the player (the
/// class's List<Candle>) for which CandleIsOn answers true, Enumerable.FirstOrDefault with that test,
/// and with one scales the drain by its StoreItem.tier, as Firelight eases the drain; with none the
/// drain is the dark's own. It reads that list nowhere else. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity_Update{
    .key = "PlayerSanity.Update", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerSanity", .name = "Update"};

/// The class's compiler-generated closure class, "<>c", nested in PlayerSanity, which holds its
/// lambdas. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity_Closures{.key = "PlayerSanity.Closures",
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "PlayerSanity", .name = "<>c",
                                                          .assembly = "Assembly-CSharp"};

inline constexpr const char* PlayerSanity_CandleIsOn_params[] = {"@Candle"};

/// The test Update gives FirstOrDefault (through the cached delegate of the closure class's field for
/// Update): whether the candle is on, its Equipment's own on flag, the same flag Candle.Update tests
/// before it burns the candle down. The closure class's one instance method of this shape. Called with
/// the closure object first. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity_CandleIsOn{
    .key = "PlayerSanity.CandleIsOn", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerSanity.Closures", .type = "bool", .params = PlayerSanity_CandleIsOn_params};

inline constexpr const char* PlayerSanity_int[] = {"int"};
inline constexpr const char* PlayerSanity_float[] = {"float"};

/// Sets the player's insanity to a number of percent, then sends it to the other players. Inferred:
/// from its name.
inline constexpr pm::sdk::EntryDesc PlayerSanity_SetInsanity{.key = "PlayerSanity.SetInsanity",
                                                             .kind = pm::sdk::Kind::Method, .owner = "PlayerSanity",
                                                             .name = "SetInsanity", .params = PlayerSanity_int};

/// Changes the player's sanity: it adds the amount to the insanity, so a positive amount is sanity
/// lost, within what the difficulty allows. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity_ChangeSanity{
    .key = "PlayerSanity.ChangeSanity", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerSanity", .name = "ChangeSanity", .params = PlayerSanity_float};

inline constexpr pm::sdk::Fact PlayerSanity_insanity_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "PlayerSanity.ChangeSanity"}};

/// The player's insanity, 0 to 100 (100 the most insane): the float ChangeSanity adds its amount to and
/// stores, which SetInsanity sets and the truck's monitor shows. SetInsanity's own store cannot serve
/// as the fact: the reader does not prove its register holds `this`. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSanity_insanity{.key = "PlayerSanity.insanity", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "PlayerSanity",
                                                          .type = "float", .facts = PlayerSanity_insanity_facts};

/// The truck's sanity monitor (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HeartRateData{.key = "HeartRateData", .name = "HeartRateData",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// Redraws the monitor: it reads every player's insanity. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HeartRateData_UpdateUI{.key = "HeartRateData.UpdateUI", .kind = pm::sdk::Kind::Method,
                                                           .owner = "HeartRateData", .name = "UpdateUI"};

}  // namespace pm::map

/// @file
/// @brief The game API for sanity (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (false) without touching the game.
namespace phasmo {
/// @addtogroup sanity
/// @{

/// A player's sanity. `sanity` below is the PlayerSanity.
struct PlayerSanity {
  /// Update(sanity). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayerSanity_Update, void(void*)> Update{};
  /// The candle test of Update: CandleIsOn(closure, candle). A mod hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::PlayerSanity_CandleIsOn, bool(void*, void*)> CandleIsOn{};
  /// ChangeSanity(sanity, amount): a positive amount is sanity lost. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayerSanity_ChangeSanity, void(void*, float)> ChangeSanity{};
  /// SetInsanity(sanity, percent): sets the insanity to a number of percent, 0 to 100. Outside a room
  /// nothing is sent. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayerSanity_SetInsanity, void(void*, int32_t)> SetInsanity{};
  /// The insanity, 0 to 100: insanity.get(sanity), insanity.set(sanity, value). Unresolved: 0, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::PlayerSanity_insanity, float> insanity{};
};

/// The truck's sanity monitor. `monitor` below is one.
struct HeartRateData {
  /// UpdateUI(monitor). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HeartRateData_UpdateUI, void(void*)> UpdateUI{};
};

/// @}
}  // namespace phasmo
