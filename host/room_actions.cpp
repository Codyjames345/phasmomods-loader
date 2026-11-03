#include "room_actions.h"

#include <windows.h>

#include "LevelController.h"
#include "Photon.h"
#include "Player.h"
#include "ServerManager.h"
#include "UnityObject.h"
#include "contained.h"
#include "game_api.h"
#include "host_game.h"
#include "il2cpp.h"
#include "log.h"
#include "runtime.h"

namespace pm::room_actions {
namespace {

il2cpp::Held g_manager;  // the lobby's ServerManager, as last found
ULONGLONG g_nextSearch = 0;

// The lobby's ServerManager: FindObjectOfType searches the whole scene, so at most once a second.
void* manager() {
  if (hostgame::alive(g_manager.get())) return g_manager.get();
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextSearch) return nullptr;
  g_nextSearch = now + 1000;
  void* type = phasmo::ServerManager::type.type_object();
  void* found = type ? phasmo::Object::FindObjectOfType(type, false) : nullptr;
  g_manager = il2cpp::Held(hostgame::alive(found) ? found : nullptr);
  return g_manager.get();
}

/** Find an actor's ID card: a card that cannot be proved to be theirs is no card, so nobody else is kicked. */
void* card_of(void* manager, int actor) {
  if (actor == 0) return nullptr;  // a number that could not be read: every card would match
  const api::List cards{phasmo::ServerManager::cards.get(manager)};
  for (size_t i = 0, n = cards.size(); i < n; ++i) {
    void* card = cards[i];
    if (!hostgame::alive(card) || !phasmo::MultiplayerProfile::type.is(card)) continue;
    void* spot = phasmo::ProfileCard::spot.get(card);
    if (!spot || !phasmo::PlayerSpot::type.is(spot)) continue;
    void* player = phasmo::PlayerSpot::photonPlayer.get(spot);
    if (player && hostgame::actor_number(player) == actor) return card;
  }
  return nullptr;
}

}  // namespace

std::string missing() {
  using phasmo::PhotonNetwork;
  using phasmo::ServerManager;
  // The lobby's own steps, then what the guard reads to know where it is (core/room_gates.h, hostgame::out_of_contract):
  // without those it can't see a public lobby or a player, so mods would run where the rules say they are off.
  return hostgame::unresolved<ServerManager::type, ServerManager::cards, ServerManager::LeaveServer,
                              ServerManager::KickPlayer, phasmo::Object::FindObjectOfType,
                              phasmo::MultiplayerProfile::type, phasmo::ProfileCard::spot, phasmo::PlayerSpot::type,
                              phasmo::PlayerSpot::photonPlayer, phasmo::PhotonPlayer::get_ActorNumber,
                              PhotonNetwork::get_IsMessageQueueRunning, PhotonNetwork::get_InRoom,
                              PhotonNetwork::get_LocalPlayer, PhotonNetwork::get_MasterClient,
                              PhotonNetwork::get_PlayerList, PhotonNetwork::get_CurrentRoom,
                              PhotonNetwork::get_OfflineMode, phasmo::PhotonRoom::get_IsVisible,
                              phasmo::LevelController::instance>();
}

bool available() { return missing().empty(); }

bool lobby_open() { return phasmo::PhotonNetwork::get_IsMessageQueueRunning() && manager() != nullptr; }

bool is_start_button(void* button) {
  void* lobby = button ? manager() : nullptr;
  return lobby && phasmo::ServerManager::startGameButton.get(lobby) == button;
}

std::string player_name(int actor) {
  const api::RefArray list{phasmo::PhotonNetwork::get_PlayerList()};
  for (size_t i = 0; i < list.size(); ++i)
    if (void* p = list[i]; p && hostgame::actor_number(p) == actor) return api::to_utf8(phasmo::PhotonPlayer::get_NickName(p));
  return {};
}

bool leave_lobby() {
  void* lobby = phasmo::ServerManager::LeaveServer.available() ? manager() : nullptr;
  if (!lobby) {
    PM_LOG("room: no lobby to leave");
    return false;
  }
  try {
    phasmo::ServerManager::LeaveServer(lobby);
  } catch (const Il2CppExceptionWrapper& e) {
    host::log_managed("room: the lobby's Leave threw", e.ex);
    return false;
  }
  PM_LOG("room: left the lobby (its mods differ)");
  return true;
}

bool kick(int actor) {
  void* lobby = phasmo::ServerManager::KickPlayer.available() ? manager() : nullptr;
  void* card = lobby ? card_of(lobby, actor) : nullptr;
  if (!card) {
    PM_LOG("room: actor %d has no ID card to kick yet", actor);
    return false;
  }
  try {
    phasmo::ServerManager::KickPlayer(lobby, card);
  } catch (const Il2CppExceptionWrapper& e) {
    host::log_managed("room: the game's kick threw", e.ex);
    return false;
  }
  PM_LOG("room: asked the game to kick actor %d (mods differ or missing)", actor);
  return true;
}

}  // namespace pm::room_actions
