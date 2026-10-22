#pragma once

#include "session.h"

// The ghost player brings no equipment. In the lobby each player's loadout is added to the shared
// pool the truck is stocked from; while this client is the ghost player its own contribution is
// withdrawn with the game's InventoryManager.RemoveMyEquipmentFromLobby and anything it tries to
// add is refused. When it stops being the ghost player the game's LoadoutManager.CheckEquipped
// equips its saved loadout again. Main thread only.
namespace pg::equipment {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s);

}  // namespace pg::equipment
