#include "registry.h"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::custom::registry {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kControllerCollection = 0x90;  // GhostController.allGhostsCollection
constexpr size_t kCollectionGhosts = 0x18;      // GhostCollection.ghosts: List<GhostData>
constexpr size_t kDataType = 0x18;              // GhostData.type
constexpr size_t kDataNameKey = 0x20;           // GhostData.ghostNameLocalisationKey
constexpr size_t kDataEvidence = 0x28;          // GhostData.evidence1..3 (int each)
constexpr size_t kGhostInfo = 0x38;             // GhostAI.GhostInfo
constexpr size_t kInfoType = 0x28;              // GhostInfo: the ghost's GhostType (what it is)
constexpr size_t kInfoBehaviour = 0x2C;         // GhostInfo: the type whose rules its AI follows
constexpr size_t kTraitsBehaviour = 0x4;        // the traits struct GhostAI.Init receives: behaviour type
constexpr size_t kTraitsSize = 0x40;            // as GhostAI.Init and StartGhost copy it
constexpr uintptr_t kLocalisedValueRva = 0x8D1EF0;  // LocalisationSystem.GetLocalisedValue(string, bool)
constexpr uintptr_t kGhostInitRva = 0x2022A70;      // GhostAI.Init(traits), every client (CreateGhost)
constexpr uintptr_t kSyncSendRva = 0x206C8B0;       // GhostInfo.SyncValues(traits): host (StartGhost), sends the RPC
constexpr uintptr_t kSyncValuesRva = 0x206C500;     // GhostInfo.SyncValuesNetworked, every client

struct Api {
  il2cpp::Class* ghostController = nullptr;
  il2cpp::Class* ghostData = nullptr;
} api;

il2cpp::Held g_collection;  // the contract's GhostCollection, while it holds the entries
il2cpp::Held g_decided;     // the contract's collection whose entries were decided on (added or not)

using Localised_t = il2cpp::String* (*)(il2cpp::String*, bool, const void*);
using Init_t = void (*)(void*, uint8_t*, const void*);
using SyncSend_t = void (*)(void*, const uint8_t*, const void*);
using SyncValues_t = void (*)(void*, int, int, bool, int, int, bool, int, int, bool, void*, const void*);
Localised_t g_localised = nullptr;
Init_t g_init = nullptr;
SyncSend_t g_syncSend = nullptr;
SyncValues_t g_syncValues = nullptr;

/** A localisation key that may be one of the custom names ("Journal_NNTitle"), checked cheaply first. */
bool maybe_custom_key(const il2cpp::String* key) {
  return key && key->length > 0 && maybe_title_key(std::u16string_view(key->chars, static_cast<size_t>(key->length)));
}

il2cpp::String* detour_localised(il2cpp::String* key, bool fallback, const void* method) {
  hooks::Guard guard;
  if (maybe_custom_key(key)) {
    char utf8[32];
    il2cpp::to_utf8(key, utf8, sizeof utf8);
    if (const Ghost* g = find(type_of_title_key(utf8))) return il2cpp::string_new(g->name);
  }
  return g_localised(key, fallback, method);
}

// The traits reach every client in GhostAI.Init and GhostInfo.SyncValuesNetworked; a custom ghost's
// behaviour type is set to GhostType.None before its AI runs.
void detour_init(void* ai, uint8_t* traits, const void* method) {
  hooks::Guard guard;
  if (traits && is_custom(*reinterpret_cast<int*>(traits)))
    *reinterpret_cast<int*>(traits + kTraitsBehaviour) = kBehaviourType;
  g_init(ai, traits, method);
}

// The host's SyncValues sends the traits' behaviour type as the ghost's type, and
// SyncValuesNetworked writes that one type to both GhostInfo fields on every client. For a custom
// ghost that would be None: the custom type is sent instead, and detour_sync_values sets the
// behaviour back to None after the sync.
void detour_sync_send(void* info, const uint8_t* traits, const void* method) {
  hooks::Guard guard;
  if (!traits || !is_custom(*reinterpret_cast<const int*>(traits))) {
    g_syncSend(info, traits, method);
    return;
  }
  uint8_t sent[kTraitsSize];
  std::memcpy(sent, traits, sizeof sent);
  std::memcpy(sent + kTraitsBehaviour, sent, sizeof(int));
  g_syncSend(info, sent, method);
}

void detour_sync_values(void* info, int type, int a, bool b, int c, int d, bool e, int f, int g, bool h, void* message,
                        const void* method) {
  hooks::Guard guard;
  g_syncValues(info, type, a, b, c, d, e, f, g, h, message, method);
  if (info && is_custom(il2cpp::field<int>(info, kInfoType))) il2cpp::set_field<int>(info, kInfoBehaviour, kBehaviourType);
}

/** Call a one-argument List<T> method (Remove) for a reference-type item. */
bool list_call(il2cpp::List* list, const char* name, void* item) {
  const il2cpp::Method* m = list ? il2cpp::method(il2cpp::class_of(list), name, 1) : nullptr;
  if (!m) return false;
  reinterpret_cast<void (*)(void*, void*, const void*)>(*reinterpret_cast<void* const*>(m))(list, item, m);
  return true;
}

/** List<T>.Insert(index, item) for a reference-type item. */
bool list_insert(il2cpp::List* list, int index, void* item) {
  const il2cpp::Method* m = list ? il2cpp::method(il2cpp::class_of(list), "Insert", 2) : nullptr;
  if (!m) return false;
  reinterpret_cast<void (*)(void*, int, void*, const void*)>(*reinterpret_cast<void* const*>(m))(list, index, item, m);
  return true;
}

/** The name an entry sorts by: a custom ghost's own, a game ghost's from its name key. */
std::string sort_name(void* data) {
  if (const Ghost* g = find(il2cpp::field<int>(data, kDataType))) return g->name;
  char key[64];
  il2cpp::to_utf8(il2cpp::field<il2cpp::String*>(data, kDataNameKey), key, sizeof key);
  return std::string(title_key_name(key));
}

/** Where `name` goes to keep the collection in the game's alphabetical order. */
int sorted_index(il2cpp::List* ghosts, std::string_view name) {
  for (int i = 0; ghosts->items && i < ghosts->size; ++i) {
    void* data = ghosts->items->data<void*>()[i];
    if (data && sorts_before(name, sort_name(data))) return i;
  }
  return ghosts->size;
}

bool holds_type(il2cpp::List* ghosts, int type) {
  for (int i = 0; ghosts && ghosts->items && i < ghosts->size; ++i) {
    void* data = ghosts->items->data<void*>()[i];
    if (data && il2cpp::field<int>(data, kDataType) == type) return true;
  }
  return false;
}

il2cpp::List* ghosts_of(void* collection) {
  return collection && game::alive(collection) ? il2cpp::field<il2cpp::List*>(collection, kCollectionGhosts) : nullptr;
}

void* new_entry(const Ghost& ghost) {
  // A plain managed GhostData: the collection's readers use only its fields, never Unity's side.
  void* data = il2cpp::object_new(api.ghostData);
  if (!data) return nullptr;
  il2cpp::set_field<int>(data, kDataType, ghost.type);
  il2cpp::set_ref_field(data, kDataNameKey, il2cpp::string_new(title_key(ghost.type).c_str()));
  for (int i = 0; i < 3; ++i) il2cpp::set_field<int>(data, kDataEvidence + 4 * i, ghost.evidence[i]);
  return data;
}

void* contract_collection() {
  void* level = game::level_controller();
  void* controller = il2cpp::static_field_by_type(api.ghostController, "GhostController");
  if (!level || !game::alive(level) || !controller || !game::alive(controller)) return nullptr;
  return il2cpp::field<void*>(controller, kControllerCollection);
}

/** Add the custom ghosts a collection lacks, each in its alphabetical place among the game's (the
 * Ghost Type option lists them in collection order); how many it holds afterwards. */
int add_entries(il2cpp::List* ghosts) {
  int held = 0;
  for (const Ghost& g : kGhosts) {
    if (!holds_type(ghosts, g.type)) {
      void* e = new_entry(g);
      if (!e || !list_insert(ghosts, sorted_index(ghosts, g.name), e)) continue;
    }
    ++held;
  }
  return held;
}

/** Take every custom ghost out of a collection, whoever added it; how many were taken. */
int remove_entries(il2cpp::List* ghosts) {
  std::vector<void*> custom;
  for (int i = 0; ghosts && ghosts->items && i < ghosts->size; ++i) {
    void* data = ghosts->items->data<void*>()[i];
    if (data && is_custom(il2cpp::field<int>(data, kDataType))) custom.push_back(data);
  }
  for (void* e : custom) list_call(ghosts, "Remove", e);
  return static_cast<int>(custom.size());
}

void withdraw_from_contract() {
  // An unloaded collection took the entries with it.
  if (remove_entries(ghosts_of(g_collection)) > 0) PG_LOG("registry: custom ghosts withdrawn");
  g_collection = nullptr;
}

}  // namespace

void offer(void* collection) {
  if (il2cpp::List* ghosts = ghosts_of(collection)) add_entries(ghosts);
}

void withdraw(void* collection) {
  if (il2cpp::List* ghosts = ghosts_of(collection)) remove_entries(ghosts);
}

bool resolve() {
  api.ghostController = il2cpp::cls("Assembly-CSharp.dll", "", "GhostController");
  api.ghostData = il2cpp::cls("Assembly-CSharp.dll", "", "GhostData");
  return api.ghostController && api.ghostData;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kLocalisedValueRva), reinterpret_cast<void*>(&detour_localised),
                       reinterpret_cast<void**>(&g_localised), "LocalisationSystem.GetLocalisedValue") &&
         hooks::create(game::game_assembly_address(kGhostInitRva), reinterpret_cast<void*>(&detour_init),
                       reinterpret_cast<void**>(&g_init), "GhostAI.Init") &&
         hooks::create(game::game_assembly_address(kSyncSendRva), reinterpret_cast<void*>(&detour_sync_send),
                       reinterpret_cast<void**>(&g_syncSend), "GhostInfo.SyncValues") &&
         hooks::create(game::game_assembly_address(kSyncValuesRva), reinterpret_cast<void*>(&detour_sync_values),
                       reinterpret_cast<void**>(&g_syncValues), "GhostInfo.SyncValuesNetworked");
}

void tick(bool enabled) {
  // Decided once per contract, when its collection appears, and kept until the contract unloads: a
  // peer's presence can lapse during a hitch or a level load, and entries withdrawn mid-contract
  // would break the evidence sync and the journals of anyone still using them.
  void* collection = contract_collection();
  if (collection == g_decided) return;
  if (g_collection) withdraw_from_contract();
  g_decided = collection;
  il2cpp::List* ghosts = ghosts_of(collection);
  if (!ghosts) return;
  if (enabled) {
    const int held = add_entries(ghosts);
    if (held > 0) g_collection = collection;
    PG_LOG("registry: %d custom ghosts in the ghost collection", held);
    return;
  }
  // The lobby's custom difficulty menu may share the collection and have left them in it.
  remove_entries(ghosts);
  PG_LOG("registry: not every player runs the custom ghosts; this contract stays vanilla");
}

bool decided() { return g_decided != nullptr && game::alive(g_decided); }

bool active() { return g_collection != nullptr; }

void teardown() {
  withdraw_from_contract();
  g_decided = nullptr;
}

void* custom_ghost(int& type) {
  void* ai = game::ghost_ai();
  void* info = ai && game::alive(ai) ? il2cpp::field<void*>(ai, kGhostInfo) : nullptr;
  type = info ? il2cpp::field<int>(info, kInfoType) : 0;
  return is_custom(type) ? ai : nullptr;
}

}  // namespace pg::custom::registry
