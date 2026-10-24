#include "scene_facts.h"

#include "log.h"

namespace pg::sdk {

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
  const int32_t index = engine_ && d.name ? engine_->name_to_layer(d.name) : -1;
  if (index >= 0 && index < 32) return index;
  PG_LOG("game map: %s: no layer is named %s", d.key, d.name ? d.name : "(none)");
  return -1;
}

void* SceneFacts::find(const EntryDesc& d, void* root) {
  if (!engine_ || !d.path) return nullptr;
  Kept& k = kept_[d.key];
  const int32_t scene = engine_->active_scene();
  if (k.valid && k.scene == scene && k.of == root && (!k.found || engine_->alive(k.found))) return k.found;
  void* found = root && engine_->alive(root) ? engine_->find(root, d.path) : nullptr;
  if (found) {
    keep(k, scene, root, found);
  } else {
    // Nothing is kept for a miss: the object may be added later in the same scene.
    release(k);
    if (miss_once(k, scene))
      PG_LOG("game map: %s: no %s under the given object in this scene; it answers null", d.key, d.path);
  }
  return found;
}

bool SceneFacts::parameter(const EntryDesc& d, const void* animator, int32_t& hash) {
  if (!engine_ || !d.name || !animator) return false;
  Kept& k = kept_[d.key];
  const int32_t scene = engine_->active_scene();
  if (!(k.valid && k.scene == scene && k.of == animator && engine_->alive(animator))) {
    int32_t value = 0;
    const bool has = engine_->alive(animator) && engine_->parameter(animator, d.name, &value);
    keep(k, scene, const_cast<void*>(animator), nullptr);
    k.has = has;
    k.hash = value;
  }
  if (k.has) {
    hash = k.hash;
    return true;
  }
  if (miss_once(k, scene)) PG_LOG("game map: %s: the Animator has no parameter %s; it answers 0", d.key, d.name);
  return false;
}

}  // namespace pg::sdk
