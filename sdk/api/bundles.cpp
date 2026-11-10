#include "bundles.h"

#include <system_error>

#include "Animator.h"
#include "UnityAssets.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::api {

void* load_bundle(const std::filesystem::path& file, std::string* why) {
  const auto fail = [&](const char* reason) -> void* {
    if (why) *why = reason;
    return nullptr;
  };
  std::error_code error;
  if (file.empty() || !std::filesystem::is_regular_file(file, error)) return fail("the file is missing");
  if (!phasmo::AssetBundle::LoadFromFile.available()) return fail("AssetBundle.LoadFromFile_Internal did not resolve");
  const std::u8string utf8 = file.u8string();
  void* path = make_string(std::string_view(reinterpret_cast<const char*>(utf8.data()), utf8.size()));
  if (!path) return fail("the path could not be made a managed string");
  void* bundle = phasmo::AssetBundle::LoadFromFile(path, 0, 0);
  if (!bundle) return fail("the engine refused the file; the game's own log says why");
  return bundle;
}

void* load_bundle_asset(void* bundle, const char* name, void* typeObject) {
  if (!bundle || !name || !typeObject) return nullptr;
  void* managedName = make_string(name);
  void* asset = managedName ? phasmo::AssetBundle::LoadAsset(bundle, managedName, typeObject) : nullptr;
  // An asset a mod loads is in no scene, so a scene load's unload of unused assets would destroy it (mods start at the
  // menu, before any lobby): keep it, as Unity's own HideFlags.DontUnloadUnusedAsset is for.
  if (asset) phasmo::Object::set_hideFlags(asset, phasmo::HideFlags::DontUnloadUnusedAsset());
  return asset;
}

void* load_controller(void* bundle, const char* name) {
  return load_bundle_asset(bundle, name, phasmo::RuntimeAnimatorController::type.type_object());
}

std::vector<std::string> bundle_asset_names(void* bundle) {
  std::vector<std::string> out;
  if (!bundle) return out;
  const RefArray names{phasmo::AssetBundle::GetAllAssetNames(bundle)};
  for (size_t i = 0; i < names.size(); ++i) out.push_back(to_utf8(names[i]));
  return out;
}

void unload_bundle(void* bundle, bool unloadAllLoadedObjects) {
  if (bundle) phasmo::AssetBundle::Unload(bundle, unloadAllLoadedObjects);
}

}  // namespace pm::api
