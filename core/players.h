#pragma once

#include "game.h"
#include "il2cpp.h"

// The investigators in the contract as this client knows them: every networked Player in Network's
// player spots (build 25811160 offsets, as everywhere they are read). Main thread.
namespace pg::game {

constexpr size_t kNetworkPlayerSpots = 0x38;  // Network: List<PlayerSpot>
constexpr size_t kPlayerSpotPlayer = 0x38;    // PlayerSpot: networked Player
constexpr size_t kPlayerIsDead = 0x28;        // Player: dead

/** Visit each Player object that is still alive as a Unity object (dead investigators included). */
template <class Visit>
void each_player(Visit visit) {
  void* net = network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkPlayerSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kPlayerSpotPlayer) : nullptr;
    if (player && alive(player)) visit(player);
  }
}

/** Whether a Player is a living investigator. */
inline bool living(void* player) { return player && alive(player) && !il2cpp::field<bool>(player, kPlayerIsDead); }

}  // namespace pg::game
