#include "equipment.h"

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::equipment {
namespace {

constexpr size_t kMainManagerGhostOs = 0x68;      // MainManager: the lobby computer (GhostOS)
constexpr size_t kGhostOsInventory = 0x28;        // GhostOS: InventoryManager
constexpr size_t kGhostOsLoadouts = 0x30;         // GhostOS: LoadoutManager

struct Api {
  il2cpp::Class* mainManager = nullptr;
  void (*removeMine)(void*, const void*) = nullptr;
  void (*checkEquipped)(void*, const void*) = nullptr;
  void* addLoadout = nullptr;
  void* addItem = nullptr;
} api;

using AddLoadout_t = void (*)(void*, void*, const void*);
using AddItem_t = void (*)(void*, void*, const void*);
AddLoadout_t g_addLoadout = nullptr;
AddItem_t g_addItem = nullptr;

const session::Session* g_session = nullptr;
bool g_withdrawn = false;

bool local_is_ghost_player() { return g_session && g_session->local_is_ghost() && game::private_room(); }

void detour_add_loadout(void* inventory, void* loadout, const void* method) {
  hooks::Guard guard;
  if (local_is_ghost_player()) return;  // the ghost player brings nothing
  g_addLoadout(inventory, loadout, method);
}

void detour_add_item(void* inventory, void* item, const void* method) {
  hooks::Guard guard;
  if (local_is_ghost_player()) return;
  g_addItem(inventory, item, method);
}

void* ghost_os() {
  void* main = api.mainManager ? il2cpp::static_field_by_type(api.mainManager, "MainManager") : nullptr;
  void* os = main && game::alive(main) ? il2cpp::field<void*>(main, kMainManagerGhostOs) : nullptr;
  return game::alive(os) ? os : nullptr;
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  api.mainManager = il2cpp::cls(kGame, "", "MainManager");
  il2cpp::Class* inventory = il2cpp::cls(kGame, "", "InventoryManager");
  api.removeMine = reinterpret_cast<decltype(api.removeMine)>(il2cpp::method_ptr(inventory, "RemoveMyEquipmentFromLobby", 0));
  api.checkEquipped = reinterpret_cast<decltype(api.checkEquipped)>(
      il2cpp::method_ptr(il2cpp::cls(kGame, "", "LoadoutManager"), "CheckEquipped", 0));
  api.addLoadout = il2cpp::method_ptr(inventory, "AddLoadoutToLobby", 1);
  api.addItem = il2cpp::method_ptr(inventory, "AddLoadoutItemToLobby", 1);
  const bool ok = api.mainManager && api.removeMine && api.checkEquipped && api.addLoadout && api.addItem;
  PG_LOG("equipment: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(api.addLoadout, reinterpret_cast<void*>(&detour_add_loadout),
                       reinterpret_cast<void**>(&g_addLoadout), "InventoryManager.AddLoadoutToLobby") &&
         hooks::create(api.addItem, reinterpret_cast<void*>(&detour_add_item), reinterpret_cast<void**>(&g_addItem),
                       "InventoryManager.AddLoadoutItemToLobby");
}

void tick(session::Session& s) {
  g_session = &s;
  if (game::level_controller()) return;  // the pool is the lobby's; a contract keeps what it has
  void* os = ghost_os();
  if (!os) return;
  const bool ghostPlayer = local_is_ghost_player();
  if (ghostPlayer && !g_withdrawn) {
    if (void* inventory = il2cpp::field<void*>(os, kGhostOsInventory)) api.removeMine(inventory, nullptr);
    g_withdrawn = true;
    PG_LOG("equipment: the ghost player's equipment left the lobby");
  } else if (!ghostPlayer && g_withdrawn) {
    if (void* loadouts = il2cpp::field<void*>(os, kGhostOsLoadouts)) api.checkEquipped(loadouts, nullptr);
    g_withdrawn = false;
    PG_LOG("equipment: saved loadout equipped again");
  }
}

}  // namespace pg::equipment
