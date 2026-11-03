#include "typestate_host.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "GhostAI.h"
#include "GhostController.h"
#include "content.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "typestate.h"

namespace pm::typestate {
namespace {

using phasmo::GhostController;

api::Held g_obamboGhost;
bool g_obamboMood = false;
ULONGLONG g_obamboSince = 0;

uint8_t byte(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }
uint8_t byte(size_t v) { return static_cast<uint8_t>(std::min<size_t>(v, 255)); }

}  // namespace

// The game members it reads are map entries, which the host checks before the mod starts.
bool resolve() { return true; }

void forget() {
  g_obamboGhost = nullptr;
  g_obamboMood = false;
  g_obamboSince = 0;
}

bool read(void* ghost, int behaviourType, uint8_t trait[2]) {
  trait[0] = trait[1] = 0;
  if (const int32_t own = api::content::hud_state(); own >= 0) {  // a registered ghost's, by its definition
    trait[0] = byte(own);
    return true;
  }
  const std::string who = api::content::id_of(behaviourType);
  void* controller = GhostController::instance.get();
  if (controller && !api::alive(controller)) controller = nullptr;
  if (who == "vanilla.gallu") {
    if (controller) trait[0] = byte(GhostController::galluState.get(controller));
  } else if (who == "vanilla.dayan") {
    if (controller) trait[0] = byte(GhostController::dayanState.get(controller));
  } else if (who == "vanilla.thaye") {
    if (controller) {
      trait[0] = byte(GhostController::thayeStep.get(controller));
      trait[1] = byte(GhostController::thayeThreshold.get(controller));
    }
  } else if (who == "vanilla.obambo") {
    const bool mood = phasmo::GhostAI::obamboAggressive.get(ghost);
    const ULONGLONG now = GetTickCount64();
    if (ghost != g_obamboGhost.get() || mood != g_obamboMood) {
      g_obamboGhost = ghost;
      g_obamboMood = mood;
      g_obamboSince = now;
    }
    trait[0] = mood ? 1 : 0;
    trait[1] = byte(static_cast<int>((now - g_obamboSince) / 1000));
  } else if (who == "vanilla.raiju") {
    // The game updates it only in a hunt (its hunt speed), so outside one it says nothing.
    if (void* activity = game::ghost_activity(ghost); activity && game::ghost_state(ghost) == phasmo::GhostState::hunting())
      trait[0] = phasmo::GhostActivity::nearElectronics.get(activity) ? 1 : 0;
  } else if (who == "vanilla.deildegast") {
    trait[0] = byte(api::count(phasmo::GhostAI::movedProps.get(ghost)));
  }
  return false;
}

}  // namespace pm::typestate
