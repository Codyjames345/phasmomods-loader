#include "mimic.h"

#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::mimic {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr uintptr_t kAttemptStateChangeRva = 0x22AEFD0;  // GhostController.AttemptGhostTypeStateChange
constexpr size_t kControllerType = 0x28;           // GhostController traits: what the ghost is
constexpr size_t kControllerTimerFired = 0xB4;     // bool: the type's own timer asks for a change
constexpr size_t kControllerOverridden = 0xBC;     // Nullable<GhostType>: hasValue (bool) ...
constexpr size_t kControllerOverride = 0xC0;       // ... and value: the next copy, used once
constexpr size_t kControllerCopies = 0xC8;         // List<GhostType>: what the Mimic may copy
constexpr int kMimic = 20;

using Attempt_t = void (*)(void*, const void*);
Attempt_t g_attempt = nullptr;
uint8_t g_pick = session::kNoType;

bool offered(void* controller, int type) {
  auto* copies = il2cpp::field<il2cpp::List*>(controller, kControllerCopies);
  for (int i = 0; copies && copies->items && i < copies->size; ++i)
    if (copies->items->data<int>()[i] == type) return true;
  return false;
}

void detour_attempt(void* controller, const void* method) {
  hooks::Guard guard;
  // Only at the game's own moment to switch, and only to a type it would itself copy.
  if (controller && g_pick != session::kNoType && il2cpp::field<int>(controller, kControllerType) == kMimic &&
      il2cpp::field<bool>(controller, kControllerTimerFired) && !il2cpp::field<bool>(controller, kControllerOverridden) &&
      offered(controller, g_pick)) {
    il2cpp::set_field<bool>(controller, kControllerOverridden, true);
    il2cpp::set_field<int>(controller, kControllerOverride, g_pick);
    PG_LOG("mimic: copying the type its player chose (%d)", g_pick);
    g_pick = session::kNoType;
  }
  g_attempt(controller, method);
}

}  // namespace

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kAttemptStateChangeRva), reinterpret_cast<void*>(&detour_attempt),
                       reinterpret_cast<void**>(&g_attempt), "GhostController.AttemptGhostTypeStateChange");
}

void tick(session::Session& s, void* ghost) {
  uint8_t pick;
  if (s.take_mimic_pick(pick)) g_pick = pick;
  if (!ghost) {
    g_pick = session::kNoType;
    return;
  }
  static il2cpp::Class* const controllerClass = il2cpp::cls("Assembly-CSharp.dll", "", "GhostController");
  void* controller = il2cpp::static_field_by_type(controllerClass, "GhostController");
  if (!controller || !game::alive(controller) || il2cpp::field<int>(controller, kControllerType) != kMimic) return;
  std::vector<uint8_t> types;
  auto* copies = il2cpp::field<il2cpp::List*>(controller, kControllerCopies);
  for (int i = 0; copies && copies->items && i < copies->size; ++i) {
    const int t = copies->items->data<int>()[i];
    if (t >= 0 && t < session::kNoType) types.push_back(static_cast<uint8_t>(t));
  }
  s.offer_mimic(types);
}

uint8_t next() { return g_pick; }

}  // namespace pg::mimic
