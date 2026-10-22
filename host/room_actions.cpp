#include "room_actions.h"

#include <windows.h>

#include "game.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::room_actions {
namespace {

constexpr size_t kManagerCards = 0xC8;      // ServerManager: List<MultiplayerProfile>, the lobby's ID cards
constexpr size_t kCardSpot = 0x60;          // ProfileCard: PlayerSpot

struct Api {
  il2cpp::Object* serverManagerType = nullptr;
  il2cpp::Field* spotPhotonPlayer = nullptr;
  void* (*findObjectOfType)(il2cpp::Object*, const void*) = nullptr;
  void (*leaveServer)(void*, const void*) = nullptr;
  void (*kickPlayer)(void*, void*, const void*) = nullptr;
} api;

il2cpp::Held g_manager;  // the lobby's ServerManager, as last found
ULONGLONG g_nextSearch = 0;

// The lobby's ServerManager: FindObjectOfType searches the whole scene, so at most once a second.
void* manager() {
  if (game::alive(g_manager.get())) return g_manager.get();
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextSearch) return nullptr;
  g_nextSearch = now + 1000;
  void* found = api.findObjectOfType ? api.findObjectOfType(api.serverManagerType, nullptr) : nullptr;
  g_manager = il2cpp::Held(game::alive(found) ? found : nullptr);
  return g_manager.get();
}

/** Find an actor's ID card, binding nested PlayerSpot metadata only after validating the object. */
void* card_of(void* manager, int actor) {
  auto* cards = il2cpp::field<il2cpp::List*>(manager, kManagerCards);
  if (!il2cpp::is_class(cards, "List`1") || !cards->items) return nullptr;
  for (int i = 0; i < cards->size && static_cast<uintptr_t>(i) < cards->items->max_length; ++i) {
    void* card = cards->items->data<void*>()[i];
    if (!game::alive(card) || !il2cpp::is_class(card, "MultiplayerProfile")) continue;
    void* spot = il2cpp::field<void*>(card, kCardSpot);
    if (!spot || !il2cpp::is_class(spot, "PlayerSpot")) continue;
    if (!api.spotPhotonPlayer)
      api.spotPhotonPlayer = il2cpp::field_info(il2cpp::class_of(spot), "photonPlayer");
    if (!api.spotPhotonPlayer) return nullptr;
    void* player = il2cpp::reference_field(spot, api.spotPhotonPlayer);
    if (player && game::actor_number(player) == actor) return card;
  }
  return nullptr;
}

}  // namespace

/** Resolve room actions without requiring a nested PlayerSpot before a lobby card is available. */
bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  il2cpp::Class* manager = il2cpp::cls(kGame, "", "ServerManager");
  api.serverManagerType = il2cpp::type_object(manager);
  api.findObjectOfType = reinterpret_cast<decltype(api.findObjectOfType)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectOfType", 1));
  api.leaveServer = reinterpret_cast<decltype(api.leaveServer)>(il2cpp::method_ptr(manager, "LeaveServer", 0));
  api.kickPlayer = reinterpret_cast<decltype(api.kickPlayer)>(il2cpp::method_ptr(manager, "KickPlayer", 1));
  const bool ok = api.serverManagerType && api.findObjectOfType && api.leaveServer && api.kickPlayer;
  PG_LOG("room: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool lobby_open() { return manager() != nullptr; }

bool leave_lobby() {
  void* lobby = manager();
  if (!lobby) {
    PG_LOG("room: no lobby to leave");
    return false;
  }
  try {
    api.leaveServer(lobby, nullptr);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("room: the lobby's Leave threw", e.ex);
    return false;
  }
  PG_LOG("room: left the lobby (its mods differ)");
  return true;
}

bool kick(int actor) {
  void* lobby = manager();
  void* card = lobby ? card_of(lobby, actor) : nullptr;
  if (!card) {
    PG_LOG("room: actor %d has no ID card to kick yet", actor);
    return false;
  }
  try {
    api.kickPlayer(lobby, card, nullptr);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("room: the game's kick threw", e.ex);
    return false;
  }
  PG_LOG("room: kicked actor %d (mods differ or missing)", actor);
  return true;
}

}  // namespace pg::room_actions
