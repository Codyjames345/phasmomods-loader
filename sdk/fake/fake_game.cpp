#include "fake_game.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "content_acting.h"
#include "content_registry.h"
#include "log.h"
#include "scene_facts.h"
#include "utf16.h"

namespace pm::fake {
namespace {

std::map<std::string, PmGameEntry> g_entries;  // by key; a map never moves its values
std::vector<std::string> g_hooked;
std::vector<std::pair<std::string, void*>> g_detours;  // (method key, detour) of every hook made
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
std::set<std::string> g_inflated;                      // the Inflated method keys given (object_new makes their class)
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
  int32_t elementSize;  // the fake's own: 8 for references, 4 for a value_list
};

void* allocate(size_t bytes, void* klass) {
  std::vector<uint64_t>& block = g_memory.emplace_back((bytes + 7) / 8 + 1, 0);
  std::memcpy(block.data(), &klass, sizeof klass);
  return block.data();
}

std::map<const void*, size_t> g_elementBytes;  // each fake array's element size, as the runtime would say
std::map<std::string, size_t> g_typeElementBytes;  // element_size(): an array_new of that type's elements

void* new_array(void* klass, size_t elementBytes, size_t length) {
  void* arr = allocate(kArrayData + elementBytes * length, klass);
  g_elementBytes[arr] = elementBytes;
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
    PM_LOG("fake: %s was given an object that is not %s; it answers 0 or null", service, what);
  }
  return false;
}

PmGameEntry& entry_for(const sdk::EntryDesc& d, uint32_t kind) {
  g_statics.erase(d.key);
  PmGameEntry& e = g_entries[d.key];
  e = {};
  e.kind = kind;
  return e;
}

const PmGameEntry* game_entry(const char* key) {
  const auto it = key ? g_entries.find(key) : g_entries.end();
  return it == g_entries.end() ? nullptr : &it->second;
}

// The host contract's game_entry: as the host's, never a native site, which only the host acts on.
const PmGameEntry* mod_game_entry(const char* key) {
  const PmGameEntry* e = game_entry(key);
  return e && e->kind != PM_ENTRY_NATIVE_SITE ? e : nullptr;
}

void* class_for(const char* typeKey) {
  const PmGameEntry* e = game_entry(typeKey);
  return e && e->kind == PM_ENTRY_TYPE ? e->klass : nullptr;
}

// The host's hook table refuses a detour already hooked on the same code (host/hooks.cpp, `repeated`):
// so does the fake, by the method's key.
bool repeated(const char* key, void* detour) {
  const auto it = std::find(g_detours.begin(), g_detours.end(), std::make_pair(std::string(key), detour));
  if (it != g_detours.end()) return true;
  g_detours.emplace_back(key, detour);
  return false;
}

bool hook_entry(const char* key, void* detour, void** original, const char*, bool) {
  const PmGameEntry* e = game_entry(key);
  if (!e || e->kind != PM_ENTRY_METHOD || repeated(key, detour)) return false;
  g_hooked.emplace_back(key);
  if (original) *original = e->code;
  return true;
}

// A fake method has one body, its code.
bool hook_body(const char* key, uint32_t bodyIndex, void* detour, void** original, const char*) {
  const PmGameEntry* e = game_entry(key);
  if (!e || e->kind != PM_ENTRY_METHOD || bodyIndex != 0 || repeated(key, detour)) return false;
  g_hooked.emplace_back(key);
  if (original) *original = e->code;
  return true;
}

uint32_t body_count(const char* key) {
  const PmGameEntry* e = game_entry(key);
  return e && e->kind == PM_ENTRY_METHOD ? 1 : 0;
}

void set_reference(void* obj, int32_t offset, void* value) {
  ++g_barrierWrites;
  std::memcpy(static_cast<char*>(obj) + offset, &value, sizeof value);
}

std::map<std::string, int32_t> g_insideDepths;
int32_t inside_entry(const char* key) {
  const auto it = key ? g_insideDepths.find(key) : g_insideDepths.end();
  return it == g_insideDepths.end() ? 0 : it->second;
}

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
  if (!class_for(elementTypeKey)) return nullptr;
  const auto sized = g_typeElementBytes.find(elementTypeKey);
  return new_array(&g_arrayClass, sized != g_typeElementBytes.end() ? sized->second : kElementBytes, length);
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

// As the host's (gamemap::instance_class): a Type entry's class, or an Inflated entry's generic instance.
void* object_new(const char* typeKey) {
  void* klass = class_for(typeKey);
  if (!klass && typeKey && g_inflated.count(typeKey)) {
    const PmGameEntry* e = game_entry(typeKey);
    klass = e ? e->klass : nullptr;
  }
  return klass ? allocate(kObjectBytes, klass) : nullptr;
}

bool is_instance(const void* obj, const char* typeKey) {
  void* const klass = class_for(typeKey);
  return obj && klass && derives(class_of(obj), klass);
}

bool alive(const void* unityObject) { return unityObject && !g_destroyed.count(unityObject); }

const void* virtual_method(const void* obj, const char* methodKey) {
  const PmGameEntry* e = game_entry(methodKey);
  if (!obj || !e || e->kind != PM_ENTRY_METHOD) return nullptr;
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
  const PmGameEntry* e = game_entry(key);
  const auto it = e && e->kind == PM_ENTRY_FIELD ? g_statics.find(key) : g_statics.end();
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
std::map<std::string, int32_t> g_propertyIds;  // Shader.PropertyToID: each name its own id, from 100

int32_t property_id_of(const std::string& name) {
  const auto [it, added] = g_propertyIds.try_emplace(name, static_cast<int32_t>(100 + g_propertyIds.size()));
  return it->second;
}

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
    [](const char* name, int32_t* id, const char**) {
      *id = property_id_of(name);
      return true;
    },
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

// Content runs the host's own registry and acting-as dispatch.
content::Registry g_content;
std::optional<content::Acting> g_acting;
bool g_playerControlled = false;
bool g_rollTaken = false;

int32_t ghost_register(const PmGhostDef* def) { return g_content.add_ghost(def); }
int32_t evidence_register(const PmEvidenceDef* def) { return g_content.add_evidence(def); }
int32_t content_type(const char* ghostId) { return ghostId ? g_content.type_of(ghostId) : -1; }
const char* content_ghost_id(int32_t type) { return g_content.id_of(type); }
int32_t content_evidence(const char* evidenceId) { return evidenceId ? g_content.evidence_of(evidenceId) : -1; }
bool content_active() { return g_content.active(); }
bool content_player_controlled() { return g_playerControlled; }
int32_t ghost_rules(const char* ghostId, const PmGhostRules* rules) { return g_content.add_rules(ghostId, rules); }
int32_t content_acting_as() { return g_acting->acting_as(); }
int32_t content_identity() { return g_acting->identity(); }
int32_t journal_lines(const char* id, const PmTextSet* lines, uint32_t count) { return g_content.add_lines(id, lines, count); }
const char* content_evidence_look(const char* evidenceId) { return g_acting->evidence_look(evidenceId); }
bool content_use_ability() { return g_acting->use_ability(); }
// The fake's player reads English.
const char* content_ability_text(bool description) {
  const content::Ghost* g = g_acting->acting_ghost();
  const content::Text* text = g ? (description ? &g->abilityDescription : &g->abilityName) : nullptr;
  if (!text) return nullptr;
  for (const auto& [lang, value] : *text)
    if (lang == "en") return value.c_str();
  return nullptr;
}
int32_t content_hud_state() { return g_acting->hud_state(); }
const char* content_hud_line(int32_t state) { return g_acting->hud_line(state); }
bool content_held_back(int32_t kind) { return g_acting->held_back(kind); }
uint32_t content_player_view(float* xyz, uint32_t max) { return g_acting->player_view(xyz, max); }
void content_player_move(const float* at, float* dx, float* dz) { g_acting->player_move(at, dx, dz); }
bool content_interaction_lost() { return g_acting->interaction_lost(); }
bool content_roll_taken() { return g_rollTaken; }
void* content_target(const void* const* excluded, uint32_t excludedCount) {
  return g_acting->target(excluded, excludedCount);
}
int g_collectionClass = 0;  // of the fake HashSets (collection())
bool g_refusedCollection = false;
// A fake HashSet: its class, then its count.
struct FakeCollection {
  void* klass;
  int32_t count;
};
int32_t collection_count(const void* c) {
  if (c && class_of(c) == &g_listClass) return static_cast<const FakeList*>(c)->count;
  return of_kind(c, g_collectionClass, g_refusedCollection, "collection_count", "a collection")
             ? static_cast<const FakeCollection*>(c)->count
             : 0;
}
size_t array_element_size(const void* arr) {
  if (!of_kind(arr, g_arrayClass, g_refusedArray, "array_element_size", "an array")) return 0;
  const auto it = g_elementBytes.find(arr);
  return it == g_elementBytes.end() ? 0 : it->second;
}
bool host_list_remove_at(void* list, int32_t index) {
  return of_kind(list, g_listClass, g_refusedList, "list_remove_at", "a List") && pm::fake::list_remove_at(list, index);
}
bool g_roomAgrees = false;
bool room_agrees() { return g_roomAgrees; }
uint32_t content_flags() {
  const content::Ghost* g = g_acting->acting_ghost();
  return g ? g->flags : 0u;
}
uint32_t content_flags_of(int32_t type) {
  const char* id = g_content.id_of(type);
  const content::Ghost* g = id ? g_content.ghost(id) : nullptr;
  return g ? g->flags : 0u;
}

// The rest of the contract does nothing.
void log(const char*) {}
bool create_hook(void*, void*, void**, const char*) { return false; }
bool enable_hooks() { return false; }
bool send(uint8_t, const uint8_t*, size_t, int32_t, bool, int32_t) { return false; }
void set_receiver(uint8_t, PmReceiver) {}
bool peer_has_mod(int32_t, const char*) { return false; }
void register_service(const char*, const void*) {}
const void* service(const char*) { return nullptr; }
void log_exception(const char*, const void*) {}
// The host contract's abandon_ghost_spawn and dictionary_int_entries (fake_game.h).
bool g_inSpawn = false;
int g_abandons = 0;
void abandon_ghost_spawn() {
  if (!g_inSpawn) return;
  ++g_abandons;
  throw pm::fake::SpawnAbandoned{};
}
std::map<const void*, std::vector<PmIntEntry>> g_dictionaries;
int32_t dictionary_int_entries(const void* dict, PmIntEntry* out, int32_t max) {
  const auto it = g_dictionaries.find(dict);
  if (it == g_dictionaries.end() || !out || max <= 0) return 0;
  int32_t written = 0;
  for (const PmIntEntry& e : it->second) {
    if (written == max) break;
    out[written++] = e;
  }
  return written;
}
// The host contract's mod_started: the mods a test says run on this client (fake_game.h).
std::set<std::string, std::less<>> g_startedMods;
bool mod_started(const char* modId) { return modId && g_startedMods.count(std::string_view(modId)); }
// The host contract's mod_name: the names a test gives its mods (fake_game.h).
std::map<std::string, std::string, std::less<>> g_modNames;
const char* mod_name(const char* modId) {
  const auto it = modId ? g_modNames.find(std::string_view(modId)) : g_modNames.end();
  return it == g_modNames.end() ? nullptr : it->second.c_str();
}
// The host contract's content_packs and content_pack_failed: the packs a test gives, and the failures reported
// (fake_game.h).
std::vector<Pack> g_packs;
std::vector<std::pair<std::string, std::string>> g_failedPacks;
uint32_t content_packs(const char* modId, PmContentPack* out, uint32_t max, uint32_t itemSize) {
  if (!modId || itemSize < sizeof(PmContentPack)) return 0;
  uint32_t count = 0;
  for (const Pack& p : g_packs) {
    if (p.framework != modId) continue;
    if (out && count < max) {
      const PmContentPack entry{sizeof(PmContentPack), p.id.c_str(), p.name.c_str(), p.version.c_str(), p.folder.c_str(),
                                p.text.c_str()};
      std::memcpy(reinterpret_cast<unsigned char*>(out) + static_cast<size_t>(count) * itemSize, &entry, sizeof entry);
    }
    ++count;
  }
  return count;
}
void content_pack_failed(const char* packId, const char* why) {
  if (packId) g_failedPacks.emplace_back(packId, why ? why : "");
}
// The host contract's show_message: every message asked for, in order (fake_game.h).
std::vector<std::pair<std::string, std::string>> g_messages;
void show_message(const char* title, const char* body) {
  if (title && body) g_messages.emplace_back(title, body);
}

// The host contract's unity_action_new: records every ask (the target and the listener) and answers a fresh
// token, which is not a fake object and is never read back: a test invokes the listener itself, as the
// game's event would (fake_game.h).
std::vector<std::pair<void*, void (*)(void*)>> g_unityActions;
uintptr_t g_actionTokens = 1;
void* unity_action_new(void* target, void (*listener)(void*)) {
  if (!target || !listener) return nullptr;
  g_unityActions.push_back({target, listener});
  return reinterpret_cast<void*>(g_actionTokens++);
}

// What the host contract's steer_agent was asked, and whether it can steer (fake_game.h).
std::vector<SteerAsk> g_steerAsks;
bool g_steerRefused = false;

bool steer_agent(void* agent, float dx, float dz) {
  if (g_steerRefused) return false;
  if (agent) g_steerAsks.push_back({agent, dx, dz});
  return true;
}

// The host contract's list_add on a fake List of references, as the List's own Add.
bool list_add(void* list, void* item) {
  return of_kind(list, g_listClass, g_refusedList, "list_add", "a List") &&
         list_insert(list, static_cast<FakeList*>(list)->count, item);
}

// The fields each class declares, as declare_fields() gave them.
std::map<void*, std::vector<DeclaredField>> g_declaredFields;

// The host contract's copy_settings over the fields declare_fields() gave, as the host reads them from
// the runtime's metadata: each public value field whole, each reference in it through the barrier. All
// or none, as the host's: a public value field of size 0 stands for one the host cannot read, and then
// nothing is copied.
bool copy_settings(const char* typeKey, void* dst, const void* src) {
  void* const klass = class_for(typeKey);
  if (!klass || !is_instance(dst, typeKey) || !is_instance(src, typeKey)) return false;
  const std::vector<DeclaredField>& fields = g_declaredFields[klass];
  if (std::any_of(fields.begin(), fields.end(), [](const DeclaredField& f) { return f.publicValue && f.size == 0; }))
    return false;
  for (const DeclaredField& f : fields) {
    if (!f.publicValue) continue;
    std::memcpy(static_cast<char*>(dst) + f.offset, static_cast<const char*>(src) + f.offset, f.size);
    for (size_t at : f.references) {
      void* value = nullptr;
      std::memcpy(&value, static_cast<const char*>(src) + f.offset + at, sizeof value);
      set_reference(dst, static_cast<int32_t>(f.offset + at), value);
    }
  }
  return true;
}

// The host contract's fields_of_type over the fields declare_fields() gave, by offset.
uint32_t fields_of_type(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                        uint32_t max) {
  void* const klass = class_for(typeKey);
  void* const fieldClass = class_for(fieldTypeKey);
  if (!klass || !fieldClass || !is_instance(obj, typeKey) || (max && !out)) return 0;
  std::vector<DeclaredField> fields = g_declaredFields[klass];
  std::sort(fields.begin(), fields.end(), [](const DeclaredField& a, const DeclaredField& b) { return a.offset < b.offset; });
  uint32_t count = 0;
  for (const DeclaredField& f : fields) {
    if (f.type != fieldClass || f.array != arrayOf) continue;
    if (count < max) std::memcpy(&out[count], static_cast<const char*>(obj) + f.offset, sizeof(void*));
    ++count;
  }
  return count;
}

// The host contract's mod_entry: the fake game installs no mods, so the list is empty.
bool mod_entry(int32_t, PmModEntry*) { return false; }

// Positional: a member added to PmHostApi after content_pack_failed fails this until it is given here too.
static_assert(offsetof(PmHostApi, content_pack_failed) + sizeof(PmHostApi::content_pack_failed) == sizeof(PmHostApi),
              "kHost lists every PmHostApi member");
const PmHostApi kHost{PMMOD_API_VERSION, nullptr, nullptr, L"", &log, &create_hook, &enable_hooks, &send,
                      &set_receiver, &peer_has_mod, &register_service, &service, &mod_game_entry, &hook_entry,
                      &set_reference, &inside_entry, &string_new, &string_utf8, &array_length, &array_data,
                      &array_new, &list_count, &list_items, &type_object, &object_new, &is_instance, &alive,
                      &virtual_method, &gc_hold, &gc_release, &log_exception, &static_get, &static_set,
                      &hook_body, &body_count, &scene_find, &animator_param, &ghost_register,
                      &evidence_register, &content_type, &content_ghost_id, &content_evidence, &content_active,
                      &content_player_controlled, &ghost_rules, &content_acting_as, &content_identity, &journal_lines,
                      &content_evidence_look, &list_add, &copy_settings, &fields_of_type, &content_use_ability,
                      &content_ability_text, &content_hud_state, &content_hud_line, &content_held_back,
                      &content_player_view, &content_target, &content_flags,
                      &room_agrees, &collection_count, &array_element_size, &host_list_remove_at,
                      &content_player_move, &content_interaction_lost, &content_roll_taken, &steer_agent,
                      &abandon_ghost_spawn, &dictionary_int_entries, &mod_started, &show_message,
                      &content_flags_of, &mod_name, &unity_action_new, &mod_entry, &content_packs,
                      &content_pack_failed};

}  // namespace

void reset() {
  facts().set_engine(nullptr);  // releases what it kept while its holds still exist
  g_entries.clear();
  g_inflated.clear();
  g_steerAsks.clear();
  g_steerRefused = false;
  g_statics.clear();
  g_hooked.clear();
  g_detours.clear();
  g_barrierWrites = 0;
  g_memory.clear();
  g_elementBytes.clear();
  g_typeElementBytes.clear();
  g_parents.clear();
  g_overrides.clear();
  g_declared.clear();
  g_declaredFields.clear();
  g_typeObjects.clear();
  g_destroyed.clear();
  g_holds.clear();
  g_refusedString = g_refusedArray = g_refusedList = g_refusedCollection = false;
  g_children.clear();
  g_layerNames.clear();
  g_propertyIds.clear();
  g_parameters.clear();
  g_wrongType.clear();
  g_sceneFacts.clear();
  g_scene = 1;
  g_insideDepths.clear();
  g_dictionaries.clear();
  g_startedMods.clear();
  g_modNames.clear();
  g_packs.clear();
  g_failedPacks.clear();
  g_messages.clear();
  g_unityActions.clear();
  g_inSpawn = false;
  g_abandons = 0;
  g_finds = g_parameterReads = 0;
  facts().set_engine(&kEngine);
  g_content = content::Registry{};
  g_acting.emplace(g_content);
  game_enums(audited_ghost_types(), audited_evidence(), kAuditedNoGhost, kAuditedNoEvidence);
  g_content.begin_group();  // as while a mod starts: registration is open until commit_content
  g_playerControlled = false;
  g_rollTaken = false;
  g_roomAgrees = false;
}

const PmHostApi* host() { return &kHost; }

void method(const sdk::EntryDesc& d, void* fn, void* owner) {
  PmGameEntry& e = entry_for(d, PM_ENTRY_METHOD);
  e.code = fn;
  e.klass = owner;
  if (d.kind == sdk::Kind::Inflated) g_inflated.insert(d.key);
  g_declared[d.key] = allocate(sizeof(void*), fn);  // a MethodInfo begins with its code
}

void field(const sdk::EntryDesc& d, int32_t offset) { entry_for(d, PM_ENTRY_FIELD).offset = offset; }

void static_field(const sdk::EntryDesc& d, void* storage, size_t size, bool reference) {
  entry_for(d, PM_ENTRY_FIELD).field = storage;  // stands in for the FieldInfo
  g_statics[d.key] = {storage, size, reference};
}

void icall(const sdk::EntryDesc& d, void* fn) { entry_for(d, PM_ENTRY_ICALL).code = fn; }

void value(const sdk::EntryDesc& d, int64_t v) { entry_for(d, PM_ENTRY_ENUM_VALUE).value = v; }

void type(const sdk::EntryDesc& d, void* klass) { entry_for(d, PM_ENTRY_TYPE).klass = klass; }

void native_site(const sdk::EntryDesc& d, void* code) { entry_for(d, PM_ENTRY_NATIVE_SITE).code = code; }

void parent(void* klass, void* parentClass) { g_parents[klass] = parentClass; }

void declare_fields(void* klass, std::vector<DeclaredField> fields) { g_declaredFields[klass] = std::move(fields); }

void override_method(const sdk::EntryDesc& d, void* klass, void* fn) {
  void* const info = allocate(sizeof(void*), fn);  // a MethodInfo begins with its code
  g_overrides[{d.key, klass}] = info;
}

void* array(size_t elementSize, size_t length) { return new_array(&g_arrayClass, elementSize, length); }

void element_size(const sdk::EntryDesc& d, size_t bytes) { g_typeElementBytes[d.key] = bytes; }

void* collection(int32_t count) {
  auto* c = static_cast<FakeCollection*>(allocate(sizeof(FakeCollection), &g_collectionClass));
  c->count = count;
  return c;
}

void* struct_list(size_t elementSize, size_t count) {
  auto* l = static_cast<FakeList*>(allocate(sizeof(FakeList), &g_listClass));
  l->items = new_array(&g_arrayClass, elementSize, count);
  l->count = static_cast<int32_t>(count);
  l->elementSize = static_cast<int32_t>(elementSize);
  return l;
}

void* list(const std::vector<void*>& items) {
  void* const backing = new_array(&g_arrayClass, sizeof(void*), items.size());
  if (!items.empty()) std::memcpy(array_data(backing), items.data(), sizeof(void*) * items.size());
  auto* l = static_cast<FakeList*>(allocate(sizeof(FakeList), &g_listClass));
  l->items = backing;
  l->count = static_cast<int32_t>(items.size());
  l->elementSize = sizeof(void*);
  return l;
}

void* value_list(const std::vector<int32_t>& values) {
  void* const backing = new_array(&g_arrayClass, sizeof(int32_t), values.size());
  if (!values.empty()) std::memcpy(array_data(backing), values.data(), sizeof(int32_t) * values.size());
  auto* l = static_cast<FakeList*>(allocate(sizeof(FakeList), &g_listClass));
  l->items = backing;
  l->count = static_cast<int32_t>(values.size());
  l->elementSize = sizeof(int32_t);
  return l;
}

std::vector<int32_t> list_values(const void* list) {
  std::vector<int32_t> out;
  const auto* l = static_cast<const FakeList*>(list);
  if (!list || l->elementSize != sizeof(int32_t)) return out;
  out.resize(static_cast<size_t>(l->count));
  if (l->count) std::memcpy(out.data(), array_data(l->items), sizeof(int32_t) * out.size());
  return out;
}

std::vector<void*> list_items_of(const void* list) {
  std::vector<void*> out;
  const auto* l = static_cast<const FakeList*>(list);
  if (!list || l->elementSize != sizeof(void*)) return out;
  out.resize(static_cast<size_t>(l->count));
  if (l->count) std::memcpy(out.data(), array_data(l->items), sizeof(void*) * out.size());
  return out;
}

namespace {

// Replaces the list's backing array with a new one of `count` elements, `fill` writing them.
template <class Fill>
void rebuild(FakeList* l, int32_t count, Fill fill) {
  void* const backing = new_array(&g_arrayClass, static_cast<size_t>(l->elementSize), static_cast<size_t>(count));
  fill(static_cast<char*>(array_data(l->items)), static_cast<char*>(array_data(backing)));
  l->items = backing;
  l->count = count;
}

}  // namespace

bool list_insert(void* list, int32_t index, void* item) {
  auto* l = static_cast<FakeList*>(list);
  if (!list || l->elementSize != sizeof(void*) || index < 0 || index > l->count) return false;
  rebuild(l, l->count + 1, [&](char* from, char* to) {
    std::memcpy(to, from, sizeof(void*) * static_cast<size_t>(index));
    std::memcpy(to + sizeof(void*) * static_cast<size_t>(index), &item, sizeof item);
    std::memcpy(to + sizeof(void*) * static_cast<size_t>(index + 1), from + sizeof(void*) * static_cast<size_t>(index),
                sizeof(void*) * static_cast<size_t>(l->count - index));
  });
  return true;
}

void set_inside(const sdk::EntryDesc& d, int32_t depth) { g_insideDepths[d.key] = depth; }
void spawn_running(bool running) { g_inSpawn = running; }
int spawn_abandons() { return g_abandons; }
void dictionary(const void* dict, std::vector<PmIntEntry> entries) { g_dictionaries[dict] = std::move(entries); }
void content_packs(std::vector<Pack> packs) { g_packs = std::move(packs); }
const std::vector<std::pair<std::string, std::string>>& failed_packs() { return g_failedPacks; }
void started_mods(std::vector<std::string> ids) { g_startedMods = {ids.begin(), ids.end()}; }
void mod_names(std::map<std::string, std::string> names) { g_modNames = {names.begin(), names.end()}; }
const std::vector<std::pair<std::string, std::string>>& messages() { return g_messages; }
const std::vector<std::pair<void*, void (*)(void*)>>& unity_actions() { return g_unityActions; }

bool list_remove_at(void* list, int32_t index) {
  auto* l = static_cast<FakeList*>(list);
  if (!list || index < 0 || index >= l->count) return false;
  const size_t size = static_cast<size_t>(l->elementSize);
  rebuild(l, l->count - 1, [&](char* from, char* to) {
    std::memcpy(to, from, size * static_cast<size_t>(index));
    std::memcpy(to + size * static_cast<size_t>(index), from + size * static_cast<size_t>(index + 1),
                size * static_cast<size_t>(l->count - index - 1));
  });
  return true;
}

bool list_add_value(void* list, int32_t value) {
  auto* l = static_cast<FakeList*>(list);
  if (!list || l->elementSize != sizeof(int32_t)) return false;
  rebuild(l, l->count + 1, [&](char* from, char* to) {
    std::memcpy(to, from, sizeof(int32_t) * static_cast<size_t>(l->count));
    std::memcpy(to + sizeof(int32_t) * static_cast<size_t>(l->count), &value, sizeof value);
  });
  return true;
}

void destroy(const void* unityObject) { g_destroyed.insert(unityObject); }

void scene_fact(const sdk::EntryDesc& d) {
  entry_for(d, d.kind == sdk::Kind::Scene ? PM_ENTRY_SCENE : PM_ENTRY_ANIMATOR_PARAM);
  g_sceneFacts[d.key] = &d;
}

void layer_names(const std::vector<std::string>& names) { g_layerNames = names; }

void layer(const sdk::EntryDesc& d) {
  g_entries.erase(d.key);
  const int32_t index = facts().layer(d);
  if (index >= 0) entry_for(d, PM_ENTRY_LAYER).value = index;
}

void shader_property(const sdk::EntryDesc& d) {
  g_entries.erase(d.key);
  int32_t id = 0;
  if (facts().shader_property(d, id)) entry_for(d, PM_ENTRY_SHADER_PROPERTY).value = id;
}

int32_t property_id(const char* name) { return property_id_of(name ? name : ""); }

void child(void* root, const char* path, void* found) { g_children[{root, path}] = found; }

void load_scene() { ++g_scene; }

void wrong_type(const void* obj) { g_wrongType.insert(obj); }

void parameters(const void* animator, const std::vector<std::pair<std::string, int32_t>>& list) {
  g_parameters[animator] = list;
}

int finds() { return g_finds; }

int parameter_reads() { return g_parameterReads; }

void room(int localActor, const std::vector<int>& actors, uint64_t now, int master) { g_content.observe(localActor, actors, now, master); }

void commit_content() {
  g_content.end_group(true);
  g_content.commit();
}

content::State content_state() { return g_content.state(); }

void game_enums(const content::EnumValues& ghostTypes, const content::EnumValues& evidenceTypes, int64_t noGhost,
                int64_t noEvidence) {
  content::GameEnums enums;
  if (const std::string why = content::read_game_enums(ghostTypes, evidenceTypes, noGhost, noEvidence, enums);
      why.empty())
    g_content.set_game_enums(std::move(enums));
  else
    g_content.refuse(why);
}

content::EnumValues audited_ghost_types() {
  static const char* const kNames[] = {
      "Spirit", "Wraith", "Phantom", "Poltergeist", "Banshee", "Jinn",   "Mare",   "Revenant", "Shade",  "Demon",  "Yurei",
      "Oni",    "Yokai",  "Hantu",   "Goryo",       "Myling",  "Onryo",  "TheTwins", "Raiju",  "Obake",  "Mimic",  "Moroi",
      "Deogen", "Thaye",  "None",    "Gallu",       "Dayan",   "Obambo", "Kormos", "Aswang",   "Deildegast"};
  content::EnumValues out;
  for (size_t i = 0; i < std::size(kNames); ++i) out.emplace_back(kNames[i], static_cast<int64_t>(i));
  return out;
}

content::EnumValues audited_evidence() {
  return {{"None", 0},     {"EMF", 1},      {"SpiritBox", 2},        {"ultraViolet", 3},
          {"GhostOrb", 4}, {"GhostWritingBook", 5}, {"Temperature", 6}, {"DotsProjector", 7}};
}

void host_message(int sender, const uint8_t* data, size_t size) { g_content.receive(sender, data, size); }

std::vector<std::vector<uint8_t>> host_messages() { return g_content.take_messages(); }

void forget(const sdk::EntryDesc& d) {
  g_entries.erase(d.key);
  g_statics.erase(d.key);
}

const PmGameEntry* host_entry(const char* key) { return game_entry(key); }

const std::vector<SteerAsk>& steer_asks() { return g_steerAsks; }
void refuse_steering() { g_steerRefused = true; }

void player_controlled(bool controlled) { g_playerControlled = controlled; }
void roll_taken(bool taken) { g_rollTaken = taken; }
void room_agrees(bool agrees) { g_roomAgrees = agrees; }

content::Registry& content_registry() { return g_content; }

content::Acting& acting() { return *g_acting; }

void contract_ghost(int32_t identity, int32_t behaviour, bool master) {
  g_acting->observe(identity, behaviour, master);
}

void tick_rules() { g_acting->tick(); }

std::vector<std::string> hooked() { return g_hooked; }

int barrier_writes() { return g_barrierWrites; }

int held() { return static_cast<int>(g_holds.size()); }

bool held(const void* object) {
  for (const auto& [handle, obj] : g_holds)
    if (obj == object) return true;
  return false;
}

}  // namespace pm::fake
