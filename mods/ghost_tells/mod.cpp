#include <windows.h>

#include <algorithm>
#include <vector>

#include "game.h"
#include "gate.h"
#include "goryo.h"
#include "hold.h"
#include "hooks.h"
#include "il2cpp.h"
#include "journal.h"
#include "log.h"
#include "mare.h"
#include "mod_glue.h"
#include "net.h"
#include "onryo.h"
#include "rolls.h"
#include "services.h"
#include "shade.h"
#include "spirit.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"
#include "yurei.h"

// Ghost Tells as a mod of the suite (core/mod_api.h): three new tells each for the Spirit, Mare, Shade,
// Yurei, Goryo and Onryo (docs/ghost-tells.md). It acts only in a private room in which every client
// runs it; otherwise the game is as it was. Players without the mod may share the lobby: the contract is
// then plain vanilla.
namespace {

constexpr const char* kId = "ghost-tells";
constexpr PgModInfo kInfo{PGMOD_API_VERSION, kId, "Ghost Tells", 6, PG_MOD_VANILLA_PEERS};

bool g_running = false;
std::vector<int> g_actors;
il2cpp::Held g_contract;  // the LevelController the decision below was made for
bool g_on = false;        // on for this contract: held until it unloads

bool everyone_has_mod() {
  using namespace pg;
  if (!game::private_room()) return false;
  game::room_actors(g_actors);
  if (g_actors.empty()) return false;
  for (int actor : g_actors)
    if (!mod::peer_has(actor, kId)) return false;
  return true;
}

/**
 * Decided per contract and then held, as Spectral Flame does: a peer's presence can lapse for a frame,
 * and switching off mid-contract would leave a rule half done. Asked by the journal too, which may
 * localise a page before this mod's first tick in the contract.
 */
bool decide() {
  using namespace pg;
  void* level = game::level_controller();
  const bool loaded = level && game::alive(level);
  if (level != g_contract) {
    g_contract = level;
    g_on = false;
  }
  if (loaded && !g_on && everyone_has_mod()) {
    g_on = true;
    PG_LOG("GhostTells: on for this contract");
  }
  return loaded && g_on;
}

bool held_back(int kind) {
  try {
    using pg::tells::gate::Kind;
    return (kind == pg::tells::gate::kInteract || kind == pg::tells::gate::kEvent || kind == pg::tells::gate::kHunt) &&
           pg::tells::gate::held_back(static_cast<Kind>(kind));
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}

constexpr PgGhostTellsService kService{1, &held_back};

// Messages from other clients, acted on by the host, and the host's, shown by every client.
void receive(int sender, const uint8_t* data, size_t size) {
  using namespace pg::tells;
  if (!tells_on()) return;
  switch (net::kind_of(data, size)) {
    case net::kName:
      if (pg::game::is_master() && net::read_name_heard(data, size)) spirit::heard_name(sender);
      break;
    case net::kTrap:
      if (pg::game::is_master() && net::read_trapped(data, size)) yurei::trapped();
      break;
    case net::kGoryoShow: {
      bool on = false;
      if (sender == pg::game::master_actor() && net::read_goryo_show(data, size, on)) goryo::receive_show(on);
      break;
    }
    case net::kOnryoCount: {
      int count = 0;
      if (sender == pg::game::master_actor() && net::read_onryo_count(data, size, count)) onryo::receive_count(count);
      break;
    }
    case net::kOnryoSpite: {
      int view = 0;
      float seconds = 0;
      if (sender == pg::game::master_actor() && net::read_onryo_spite(data, size, view, seconds)) onryo::receive_spite(view, seconds);
      break;
    }
    default: break;
  }
}

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && tells::spirit::resolve() && tells::mare::resolve() && tells::shade::resolve() &&
        tells::yurei::resolve() && tells::goryo::resolve() && tells::onryo::resolve())) {
    PG_LOG("GhostTells inactive: game binding resolution failed");
    return false;
  }
  if (!(tells::journal::create_hooks() && tells::gate::create_hooks() && tells::rolls::create_hooks() &&
        tells::spirit::create_hooks() && tells::mare::create_hooks() && tells::yurei::create_hooks() &&
        tells::onryo::create_hooks() && hooks::enable_all())) {
    PG_LOG("GhostTells inactive: hook installation failed");
    return false;
  }
  net::set_handler(&receive);
  api->register_service(PG_SERVICE_GHOST_TELLS, &kService);
  return true;
}

void reset_all() {
  using namespace pg::tells;
  gate::tick(nullptr);
  spirit::reset();
  mare::reset();
  shade::reset();
  yurei::reset();
  goryo::reset();
  onryo::reset();
  hold::reset();
}

ULONGLONG g_lastTick = 0;
bool g_acting = false;  // a told ghost had its rules run last frame

void tick() {
  using namespace pg;
  const ULONGLONG now = GetTickCount64();
  const float dt = g_lastTick ? std::min((now - g_lastTick) / 1000.0f, 0.25f) : 0.0f;
  g_lastTick = now;
  void* ghost = decide() ? game::ghost_ai() : nullptr;
  const int type = tells::world::behaviour_type(ghost);
  // Out of a contract, or a ghost with no tells (a Mimic may switch to another type): every rule ends,
  // so no door stays locked, no flame stays low and no figure stays on camera.
  if (!ghost || !game::alive(ghost) || !tells::told(type)) {
    if (g_acting) reset_all();
    g_acting = false;
    return;
  }
  g_acting = true;
  const bool host = game::is_master();  // the host runs the ghost
  tells::gate::tick(host ? ghost : nullptr);
  if (host) {
    tells::spirit::tick_host(ghost, type, dt);
    tells::shade::tick_host(ghost, type, dt);
    tells::yurei::tick_host(ghost, type, dt);
    tells::goryo::tick_host(ghost, type, dt);
    tells::onryo::tick_host(ghost, type, dt);
  }
  tells::goryo::tick_all(ghost, type);
  tells::onryo::tick_all(type, dt);
  tells::mare::tick(ghost, type, host, dt);
}

}  // namespace

bool pg::tells::tells_on() {
  try {
    return g_running && decide();
  } catch (...) {
    return false;  // asked from the journal's detour: nothing thrown may reach the game
  }
}

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("GhostTells", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    reset_all();
    g_acting = false;
    g_contract = nullptr;
    g_on = false;
  });
}
