#include "lobby_gate.h"

#include <algorithm>
#include <optional>

#include "JournalController.h"
#include "Photon.h"
#include "ServerManager.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "content_registry.h"
#include "entry_hooks.h"
#include "game_api.h"
#include "held.h"
#include "host_game.h"
#include "log.h"
#include "runtime.h"

namespace pm::lobby_gate {
namespace {

// The RPC a journal's guess and evidence marks send. Its method lies in a generic base class, whose open method has no code of
// its own to map (sdk/systems/journal.md), so its name is written here. A build that renames it sends its marks as the game does,
// and a client without the mods matches them against nothing.
constexpr const char* kMarkRpc = "ToEveryone_NotifyOfButtonStateChange_Rpc";

Config g;

using Press_t = void (*)(void* button, const void* method);
Press_t g_press = nullptr;

// The start's send, held while the room is closed for it: the call as the game made it, its objects kept from the collector, and
// the chain below this hook that it goes on to. game_api.h tells a detour not to keep its original, and keeping it here is safe
// only because of where this hook sits: create_hooks installs it as the chain's lowest link, at consent and before any mod can
// hook the method, the host unwinds a chain newest first, and a mod restart removes only what the mods made after this, so
// nothing can be unhooked under this one while a start is held and the trampoline the original names stays alive. release_held
// calls it on the main thread while that link stands; only the loader standing aside removes it, and nothing sends then.
struct HeldSend {
  api::Held view, name, parameters;
  int32_t target = 0;
  api::Original<phasmo::PhotonView::RPC> original{};
};
std::optional<HeldSend> g_held;

bool start_pending() {
  try {
    return g.startPending && g.startPending();
  } catch (...) {
    PM_LOG("lobby: exception while asking whether a start is underway; the start does not go ahead");
    return true;  // the safe way: nothing starts twice, and the send's answer stands
  }
}

Send start_send() {
  if (!g.startSend) return Send::AsSent;
  try {
    return static_cast<Send>(g.startSend());
  } catch (...) {
    PM_LOG("lobby: exception while holding a contract's start; it does not go ahead");
    return Send::Refused;
  }
}

bool in_a_private_room() {
  try {
    return !g.privateRoom || g.privateRoom();
  } catch (...) {
    return true;  // the rules' side: the start's own checks still answer
  }
}

// The check of a start fails the safe way: whatever goes wrong refuses it.
bool start_goes_on() {
  try {
    return g.startAllowed && g.startAllowed();
  } catch (...) {
    PM_LOG("lobby: exception while checking a contract's start; it does not go ahead");
    return false;
  }
}

bool verified_now() {
  try {
    return g.roomVerified && g.roomVerified();
  } catch (...) {
    return false;
  }
}

// Whether the value a journal's mark sends is a custom one. A view holding neither controller is judged by the lower number.
bool custom_mark(void* view, int32_t value) {
  if (!g.registry || !g.registry->game_enums().read()) return false;
  const content::GameEnums& enums = g.registry->game_enums();
  if (phasmo::Component::GetComponent(view, phasmo::JournalGhostSyncController::type.type_object()))
    return value >= enums.firstGhostType;
  if (phasmo::Component::GetComponent(view, phasmo::JournalEvidenceSyncController::type.type_object()))
    return value >= enums.firstEvidence;
  return value >= std::min(enums.firstGhostType, enums.firstEvidence);
}

void detour_press(void* button, const void* method) {
  hooks::Guard guard;
  if (press_goes_on(button)) g_press(button, method);
}

void detour_rpc(api::Original<phasmo::PhotonView::RPC> original, void* view, void* name, int32_t target, void* parameters) {
  static bool logged = false;
  Send send = Send::AsSent;
  try {
    send = decide_rpc(view, name, parameters);
  } catch (...) {
    if (!logged) PM_LOG("lobby: exception while reading an RPC; it goes as the game sent it");
    logged = true;
  }
  switch (send) {
    case Send::AsSent: original(view, name, target, parameters); return;
    case Send::Refused: return;
    case Send::Held:
      g_held.reset();  // never two: while a start is underway every other send of it is refused
      g_held.emplace(HeldSend{api::Held(view), api::Held(name), api::Held(parameters), target, original});
      return;
    case Send::NotBuffered: original(view, name, phasmo::RpcTarget::All(), parameters); return;
    case Send::HereAlone: phasmo::PhotonView::RPC_toPlayer(view, name, phasmo::PhotonNetwork::get_LocalPlayer(), parameters); return;
  }
}

}  // namespace

std::string missing() {
  // The room is closed for a start before it goes (host/start_close.h): without these it could not be, and a late joiner could
  // reach a contract.
  std::string out = hostgame::unresolved<phasmo::ServerManager::startGameButton, phasmo::Button::Press, phasmo::PhotonView::RPC,
                                         phasmo::PhotonNetwork::get_CurrentRoom, phasmo::PhotonRoom::set_IsOpen>();
  if (!api::host_only::game_entry(map::ServerManager_LoadScene.key))
    out += (out.empty() ? "" : ", ") + std::string(map::ServerManager_LoadScene.key);
  return out;
}

bool create_hooks(const Config& config) {
  g = config;
  const PmGameEntry* press = api::host_only::game_entry(map::Button_Press.key);
  if (!press || !press->code) return false;
  if (!entry_hooks::hook(press->code, {press->twins, press->twinCount}, reinterpret_cast<void*>(&detour_press),
                         reinterpret_cast<void**>(&g_press), "lobby: a contract's start, pressed", true))
    return false;
  return api::hook<phasmo::PhotonView::RPC>(&detour_rpc, "lobby: what goes out of a lobby");
}

bool press_goes_on(void* button) {
  bool isStart = false;
  try {
    isStart = button && g.isStartButton && g.isStartButton(button);
  } catch (...) {
    return true;  // nothing says it is the start, and the send's own check still stands
  }
  return !isStart || (!start_pending() && start_goes_on());
}

Send decide_rpc(void* view, void* methodName, void* parameters) {
  if (api::string_is(methodName, map::ServerManager_LoadScene.name)) {
    if (!in_a_private_room()) return Send::AsSent;
    if (!start_pending() && !start_goes_on()) return Send::Refused;
    return start_send();
  }
  const bool pageSync = api::string_is(methodName, map::JournalController_PageSync.name);
  if (!pageSync && !api::string_is(methodName, kMarkRpc)) return Send::AsSent;
  const api::RefArray arguments{parameters};
  int32_t value = 0;
  const bool read = g.boxedInt && arguments.size() > 0 && arguments[0] && g.boxedInt(arguments[0], value);
  if (pageSync) {
    // A custom page's number is never kept by the server for a later joiner, and goes only to the verified. One that cannot be
    // read is taken for custom: this turn then stays off the wire, and the next one goes.
    if (read && g.customPage && !g.customPage(view, value)) return Send::AsSent;
    return verified_now() ? Send::NotBuffered : Send::HereAlone;
  }
  return read && !verified_now() && custom_mark(view, value) ? Send::HereAlone : Send::AsSent;
}

bool holding() { return g_held.has_value(); }

bool release_held() {
  if (!g_held) return false;
  HeldSend held = std::move(*g_held);
  g_held.reset();
  held.original(held.view.get(), held.name.get(), held.target, held.parameters.get());
  return true;
}

void drop_held() { g_held.reset(); }

}  // namespace pm::lobby_gate
