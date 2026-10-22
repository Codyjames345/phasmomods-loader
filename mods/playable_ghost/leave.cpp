#include "leave.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>


#include "body.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::leave {
namespace {

// Written by PauseMenuController.Leave before it loads the menu (build 25811160, obfuscated names).
constexpr size_t kNetworkSceneChanging = 0xD8;  // Network: set by every scene change the game makes
constexpr size_t kLevelValuesInLevel = 0x20;    // LevelValues: cleared on leaving a level
// The vanilla leave (PauseMenuController's coroutine) disconnects once the loading screen is ready
// (LoadingController +0x30) and a frame has passed; kLeaveTimeoutMs bounds the wait.
constexpr size_t kLoadingReady = 0x30;
constexpr ULONGLONG kLeaveTimeoutMs = 15000;

struct Api {
  il2cpp::Class* loadingController = nullptr;
  il2cpp::Class* levelValues = nullptr;
  void* pauseLeave = nullptr;  // PauseMenuController.Leave(), hooked
  void* pauseQuit = nullptr;   // PauseMenuController.Quit(), hooked
  void* (*networkInstance)(const void*) = nullptr;
  void (*loadScene)(void*, bool, int, il2cpp::String*, bool, const void*) = nullptr;
  void (*disconnect)(const void*) = nullptr;
  bool (*isConnected)(const void*) = nullptr;
  il2cpp::Class* photonNetwork = nullptr;
  intptr_t (*socketHandle)(void*, const void*) = nullptr;  // System.Net.Sockets.Socket.get_Handle
  void* (*acquireSocket)(intptr_t) = nullptr;
  void (*releaseSocket)(intptr_t, void*, bool) = nullptr;
  void* shutdown = nullptr;  // GameAssembly's il2cpp_shutdown export, hooked
  void (*quit)(const void*) = nullptr;
  void (*setListenerVolume)(float, const void*) = nullptr;
} api;

using Button_t = void (*)(void*, const void*);
using Shutdown_t = void (*)();
Button_t g_leave = nullptr;
Button_t g_quit = nullptr;
Shutdown_t g_shutdown = nullptr;
ULONGLONG g_leaveStarted = 0;  // a leave is under way (local clock), until Photon is disconnected
bool g_loadingReady = false;    // the loading screen was ready last frame: disconnect this frame
bool g_leaving = false;        // from the leave until the next level

void leave() {
  void* loading = api.loadingController ? il2cpp::static_field_by_type(api.loadingController, "LoadingController") : nullptr;
  if (!loading) {
    PG_LOG("leave: no LoadingController; cannot leave the contract");
    return;
  }
  g_leaving = true;
  body::teardown();  // the rig, camera and body come back for the main menu
  if (void* values = il2cpp::static_field_by_type(api.levelValues, "LevelValues")) il2cpp::set_field<bool>(values, kLevelValuesInLevel, false);
  if (void* network = api.networkInstance(nullptr)) il2cpp::set_field<bool>(network, kNetworkSceneChanging, true);
  api.setListenerVolume(0, nullptr);
  api.loadScene(loading, false, 0, il2cpp::string_new(""), true, nullptr);
  g_leaveStarted = GetTickCount64();
  g_loadingReady = false;
  PG_LOG("leave: leaving the contract");
}

void detour_leave(void* menu, const void* method) {
  hooks::Guard guard;
  if (!body::camera()) {
    g_leave(menu, method);
    return;
  }
  leave();
}

void detour_quit(void* menu, const void* method) {
  hooks::Guard guard;
  if (!body::camera()) {
    g_quit(menu, method);
    return;
  }
  PG_LOG("leave: quitting the game");
  body::teardown();
  api.quit(nullptr);
}

// IL2CPP's socket table (build 25811160): a managed Socket's Handle is a key, not the Windows socket.
// AcquireSocketHandle(key) returns its entry with a reference added; the entry's first field is the
// runtime's socket object, which holds the Windows SOCKET at +0x8 (it is what recvfrom is given).
// ReleaseSocketHandle(key, entry, false) takes the reference back.
constexpr uintptr_t kAcquireSocketRva = 0x3EB650;
constexpr uintptr_t kReleaseSocketRva = 0x3EBC80;
constexpr size_t kRuntimeSocketOs = 0x8;

// Photon's UDP socket, through Photon's own objects (obfuscated names): PhotonNetwork's
// LoadBalancingClient +0x10 its LoadBalancingPeer, PhotonPeer +0xD0 its connection (EnetPeer), +0x28
// that connection's SocketUdp, +0x50 its System.Net.Sockets.Socket. Returns the Windows SOCKET.
SOCKET photon_socket() {
  void* client = api.photonNetwork ? il2cpp::static_field_by_type(api.photonNetwork, "Photon.Realtime.LoadBalancingClient") : nullptr;
  void* peer = client ? il2cpp::field<void*>(client, 0x10) : nullptr;
  void* connection = il2cpp::is_class(peer, "LoadBalancingPeer") ? il2cpp::field<void*>(peer, 0xD0) : nullptr;
  void* udp = il2cpp::is_class(connection, "EnetPeer") ? il2cpp::field<void*>(connection, 0x28) : nullptr;
  void* socket = il2cpp::is_class(udp, "SocketUdp") ? il2cpp::field<void*>(udp, 0x50) : nullptr;
  if (!il2cpp::is_class(socket, "Socket")) {
    PG_LOG("leave: Photon's socket not found (client %p peer %p connection %p udp %p)", client, peer, connection, udp);
    return INVALID_SOCKET;
  }
  const intptr_t key = api.socketHandle(socket, nullptr);
  void* entry = key != -1 ? api.acquireSocket(key) : nullptr;
  void* runtime = entry ? *static_cast<void**>(entry) : nullptr;
  const SOCKET os = runtime ? il2cpp::field<SOCKET>(runtime, kRuntimeSocketOs) : INVALID_SOCKET;
  if (entry) api.releaseSocket(key, entry, false);
  PG_LOG("leave: Photon's socket key %lld is Windows socket %llu", static_cast<long long>(key), static_cast<unsigned long long>(os));
  return os;
}

// The game is closing: the runtime's shutdown waits for every managed thread to end, and Photon's
// receive thread only ends once its socket is closed. Photon normally disconnects as its handler is
// disabled on quit; if it is still connected here (a hung exit, seen with the ghost player in a
// contract) it is disconnected now, the game's own way.
void detour_shutdown() {
  hooks::Guard guard;
  try {
    // Photon's receive thread waits in a blocking Socket.Receive, which keeps the socket open while
    // it waits, so disconnecting alone does not end it: alone in a room nothing arrives (and only
    // the server's datagrams reach a connected socket). Once disconnected, its pending receive is
    // cancelled; the receive fails, the thread sees it is disconnected and ends.
    const SOCKET handle = photon_socket();
    if (api.isConnected(nullptr)) PG_LOG("leave: the game is closing with Photon still connected; disconnecting first");
    api.disconnect(nullptr);  // does nothing once the client is fully disconnected
    if (handle != INVALID_SOCKET && handle != 0) {
      const bool cancelled = CancelIoEx(reinterpret_cast<HANDLE>(handle), nullptr) != 0;
      PG_LOG("leave: Photon's pending receive %s", cancelled ? "cancelled" : "was not pending");
    }
  } catch (...) {
    PG_LOG("leave: Photon could not be disconnected before shutdown");
  }
  g_shutdown();
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  api.loadingController = il2cpp::cls(kGame, "", "LoadingController");
  api.levelValues = il2cpp::cls(kGame, "", "LevelValues");
  il2cpp::Class* pause = il2cpp::cls(kGame, "", "PauseMenuController");
  api.pauseLeave = il2cpp::method_ptr(pause, "Leave", 0);
  api.pauseQuit = il2cpp::method_ptr(pause, "Quit", 0);
  api.networkInstance = reinterpret_cast<decltype(api.networkInstance)>(
      il2cpp::method_ptr(il2cpp::cls(kGame, "", "Network"), "get_Instance", 0));
  api.loadScene = reinterpret_cast<decltype(api.loadScene)>(il2cpp::method_ptr(api.loadingController, "LoadScene", 4));
  api.disconnect = reinterpret_cast<decltype(api.disconnect)>(
      il2cpp::method_ptr(il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonNetwork"), "Disconnect", 0));
  api.photonNetwork = il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonNetwork");
  api.socketHandle = reinterpret_cast<decltype(api.socketHandle)>(
      il2cpp::method_ptr(il2cpp::cls("System.dll", "System.Net.Sockets", "Socket"), "get_Handle", 0));
  api.acquireSocket = reinterpret_cast<decltype(api.acquireSocket)>(game::game_assembly_address(kAcquireSocketRva));
  api.releaseSocket = reinterpret_cast<decltype(api.releaseSocket)>(game::game_assembly_address(kReleaseSocketRva));
  api.isConnected = reinterpret_cast<decltype(api.isConnected)>(
      il2cpp::method_ptr(il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonNetwork"), "get_IsConnected", 0));
  if (HMODULE ga = GetModuleHandleW(L"GameAssembly.dll")) api.shutdown = reinterpret_cast<void*>(GetProcAddress(ga, "il2cpp_shutdown"));
  api.quit = reinterpret_cast<decltype(api.quit)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Application"), "Quit", 0));
  api.setListenerVolume = reinterpret_cast<decltype(api.setListenerVolume)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioListener"), "set_volume", 1));
  const bool ok = api.levelValues && api.pauseLeave && api.pauseQuit && api.networkInstance && api.loadScene &&
                  api.disconnect && api.isConnected && api.photonNetwork && api.socketHandle && api.shutdown && api.quit && api.setListenerVolume;
  PG_LOG("leave: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(api.pauseLeave, reinterpret_cast<void*>(&detour_leave), reinterpret_cast<void**>(&g_leave),
                       "PauseMenuController.Leave") &&
         hooks::create(api.pauseQuit, reinterpret_cast<void*>(&detour_quit), reinterpret_cast<void**>(&g_quit),
                       "PauseMenuController.Quit") &&
         hooks::create(api.shutdown, reinterpret_cast<void*>(&detour_shutdown), reinterpret_cast<void**>(&g_shutdown),
                       "il2cpp_shutdown");
}

bool leaving() {
  if (g_leaving && !game::level_controller()) g_leaving = false;  // the level is gone
  return g_leaving;
}

void tick_background() {
  if (!g_leaveStarted) return;
  void* loading = api.loadingController ? il2cpp::static_field_by_type(api.loadingController, "LoadingController") : nullptr;
  const bool timedOut = GetTickCount64() - g_leaveStarted >= kLeaveTimeoutMs;
  if (!g_loadingReady && !timedOut) {
    g_loadingReady = game::alive(loading) && il2cpp::field<bool>(loading, kLoadingReady);
    return;  // as the coroutine does: the frame after the loading screen is ready
  }
  g_leaveStarted = 0;
  try {
    api.disconnect(nullptr);
    PG_LOG("leave: disconnected after leaving%s", timedOut ? " (the loading screen never reported ready)" : "");
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("leave: Photon's disconnect after leaving threw", e.ex);
  } catch (...) {
    PG_LOG("leave: Photon's disconnect after leaving threw");
  }
}

}  // namespace pg::leave
