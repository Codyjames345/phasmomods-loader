#include "game_api.h"

#include <algorithm>
#include <vector>

#include "log.h"

namespace pm::api {
namespace {

const PmHostApi* g_host = nullptr;

// Every entry the mod's code uses, filled while its DLL loads. Built on first use, so a wrapper
// registered from any translation unit's initialisation finds it.
std::vector<detail::Slot*>& registry() {
  static std::vector<detail::Slot*> slots;
  return slots;
}

bool optional(const detail::Slot* slot) { return (slot->desc->flags & sdk::kOptional) != 0; }

bool key_less(const char* a, const char* b) { return std::strcmp(a, b) < 0; }

}  // namespace

namespace detail {

bool register_use(Slot* slot) {
  std::vector<Slot*>& slots = registry();
  if (std::find(slots.begin(), slots.end(), slot) == slots.end()) slots.push_back(slot);
  return true;
}

const PmHostApi* host() { return g_host; }

void report_unavailable(Slot& slot) {
  if (!slot.reported.exchange(true)) PM_LOG("%s is not available in this game build", slot.desc->key);
}

void report_refused_call(Slot& slot) {
  if (!slot.refused.exchange(true))
    PM_LOG("%s: virtual_call on a null object or one not of its declaring type; nothing was called", slot.desc->key);
}

void report_refused_static(Slot& slot) {
  if (!slot.refused.exchange(true))
    PM_LOG("%s: the host could not read or write the static field; the default was used", slot.desc->key);
}

bool static_get(const char* key, void* out, size_t size) { return g_host && g_host->static_get(key, out, size); }

bool static_set(const char* key, const void* in, size_t size) { return g_host && g_host->static_set(key, in, size); }

bool is_instance(const void* obj, const char* typeKey) { return g_host && g_host->is_instance(obj, typeKey); }

void* type_object(const char* typeKey) { return g_host ? g_host->type_object(typeKey) : nullptr; }

void* object_new(const char* typeKey) { return g_host ? g_host->object_new(typeKey) : nullptr; }

void* array_new(const char* elementTypeKey, size_t length) {
  return g_host ? g_host->array_new(elementTypeKey, length) : nullptr;
}

bool copy_settings(const char* typeKey, void* dst, const void* src) {
  return g_host && g_host->copy_settings && g_host->copy_settings(typeKey, dst, src);
}

uint32_t fields_of_type(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                        uint32_t max) {
  return g_host && g_host->fields_of_type ? g_host->fields_of_type(typeKey, obj, fieldTypeKey, arrayOf, out, max) : 0;
}

const void* virtual_method(const void* obj, const char* methodKey) {
  return g_host ? g_host->virtual_method(obj, methodKey) : nullptr;
}

bool hook_bodies(const char* key, bool hasDetour, void* (*entry)(uint32_t), void** originals, const char* name,
                 bool allTwins, uint32_t& created) {
  created = 0;
  if (!g_host) {
    PM_LOG("%s cannot be hooked before the game API is bound", key);
    return false;
  }
  if (!hasDetour) {
    PM_LOG("%s: the hook was given no detour; nothing was hooked", key);
    return false;
  }
  const uint32_t count = allTwins ? g_host->body_count(key) : 1;
  if (count > kMaxBodies) {
    PM_LOG("%s has %u bodies, more than the %u a hook covers; nothing was hooked", key, count, kMaxBodies);
    return false;
  }
  for (; created < count; ++created)
    if (!g_host->hook_body(key, created, entry(created), &originals[created], name)) break;
  if (created == count && count > 0) return true;
  if (created > 0) PM_LOG("%s is hooked on %u of its %u bodies only", key, created, count);
  else if (count == 0) PM_LOG("%s is not a method the host can hook; nothing was hooked", key);
  return false;
}

int32_t inside(const char* key) { return g_host ? g_host->inside_entry(key) : 0; }

void* scene_find(const char* key, void* root) { return g_host ? g_host->scene_find(key, root) : nullptr; }

bool animator_param(const char* key, const void* animator, int32_t* hash) {
  return g_host && g_host->animator_param(key, animator, hash);
}

bool steer_agent(void* agent, float dx, float dz) { return g_host && g_host->steer_agent(agent, dx, dz); }

void abandon_ghost_spawn() {
  if (g_host) g_host->abandon_ghost_spawn();
}

}  // namespace detail

bool bind(const PmHostApi* host, std::string& missing) {
  missing.clear();
  g_host = host;
  if (!host) return false;
  std::vector<const char*> lacking;
  for (detail::Slot* slot : registry()) {
    slot->entry = host->game_entry(slot->desc->key);
    slot->reported = false;
    slot->refused = false;
    if (!slot->entry && !optional(slot)) lacking.push_back(slot->desc->key);
  }
  std::sort(lacking.begin(), lacking.end(), key_less);
  for (const char* key : lacking) missing += (missing.empty() ? "" : ", ") + std::string(key);
  return lacking.empty();
}

size_t used_required(const char* const** keys) {
  // Built once: every entry registered while the DLL loaded, so the list never changes after.
  static const std::vector<const char*> required = [] {
    std::vector<const char*> out;
    for (const detail::Slot* slot : registry())
      if (!optional(slot)) out.push_back(slot->desc->key);
    std::sort(out.begin(), out.end(), key_less);
    return out;
  }();
  *keys = required.data();
  return required.size();
}

}  // namespace pm::api
