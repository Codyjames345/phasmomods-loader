#pragma once

#include "Photon.h"
#include "Player.h"
#include "runtime.h"

// A networked Player's Photon actor number, through the lobby's spots (Network.playerSpots), for the
// messages on Custom Ghosts' channel that name a player. Main thread.
namespace pm::custom {

/** The actor of the networked Player `player` (its PlayerSpot's Photon player), or 0 for none. */
inline int actor_of_player(void* player) {
  void* net = player ? phasmo::Network::get_Instance() : nullptr;
  const pm::api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i)
    if (void* spot = spots[i]; spot && phasmo::PlayerSpot::player.get(spot).raw() == player) {
      void* photon = phasmo::PlayerSpot::photonPlayer.get(spot);
      return photon ? phasmo::PhotonPlayer::get_ActorNumber(photon) : 0;
    }
  return 0;
}

}  // namespace pm::custom
