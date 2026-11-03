#pragma once

// Candle: the Firelight equipment (a candle of tier 1 or 2, the tier 3 lantern) and the level's own
// candles, with their flames (CandleObject), and Candlebra, the level's candelabras. Every member used
// keeps its real name. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "UnityRendering.h"

namespace pm::map {

/// A candle or lantern (Assembly-CSharp, global namespace), an Equipment: Firelight from the store,
/// whose Equipment.storeItem gives its tier, and the level's own candles, which have none. Decompiled.
inline constexpr pm::sdk::EntryDesc Candle{.key = "Candle", .confidence = pm::sdk::Confidence::Decompiled,
                                           .name = "Candle", .assembly = "Assembly-CSharp", .ns = ""};

/// The candle's flames, a CandleObject[]: Candle.UseNetworked lights or puts out each one's
/// FireSource and sets its wax, and its Update melts the wax. Decompiled.
inline constexpr pm::sdk::EntryDesc Candle_candleObjects{
    .key = "Candle.candleObjects", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Candle", .name = "candleObjects"};

/// The wax's glow while the candle burns, a Color: UseNetworked sets it as the _EmissionColor of every
/// wax renderer's material when the candle is lit, and black when it goes out, on every client. The
/// lantern's wax has no such glow of its own. Decompiled.
inline constexpr pm::sdk::EntryDesc Candle_candleEmissionColor{
    .key = "Candle.candleEmissionColor", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Candle", .name = "candleEmissionColor"};

/// The seconds the candle has left to burn: Update takes each frame's time off it while the candle is
/// on, and the candle's owner puts the candle out once it is below zero. Decompiled.
inline constexpr pm::sdk::EntryDesc Candle_candleDuration{
    .key = "Candle.candleDuration", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Candle", .name = "candleDuration"};

/// One flame of a candle with its wax (Assembly-CSharp), a class nested in Candle. Decompiled.
inline constexpr pm::sdk::EntryDesc CandleObject{.key = "CandleObject", .confidence = pm::sdk::Confidence::Decompiled,
                                                 .owner = "Candle", .name = "CandleObject",
                                                 .assembly = "Assembly-CSharp"};

/// The flame's wax, a MeshRenderer[]: the renderers whose material UseNetworked gives the candle's
/// glow and Update melts. Decompiled.
inline constexpr pm::sdk::EntryDesc CandleObject_candleRend{
    .key = "CandleObject.candleRend", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "CandleObject", .name = "candleRend"};

/// The flame itself, its FireSource, which UseNetworked lights and puts out. Decompiled.
inline constexpr pm::sdk::EntryDesc CandleObject_fireSource{
    .key = "CandleObject.fireSource", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "CandleObject", .name = "fireSource"};

/// A candelabra of the level (Assembly-CSharp, global namespace), a MonoBehaviour holding candles.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Candlebra{.key = "Candlebra", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "Candlebra", .assembly = "Assembly-CSharp", .ns = ""};

/// Its candles, a Candle[]: its Start listens to each one's lighting and putting out and sets its
/// own light by how many burn. Decompiled.
inline constexpr pm::sdk::EntryDesc Candlebra_allCandles{
    .key = "Candlebra.allCandles", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Candlebra", .name = "allCandles"};

/// Its own Light, which Start sets by the share of its candles that burn. Decompiled.
inline constexpr pm::sdk::EntryDesc Candlebra_lightSource{
    .key = "Candlebra.lightSource", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Candlebra", .name = "lightSource"};

}  // namespace pm::map

// The game API for candles (sdk/api/game_api.h). Every member is called on Unity's main thread. One that
// did not resolve in the running game build logs "<key> is not available in this game build" once and
// returns its type's default (null, zero) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// A candle or lantern. `candle` below is the Candle.
struct Candle {
  /// The Candle type: is(obj) tells a candle. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Candle> type{};
  /// Its flames, a CandleObject[]: candleObjects.get(candle), read as a pm::api::RefArray. Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::Candle_candleObjects, void*> candleObjects{};
  /// The wax's glow while lit: candleEmissionColor.get(candle). Unresolved: all zero.
  static constexpr pm::api::Field<pm::map::Candle_candleEmissionColor, Color> candleEmissionColor{};
  /// Seconds left to burn: candleDuration.get(candle), .set(candle, s). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Candle_candleDuration, float> candleDuration{};
};

/// One flame of a candle. `object` below is the CandleObject.
struct CandleObject {
  /// Its wax, a MeshRenderer[]: candleRend.get(object), read as a pm::api::RefArray. Unresolved: null.
  static constexpr pm::api::Field<pm::map::CandleObject_candleRend, void*> candleRend{};
  /// Its FireSource: fireSource.get(object). Unresolved: null.
  static constexpr pm::api::Field<pm::map::CandleObject_fireSource, void*> fireSource{};
};

/// A candelabra. `candelabra` below is the Candlebra.
struct Candlebra {
  /// The Candlebra type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Candlebra> type{};
  /// Its candles, a Candle[]: allCandles.get(candelabra), read as a pm::api::RefArray. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Candlebra_allCandles, void*> allCandles{};
  /// Its Light: lightSource.get(candelabra). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Candlebra_lightSource, void*> lightSource{};
};

/// @}
}  // namespace phasmo
