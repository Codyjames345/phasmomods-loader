#include "scene_facts.h"

#include "log.h"

namespace pm::sdk {

void SceneFacts::set_engine(const SceneEngine* engine) {
  forget();
  engine_ = engine;
}

void SceneFacts::forget() {
  for (auto& [key, k] : kept_) release(k);
  kept_.clear();
}

void SceneFacts::release(Kept& k) {
  if (engine_) {
    engine_->release(k.ofHandle);
    engine_->release(k.foundHandle);
  }
  k.of = k.found = nullptr;
  k.ofHandle = k.foundHandle = 0;
  k.valid = false;
}

void SceneFacts::keep(Kept& k, int32_t scene, void* of, void* found) {
  release(k);
  k.scene = scene;
  k.of = of;
  k.ofHandle = engine_->hold(of);
  k.found = found;
  k.foundHandle = engine_->hold(found);
  k.valid = true;
}

bool SceneFacts::miss_once(Kept& k, int32_t scene) {
  if (k.missLoggedOnce && k.missLogged == scene) return false;
  k.missLoggedOnce = true;
  k.missLogged = scene;
  return true;
}

int32_t SceneFacts::layer(const EntryDesc& d) {
  const char* why = engine_ ? nullptr : "no scene engine";
  const int32_t index = engine_ && d.name ? engine_->name_to_layer(d.name, &why) : -1;
  if (index >= 0 && index < 32) return index;
  if (why) PM_LOG("game map: %s: %s", d.key, why);
  else PM_LOG("game map: %s: no layer is named %s", d.key, d.name ? d.name : "(none)");
  return -1;
}

bool SceneFacts::shader_property(const EntryDesc& d, int32_t& id) {
  const char* why = engine_ ? nullptr : "no scene engine";
  if (engine_ && engine_->property_id && d.name && engine_->property_id(d.name, &id, &why)) return true;
  PM_LOG("game map: %s: %s", d.key, why ? why : "the property's id could not be read");
  return false;
}

void* SceneFacts::find(const EntryDesc& d, void* root) {
  if (!engine_ || !d.path || !root) return nullptr;
  Kept& k = kept_[d.key];
  const int32_t scene = engine_->active_scene();
  if (k.valid && k.scene == scene && k.of == root && engine_->alive(k.found)) return k.found;
  const char* why = nullptr;
  void* found = nullptr;
  if (!engine_->alive(root)) why = "the root is destroyed";
  else found = engine_->find(root, d.path, &why);
  if (found) {
    keep(k, scene, root, found);
    return found;
  }
  // Nothing is kept for a miss: the object may be added later in the same scene.
  release(k);
  if (miss_once(k, scene)) {
    if (why) PM_LOG("game map: %s: %s; it answers null", d.key, why);
    else PM_LOG("game map: %s: no %s under the given object in this scene; it answers null", d.key, d.path);
  }
  return nullptr;
}

bool SceneFacts::parameter(const EntryDesc& d, const void* animator, int32_t& hash) {
  if (!engine_ || !d.name || !animator) return false;
  Kept& k = kept_[d.key];
  const int32_t scene = engine_->active_scene();
  if (!(k.valid && k.scene == scene && k.of == animator && engine_->alive(animator))) {
    int32_t value = 0;
    const char* why = nullptr;
    bool has = false;
    if (!engine_->alive(animator)) why = "the Animator is destroyed";
    else has = engine_->parameter(animator, d.name, &value, &why);
    keep(k, scene, const_cast<void*>(animator), nullptr);
    k.has = has;
    k.hash = value;
    k.why = why;
  }
  if (k.has) {
    hash = k.hash;
    return true;
  }
  if (miss_once(k, scene)) {
    if (k.why) PM_LOG("game map: %s: %s; it answers 0", d.key, k.why);
    else PM_LOG("game map: %s: the Animator has no parameter %s; it answers 0", d.key, d.name);
  }
  return false;
}

}  // namespace pm::sdk
