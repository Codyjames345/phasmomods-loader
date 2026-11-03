#pragma once

// The contract's weather: a WeatherProfile asset (LevelValues.weather, in Difficulty.h) and the
// weather type it carries, an enum nested in LevelValues. The profile's members keep their real names;
// the enum's name is obfuscated and its value names are real. Notes read from the class dump are from
// build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// A weather's settings (Assembly-CSharp, global namespace), a ScriptableObject: its sky, its fog, its
/// time of day and its weather type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc WeatherProfile{.key = "WeatherProfile", .name = "WeatherProfile",
                                                   .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* WeatherType_values[] = {"clear", "foggy", "rain", "heavyRain"};

/// The weather types, an enum nested in LevelValues (name obfuscated, value names real): clear,
/// foggy, rain, heavyRain, snow, wind, sunrise, bloodMoon and dead. Found by its values. Inferred:
/// from the class dump.
inline constexpr pm::sdk::EntryDesc WeatherType{.key = "WeatherType", .owner = "LevelValues",
                                                .values = WeatherType_values};

/// Rain. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc WeatherType_rain{.key = "WeatherType.rain", .kind = pm::sdk::Kind::EnumValue,
                                                     .owner = "WeatherType", .name = "rain"};

/// Heavy rain. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc WeatherType_heavyRain{.key = "WeatherType.heavyRain",
                                                          .kind = pm::sdk::Kind::EnumValue, .owner = "WeatherType",
                                                          .name = "heavyRain"};

/// The dead players' weather: RandomWeather's own profile for them (its deadWeatherProfile), never a
/// contract's. Inferred: from the value's name and the class dump.
inline constexpr pm::sdk::EntryDesc WeatherType_dead{.key = "WeatherType.dead", .kind = pm::sdk::Kind::EnumValue,
                                                     .owner = "WeatherType", .name = "dead"};

/// The scene's weather (Assembly-CSharp, global namespace): its sky, effects and profiles. Its
/// TransitionWeatherProfile is the Monkey Paw's weather wish. Decompiled.
inline constexpr pm::sdk::EntryDesc RandomWeather{.key = "RandomWeather", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .name = "RandomWeather", .assembly = "Assembly-CSharp", .ns = ""};

/// The scene's RandomWeather: the class's one static field of its own type. Decompiled.
inline constexpr pm::sdk::EntryDesc RandomWeather_instance{.key = "RandomWeather.instance", .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .flags = pm::sdk::kStatic, .owner = "RandomWeather",
                                                           .type = "@RandomWeather"};

/// The weather's effects controller, an AzureEffectsController: the one its storm's lightning strikes
/// from (LightningController.PlayLightningNetworked uses RandomWeather.instance's). Decompiled.
inline constexpr pm::sdk::EntryDesc RandomWeather_effects{.key = "RandomWeather.effects", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "RandomWeather", .name = "effects"};

/// The sky's effects (Assembly-CSharp, UnityEngine.AzureSky): rain, wind and the thunder strikes.
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc AzureEffectsController{.key = "AzureEffectsController", .name = "AzureEffectsController",
                                                           .assembly = "Assembly-CSharp", .ns = "UnityEngine.AzureSky"};

/// The thunder strikes it can play, a List<AzureThunderSettings> (each a prefab, a clip and a position).
/// PlayLocalLightning throws on an empty list. Decompiled.
inline constexpr pm::sdk::EntryDesc AzureEffectsController_thunderSettingsList{
    .key = "AzureEffectsController.thunderSettingsList", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "AzureEffectsController", .name = "thunderSettingsList"};

/// The scene's lightning (Assembly-CSharp, global namespace). Its Update on the master sends
/// PlayLightningNetworked to every client every 0 to 30 seconds while the weather is heavy rain.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LightningController{.key = "LightningController",
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .name = "LightningController", .assembly = "Assembly-CSharp",
                                                        .ns = ""};

/// The controller's own effects controller, an AzureEffectsController: no contract scene sets it, so it
/// is null there. Decompiled.
inline constexpr pm::sdk::EntryDesc LightningController_effects{.key = "LightningController.effects",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "LightningController", .name = "effects"};

/// A strike on this client only, from the controller's own effects: the lobby hoop's (HoopCounter),
/// which throws where the effects are null, as in a contract. Decompiled.
inline constexpr pm::sdk::EntryDesc LightningController_PlayLocalLightning{
    .key = "LightningController.PlayLocalLightning", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LightningController", .name = "PlayLocalLightning"};

/// The game's storm strike, an RPC every client runs; only the game sends it. It strikes from
/// RandomWeather.instance's effects. Decompiled.
inline constexpr pm::sdk::EntryDesc LightningController_PlayLightningNetworked{
    .key = "LightningController.PlayLightningNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LightningController", .name = "PlayLightningNetworked"};

/// The profile's weather type, a WeatherType number. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc WeatherProfile_weatherType{.key = "WeatherProfile.weatherType",
                                                               .kind = pm::sdk::Kind::Field, .owner = "WeatherProfile",
                                                               .name = "weatherType"};

}  // namespace pm::map

// The game API for the weather (sdk/api/game_api.h). Every member is called on Unity's main thread.
// One that did not resolve in the running game build logs "<key> is not available in this game build"
// once and returns its type's default (0) without touching the game.
namespace phasmo {
/// @addtogroup weather
/// @{

/// A weather's settings. `profile` below is one.
struct WeatherProfile {
  /// Its weather type, a WeatherType number: weatherType.get(profile). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::WeatherProfile_weatherType, int32_t> weatherType{};
};

/// The weather types. Unresolved values are 0, which is clear: check use::resolved() before relying
/// on one.
namespace WeatherType {
/// rain(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherType_rain> rain{};
/// heavyRain(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherType_heavyRain> heavyRain{};
/// dead(): the dead players' weather. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherType_dead> dead{};
}  // namespace WeatherType

/// The scene's weather. `weather` below is one.
struct RandomWeather {
  /// The scene's RandomWeather: instance.get(). Check it with pm::api::alive. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::RandomWeather_instance, void*> instance{};
  /// Its effects controller: effects.get(weather). Unresolved: null.
  static constexpr pm::api::Field<pm::map::RandomWeather_effects, void*> effects{};
};

/// The sky's effects. `effects` below is one.
struct AzureEffectsController {
  /// The thunder strikes it can play, a List: thunderSettingsList.get(effects). Unresolved: null.
  static constexpr pm::api::Field<pm::map::AzureEffectsController_thunderSettingsList, void*> thunderSettingsList{};
};

/// The scene's lightning. `controller` below is one.
struct LightningController {
  /// The type: type_object() for Object.FindObjectOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::LightningController> type{};
  /// Its own effects controller: effects.get(controller), .set(controller, e) (through the GC write
  /// barrier). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::LightningController_effects, void*> effects{};
  /// PlayLocalLightning(controller): a strike on this client. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LightningController_PlayLocalLightning, void(void*)> PlayLocalLightning{};
  /// The storm strike RPC, PlayLightningNetworked(controller, &info). A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::LightningController_PlayLightningNetworked, void(void*, const void*)>
      PlayLightningNetworked{};
};

/// @}
}  // namespace phasmo
