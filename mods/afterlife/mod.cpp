#include "game.h"
#include "haunt.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "ui.h"

// Afterlife as a mod of the suite (core/mod_api.h): a dead investigator may watch through the ghost's
// eyes (mods/afterlife/haunt.h). It sends nothing, so it needs nobody else to run it.
namespace {

constexpr PgModInfo kInfo{PGMOD_API_VERSION, "afterlife", "Afterlife", 3, PG_MOD_VANILLA_PEERS};
bool g_running = false;

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && afterlife::resolve())) {
    PG_LOG("Afterlife inactive: game binding resolution failed");
    return false;
  }
  return true;
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("Afterlife", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &pg::afterlife::tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    pg::afterlife::teardown();
  });
}
