#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"

// The ghost's type on its way from the host's spawn to every client's GhostInfo, through the
// registry's detours and fakes that do with each field what the game's code does (REA, build
// 25811160): GhostAI.Init copies the traits into GhostInfo +0x28 and calls StartGhost, which on
// the host calls GhostInfo.SyncValues; SyncValues sends the traits' second type (+0x4) as the
// ghost's type, and SyncValuesNetworked writes that one type to both +0x28 and +0x2C.
namespace {
struct Info {
  alignas(8) unsigned char fields[0x80]{};
};
Info fixtureInfo;
int sentType = -1;  // the type the host's SyncValues put in the RPC
std::map<std::string, void*> detours;

int at(size_t offset) { return il2cpp::field<int>(&fixtureInfo, offset); }

void native_sync_values(void* info, int type, int, bool, int, int, bool, int, int, bool, void*, const void*) {
  il2cpp::set_field<int>(info, 0x28, type);
  il2cpp::set_field<int>(info, 0x2C, type);
}
using SyncValues_t = void (*)(void*, int, int, bool, int, int, bool, int, int, bool, void*, const void*);
/** The RPC runs on every client, the host included, through the hooked SyncValuesNetworked. */
void native_sync_send(void* info, const uint8_t* traits, const void*) {
  std::memcpy(&sentType, traits + 4, sizeof sentType);
  reinterpret_cast<SyncValues_t>(detours.at("GhostInfo.SyncValuesNetworked"))(info, sentType, 0, false, 0, 0, false, 0, 0,
                                                                              false, nullptr, nullptr);
}
using SyncSend_t = void (*)(void*, const uint8_t*, const void*);
/** Init copies the traits into GhostInfo, then StartGhost (host) sends them through SyncValues. */
void native_init(void*, uint8_t* traits, const void*) {
  std::memcpy(fixtureInfo.fields + 0x28, traits, 0x40);
  uint8_t copy[0x40];
  std::memcpy(copy, fixtureInfo.fields + 0x28, sizeof copy);
  auto send = detours.count("GhostInfo.SyncValues") ? reinterpret_cast<SyncSend_t>(detours.at("GhostInfo.SyncValues"))
                                                    : &native_sync_send;
  send(&fixtureInfo, copy, nullptr);
}
void* native_localised(void*, bool, const void*) { return nullptr; }
std::map<const void*, std::string> strings;  // the managed strings the fakes made, by address

// A GhostCollection's List<GhostData>, with List.Insert and List.Remove as the registry calls them,
// and GhostData objects (type at +0x18).
struct Collection {
  alignas(8) unsigned char fields[0x20]{};  // GhostCollection: ghosts at +0x18
  il2cpp::List list{};
  alignas(8) unsigned char items[0x20 + 8 * 64]{};
  Collection() {
    list.items = reinterpret_cast<il2cpp::Array*>(items);
    list.items->max_length = 64;
    il2cpp::set_field<il2cpp::List*>(fields, 0x18, &list);
  }
  void* object() { return fields; }
  /** Each entry's name: a custom ghost's own, a game ghost's from its key. */
  std::vector<std::string> names() const {
    std::vector<std::string> out;
    for (int i = 0; i < list.size; ++i) {
      void* data = list.items->data<void*>()[i];
      const pg::custom::Ghost* g = pg::custom::find(il2cpp::field<int>(data, 0x18));
      const std::string key = g ? "" : strings[il2cpp::field<il2cpp::String*>(data, 0x20)];
      out.push_back(g ? g->name : key.substr(8, key.size() - 13));
    }
    return out;
  }
  int count(bool custom) const {
    int n = 0;
    for (int i = 0; i < list.size; ++i)
      n += pg::custom::is_custom(il2cpp::field<int>(list.items->data<void*>()[i], 0x18)) == custom;
    return n;
  }
};
struct Data {
  alignas(8) unsigned char fields[0x40]{};
};
std::vector<std::unique_ptr<Data>> ghostData;
void* ghost_data(int type) {
  ghostData.push_back(std::make_unique<Data>());
  il2cpp::set_field<int>(ghostData.back()->fields, 0x18, type);
  return ghostData.back()->fields;
}
il2cpp::String* text(const std::string& s) {
  ghostData.push_back(std::make_unique<Data>());
  strings[ghostData.back()->fields] = s;
  return reinterpret_cast<il2cpp::String*>(ghostData.back()->fields);
}
/** A game ghost's GhostData, its name key (+0x20) as its asset carries it: "Journal_" + name + "Title". */
void* game_ghost(const char* name, int type) {
  void* data = ghost_data(type);
  il2cpp::set_field<il2cpp::String*>(data, 0x20, text(std::string("Journal_") + name + "Title"));
  return data;
}
// The game's GhostCollection as resources.assets holds it (build 25811160): sorted by asset name.
constexpr std::pair<const char*, int> kGameGhosts[] = {
    {"Aswang", 29}, {"Banshee", 4},  {"Dayan", 26},     {"Deildegast", 30}, {"Demon", 9},    {"Deogen", 22},
    {"Gallu", 25},  {"Goryo", 14},   {"Hantu", 13},     {"Jinn", 5},        {"Kormos", 28},  {"Mare", 6},
    {"Mimic", 20},  {"Moroi", 21},   {"Myling", 15},    {"Obake", 19},      {"Obambo", 27},  {"Oni", 11},
    {"Onryo", 16},  {"Phantom", 2},  {"Poltergeist", 3}, {"Raiju", 18},     {"Revenant", 7}, {"Shade", 8},
    {"Spirit", 0},  {"Thaye", 23},   {"TheTwins", 17},  {"Wraith", 1},      {"Yokai", 12},   {"Yurei", 10}};
void list_add(void* list, void* item, const void*) {
  auto* l = static_cast<il2cpp::List*>(list);
  l->items->data<void*>()[l->size++] = item;
}
void list_insert(void* list, int index, void* item, const void*) {
  auto* l = static_cast<il2cpp::List*>(list);
  for (int j = l->size; j > index; --j) l->items->data<void*>()[j] = l->items->data<void*>()[j - 1];
  l->items->data<void*>()[index] = item;
  ++l->size;
}
void list_remove(void* list, void* item, const void*) {
  auto* l = static_cast<il2cpp::List*>(list);
  for (int i = 0; i < l->size; ++i) {
    if (l->items->data<void*>()[i] != item) continue;
    for (int j = i + 1; j < l->size; ++j) l->items->data<void*>()[j - 1] = l->items->data<void*>()[j];
    --l->size;
    return;
  }
}
// The contract's GhostController (allGhostsCollection at +0x90), while a contract is loaded.
struct Controller {
  alignas(8) unsigned char fields[0xA0]{};
};
Controller fixtureController;
bool inContract = false;

/** Spawn a ghost of `type` on the host as the game does, and return the type the RPC carried. */
int spawn(int type) {
  fixtureInfo = {};
  sentType = -1;
  uint8_t traits[0x40] = {};
  std::memcpy(traits, &type, sizeof type);
  std::memcpy(traits + 4, &type, sizeof type);  // the spawn copies the rolled type to both
  using Init_t = void (*)(void*, uint8_t*, const void*);
  reinterpret_cast<Init_t>(detours.at("GhostAI.Init"))(nullptr, traits, nullptr);
  return sentType;
}
}  // namespace

#include "../mods/custom_ghosts/registry.cpp"

namespace pg::hooks {
std::atomic<int> g_inflight{0};
bool create(void*, void* detour, void** original, const char* name) {
  detours[name] = detour;
  const std::string n = name;
  if (n == "GhostAI.Init") *original = reinterpret_cast<void*>(&native_init);
  else if (n == "GhostInfo.SyncValuesNetworked") *original = reinterpret_cast<void*>(&native_sync_values);
  else if (n == "GhostInfo.SyncValues") *original = reinterpret_cast<void*>(&native_sync_send);
  else *original = reinterpret_cast<void*>(&native_localised);
  return true;
}
}  // namespace pg::hooks
namespace pg::game {
void* game_assembly_address(uintptr_t rva) { return reinterpret_cast<void*>(rva); }
void* level_controller() { return inContract ? &fixtureController : nullptr; }
void* ghost_ai() { return nullptr; }
bool alive(void* object) { return object != nullptr; }
}  // namespace pg::game
namespace il2cpp {
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
const Method* method(Class*, const char* name, int argc) {
  static void* add = reinterpret_cast<void*>(&list_add);
  static void* remove = reinterpret_cast<void*>(&list_remove);
  static void* insert = reinterpret_cast<void*>(&list_insert);
  if (argc == 2 && std::strcmp(name, "Insert") == 0) return reinterpret_cast<const Method*>(&insert);
  if (argc != 1) return nullptr;
  if (std::strcmp(name, "Add") == 0) return reinterpret_cast<const Method*>(&add);
  if (std::strcmp(name, "Remove") == 0) return reinterpret_cast<const Method*>(&remove);
  return nullptr;
}
void* static_field_by_type(Class*, const char*) { return inContract ? &fixtureController : nullptr; }
String* string_new(const char* utf8) { return text(utf8); }
Object* object_new(Class*) { return static_cast<Object*>(ghost_data(0)); }
Class* class_of(const void*) { return nullptr; }
void set_ref_field(void* obj, size_t offset, void* value) { set_field<void*>(obj, offset, value); }
void to_utf8(const String* s, char* out, size_t size) {
  if (!size) return;
  auto it = strings.find(s);
  const std::string value = it == strings.end() ? "" : it->second;
  std::snprintf(out, size, "%s", value.c_str());
}
uint32_t gc_hold(void*) { return 0; }
void gc_release(uint32_t) {}
}  // namespace il2cpp
namespace pg::log {
void write(const char*, ...) {}
}  // namespace pg::log

int main() {
  CHECK(pg::custom::registry::resolve());
  CHECK(pg::custom::registry::create_hooks());
  // A custom ghost: every client learns what it is, and its AI follows no vanilla type's rules.
  CHECK(spawn(pg::custom::kAsterion) == pg::custom::kAsterion);
  CHECK(at(0x28) == pg::custom::kAsterion && at(0x2C) == pg::custom::kBehaviourType);
  // A vanilla ghost is sent and synced as the game sends it.
  CHECK(spawn(5) == 5);
  CHECK(at(0x28) == 5 && at(0x2C) == 5);

  // The lobby's custom difficulty menu is offered the custom ghosts once, and they are withdrawn
  // again, every custom entry whoever added it, while the game's own stay.
  {
    Collection menu;
    list_add(&menu.list, ghost_data(3), nullptr);
    list_add(&menu.list, ghost_data(5), nullptr);
    pg::custom::registry::offer(menu.object());
    pg::custom::registry::offer(menu.object());
    CHECK(menu.count(true) == pg::custom::kGhostCount && menu.count(false) == 2);
    list_add(&menu.list, ghost_data(pg::custom::kEcho), nullptr);
    pg::custom::registry::withdraw(menu.object());
    CHECK(menu.count(true) == 0 && menu.count(false) == 2);
  }
  // The custom ghosts take their alphabetical places among the game's ghosts, which its collection
  // keeps sorted by the names in their keys ("TheTwins"), so its menu and lists stay in order.
  {
    Collection menu;
    for (const auto& [name, type] : kGameGhosts) list_add(&menu.list, game_ghost(name, type), nullptr);
    pg::custom::registry::offer(menu.object());
    const std::vector<std::string> expected = {
        "Asterion", "Aswang",   "Banshee",  "Black Shuck", "Bogeyman",   "Brownie",  "Dayan",   "Deildegast", "Demon",
        "Deogen",   "Draugr",   "Dullahan", "Echo",        "Gallu",      "Goryo",    "Gremlin", "Hantu",      "Hydra",
        "Jinn",     "Kikimora", "Knell",    "Kormos",      "Lethe",      "Lithobolia", "Mare",  "Mesmer",     "Mimic",
        "Moroi",    "Myling",   "Obake",    "Obambo",      "Oni",        "Onryo",    "Phantom", "Poltergeist", "Raiju",
        "Revenant", "Shade",    "Spirit",   "Tempest",     "Thaye",      "TheTwins", "Wraith",  "Yokai",      "Yurei"};
    CHECK(menu.names() == expected);
    pg::custom::registry::withdraw(menu.object());
    std::vector<std::string> game;
    for (const auto& [name, type] : kGameGhosts) game.push_back(name);
    CHECK(menu.names() == game);
  }
  // A contract where not everyone runs the custom ghosts loses any the lobby left in its collection
  // (the menu and the contract may share the game's GhostCollection).
  {
    Collection shared;
    list_add(&shared.list, ghost_data(3), nullptr);
    pg::custom::registry::offer(shared.object());
    il2cpp::set_field<void*>(fixtureController.fields, 0x90, shared.object());
    inContract = true;
    pg::custom::registry::tick(false);
    CHECK(!pg::custom::registry::active());
    CHECK(shared.count(true) == 0 && shared.count(false) == 1);
    inContract = false;
    pg::custom::registry::tick(false);
  }
  // A contract where everyone does keeps them, those already there included.
  {
    Collection shared;
    pg::custom::registry::offer(shared.object());
    il2cpp::set_field<void*>(fixtureController.fields, 0x90, shared.object());
    inContract = true;
    pg::custom::registry::tick(true);
    CHECK(pg::custom::registry::active());
    CHECK(shared.count(true) == pg::custom::kGhostCount);
    inContract = false;
    pg::custom::registry::tick(true);  // the contract unloads: its entries go with it
    CHECK(!pg::custom::registry::active());
    CHECK(shared.count(true) == 0);
  }
  return g_failures ? 1 : 0;
}
