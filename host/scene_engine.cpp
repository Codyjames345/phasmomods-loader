#include "scene_engine.h"

#include <atomic>
#include <cstring>
#include <exception>
#include <string>

#include "Animator.h"
#include "LayerMask.h"
#include "SceneManager.h"
#include "Transform.h"
#include "contained.h"
#include "game.h"
#include "game_map.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_api.h"

namespace pg::scene_engine {
namespace {

// The resolved entry `d`, or null.
const PgGameEntry* mapped(const sdk::EntryDesc& d) { return gamemap::entry(d.key); }

// Whether the managed object `obj` is of the type entry `type`'s class or a class derived from it.
bool of_type(const void* obj, const sdk::EntryDesc& type) {
  const PgGameEntry* t = mapped(type);
  return obj && t && t->klass && il2cpp::is_instance(obj, static_cast<il2cpp::Class*>(t->klass));
}

int32_t active_scene() {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "reading the active scene", int32_t{0}, []() -> int32_t {
    const PgGameEntry* e = mapped(map::SceneManager_GetActiveScene);
    // A Scene is a struct of one int, its handle, so it comes back in a register as that int.
    return e && e->code ? reinterpret_cast<int32_t (*)(const void*)>(e->code)(e->method) : 0;
  });
}

void* find(void* root, const char* path, const char** why) {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "finding a path", static_cast<void*>(nullptr), [&]() -> void* {
    const PgGameEntry* e = mapped(map::Transform_Find);
    if (!e || !e->code) *why = "Transform.Find unresolved";
    else if (!mapped(map::Transform)) *why = "Transform unresolved";
    else if (!of_type(root, map::Transform)) *why = "the root is not a Transform";
    if (*why || !path) return nullptr;
    void* name = il2cpp::string_new(path);
    return name ? reinterpret_cast<void* (*)(void*, void*, const void*)>(e->code)(root, name, e->method) : nullptr;
  });
}

int32_t name_to_layer(const char* layerName, const char** why) {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "finding a layer", int32_t{-1}, [&]() -> int32_t {
    const PgGameEntry* e = mapped(map::LayerMask_NameToLayer);
    if (!e || !e->code) *why = "LayerMask.NameToLayer unresolved";
    void* name = e && e->code && layerName ? il2cpp::string_new(layerName) : nullptr;
    return name ? reinterpret_cast<int32_t (*)(void*, const void*)>(e->code)(name, e->method) : -1;
  });
}

bool parameter(const void* animator, const char* name, int32_t* hash, const char** why) {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "reading an Animator's parameters", false, [&] {
    const PgGameEntry* list = mapped(map::Animator_get_parameters);
    const PgGameEntry* toHash = mapped(map::Animator_StringToHash);
    const PgGameEntry* field = mapped(map::AnimatorControllerParameter_m_Name);
    if (!list || !list->code) *why = "Animator.get_parameters unresolved";
    else if (!toHash || !toHash->code) *why = "Animator.StringToHash unresolved";
    else if (!field) *why = "AnimatorControllerParameter.m_Name unresolved";
    else if (!mapped(map::Animator)) *why = "Animator unresolved";
    else if (!of_type(animator, map::Animator)) *why = "the object is not an Animator";
    if (*why || !name || !hash) return false;
    auto* params = static_cast<il2cpp::Array*>(reinterpret_cast<void* (*)(const void*, const void*)>(list->code)(
        animator, list->method));
    if (!params || !il2cpp::is_array(params)) return false;
    char buffer[256];
    for (uintptr_t i = 0; i < params->max_length; ++i) {
      void* p = params->data<void*>()[i];
      if (!of_type(p, map::AnimatorControllerParameter)) continue;
      void* text = *reinterpret_cast<void**>(static_cast<char*>(p) + field->offset);
      if (!text || !il2cpp::is_string(text)) continue;
      const size_t length = il2cpp::to_utf8(static_cast<const il2cpp::String*>(text), buffer, sizeof buffer);
      if (length >= sizeof buffer || std::strcmp(buffer, name) != 0) continue;
      *hash = reinterpret_cast<int32_t (*)(void*, const void*)>(toHash->code)(text, toHash->method);
      return true;
    }
    return false;
  });
}

bool alive(const void* obj) {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "checking an object is alive", false,
                         [&] { return game::alive(const_cast<void*>(obj)); });
}

uint32_t hold(void* obj) {
  static std::atomic<bool> logged{false};
  return host::contained(&logged, "scene facts", "holding an object", uint32_t{0},
                         [&] { return il2cpp::gc_hold(obj); });
}

void release(uint32_t handle) {
  try {
    il2cpp::gc_release(handle);
  } catch (...) {
    PG_LOG("scene facts: exception while releasing an object");
  }
}

const sdk::SceneEngine kEngine{&active_scene, &find, &name_to_layer, &parameter, &alive, &hold, &release};

}  // namespace

const sdk::SceneEngine* engine() { return &kEngine; }

}  // namespace pg::scene_engine
