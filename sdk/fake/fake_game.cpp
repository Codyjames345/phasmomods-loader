#include "fake_game.h"

#include <windows.h>

#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "log.h"
#include "utf16.h"

namespace pg::fake {
namespace {

std::map<std::string, PgGameEntry> g_entries;  // by key; a map never moves its values
std::vector<std::string> g_hooked;
int g_barrierWrites = 0;

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
                      &virtual_method, &gc_hold, &gc_release, &log_exception};

}  // namespace

void reset() {
  g_entries.clear();
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
}

const PgHostApi* host() { return &kHost; }

void method(const sdk::EntryDesc& d, void* fn, void* owner) {
  PgGameEntry& e = entry_for(d, PG_ENTRY_METHOD);
  e.code = fn;
  e.klass = owner;
  g_declared[d.key] = allocate(sizeof(void*), fn);  // a MethodInfo begins with its code
}

void field(const sdk::EntryDesc& d, int32_t offset) { entry_for(d, PG_ENTRY_FIELD).offset = offset; }

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

std::vector<std::string> hooked() { return g_hooked; }

int barrier_writes() { return g_barrierWrites; }

int held() { return static_cast<int>(g_holds.size()); }

}  // namespace pg::fake
