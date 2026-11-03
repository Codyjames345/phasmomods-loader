#include "loadout.h"

#include "MainManager.h"
#include "game.h"
#include "log.h"
#include "runtime.h"

namespace pm::loadout {
namespace {

using phasmo::InventoryManager;

const session::Session* g_session = nullptr;
bool g_withdrawn = false;

bool local_is_ghost_player() { return g_session && g_session->local_is_ghost() && game::private_room(); }

void detour_add_loadout(api::Original<InventoryManager::AddLoadoutToLobby> original, void* inventory, void* loadout) {
  if (local_is_ghost_player()) return;  // the ghost player brings nothing
  original(inventory, loadout);
}

void detour_add_item(api::Original<InventoryManager::AddLoadoutItemToLobby> original, void* inventory, void* item) {
  if (local_is_ghost_player()) return;
  original(inventory, item);
}

void* ghost_os() {
  void* main = phasmo::MainManager::instance.get();
  void* os = main && api::alive(main) ? phasmo::MainManager::ghostOS.get(main) : nullptr;
  return os && api::alive(os) ? os : nullptr;
}

}  // namespace

bool create_hooks() {
  return api::hook<InventoryManager::AddLoadoutToLobby>(&detour_add_loadout, "InventoryManager.AddLoadoutToLobby",
                                                        api::Twins::CanonicalOnly) &&
         api::hook<InventoryManager::AddLoadoutItemToLobby>(&detour_add_item, "InventoryManager.AddLoadoutItemToLobby",
                                                            api::Twins::CanonicalOnly);
}

void tick(session::Session& s) {
  g_session = &s;
  if (game::level_controller()) return;  // the pool is the lobby's; a contract keeps what it has
  void* os = ghost_os();
  if (!os) return;
  const bool ghostPlayer = local_is_ghost_player();
  if (ghostPlayer && !g_withdrawn) {
    if (void* inventory = phasmo::GhostOS::inventory.get(os)) InventoryManager::RemoveMyEquipmentFromLobby(inventory);
    g_withdrawn = true;
    PM_LOG("loadout: the ghost player's equipment left the lobby");
  } else if (!ghostPlayer && g_withdrawn) {
    if (void* loadouts = phasmo::GhostOS::loadouts.get(os)) phasmo::LoadoutManager::CheckEquipped(loadouts);
    g_withdrawn = false;
    PM_LOG("loadout: saved loadout equipped again");
  }
}

}  // namespace pm::loadout
