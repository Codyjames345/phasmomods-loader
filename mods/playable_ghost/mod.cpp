#include "actions.h"
#include "contract_end.h"
#include "loadout.h"
#include "game.h"
#include "hide.h"
#include "hooks.h"
#include "journal.h"
#include "leave.h"
#include "lobby.h"
#include "log.h"
#include "mapview.h"
#include "alerts.h"
#include "form.h"
#include "marks.h"
#include "mimic.h"
#include "perceive_view.h"
#include "mod_glue.h"
#include "outlist.h"
#include "panorama.h"
#include "reward.h"
#include "room.h"
#include "round.h"
#include "senses.h"
#include "services.h"
#include "typestate_host.h"
#include "ui.h"
#include "UnityWorld.h"

// The playable ghost as a mod of the suite (core/mod_api.h).
namespace {

constexpr PmModInfo kInfo{PMMOD_API_VERSION, "phasmomods.playable-ghost", "Playable Ghost", 1, 0};
bool g_running = false;

bool service_ghost_controlled() {
  try {
    return g_running && pm::room::ghost_controlled();
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}
int32_t service_ghost_actor() {
  try {
    return g_running ? pm::room::ghost_actor() : 0;
  } catch (...) {
    return 0;  // nothing thrown may reach another mod
  }
}
bool service_roll_taken() {
  try {
    return g_running && pm::actions::roll_taken();
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}
constexpr PmPlayableGhostService kService{3, &service_ghost_controlled, &service_ghost_actor, &service_roll_taken};

bool start(const PmHostApi* api) {
  using namespace pm;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && lobby::resolve() &&
        mapview::resolve() && panorama::resolve() && typestate::resolve() && marks::resolve() &&
        perceive_view::resolve())) {
    PM_LOG("PlayableGhost inactive: game binding resolution failed");
    return false;
  }
  if (!(actions::create_hooks() && round::create_hooks() && senses::create_hooks() && outlist::create_hooks() &&
        contract_end::create_hooks() && loadout::create_hooks() && reward::create_hooks() && journal::create_hooks() && leave::create_hooks() && mimic::create_hooks() &&
        alerts::create_hooks() &&
        hooks::enable_all())) {
    PM_LOG("PlayableGhost inactive: hook installation failed");
    return false;
  }
  if (!phasmo::NavMeshAgent::steer.available()) {
    PM_LOG("PlayableGhost inactive: steering initialization failed");
    return false;
  }
  room::init();
  api->register_service(PM_SERVICE_PLAYABLE_GHOST, &kService);
  return true;
}

}  // namespace

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("PlayableGhost", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pmmod_tick() { pm::mod::guarded_tick(g_running, &pm::room::tick); }

extern "C" __declspec(dllexport) void pmmod_stop() {
  pm::mod::guarded_stop(g_running, [] {
    pm::room::teardown();
    pm::actions::clear();
  });
}
