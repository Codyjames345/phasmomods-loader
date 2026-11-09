#pragma once

// Equipment: the base class of every item a player carries, sets down and uses (a Candle among them),
// and StoreItem, the store's description of an item, with its tier. The fields used here are
// obfuscated but for the tier, so they are found by shape. Notes read from decompiled code are from
// build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// A carried item (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment{.key = "Equipment", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "Equipment", .assembly = "Assembly-CSharp", .ns = ""};

/// The store's item this is, a StoreItem, whose tier PlayerSanity.Update reads from the first lit
/// candle near the player and Candle.Update reads to leave the lantern's wax unmelted. Null for an
/// item the store does not sell, such as the level's own candles. The class's one StoreItem field.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment_storeItem{
    .key = "Equipment.storeItem", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Equipment", .type = "@StoreItem"};

inline constexpr const char* Equipment_PlaceNetworked_params[] = {"Vector3", "Quaternion", "float", "bool", "int",
                                                                  "PhotonMessageInfo"};

/// The RPC that sets the item down where it was placed, on every client: it sets `placed`, takes it out
/// of the player's hand and lays it at the position and rotation it is given. Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment_PlaceNetworked{
    .key = "Equipment.PlaceNetworked", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Equipment", .name = "PlaceNetworked", .params = Equipment_PlaceNetworked_params};

inline constexpr const char* Equipment_GrabNetworked_params[] = {"PhotonMessageInfo"};

/// The RPC of a player picking the item up, on every client: it clears `placed` and frees the item
/// from a socket it was in. Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment_GrabNetworked{
    .key = "Equipment.GrabNetworked", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Equipment", .name = "GrabNetworked", .params = Equipment_GrabNetworked_params};

// Of the class's many bools, PlaceNetworked stores true in this one alone and GrabNetworked false.
inline constexpr pm::sdk::Fact Equipment_placed_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "Equipment.PlaceNetworked", .value = 1},
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "Equipment.GrabNetworked", .value = 0}};

/// Whether the item is set down, on every client: PlaceNetworked sets it and GrabNetworked clears it.
/// Live: Spectral Flame read it in the game on 2026-10-21 (build 25811160).
inline constexpr pm::sdk::EntryDesc Equipment_placed{
    .key = "Equipment.placed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Live,
    .owner = "Equipment", .type = "bool", .facts = Equipment_placed_facts};

/// What the store sells (Assembly-CSharp, global namespace), a ScriptableObject. Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItem{.key = "StoreItem", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "StoreItem", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* StoreItemTier_values[] = {"One", "Two", "Three"};

// Found by its values' names.
/// The tier of a store item, an obfuscated enum (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItemTier{.key = "StoreItemTier", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .assembly = "Assembly-CSharp", .ns = "",
                                                  .values = StoreItemTier_values};

/// Tier I. Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItemTier_One{.key = "StoreItemTier.One", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "StoreItemTier", .name = "One"};

/// Tier II. Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItemTier_Two{.key = "StoreItemTier.Two", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "StoreItemTier", .name = "Two"};

/// Tier III: for Firelight, the lantern, whose wax Candle.Update leaves unmelted. Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItemTier_Three{.key = "StoreItemTier.Three", .kind = pm::sdk::Kind::EnumValue,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "StoreItemTier", .name = "Three"};

/// The item's tier, a StoreItemTier stored as an int. PlayerSanity.Update scales the sanity drain by
/// it, one factor for each of the three tiers, and Candle.Update leaves the wax of a candle of tier
/// Three (the lantern) unmelted. Decompiled.
inline constexpr pm::sdk::EntryDesc StoreItem_tier{.key = "StoreItem.tier", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "StoreItem", .name = "tier", .type = "@StoreItemTier"};

// The candle test of PlayerSanity reads one bool of Equipment: its owner is the closure class, so the
// fact matches that offset through any register.
inline constexpr pm::sdk::Fact Equipment_isOn_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "PlayerSanity.CandleIsOn"}};

/// Whether the item is switched on: PlayerSanity.CandleIsOn returns it for a candle, and
/// Candle.Update burns a candle down while it is set. Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment_isOn{.key = "Equipment.isOn", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "Equipment", .type = "bool",
                                                   .facts = Equipment_isOn_facts};

/// A video camera (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc CCTV{.key = "CCTV", .name = "CCTV", .assembly = "Assembly-CSharp", .ns = ""};

/// The truck's monitor (Assembly-CSharp, global namespace): the cameras that are switched on, the kind of
/// camera it shows, and who is viewing it. Decompiled.
inline constexpr pm::sdk::EntryDesc CCTVController{.key = "CCTVController",
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "CCTVController", .assembly = "Assembly-CSharp", .ns = ""};

/// The one CCTVController: the class's one static field of its own type. Null outside a contract. Decompiled.
inline constexpr pm::sdk::EntryDesc CCTVController_instance{
    .key = "CCTVController.instance", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "CCTVController", .type = "@CCTVController"};

/// The players actively viewing the monitor, on every client: the class's one List<Player>.
/// SetActivelyViewingCCTVMonitor sends each change to everyone, and the game takes a dead player out. Decompiled.
inline constexpr pm::sdk::EntryDesc CCTVController_viewers{
    .key = "CCTVController.viewers", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "CCTVController", .type = "List<Player>"};

/// A new List<CCTV> of the switched-on cameras of the kind the monitor shows, which every client knows. A camera joins
/// and leaves the list its kind is drawn from as it is switched on and off. The grid's page is each viewer's own, so
/// this is the closest any other client can come to what a viewer has on screen. Decompiled.
inline constexpr pm::sdk::EntryDesc CCTVController_GetCCTVCamerasByCurrentCameraType{
    .key = "CCTVController.GetCCTVCamerasByCurrentCameraType", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "CCTVController",
    .name = "GetCCTVCamerasByCurrentCameraType"};

/// The item's use: for electronics, switching it on or off, which Equipment.Use sends to every client as
/// UseNetworked(!on) (REA decompile, 2026-10-20). Virtual: each kind of equipment overrides it, so call
/// it with virtual_call. Decompiled.
inline constexpr pm::sdk::EntryDesc Equipment_Use{.key = "Equipment.Use", .kind = pm::sdk::Kind::Method,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Equipment",
                                                  .name = "Use"};

/// A motion sensor (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc MotionSensor{.key = "MotionSensor", .name = "MotionSensor",
                                                 .assembly = "Assembly-CSharp", .ns = ""};

/// A sound sensor (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc SoundSensor{.key = "SoundSensor", .name = "SoundSensor",
                                                .assembly = "Assembly-CSharp", .ns = ""};

/// The EMF reader (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc EMFReader{.key = "EMFReader", .name = "EMFReader", .assembly = "Assembly-CSharp",
                                              .ns = ""};

/// The parabolic microphone (Assembly-CSharp, global namespace), an Equipment. Inferred: from the
/// class's name.
inline constexpr pm::sdk::EntryDesc ParabolicMicrophone{.key = "ParabolicMicrophone", .name = "ParabolicMicrophone",
                                                        .assembly = "Assembly-CSharp", .ns = ""};

/// The sound recorder (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's
/// name.
inline constexpr pm::sdk::EntryDesc SoundRecorder{.key = "SoundRecorder", .name = "SoundRecorder",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// The thermometer (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc Thermometer{.key = "Thermometer", .name = "Thermometer",
                                                .assembly = "Assembly-CSharp", .ns = ""};

/// The spirit box (Assembly-CSharp, global namespace), an Equipment. Inferred: from its use in the
/// game's prefabs.
inline constexpr pm::sdk::EntryDesc EVPRecorder{.key = "EVPRecorder", .name = "EVPRecorder",
                                                .assembly = "Assembly-CSharp", .ns = ""};

/// The D.O.T.S. projector (Assembly-CSharp, global namespace), an Equipment. Inferred: from its prefab.
inline constexpr pm::sdk::EntryDesc IRLightSensor{.key = "IRLightSensor", .name = "IRLightSensor",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// A torch (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc Torch{.key = "Torch", .name = "Torch", .assembly = "Assembly-CSharp", .ns = ""};

/// A crucifix (Assembly-CSharp, global namespace), an Equipment. Inferred: from the class's name.
inline constexpr pm::sdk::EntryDesc Crucifix{.key = "Crucifix", .name = "Crucifix", .assembly = "Assembly-CSharp",
                                             .ns = ""};

inline constexpr const char* Equipment_bool_info[] = {"bool", "PhotonMessageInfo"};

/// The crucifix is used, on every client. A use the game accepts adds one to useCount. Inferred: from
/// its name and Playable Ghost's use since it was built.
inline constexpr pm::sdk::EntryDesc Crucifix_CrucifixUseNetworked{
    .key = "Crucifix.CrucifixUseNetworked", .kind = pm::sdk::Kind::Method, .owner = "Crucifix",
    .name = "CrucifixUseNetworked", .params = Equipment_bool_info};

/// How many times the crucifix has been used. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Crucifix_useCount{.key = "Crucifix.useCount", .kind = pm::sdk::Kind::Field,
                                                      .owner = "Crucifix", .name = "useCount", .type = "int"};

inline constexpr const char* EVPRecorder_int_int[] = {"int", "int"};

/// The spirit box plays an answer about where the ghost is. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc EVPRecorder_PlayLocationSound{
    .key = "EVPRecorder.PlayLocationSound", .kind = pm::sdk::Kind::Method, .owner = "EVPRecorder",
    .name = "PlayLocationSound", .params = EVPRecorder_int_int};

/// The spirit box plays an answer about what the ghost is. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc EVPRecorder_PlayAboutSound{.key = "EVPRecorder.PlayAboutSound",
                                                               .kind = pm::sdk::Kind::Method, .owner = "EVPRecorder",
                                                               .name = "PlayAboutSound",
                                                               .params = EVPRecorder_int_int};

/// The spirit box plays an answer about how hard the contract is. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc EVPRecorder_PlayDifficultySound{
    .key = "EVPRecorder.PlayDifficultySound", .kind = pm::sdk::Kind::Method, .owner = "EVPRecorder",
    .name = "PlayDifficultySound", .params = EVPRecorder_int_int};

/// Incense (Assembly-CSharp, global namespace), an Equipment. Decompiled.
inline constexpr pm::sdk::EntryDesc WhiteSage{.key = "WhiteSage", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "WhiteSage", .assembly = "Assembly-CSharp", .ns = ""};

/// The incense is used, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc WhiteSage_UseNetworked{.key = "WhiteSage.UseNetworked",
                                                           .kind = pm::sdk::Kind::Method, .owner = "WhiteSage",
                                                           .name = "UseNetworked", .params = Equipment_bool_info};

inline constexpr const char* WhiteSage_sensorHandler_params[] = {"GameObject", "Sensor"};
// Five private (GameObject, Sensor) handlers of the incense call TemporarilyStopWander. Each first compares
// the ghost's behaviour type with a one-byte constant, at the same place in its body: one compares it with
// the Yurei (0x0A); the other four's constants (0x5F, -0x34, 0x5D, -0x6E) are no GhostType value (0 to 30 in
// build 25811160).
inline constexpr pm::sdk::Fact WhiteSage_YureiTrap_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.TemporarilyStopWander"},
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Yurei"}};

// Its four look-alikes are the handlers above whose constants name no ghost type.
/// The incense's handler that traps the Yurei: when the ghost's behaviour type is the Yurei it calls
/// GhostAI.TemporarilyStopWander, and it removes itself as a listener of the incense's sensor event.
/// Nothing calls it directly; it is reached through a delegate.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc WhiteSage_YureiTrap{
    .key = "WhiteSage.YureiTrap", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "WhiteSage", .type = "void", .params = WhiteSage_sensorHandler_params,
    .facts = WhiteSage_YureiTrap_facts};

}  // namespace pm::map

/// @file
/// @brief The game API for equipment (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null, false, 0) without touching the
/// game.
namespace phasmo {
/// @addtogroup equipment
/// @{

/// A carried item. `item` below is the Equipment (a Candle, say).
struct Equipment {
  /// The Equipment type. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Equipment> type{};
  /// Its StoreItem, or null: storeItem.get(item). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Equipment_storeItem, void*> storeItem{};
  /// Whether it is set down: placed.get(item). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Equipment_placed, bool> placed{};
  /// Whether it is switched on: isOn.get(item). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Equipment_isOn, bool> isOn{};
  /// Use(item), as the item's class runs it: call Use.virtual_call(item). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Equipment_Use, void(void*)> Use{};
};

/// A video camera.
struct CCTV {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::CCTV> type{};
};

/// The truck's monitor.
struct CCTVController {
  /// The one controller, or null. Reading it makes nothing. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::CCTVController_instance, void*> instance{};
  /// A List<Player> of who is actively viewing the monitor, on every client. Read it as pm::api::List.
  static constexpr pm::api::Field<pm::map::CCTVController_viewers, void*> viewers{};
  /// GetCCTVCamerasByCurrentCameraType(controller): a new List<CCTV> of the switched-on cameras of the kind the monitor
  /// shows. Every client. Unresolved: null.
  static constexpr pm::api::Method<pm::map::CCTVController_GetCCTVCamerasByCurrentCameraType, void*(void*)>
      GetCCTVCamerasByCurrentCameraType{};
};

/// The other kinds of electronic equipment, each mapped by its type only (some with a few methods).
/// Each type: klass(), is(obj), type_object(). Unresolved: null and false. This one is a motion sensor
/// (MotionSensor).
struct MotionSensor {
  static constexpr pm::api::Type<pm::map::MotionSensor> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// A sound sensor (SoundSensor).
struct SoundSensor {
  static constexpr pm::api::Type<pm::map::SoundSensor> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// The EMF reader (EMFReader).
struct EMFReader {
  static constexpr pm::api::Type<pm::map::EMFReader> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// The parabolic microphone (ParabolicMicrophone).
struct ParabolicMicrophone {
  static constexpr pm::api::Type<pm::map::ParabolicMicrophone> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// The sound recorder (SoundRecorder).
struct SoundRecorder {
  static constexpr pm::api::Type<pm::map::SoundRecorder> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// The thermometer (Thermometer).
struct Thermometer {
  static constexpr pm::api::Type<pm::map::Thermometer> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// The spirit box (EVPRecorder). `box` below is the spirit box.
struct EVPRecorder {
  static constexpr pm::api::Type<pm::map::EVPRecorder> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
  /// PlayLocationSound(box, a, b). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EVPRecorder_PlayLocationSound, void(void*, int32_t, int32_t)>
      PlayLocationSound{};
  /// PlayAboutSound(box, a, b). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EVPRecorder_PlayAboutSound, void(void*, int32_t, int32_t)> PlayAboutSound{};
  /// PlayDifficultySound(box, a, b). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EVPRecorder_PlayDifficultySound, void(void*, int32_t, int32_t)>
      PlayDifficultySound{};
};
/// The D.O.T.S. projector (IRLightSensor).
struct IRLightSensor {
  static constexpr pm::api::Type<pm::map::IRLightSensor> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// A torch (Torch).
struct Torch {
  static constexpr pm::api::Type<pm::map::Torch> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};
/// A crucifix.
struct Crucifix {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Crucifix> type{};
  /// CrucifixUseNetworked(crucifix, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Crucifix_CrucifixUseNetworked, void(void*, bool, void*)>
      CrucifixUseNetworked{};
  /// How many times it has been used: useCount.get(crucifix). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Crucifix_useCount, int32_t> useCount{};
};

/// Incense. `sage` below is the WhiteSage.
struct WhiteSage {
  /// UseNetworked(sage, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::WhiteSage_UseNetworked, void(void*, bool, void*)> UseNetworked{};
  /// The handler that traps the Yurei: YureiTrap(sage, gameObject, sensor). A mod hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::WhiteSage_YureiTrap, void(void*, void*, void*)> YureiTrap{};
};

/// The store's description of an item. `storeItem` below is the StoreItem.
struct StoreItem {
  /// Its tier, a StoreItemTier number: tier.get(storeItem). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::StoreItem_tier, int32_t> tier{};
};

/// The tiers of a store item: compare StoreItem::tier with them.
namespace StoreItemTier {
/// Tier I: One(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::StoreItemTier_One> One{};
/// Tier II: Two(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::StoreItemTier_Two> Two{};
/// Tier III: Three(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::StoreItemTier_Three> Three{};
}  // namespace StoreItemTier

/// @}
}  // namespace phasmo
