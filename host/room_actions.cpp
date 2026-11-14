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
  void* type = phasmo::ServerManager::type.type_object();  // the System.Type from the raw runtime: not a game object
  const auto found = type ? phasmo::Object::FindObjectOfType(type, false) : nullptr;
  g_manager = il2cpp::Held(hostgame::alive(found.raw()) ? found.raw() : nullptr);
  return g_manager.get();
}

/** Find an actor's ID card: a card that cannot be proved to be theirs is no card, so nobody else is kicked. */
void* card_of(void* manager, int actor) {
  if (actor == 0) return nullptr;  // a number that could not be read: every card would match
  const api::List cards{phasmo::ServerManager::cards.get(manager).raw()};  // the list's array goes to the raw List
  for (size_t i = 0, n = cards.size(); i < n; ++i) {
    const auto card = api::Object::unchecked(cards[i]);  // a List element read
    if (!hostgame::alive(card.raw()) || !phasmo::MultiplayerProfile::type.is(card)) continue;
    const auto spot = phasmo::ProfileCard::spot.get(card);
    if (!spot || !phasmo::PlayerSpot::type.is(spot)) continue;
    const auto player = phasmo::PlayerSpot::photonPlayer.get(spot);
    if (player && hostgame::actor_number(player.raw()) == actor) return card.raw();
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
  const auto lobby = button ? api::Object::unchecked(manager()) : nullptr;  // the finder answers void* until 3c
  return lobby && phasmo::ServerManager::startGameButton.get(lobby).raw() == button;
}

std::string player_name(int actor) {
  const api::RefArray list{phasmo::PhotonNetwork::get_PlayerList()};
  for (size_t i = 0; i < list.size(); ++i)
    if (const auto p = api::Object::unchecked(list[i]);  // a RefArray element read
        p && hostgame::actor_number(p.raw()) == actor)
      return api::to_utf8(phasmo::PhotonPlayer::get_NickName(p).raw());
  return {};
}

bool leave_lobby() {
  const auto lobby = phasmo::ServerManager::LeaveServer.available() ? api::Object::unchecked(manager())
                                                                    : nullptr;  // the finder answers void* until 3c
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
  const auto lobby = phasmo::ServerManager::KickPlayer.available() ? api::Object::unchecked(manager())
                                                                   : nullptr;  // the finder answers void* until 3c
  const auto card = lobby ? api::Object::unchecked(card_of(lobby.raw(), actor)) : nullptr;  // the card finder, the same
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
