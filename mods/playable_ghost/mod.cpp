#include "actions.h"
#include "body.h"
#include "equipment.h"
#include "game.h"
#include "hide.h"
#include "hooks.h"
#include "il2cpp.h"
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
#include "panorama.h"
#include "reward.h"
#include "room.h"
#include "round.h"
#include "senses.h"
#include "services.h"
#include "steer.h"
#include "typestate_host.h"
#include "ui.h"

// The playable ghost as a mod of the suite (core/mod_api.h).
namespace {

constexpr PgModInfo kInfo{PGMOD_API_VERSION, "playable-ghost", "Playable Ghost", 1, PG_MOD_VANILLA_PEERS};
bool g_running = false;

bool service_ghost_controlled() {
  try {
    return g_running && pg::room::ghost_controlled();
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}
int32_t service_ghost_actor() {
  try {
    return g_running ? pg::room::ghost_actor() : 0;
  } catch (...) {
    return 0;  // nothing thrown may reach another mod
  }
}
constexpr PgPlayableGhostService kService{2, &service_ghost_controlled, &service_ghost_actor};

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && lobby::resolve() && round::resolve() && senses::resolve() &&
        body::resolve() && equipment::resolve() && reward::resolve() && leave::resolve() && journal::resolve() &&
        mapview::resolve() && panorama::resolve() && hide::resolve() && typestate::resolve() && marks::resolve() &&
        alerts::resolve() && perceive_view::resolve() && form::resolve())) {
    PG_LOG("PlayableGhost inactive: game binding resolution failed");
    return false;
  }
  if (!(actions::create_hooks() && round::create_hooks() && senses::create_hooks() && body::create_hooks() &&
        equipment::create_hooks() && reward::create_hooks() && journal::create_hooks() && leave::create_hooks() && mimic::create_hooks() &&
        alerts::create_hooks() &&
        hooks::enable_all())) {
    PG_LOG("PlayableGhost inactive: hook installation failed");
    return false;
  }
  // Called from the host's LateUpdate: the crowd update has finished, so steering is patched here.
  if (!steer::install(api->unityPlayer)) {
    PG_LOG("PlayableGhost inactive: steering initialization failed");
    return false;
  }
  room::init();
  api->register_service(PG_SERVICE_PLAYABLE_GHOST, &kService);
  return true;
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("PlayableGhost", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &pg::room::tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  const bool wasRunning = g_running;
  pg::mod::guarded_stop(g_running, [] {
    pg::room::teardown();
    pg::actions::clear();
  });
  if (wasRunning) pg::steer::uninstall();  // even when tearing down threw: the patch must not outlive the mod
}
