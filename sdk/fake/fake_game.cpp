#include "fake_game.h"

#include <windows.h>

#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "log.h"
#include "scene_facts.h"
#include "utf16.h"

namespace pg::fake {
namespace {

std::map<std::string, PgGameEntry> g_entries;  // by key; a map never moves its values
std::vector<std::string> g_hooked;
int g_barrierWrites = 0;

// The storage given to static_field(), by key.
struct Static {
  void* storage;
  size_t size;
  bool reference;
};
std::map<std::string, Static> g_statics;

// The fake's managed memory, freed by reset. Every fake object begins with its class pointer.
std::deque<std::vector<uint64_t>> g_memory;
std::map<void*, void*> g_parents;                      // class -> the class it derives from
std::map<std::pair<std::string, void*>, void*> g_overrides;  // (method key, class) -> fake MethodInfo
std::map<std::string, void*> g_declared;               // method key -> its fake MethodInfo
std::map<void*, void*> g_typeObjects;                  // class -> its fake System.Type object
std::set<const void*> g_destroyed;
std::map<uint32_t, void*> g_holds;                     // live GC handles
uint32_t g_nextHandle = 0;
int g_typeClass = 0;    // the class of the fake System.Type objects
int g_stringClass = 0;  // of the fake strings
int g_arrayClass = 0;   // of the fake arrays
int g_listClass = 0;    // of the fake lists
// The services that read a fake object check its kind, as the host does, and log a mismatch once.
bool g_refusedString = false, g_refusedArray = false, g_refusedList = false;

// An array, and a string, is its class, its length, then its elements from kArrayData. A list is its
// class, its backing array and its count.
constexpr size_t kArrayData = 16;
constexpr size_t kObjectBytes = 0x200;  // an object made by object_new
constexpr size_t kElementBytes = 16;    // an element of an array made by array_new
struct FakeList {
  void* klass;
  void* items;
  int32_t count;
};

void* allocate(size_t bytes, void* klass) {
  std::vector<uint64_t>& block = g_memory.emplace_back((bytes + 7) / 8 + 1, 0);
  std::memcpy(block.data(), &klass, sizeof klass);
  return block.data();
}

void* new_array(void* klass, size_t elementBytes, size_t length) {
  void* arr = allocate(kArrayData + elementBytes * length, klass);
  std::memcpy(static_cast<char*>(arr) + sizeof(void*), &length, sizeof length);
  return arr;
}

void* class_of(const void* obj) { return *static_cast<void* const*>(obj); }

bool derives(void* klass, void* ancestor) {
  for (void* k = klass; k; k = g_parents.count(k) ? g_parents[k] : nullptr)
    if (k == ancestor) return true;
  return false;
}

// Whether `obj` is of the fake class `kind`; a mismatch is logged once per kind.
bool of_kind(const void* obj, const int& kind, bool& refused, const char* service, const char* what) {
  if (obj && class_of(obj) == static_cast<const void*>(&kind)) return true;
  if (obj && !refused) {
    refused = true;
    PG_LOG("fake: %s was given an object that is not %s; it answers 0 or null", service, what);
  }
  return false;
}

PgGameEntry& entry_for(const sdk::EntryDesc& d, uint32_t kind) {
  g_statics.erase(d.key);
  PgGameEntry& e = g_entries[d.key];
  e = {};
  e.kind = kind;
  return e;
}

const PgGameEntry* game_entry(const char* key) {
  const auto it = key ? g_entries.find(key) : g_entries.end();
  return it == g_entries.end() ? nullptr : &it->second;
}

void* class_for(const char* typeKey) {
  const PgGameEntry* e = game_entry(typeKey);
  return e && e->kind == PG_ENTRY_TYPE ? e->klass : nullptr;
}

bool hook_entry(const char* key, void*, void** original, const char*, bool) {
  const PgGameEntry* e = game_entry(key);
  if (!e || e->kind != PG_ENTRY_METHOD) return false;
  g_hooked.emplace_back(key);
  if (original) *original = e->code;
  return true;
}

// A fake method has one body, its code.
bool hook_body(const char* key, uint32_t bodyIndex, void*, void** original, const char*) {
  const PgGameEntry* e = game_entry(key);
  if (!e || e->kind != PG_ENTRY_METHOD || bodyIndex != 0) return false;
  g_hooked.emplace_back(key);
  if (original) *original = e->code;
  return true;
}

uint32_t body_count(const char* key) {
  const PgGameEntry* e = game_entry(key);
  return e && e->kind == PG_ENTRY_METHOD ? 1 : 0;
}

void set_reference(void* obj, int32_t offset, void* value) {
  ++g_barrierWrites;
  std::memcpy(static_cast<char*>(obj) + offset, &value, sizeof value);
}

int32_t inside_entry(const char*) { return 0; }

void* string_new(const char* utf8) {
  if (!utf8) return nullptr;
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);  // with the NUL
  if (n <= 0) return nullptr;
  void* str = new_array(&g_stringClass, sizeof(char16_t), static_cast<size_t>(n - 1));
  std::vector<wchar_t> wide(static_cast<size_t>(n));
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), n);
  std::memcpy(static_cast<char*>(str) + kArrayData, wide.data(), sizeof(char16_t) * static_cast<size_t>(n - 1));
  return str;
}

// The length stored in a fake array or string.
size_t stored_length(const void* arr) {
  size_t length = 0;
  std::memcpy(&length, static_cast<const char*>(arr) + sizeof(void*), sizeof length);
  return length;
}

size_t array_length(const void* arr) {
  return of_kind(arr, g_arrayClass, g_refusedArray, "array_length", "an array") ? stored_length(arr) : 0;
}

void* array_data(void* arr) {
  return of_kind(arr, g_arrayClass, g_refusedArray, "array_data", "an array") ? static_cast<char*>(arr) + kArrayData
                                                                              : nullptr;
}

size_t string_utf8(const void* str, char* out, size_t size) {
  if (!of_kind(str, g_stringClass, g_refusedString, "string_utf8", "a string")) {
    if (out && size) out[0] = '\0';
    return 0;
  }
  const auto* chars = reinterpret_cast<const char16_t*>(static_cast<const char*>(str) + kArrayData);
  return utf16_to_utf8(std::u16string_view(chars, stored_length(str)), out, size);
}

void* array_new(const char* elementTypeKey, size_t length) {
  return class_for(elementTypeKey) ? new_array(&g_arrayClass, kElementBytes, length) : nullptr;
}

int32_t list_count(const void* list) {
  return of_kind(list, g_listClass, g_refusedList, "list_count", "a List") ? static_cast<const FakeList*>(list)->count : 0;
}

void* list_items(const void* list) {
  return of_kind(list, g_listClass, g_refusedList, "list_items", "a List") ? static_cast<const FakeList*>(list)->items
                                                                          : nullptr;
}

void* type_object(const char* typeKey) {
  void* const klass = class_for(typeKey);
  if (!klass) return nullptr;
  void*& object = g_typeObjects[klass];
  if (!object) object = allocate(kObjectBytes, &g_typeClass);
  return object;
}

void* object_new(const char* typeKey) {
  void* const klass = class_for(typeKey);
  return klass ? allocate(kObjectBytes, klass) : nullptr;
}

bool is_instance(const void* obj, const char* typeKey) {
  void* const klass = class_for(typeKey);
  return obj && klass && derives(class_of(obj), klass);
}

bool alive(const void* unityObject) { return unityObject && !g_destroyed.count(unityObject); }

const void* virtual_method(const void* obj, const char* methodKey) {
  const PgGameEntry* e = game_entry(methodKey);
  if (!obj || !e || e->kind != PG_ENTRY_METHOD) return nullptr;
  if (e->klass && !derives(class_of(obj), e->klass)) return nullptr;  // not of the declaring type
  for (void* k = class_of(obj); k; k = g_parents.count(k) ? g_parents[k] : nullptr) {
    const auto it = g_overrides.find({methodKey, k});
    if (it != g_overrides.end()) return it->second;
  }
  const auto declared = g_declared.find(methodKey);
  return declared == g_declared.end() ? nullptr : declared->second;
}

uint32_t gc_hold(void* obj) {
  if (!obj) return 0;
  g_holds[++g_nextHandle] = obj;
  return g_nextHandle;
}

void gc_release(uint32_t handle) { g_holds.erase(handle); }

// The storage of the static field `key` when it was given with `size` bytes, or null.
const Static* static_of(const char* key, size_t size) {
  const PgGameEntry* e = game_entry(key);
  const auto it = e && e->kind == PG_ENTRY_FIELD ? g_statics.find(key) : g_statics.end();
  return it != g_statics.end() && it->second.size == size ? &it->second : nullptr;
}

bool static_get(const char* key, void* out, size_t size) {
  const Static* s = static_of(key, size);
  if (!s || !out) return false;
  std::memcpy(out, s->storage, size);
  return true;
}

bool static_set(const char* key, const void* in, size_t size) {
  const Static* s = static_of(key, size);
  if (!s || !in) return false;
  if (s->reference) ++g_barrierWrites;
  std::memcpy(s->storage, in, size);
  return true;
}

// The fake's scene: what Transform.Find answers, the layer names, the Animators' parameters and the
// active scene's handle, and the scene facts the host would run over them.
std::map<std::pair<void*, std::string>, void*> g_children;
std::vector<std::string> g_layerNames;
std::map<const void*, std::vector<std::pair<std::string, int32_t>>> g_parameters;
std::set<const void*> g_wrongType;  // objects that are neither a Transform nor an Animator
std::map<std::string, const sdk::EntryDesc*> g_sceneFacts;  // the Scene and AnimatorParam entries given
int32_t g_scene = 1;
int g_finds = 0, g_parameterReads = 0;

const sdk::SceneEngine kEngine{
    [] { return g_scene; },
    [](void* root, const char* path, const char** why) -> void* {
      ++g_finds;
      if (g_wrongType.count(root)) {
        *why = "the root is not a Transform";
        return nullptr;
      }
      const auto it = g_children.find({root, path});
      return it != g_children.end() && !g_destroyed.count(it->second) ? it->second : nullptr;
    },
    [](const char* name, const char** why) -> int32_t {
      if (g_layerNames.empty()) {
        *why = "LayerMask.NameToLayer unresolved";
        return -1;
      }
      for (size_t i = 0; i < g_layerNames.size(); ++i)
        if (g_layerNames[i] == name) return static_cast<int32_t>(i);
      return -1;
    },
    [](const void* animator, const char* name, int32_t* hash, const char** why) {
      ++g_parameterReads;
      if (g_wrongType.count(animator)) {
        *why = "the object is not an Animator";
        return false;
      }
      const auto it = g_parameters.find(animator);
      if (it == g_parameters.end()) return false;
      for (const auto& [n, h] : it->second) {
        if (n != name) continue;
        *hash = h;
        return true;
      }
      return false;
    },
    [](const void* obj) { return obj && !g_destroyed.count(obj); },
    [](void* obj) { return gc_hold(obj); },
    [](uint32_t handle) { gc_release(handle); },
};

sdk::SceneFacts& facts() {
  static sdk::SceneFacts f;
  return f;
}

// The scene fact `key` was given and is of the kind asked for.
const sdk::EntryDesc* scene_fact_of(const char* key, sdk::Kind kind) {
  const auto it = key ? g_sceneFacts.find(key) : g_sceneFacts.end();
  return it != g_sceneFacts.end() && it->second->kind == kind ? it->second : nullptr;
}

void* scene_find(const char* key, void* root) {
  const sdk::EntryDesc* d = scene_fact_of(key, sdk::Kind::Scene);
  return d ? facts().find(*d, root) : nullptr;
}

bool animator_param(const char* key, const void* animator, int32_t* hash) {
  const sdk::EntryDesc* d = scene_fact_of(key, sdk::Kind::AnimatorParam);
  int32_t value = 0;
  if (!d || !hash || !facts().parameter(*d, animator, value)) return false;
  *hash = value;
  return true;
}

// The rest of the contract does nothing.
void log(const char*) {}
bool create_hook(void*, void*, void**, const char*) { return false; }
bool enable_hooks() { return false; }
bool send(uint8_t, const uint8_t*, size_t, int32_t, bool, int32_t) { return false; }
void set_receiver(uint8_t, PgReceiver) {}
bool peer_has_mod(int32_t, const char*) { return false; }
void register_service(const char*, const void*) {}
const void* service(const char*) { return nullptr; }
void log_exception(const char*, const void*) {}

const PgHostApi kHost{PGMOD_API_VERSION, nullptr, nullptr, L"", &log, &create_hook, &enable_hooks, &send,
                      &set_receiver, &peer_has_mod, &register_service, &service, &game_entry, &hook_entry,
                      &set_reference, &inside_entry, &string_new, &string_utf8, &array_length, &array_data,
                      &array_new, &list_count, &list_items, &type_object, &object_new, &is_instance, &alive,
                      &virtual_method, &gc_hold, &gc_release, &log_exception, &static_get, &static_set,
                      &hook_body, &body_count, &scene_find, &animator_param};

}  // namespace

void reset() {
  facts().set_engine(nullptr);  // releases what it kept while its holds still exist
  g_entries.clear();
  g_statics.clear();
  g_hooked.clear();
  g_barrierWrites = 0;
  g_memory.clear();
  g_parents.clear();
  g_overrides.clear();
  g_declared.clear();
  g_typeObjects.clear();
  g_destroyed.clear();
  g_holds.clear();
  g_refusedString = g_refusedArray = g_refusedList = false;
  g_children.clear();
  g_layerNames.clear();
  g_parameters.clear();
  g_wrongType.clear();
  g_sceneFacts.clear();
  g_scene = 1;
  g_finds = g_parameterReads = 0;
  facts().set_engine(&kEngine);
}

const PgHostApi* host() { return &kHost; }

void method(const sdk::EntryDesc& d, void* fn, void* owner) {
  PgGameEntry& e = entry_for(d, PG_ENTRY_METHOD);
  e.code = fn;
  e.klass = owner;
  g_declared[d.key] = allocate(sizeof(void*), fn);  // a MethodInfo begins with its code
}

void field(const sdk::EntryDesc& d, int32_t offset) { entry_for(d, PG_ENTRY_FIELD).offset = offset; }

void static_field(const sdk::EntryDesc& d, void* storage, size_t size, bool reference) {
  entry_for(d, PG_ENTRY_FIELD).field = storage;  // stands in for the FieldInfo
  g_statics[d.key] = {storage, size, reference};
}

void icall(const sdk::EntryDesc& d, void* fn) { entry_for(d, PG_ENTRY_ICALL).code = fn; }

void value(const sdk::EntryDesc& d, int64_t v) { entry_for(d, PG_ENTRY_ENUM_VALUE).value = v; }

void type(const sdk::EntryDesc& d, void* klass) { entry_for(d, PG_ENTRY_TYPE).klass = klass; }

void parent(void* klass, void* parentClass) { g_parents[klass] = parentClass; }

void override_method(const sdk::EntryDesc& d, void* klass, void* fn) {
  void* const info = allocate(sizeof(void*), fn);  // a MethodInfo begins with its code
  g_overrides[{d.key, klass}] = info;
}

void* array(size_t elementSize, size_t length) { return new_array(&g_arrayClass, elementSize, length); }

void* list(const std::vector<void*>& items) {
  void* const backing = new_array(&g_arrayClass, sizeof(void*), items.size());
  if (!items.empty()) std::memcpy(array_data(backing), items.data(), sizeof(void*) * items.size());
  auto* l = static_cast<FakeList*>(allocate(sizeof(FakeList), &g_listClass));
  l->items = backing;
  l->count = static_cast<int32_t>(items.size());
  return l;
}

void destroy(const void* unityObject) { g_destroyed.insert(unityObject); }

void scene_fact(const sdk::EntryDesc& d) {
  entry_for(d, d.kind == sdk::Kind::Scene ? PG_ENTRY_SCENE : PG_ENTRY_ANIMATOR_PARAM);
  g_sceneFacts[d.key] = &d;
}

void layer_names(const std::vector<std::string>& names) { g_layerNames = names; }

void layer(const sdk::EntryDesc& d) {
  g_entries.erase(d.key);
  const int32_t index = facts().layer(d);
  if (index >= 0) entry_for(d, PG_ENTRY_LAYER).value = index;
}

void child(void* root, const char* path, void* found) { g_children[{root, path}] = found; }

void load_scene() { ++g_scene; }

void wrong_type(const void* obj) { g_wrongType.insert(obj); }

void parameters(const void* animator, const std::vector<std::pair<std::string, int32_t>>& list) {
  g_parameters[animator] = list;
}

int finds() { return g_finds; }

int parameter_reads() { return g_parameterReads; }

std::vector<std::string> hooked() { return g_hooked; }

int barrier_writes() { return g_barrierWrites; }

int held() { return static_cast<int>(g_holds.size()); }

}  // namespace pg::fake
