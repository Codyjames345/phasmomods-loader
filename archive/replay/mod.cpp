#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "recorder.h"
#include "ui.h"
#include "view.h"

// Replay as a mod of the suite (core/mod_api.h): the contract's top moments, recorded by the host and
// shown to everyone running this mod in the lobby afterwards (mods/replay/recorder.h, view.h). With
// the Playable Ghost mod it also names who played the ghost.
namespace {

constexpr PgModInfo kInfo{PGMOD_API_VERSION, "replay", "Replay", 4, PG_MOD_VANILLA_PEERS};
bool g_running = false;

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && replay::recorder::resolve() && replay::view::resolve())) {
    PG_LOG("Replay inactive: game binding resolution failed");
    return false;
  }
  if (!(replay::recorder::create_hooks() && hooks::enable_all())) {
    PG_LOG("Replay inactive: hook installation failed");
    return false;
  }
  net::set_handler(&replay::recorder::receive);
  return true;
}

void tick() {
  using namespace pg::replay;
  recorder::tick();
  Replay r;
  if (recorder::take(r)) view::show(r);
  view::tick();
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("Replay", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    pg::replay::view::hide();
    pg::replay::recorder::reset();
  });
}
