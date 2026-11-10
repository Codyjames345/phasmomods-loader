#pragma once

#include <filesystem>
#include <string>
#include <vector>

// A mod's own Unity assets: an AssetBundle the mod ships in its assets folder (assets.h), and the assets in it. A
// bundle holds what the game cannot make at run time, such as animation clips behind an animator controller. How to
// build one that this game loads is in sdk/guides/custom-animation.md.
//
// The names follow Unity's own AssetBundle API (LoadFromFile, LoadAsset, GetAllAssetNames, Unload), as mods for other
// Unity loaders call it; this build strips the managed class, so these reach the engine functions behind it
// (sdk/map/UnityAssets.h). Main thread, every function.
namespace pm::api {

/**
 * @brief Loads the AssetBundle in a file, as AssetBundle.LoadFromFile does.
 * @param file The bundle's full path, usually under assets_folder().
 * @param why Set to the reason when the load fails, if not null.
 * @return The bundle, a managed object: keep it in a pm::api::Held for as long as the mod uses it, and unload it with
 *         unload_bundle when done. Null when the file is missing, the engine refuses it (the engine writes its reason
 *         to the game's own log: a bundle built for another Unity version or platform, say), or the bundle is already
 *         loaded: the engine loads a bundle once, so a second load fails until it is unloaded.
 * @pre Main thread, once the mod is started. Reads the file; the engine keeps it open while the bundle is loaded.
 * @note This client only. Nothing is networked; every client loads its own copy.
 * @since Contract 2.3 (a helper in the mod's own code; the host contract itself does not change)
 */
void* load_bundle(const std::filesystem::path& file, std::string* why = nullptr);

/**
 * @brief An asset of the bundle by its name, as AssetBundle.LoadAsset does.
 * @param bundle A bundle load_bundle gave.
 * @param name The asset's path in the bundle as bundle_asset_names lists it, lower case ("assets/door.controller"),
 *        or its plain name.
 * @param typeObject The System.Type to load it as: a Type wrapper's type_object().
 * @return The asset, a managed object (keep it in a Held past this frame), or null when the bundle has no such asset
 *         of that type, or a null argument.
 * @pre Main thread. The same asset loaded twice is the same object.
 * @note The asset is kept from Unity's unload of unused assets (HideFlags.DontUnloadUnusedAsset): it is in no scene, so a
 *       scene load would otherwise destroy it before it is used. Destroy it yourself, or unload the bundle with its
 *       objects, when it is no longer needed.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* load_bundle_asset(void* bundle, const char* name, void* typeObject);

/**
 * @brief The animator controller of the bundle by its name: load_bundle_asset as a RuntimeAnimatorController.
 * @return The controller, for use_controller (models.h), or null when the bundle has none of that name.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void* load_controller(void* bundle, const char* name);

/**
 * @brief The names of every asset in the bundle, as AssetBundle.GetAllAssetNames gives them: for a log when an asset
 *        is not found.
 * @return The names in UTF-8, or none for a null bundle.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
std::vector<std::string> bundle_asset_names(void* bundle);

/**
 * @brief Unloads a bundle, as AssetBundle.Unload does.
 * @param bundle A bundle load_bundle gave; null does nothing.
 * @param unloadAllLoadedObjects True also destroys every asset loaded from it, which breaks anything still using one.
 *        False keeps those assets alive and only frees the bundle.
 * @pre Main thread. Unload in pmmod_stop, after the mod has stopped using its assets.
 * @since Contract 2.3 (a helper in the mod's own code)
 */
void unload_bundle(void* bundle, bool unloadAllLoadedObjects);

}  // namespace pm::api
