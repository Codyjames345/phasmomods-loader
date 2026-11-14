#pragma once

#include "Player.h"
#include "game.h"
#include "runtime.h"

// The investigators in the contract as this client knows them: every networked Player in Network's
// player spots (phasmo::Network::playerSpots, phasmo::PlayerSpot::player), read through the game API.
// Main thread.
namespace pm::game {

/** Visit each Player object that is still alive as a Unity object (dead investigators included). */
template <class Visit>
void each_player(Visit visit) {
  const auto net = network();
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i) {
    const auto spot = api::Object::unchecked(spots[i]);  // a List element read
    const auto player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (player && alive(player.raw())) visit(player.raw());
  }
}

/** Whether a Player is a living investigator (phasmo::Player::isDead). */
inline bool living(void* player) { return player && alive(player) && !phasmo::Player::isDead.get(player); }

}  // namespace pm::game
