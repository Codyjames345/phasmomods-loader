#include "game_api.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "log.h"

namespace pg::api {
namespace {

const PgHostApi* g_host = nullptr;
// GameAssembly's il2cpp_class_is_subclass_of, for Type::is; null without a game module (the fake).
bool (*g_isSubclass)(void* klass, void* parent, bool checkInterfaces) = nullptr;

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

const PgHostApi* host() { return g_host; }

void report_unavailable(Slot& slot) {
  if (slot.reported) return;
  slot.reported = true;
  PG_LOG("%s is not available in this game build", slot.desc->key);
}

bool is_instance(const void* obj, void* klass) {
  void* const objClass = *static_cast<void* const*>(obj);  // a managed object begins with its class
  if (objClass == klass) return true;
  return objClass && g_isSubclass && g_isSubclass(objClass, klass, true);
}

bool hook(const char* key, void* detour, void** original, const char* name, bool allTwins) {
  if (!g_host) {
    PG_LOG("%s cannot be hooked before the game API is bound", key);
    return false;
  }
  return g_host->hook_entry(key, detour, original, name, allTwins);
}

int32_t inside(const char* key) { return g_host ? g_host->inside_entry(key) : 0; }

}  // namespace detail

bool bind(const PgHostApi* host, std::string& missing) {
  missing.clear();
  g_host = host;
  g_isSubclass = nullptr;
  if (!host) return false;
  if (host->gameAssembly)
    g_isSubclass = reinterpret_cast<decltype(g_isSubclass)>(
        GetProcAddress(static_cast<HMODULE>(host->gameAssembly), "il2cpp_class_is_subclass_of"));
  std::vector<const char*> lacking;
  for (detail::Slot* slot : registry()) {
    slot->entry = host->game_entry(slot->desc->key);
    slot->reported = false;
    if (!slot->entry && !optional(slot)) lacking.push_back(slot->desc->key);
  }
  std::sort(lacking.begin(), lacking.end(), key_less);
  for (const char* key : lacking) missing += (missing.empty() ? "" : ", ") + std::string(key);
  return lacking.empty();
}

size_t used_required(const char* const** keys) {
  static std::vector<const char*> required;
  required.clear();
  for (const detail::Slot* slot : registry())
    if (!optional(slot)) required.push_back(slot->desc->key);
  std::sort(required.begin(), required.end(), key_less);
  *keys = required.data();
  return required.size();
}

}  // namespace pg::api
