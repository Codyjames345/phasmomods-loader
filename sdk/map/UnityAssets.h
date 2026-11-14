#pragma once

// Loading assets: a prefab of the game's own Resources, and a mod's own AssetBundle. The managed AssetBundle class is
// stripped from this build, but the engine functions behind it are registered by their icall names, so those entries
// resolve only in the running game. A bundle the engine gives is passed back only into these functions. What an
// in-game test showed of them is in sdk/internals/engine-experiments.md; sdk/api/bundles.h wraps them for a mod, and
// sdk/guides/custom-animation.md shows how to build a bundle for this game.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* UnityAssets_Load_params[] = {"string", "Type"};
inline constexpr const char* UnityAssets_LoadFromFile_params[] = {"string", "uint", "ulong"};
inline constexpr const char* UnityAssets_LoadAsset_params[] = {"string", "Type"};
inline constexpr const char* UnityAssets_bool[] = {"bool"};

/// Unity's Resources (UnityEngine.CoreModule): the assets the game ships in its Resources folders. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Resources{.key = "Resources", .name = "Resources",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Resources.Load(path, type), static: the asset at a Resources path, of that System.Type, or null. The asset itself,
/// not a copy: copy a prefab with Object.Instantiate. LocalPCPlayer.CreateNetworkedPlayer loads the investigator's
/// prefab, PCPlayer, by it. Decompiled; the Squatter's engine test loaded PCPlayer by it in game (2026-11-05).
inline constexpr pm::sdk::EntryDesc Resources_Load{.key = "Resources.Load(string,Type)",
                                                   .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .flags = pm::sdk::kStatic, .owner = "Resources", .name = "Load",
                                                   .params = UnityAssets_Load_params};

/// AssetBundle.LoadFromFile_Internal(path, crc, offset), static: loads the bundle in the file at `path` (crc 0 and
/// offset 0 for a plain file), or null, with the engine's reason in the game's own log. A bundle is loaded once: a
/// second load of the same one fails until it is unloaded. Live: the Squatter's engine test loaded a bundle built for
/// this game's Unity version with it (2026-11-05).
inline constexpr pm::sdk::EntryDesc AssetBundle_LoadFromFile{
    .key = "AssetBundle.LoadFromFile_Internal", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Live,
    .flags = pm::sdk::kStatic, .name = "UnityEngine.AssetBundle::LoadFromFile_Internal",
    .params = UnityAssets_LoadFromFile_params};

/// AssetBundle.LoadAsset_Internal(bundle, name, type): the bundle's asset by its name, of that System.Type, or null.
/// The name is the asset's path in the bundle, lower case ("assets/squatter.controller"), or its plain name. Live:
/// the engine test loaded a controller with it (2026-11-05).
inline constexpr pm::sdk::EntryDesc AssetBundle_LoadAsset{
    .key = "AssetBundle.LoadAsset_Internal", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Live,
    .name = "UnityEngine.AssetBundle::LoadAsset_Internal", .params = UnityAssets_LoadAsset_params};

/// AssetBundle.GetAllAssetNames(bundle): the paths of every asset in the bundle, a string[]. Live: the engine test
/// listed a bundle with it (2026-11-05).
inline constexpr pm::sdk::EntryDesc AssetBundle_GetAllAssetNames{
    .key = "AssetBundle.GetAllAssetNames", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Live,
    .name = "UnityEngine.AssetBundle::GetAllAssetNames"};

/// AssetBundle.Unload(bundle, unloadAllLoadedObjects): frees the bundle; with true, every asset loaded from it too.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AssetBundle_Unload{.key = "AssetBundle.Unload", .kind = pm::sdk::Kind::Icall,
                                                       .name = "UnityEngine.AssetBundle::Unload",
                                                       .params = UnityAssets_bool};

}  // namespace pm::map

// The game API for loading assets (sdk/api/game_api.h). Every member is called on Unity's main thread. One that did
// not resolve in the running game build logs "<key> is not available in this game build" once and returns null or
// does nothing. Strings are System.String (pm::api::make_string), types System.Type (a Type wrapper's type_object()).
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's Resources.
struct Resources {
  /// Load(path, type): the asset or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Resources_Load, pm::api::Object(pm::api::Object, pm::api::Object)> Load{};
};

/// Unity's AssetBundle, by its engine functions. `bundle` below is one LoadFromFile gave.
struct AssetBundle {
  /// LoadFromFile(path, crc, offset): the bundle or null. Unresolved: null.
  static constexpr pm::api::Icall<pm::map::AssetBundle_LoadFromFile, pm::api::Object(pm::api::Object, uint32_t, uint64_t)> LoadFromFile{};
  /// LoadAsset(bundle, name, type): the asset or null. Unresolved: null.
  static constexpr pm::api::Icall<pm::map::AssetBundle_LoadAsset, pm::api::Object(pm::api::Object, pm::api::Object, pm::api::Object)> LoadAsset{};
  /// GetAllAssetNames(bundle): a string[] (pm::api::RefArray). Unresolved: null.
  static constexpr pm::api::Icall<pm::map::AssetBundle_GetAllAssetNames, pm::api::Object(pm::api::Object)> GetAllAssetNames{};
  /// Unload(bundle, unloadAllLoadedObjects). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::AssetBundle_Unload, void(pm::api::Object, bool)> Unload{};
};

/// @}
}  // namespace phasmo
