// Offline test of the ghost player's lobby equipment (mods/playable_ghost/loadout.cpp): the ghost player adds
// nothing to the lobby's pool, their equipment leaves it once, and their saved loadout comes back once they stop
// being the ghost. The game is the fake backend, with its fields at offsets of the test's own; core's helpers it
// still uses are modelled here.
// Run: build\loadout_test.exe
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "MainManager.h"
#include "check.h"
#include "fake_game.h"
#include "hooks.h"
#include "game.h"
#include "session.h"

#include "../mods/playable_ghost/loadout.cpp"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct Object {
  void* klass = nullptr;
  alignas(8) unsigned char fields[0x40]{};
};
Object mainManager, ghostOS, inventory, loadouts, level, aLoadout, anItem;
void* mainStatic = &mainManager;  // MainManager.instance's storage
bool privateRoom = true;
bool inLevel = false;
int loadoutsAdded = 0, itemsAdded = 0, removed = 0, equipped = 0;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

template <class T>
void put(Object& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

void add_loadout(void* pool, void* loadout, const void*) {
  CHECK(pool == &inventory && loadout == &aLoadout);
  ++loadoutsAdded;
}
void add_item(void* pool, void* item, const void*) {
  CHECK(pool == &inventory && item == &anItem);
  ++itemsAdded;
}
void remove_mine(void* pool, const void*) {
  CHECK(pool == &inventory);
  ++removed;
}
void check_equipped(void* manager, const void*) {
  CHECK(manager == &loadouts);
  ++equipped;
}

/** Gives every entry loadout.cpp uses, and binds through a host that records each hook's detour. */
void give_entries() {
  fake::reset();
  fake::static_field(map::MainManager_instance, &mainStatic, sizeof mainStatic, true);
  fake::field(map::MainManager_ghostOS, 0x10);
  fake::field(map::GhostOS_inventory, 0x10);
  fake::field(map::GhostOS_loadouts, 0x18);
  fake::method(map::InventoryManager_AddLoadoutToLobby, reinterpret_cast<void*>(&add_loadout));
  fake::method(map::InventoryManager_AddLoadoutItemToLobby, reinterpret_cast<void*>(&add_item));
  fake::method(map::InventoryManager_RemoveMyEquipmentFromLobby, reinterpret_cast<void*>(&remove_mine));
  fake::method(map::LoadoutManager_CheckEquipped, reinterpret_cast<void*>(&check_equipped));
  put<void*>(mainManager, 0x10, &ghostOS);
  put<void*>(ghostOS, 0x10, &inventory);
  put<void*>(ghostOS, 0x18, &loadouts);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::loadout::create_hooks());
}

using Add_t = void (*)(void*, void*, const void*);
/** The game adds a loadout, then one item, to the lobby's pool; our hooks see each call first. */
void game_adds() {
  const auto loadout = detours.find("InventoryManager.AddLoadoutToLobby");
  const auto item = detours.find("InventoryManager.AddLoadoutItemToLobby");
  CHECK(loadout != detours.end() && item != detours.end());
  if (loadout != detours.end()) reinterpret_cast<Add_t>(loadout->second)(&inventory, &aLoadout, nullptr);
  if (item != detours.end()) reinterpret_cast<Add_t>(item->second)(&inventory, &anItem, nullptr);
}

/** A one-player private room in which this client plays the ghost. */
pm::session::Session ghost_session() {
  pm::session::Session s;
  s.set_volunteer(true);
  for (uint64_t now = 1000; now < 60000 && !s.local_is_ghost(); now += 100) s.observe(1, 1, {1}, now);
  CHECK(s.local_is_ghost());
  return s;
}
}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::game {
bool private_room() { return privateRoom; }
void* level_controller() { return inLevel ? &level : nullptr; }
}  // namespace pm::game

int main() {
  give_entries();
  pm::session::Session investigator;
  pm::session::Session ghost = ghost_session();

  // An investigator's equipment goes into the pool as the game adds it, and nothing is withdrawn.
  pm::loadout::tick(investigator);
  game_adds();
  CHECK(loadoutsAdded == 1 && itemsAdded == 1 && removed == 0 && equipped == 0);

  // The ghost player brings nothing: their equipment leaves the pool once, and the game's adds are refused.
  pm::loadout::tick(ghost);
  pm::loadout::tick(ghost);
  CHECK(removed == 1 && equipped == 0);
  game_adds();
  CHECK(loadoutsAdded == 1 && itemsAdded == 1);

  // Outside a private room nobody is the ghost player: the game's adds go through.
  privateRoom = false;
  game_adds();
  CHECK(loadoutsAdded == 2 && itemsAdded == 2);
  privateRoom = true;

  // In a contract the pool is the lobby's to keep: nothing is withdrawn or equipped again.
  inLevel = true;
  pm::loadout::tick(investigator);
  CHECK(removed == 1 && equipped == 0);
  inLevel = false;

  // No longer the ghost: the saved loadout is equipped again, once.
  pm::loadout::tick(investigator);
  pm::loadout::tick(investigator);
  CHECK(removed == 1 && equipped == 1);
  game_adds();
  CHECK(loadoutsAdded == 3 && itemsAdded == 3);

  // Without the menu's MainManager nothing is touched.
  mainStatic = nullptr;
  pm::loadout::tick(ghost);
  CHECK(removed == 1 && equipped == 1);
  return g_failures ? 1 : 0;
}
