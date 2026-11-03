#pragma once

#include <vector>

#include "Photon.h"
#include "runtime.h"

// What a client may do in a room rests on these answers: a mod reaches them through core/game.h and the host through
// host/host_game.h, so there is one way to judge a room. Each fails closed: a Photon member it reads that did not resolve
// gives the answer that lets nothing act. Main thread.
namespace pm::room_gates {

// Single player, or a room that is not listed publicly. False whenever a member it reads did not resolve: a room it
// cannot judge is no private one, rather than its wrapper's default (false, "not listed") deciding.
inline bool private_room() {
  using Net = phasmo::PhotonNetwork;
  if (!Net::get_InRoom.available() || !Net::get_OfflineMode.available() || !Net::get_CurrentRoom.available() ||
      !phasmo::PhotonRoom::get_IsVisible.available())
    return false;
  if (!Net::get_InRoom()) return false;
  if (Net::get_OfflineMode()) return true;  // single player
  void* room = Net::get_CurrentRoom();
  return room && !phasmo::PhotonRoom::get_IsVisible(room);  // not listed in the public lobby browser
}

// A Photon player's actor number; 0 when it cannot be read.
inline int actor_number(void* photonPlayer) { return phasmo::PhotonPlayer::get_ActorNumber(photonPlayer); }

// This client's actor number; 0 when not in a room, or when the numbers cannot be read.
inline int local_actor() {
  if (!phasmo::PhotonNetwork::get_InRoom()) return 0;
  void* p = phasmo::PhotonNetwork::get_LocalPlayer();
  return p ? actor_number(p) : 0;
}

// The room's master's actor number; 0 when not in a room, or when the numbers cannot be read.
inline int master_actor() {
  if (!phasmo::PhotonNetwork::get_InRoom()) return 0;
  void* p = phasmo::PhotonNetwork::get_MasterClient();
  return p ? actor_number(p) : 0;
}

// The room's players by actor number; empty outside a room or when the list cannot be read. An actor number that cannot
// be read is 0, and local_actor is 0 then too, so no gate acts on the list. False when the list cannot be trusted, which
// says nothing about who is there and is not a room with nobody else in it: a room online always holds this client, so an
// empty list, an empty slot or a number of 0 is no reading. Single player is alone whatever its list reads.
inline bool room_actors(std::vector<int>& out) {
  out.clear();
  if (!phasmo::PhotonNetwork::get_InRoom()) return true;
  const api::RefArray list{phasmo::PhotonNetwork::get_PlayerList()};
  bool whole = list.size() > 0;
  for (size_t i = 0; i < list.size(); ++i) {
    void* p = list[i];
    if (p) out.push_back(actor_number(p));
    if (!p || out.back() == 0) whole = false;
  }
  return whole || phasmo::PhotonNetwork::get_OfflineMode();
}

}  // namespace pm::room_gates
