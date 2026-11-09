#pragma once

// FireSource: a flame that burns, goes out and is blown out: each flame of a Firelight candle, of a
// Firelight lantern and of the level's own candles. Its flame, its light and its blow-out keep their
// real names; the equipment it belongs to and whether it burns are obfuscated, so they are found by
// shape. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// A flame (Assembly-CSharp, global namespace), a MonoBehaviourPun. Its Start adds it to the level's
/// LevelController.fireSources. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource{.key = "FireSource", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "FireSource", .assembly = "Assembly-CSharp", .ns = ""};

/// The flame's own GameObject, which holds what it shows: Light switches it on (GameObject.SetActive)
/// and Extinguish off. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_flame{.key = "FireSource.flame", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "FireSource", .name = "flame"};

/// The flame's own Light: Light enables it (Behaviour.set_enabled) and Extinguish disables it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_flameLight{.key = "FireSource.flameLight", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "FireSource", .name = "flameLight"};

/// The Equipment the flame belongs to: a Candle for a Firelight's flames. BlowOutFire puts out the
/// whole Candle through it (Candle.Extinguish). The class's one Equipment field. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_equipment{
    .key = "FireSource.equipment", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FireSource", .type = "@Equipment"};

/// Shows the flame burning: switches its Light and its flame's GameObject on, plays its particles, sets
/// `isOn` and warms its heat spot. Candle.UseNetworked calls it for each of a candle's flames when the
/// candle is lit, on every client. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_Light{.key = "FireSource.Light", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "FireSource", .name = "Light"};

/// Shows the flame out: switches its flame's GameObject and its Light off, stops its particles, clears
/// `isOn` and cools its heat spot. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_Extinguish{.key = "FireSource.Extinguish", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "FireSource", .name = "Extinguish"};

// Of the class's three bools, Light stores true in this one alone and Extinguish false. (BlowOutFire
// reads it too, but through a register the resolver cannot prove holds this.)
inline constexpr pm::sdk::Fact FireSource_isOn_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "FireSource.Light", .value = 1},
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "FireSource.Extinguish", .value = 0}};

/// Whether the flame burns, on every client: Light sets it and Extinguish clears it, Candle.UseNetworked
/// sets or clears it on each of a candle's flames as the candle is lit or goes out, and BlowOutFire
/// does nothing while it is clear. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_isOn{
    .key = "FireSource.isOn", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FireSource", .type = "bool", .facts = FireSource_isOn_facts};

inline constexpr const char* FireSource_BlowOutFire_params[] = {"bool"};

/// Blows the flame out, `byGhost` the ghost's doing: nothing while it is not burning; by the ghost, an
/// EMF reading where it burns first; then it puts out the Equipment it belongs to (a Candle's
/// Extinguish, a Lighter's own blow-out; with none, its own Extinguish) and sends BlowOutFireNetworked
/// to every client. The ghost calls it on the master. Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_BlowOutFire{
    .key = "FireSource.BlowOutFire", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FireSource", .name = "BlowOutFire", .params = FireSource_BlowOutFire_params};

inline constexpr const char* FireSource_IgniteFireNetworked_params[] = {"bool", "PhotonMessageInfo"};
// BlowOutFire calls two void() methods of its class, Extinguish and the count of the Onryo's
// blow-outs; only the count compares the ghost's type with the Onryo.
inline constexpr pm::sdk::Fact FireSource_CountGhostBlowOut_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "FireSource.BlowOutFire"},
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Onryo"}};

/// Counts a blow-out by the ghost for the Onryo's hunt (called by BlowOutFire). When the ghost's
/// behaviour type is the Onryo (16) it adds one to GhostAI.flamesBlownOut. Then, if the count is 3
/// while the ghost is hunting (GhostAI.isHunting) or its hunts are held off (GhostAI.huntsHeldOff),
/// it sets the count back to 0, so that third flame is wasted. The hunt itself starts in the idle
/// state (GhostIdleState.Execute). Decompiled.
inline constexpr pm::sdk::EntryDesc FireSource_CountGhostBlowOut{
    .key = "FireSource.CountGhostBlowOut", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FireSource", .type = "void", .facts = FireSource_CountGhostBlowOut_facts};

/// Lights the flame, on the client that lights it; its argument says whether the ghost lights it.
/// Inferred: from the method's name and its networked RPC.
inline constexpr pm::sdk::EntryDesc FireSource_IgniteFire{.key = "FireSource.IgniteFire", .kind = pm::sdk::Kind::Method,
                                                          .owner = "FireSource", .name = "IgniteFire",
                                                          .params = FireSource_BlowOutFire_params};

/// The RPC that lights the flame on every client, its first argument whether the ghost lit it.
/// Inferred: from the method's name.
inline constexpr pm::sdk::EntryDesc FireSource_IgniteFireNetworked{
    .key = "FireSource.IgniteFireNetworked", .kind = pm::sdk::Kind::Method, .owner = "FireSource",
    .name = "IgniteFireNetworked", .params = FireSource_IgniteFireNetworked_params};

}  // namespace pm::map

/// @file
/// @brief The game API for flames (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null, false) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// A flame. `fire` below is the FireSource.
struct FireSource {
  /// The FireSource type. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::FireSource> type{};
  /// Its flame's GameObject: flame.get(fire). Unresolved: null.
  static constexpr pm::api::Field<pm::map::FireSource_flame, void*> flame{};
  /// Its Light: flameLight.get(fire). Unresolved: null.
  static constexpr pm::api::Field<pm::map::FireSource_flameLight, void*> flameLight{};
  /// The Equipment it belongs to: equipment.get(fire). Unresolved: null.
  static constexpr pm::api::Field<pm::map::FireSource_equipment, void*> equipment{};
  /// Whether it burns: isOn.get(fire). Unresolved: false.
  static constexpr pm::api::Field<pm::map::FireSource_isOn, bool> isOn{};
  /// BlowOutFire(fire, byGhost). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::FireSource_BlowOutFire, void(void*, bool)> BlowOutFire{};
  /// IgniteFire(fire, byGhost). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::FireSource_IgniteFire, void(void*, bool)> IgniteFire{};
  /// The RPC's body on this client: IgniteFireNetworked(fire, byGhost, info), `info` pointing at a
  /// PhotonMessageInfo. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::FireSource_IgniteFireNetworked, void(void*, bool, void*)>
      IgniteFireNetworked{};
};

/// @}
}  // namespace phasmo
