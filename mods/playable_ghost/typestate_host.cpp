#include "typestate_host.h"

#include <windows.h>

#include <algorithm>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "services.h"
#include "typestate.h"

namespace pg::typestate {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kControllerThayeStep = 0xD0;      // GhostController: the Thaye's age steps (0..10)
constexpr size_t kControllerThayeThreshold = 0xD4; // GhostController: the Thaye's hunt threshold
constexpr size_t kControllerDayanState = 0xE4;     // GhostController: Normal, Empowered, Weakened
constexpr size_t kControllerGalluState = 0xE8;     // GhostController: Normal, Enraged, Exhausted
constexpr size_t kGhostObamboAggressive = 0x130;   // GhostAI: the Obambo's mood (AttemptGhostTypeStateChange)
constexpr size_t kGhostMovedThings = 0x148;        // GhostAI: HashSet<PhotonObjectInteract>, the Deildegast's
constexpr size_t kActivityNearElectronics = 0x38;  // GhostActivity: the Raiju's sensor finds electronics on

il2cpp::Class* g_controllerClass = nullptr;
il2cpp::Held g_obamboGhost;
bool g_obamboMood = false;
ULONGLONG g_obamboSince = 0;

uint8_t byte(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }

int set_count(void* set) {
  const il2cpp::Method* m = set ? il2cpp::method(il2cpp::class_of(set), "get_Count", 0) : nullptr;
  return m ? reinterpret_cast<int (*)(void*, const void*)>(*reinterpret_cast<void* const*>(m))(set, m) : 0;
}

}  // namespace

bool resolve() {
  g_controllerClass = il2cpp::cls("Assembly-CSharp.dll", "", "GhostController");
  return g_controllerClass != nullptr;
}

void forget() {
  g_obamboGhost = nullptr;
  g_obamboMood = false;
  g_obamboSince = 0;
}

void read(void* ghost, int behaviourType, int identity, uint8_t trait[2]) {
  trait[0] = trait[1] = 0;
  const int who = subject(behaviourType, identity);
  void* controller = il2cpp::static_field_by_type(g_controllerClass, "GhostController");
  if (controller && !game::alive(controller)) controller = nullptr;
  switch (who) {
    case kGallu:
      if (controller) trait[0] = byte(il2cpp::field<int>(controller, kControllerGalluState));
      break;
    case kDayan:
      if (controller) trait[0] = byte(il2cpp::field<int>(controller, kControllerDayanState));
      break;
    case kThaye:
      if (controller) {
        trait[0] = byte(il2cpp::field<int>(controller, kControllerThayeStep));
        trait[1] = byte(il2cpp::field<int>(controller, kControllerThayeThreshold));
      }
      break;
    case kObambo: {
      const bool mood = il2cpp::field<bool>(ghost, kGhostObamboAggressive);
      const ULONGLONG now = GetTickCount64();
      if (ghost != g_obamboGhost || mood != g_obamboMood) {
        g_obamboGhost = ghost;
        g_obamboMood = mood;
        g_obamboSince = now;
      }
      trait[0] = mood ? 1 : 0;
      trait[1] = byte(static_cast<int>((now - g_obamboSince) / 1000));
      break;
    }
    case kRaiju:
      // The game updates it only in a hunt (its hunt speed), so outside one it says nothing.
      if (void* activity = game::ghost_activity(ghost); activity && game::ghost_state(ghost) == game::kStateHunting)
        trait[0] = il2cpp::field<bool>(activity, kActivityNearElectronics) ? 1 : 0;
      break;
    case kDeildegast:
      trait[0] = byte(set_count(il2cpp::field<void*>(ghost, kGhostMovedThings)));
      break;
    default:
      if (identity >= kEcho) {  // a custom ghost: the Custom Ghosts mod knows its state
        const auto* s = static_cast<const PgCustomGhostsService*>(mod::host().service(PG_SERVICE_CUSTOM_GHOSTS));
        if (s && s->version >= 2 && s->sense) trait[0] = byte(s->sense(identity));
      }
      break;
  }
}

}  // namespace pg::typestate
