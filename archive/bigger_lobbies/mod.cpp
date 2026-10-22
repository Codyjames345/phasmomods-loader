#include "cards.h"
#include "expand.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "markers.h"
#include "mod_glue.h"
#include "monitor.h"
#include "room.h"
#include "ui.h"

// Bigger Lobbies as a mod of the suite (core/mod_api.h): private rooms for eight players
// (mods/bigger_lobbies). Every player in the room must run it: the game's four-player lists grow on
// each client, and a client without them cannot show a fifth player.
namespace {

constexpr PgModInfo kInfo{PGMOD_API_VERSION, "bigger-lobbies", "Bigger Lobbies", 5, 0};
bool g_running = false;

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && lobbies::expand::resolve() && lobbies::room::resolve() &&
        lobbies::cards::resolve() && lobbies::monitor::resolve() && lobbies::markers::resolve())) {
    PG_LOG("BiggerLobbies inactive: game binding resolution failed");
    return false;
  }
  if (!(lobbies::room::create_hooks() && lobbies::markers::create_hooks() && hooks::enable_all())) {
    PG_LOG("BiggerLobbies inactive: hook installation failed");
    return false;
  }
  return true;
}

void tick() {
  using namespace pg::lobbies;
  if (!pg::game::private_room()) return;  // public rooms stay the game's own
  room::tick();
  cards::tick();
  monitor::tick();
  markers::tick();
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("BiggerLobbies", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    pg::lobbies::cards::teardown();
    pg::lobbies::monitor::teardown();
  });
}
