#include "room.h"

#include <windows.h>

#include <cstring>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "paging.h"

namespace pg::lobbies::room {
namespace {

// Build 25811160 (verified by the host at load).
constexpr uintptr_t kCreateRoomRva = 0x434A960;  // PhotonNetwork.CreateRoom(string, RoomOptions, TypedLobby, string[])
constexpr uintptr_t kSetMaxRva = 0x433FFE0;      // Photon.Realtime.Room.set_MaxPlayers(byte)
constexpr size_t kOptionsVisible = 0x10;         // RoomOptions.isVisible
constexpr size_t kOptionsMax = 0x12;             // RoomOptions.MaxPlayers (byte)
constexpr size_t kNetworkColours = 0x40;         // Network: Color[4], each player's colour
constexpr size_t kNetworkColoursToo = 0x48;      // Network: Color[4], the second set (readonly)
constexpr ULONGLONG kLockHoldMs = 15000;         // a contract's lock is left alone this long
constexpr ULONGLONG kEnforceMs = 1000;

struct Colour {
  float r, g, b, a;
};
// Four more player colours, in the spirit of the game's own.
constexpr Colour kExtra[4] = {{0.85f, 0.35f, 0.85f, 1}, {0.25f, 0.85f, 0.80f, 1}, {0.95f, 0.55f, 0.20f, 1}, {0.70f, 0.70f, 0.70f, 1}};

using CreateRoom_t = bool (*)(il2cpp::String*, void*, void*, void*, const void*);
using SetMax_t = void (*)(void*, uint8_t, const void*);
CreateRoom_t g_createRoom = nullptr;
SetMax_t g_setMax = nullptr;

struct Api {
  void* (*currentRoom)(const void*) = nullptr;
  bool (*visible)(void*, const void*) = nullptr;
  uint8_t (*playerCount)(void*, const void*) = nullptr;
  uint8_t (*maxPlayers)(void*, const void*) = nullptr;
  il2cpp::Class* colourClass = nullptr;
} api;

ULONGLONG g_lockedAt = 0;
ULONGLONG g_nextEnforce = 0;

bool detour_create_room(il2cpp::String* name, void* options, void* lobby, void* users, const void* method) {
  hooks::Guard guard;
  if (options && !il2cpp::field<bool>(options, kOptionsVisible) && il2cpp::field<uint8_t>(options, kOptionsMax) == kVanilla) {
    il2cpp::set_field<uint8_t>(options, kOptionsMax, static_cast<uint8_t>(kPlayers));
    PG_LOG("room: a private room for %d players", kPlayers);
  }
  return g_createRoom(name, options, lobby, users, method);
}

void detour_set_max(void* room, uint8_t value, const void* method) {
  hooks::Guard guard;
  const ULONGLONG now = GetTickCount64();
  const int count = room ? api.playerCount(room, nullptr) : 0;
  const bool lock = value == count;  // the game closes the room to who is in it (a contract starts)
  if (lock) g_lockedAt = now;
  const bool recent = lock || (g_lockedAt && now - g_lockedAt < kLockHoldMs);
  const int limit = room_limit(value, room && !api.visible(room, nullptr), count, recent);
  g_setMax(room, static_cast<uint8_t>(limit), method);
}

/** The per-player colours, grown to eight on this client (every client looks them up by slot). */
void grow_colours() {
  void* net = game::network();
  if (!net || !api.colourClass) return;
  for (size_t offset : {kNetworkColours, kNetworkColoursToo}) {
    auto* colours = il2cpp::field<il2cpp::Array*>(net, offset);
    if (!colours || colours->max_length != static_cast<uintptr_t>(kVanilla)) continue;
    il2cpp::Array* more = il2cpp::array_new(api.colourClass, kPlayers);
    if (!more) continue;
    std::memcpy(more->data<Colour>(), colours->data<Colour>(), sizeof(Colour) * kVanilla);
    std::memcpy(more->data<Colour>() + kVanilla, kExtra, sizeof kExtra);
    il2cpp::set_ref_field(net, offset, more);
    PG_LOG("room: player colours for %d players", kPlayers);
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* pun = il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonNetwork");
  il2cpp::Class* room = il2cpp::cls("PhotonRealtime.dll", "Photon.Realtime", "Room");
  api.colourClass = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Color");
  bool ok = pun && room && api.colourClass;
  ok &= il2cpp::bind(api.currentRoom, pun, "get_CurrentRoom", 0);
  ok &= il2cpp::bind(api.visible, room, "get_IsVisible", 0);
  ok &= il2cpp::bind(api.playerCount, room, "get_PlayerCount", 0);
  ok &= il2cpp::bind(api.maxPlayers, room, "get_MaxPlayers", 0);
  PG_LOG("room: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kCreateRoomRva), reinterpret_cast<void*>(&detour_create_room),
                       reinterpret_cast<void**>(&g_createRoom), "PhotonNetwork.CreateRoom") &&
         hooks::create(game::game_assembly_address(kSetMaxRva), reinterpret_cast<void*>(&detour_set_max),
                       reinterpret_cast<void**>(&g_setMax), "Room.set_MaxPlayers");
}

void tick() {
  grow_colours();
  const ULONGLONG now = GetTickCount64();
  if (game::level_controller()) {
    g_lockedAt = 0;  // the contract is under way; back in the lobby the room opens again
    return;
  }
  if (now < g_nextEnforce || !game::in_room() || !game::is_master() || !game::private_room()) return;
  g_nextEnforce = now + kEnforceMs;
  void* room = api.currentRoom(nullptr);
  if (room && api.maxPlayers(room, nullptr) == kVanilla && !(g_lockedAt && now - g_lockedAt < kLockHoldMs)) {
    // The game's own four, made eight (not through the detour, which would take four players' four for a lock).
    const int limit = room_limit(kVanilla, !api.visible(room, nullptr), api.playerCount(room, nullptr), false);
    if (limit != kVanilla) g_setMax(room, static_cast<uint8_t>(limit), nullptr);
  }
}

}  // namespace pg::lobbies::room
