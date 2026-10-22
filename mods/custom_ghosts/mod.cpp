#include <string>
#include <vector>

#include "behaviour.h"
#include "difficulty.h"
#include "echo.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "apparition.h"
#include "asterion.h"
#include "bogeyman.h"
#include "brownie.h"
#include "flame_net.h"
#include "hydra.h"
#include "gremlin.h"
#include "kikimora.h"
#include "lethe.h"
#include "lithobolia.h"
#include "mesmer.h"
#include "tempest.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "pages.h"
#include "registry.h"
#include "restraint.h"
#include "services.h"
#include "sound.h"
#include "ui.h"

// The custom ghosts (Echo, Dullahan, Kikimora, Draugr, Black Shuck) as a mod of the suite
// (core/mod_api.h). They join the game's ghost types only in a private room where every client runs
// this mod: anyone without it would have no journal page or evidence for them.
namespace {

constexpr const char* kId = "custom-ghosts";
// Players without the mod may share its lobby: the custom ghosts stay out of any contract where not
// everyone runs it (everyone_has_mod), which is then vanilla.
constexpr PgModInfo kInfo{PGMOD_API_VERSION, kId, "Custom Ghosts", 2, PG_MOD_RECORDS_VOICE | PG_MOD_VANILLA_PEERS};
bool g_running = false;
std::vector<int> g_actors;

// Two of the custom ghosts (and more to come) carry Spectral Flame: without it running for everyone
// they would give an evidence nobody can find, so the custom ghosts wait for it.
bool spectral_flame_ready() {
  const auto* flame = static_cast<const PgSpectralFlameService*>(pg::mod::host().service(PG_SERVICE_SPECTRAL_FLAME));
  return flame && flame->version >= 1 && flame->ready && flame->ready();
}

bool everyone_has_mod() {
  if (!pg::game::private_room() || !spectral_flame_ready()) return false;
  pg::game::room_actors(g_actors);
  if (g_actors.empty()) return false;
  for (int actor : g_actors)
    if (!pg::mod::peer_has(actor, kId)) return false;
  return true;
}

bool ghost_controlled() {
  const auto* pg = static_cast<const PgPlayableGhostService*>(pg::mod::host().service(PG_SERVICE_PLAYABLE_GHOST));
  return pg && pg->version >= 1 && pg->ghostControlled && pg->ghostControlled();
}

const char* ability_name(int type) {
  const pg::custom::Ghost* g = pg::custom::find(type);
  return g ? g->ability : nullptr;
}

const char* ability_description(int type) {
  const pg::custom::Ghost* g = pg::custom::find(type);
  return g ? g->abilityDescription : nullptr;
}

bool use_ability(int type) {
  using namespace pg;
  try {
    int haunting = 0;
    void* ghost = game::is_master() && custom::registry::active() ? custom::registry::custom_ghost(haunting) : nullptr;
    if (!ghost || haunting != type) return false;
    if (type == custom::kEcho) return custom::echo::speak(ghost, 0);
    if (type == custom::kKikimora) return custom::kikimora::hide(ghost, custom::kikimora::nearest(ghost));
    if (type == custom::kLithobolia) return custom::lithobolia::throw_now(ghost);
    if (type == custom::kGremlin) return custom::gremlin::short_out(ghost);
  } catch (...) {
    PG_LOG("ability: exception while using a custom ghost's ability");  // nothing thrown may reach another mod
  }
  return false;
}

int sense(int type) {
  using namespace pg;
  try {
    if (type == custom::kEcho) return custom::echo::dormant_now() ? 1 : 0;
    if (type == custom::kKikimora) return custom::restraint::held_back(type, custom::restraint::kInteract) ? 1 : 0;
    return custom::behaviour::sense(type);
  } catch (...) {
    return -1;  // nothing thrown may reach another mod
  }
}

bool held_back(int type, int kind) {
  using namespace pg;
  try {
    return (kind == custom::restraint::kInteract || kind == custom::restraint::kEvent) &&
           custom::restraint::held_back(type, static_cast<custom::restraint::Kind>(kind));
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}

int scent(float* xyz, int maxPoints) {
  try {
    return pg::custom::behaviour::scent(xyz, maxPoints);
  } catch (...) {
    return 0;  // nothing thrown may reach another mod
  }
}

// The Hydra's Spectral Flame burns green, a tell even with the evidence withheld.
bool green_flame(int type) { return type == pg::custom::kHydra; }

// Every message on the channel: the flame ghosts' ('C' 'G' 'F') to their own, the rest to the Echo.
void receive(int sender, const uint8_t* data, size_t size) {
  using namespace pg::custom;
  switch (net_msg::kind_of(data, size)) {
    case net_msg::kDecoys:
    case net_msg::kDouble: hydra::receive(sender, data, size); break;
    case net_msg::kHit: lithobolia::receive(sender, data, size); break;
    case net_msg::kStrike: tempest::receive(sender, data, size); break;
    case net_msg::kViewing:
    case net_msg::kForget: lethe::receive(sender, data, size); break;
    case 0: echo::receive(sender, data, size); break;
    default: break;
  }
}

constexpr PgCustomGhostsService kService{5, &ability_name, &ability_description, &use_ability, &sense, &held_back, &scent,
                                         &green_flame};

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve() && custom::registry::resolve() && custom::difficulty::resolve() && custom::pages::resolve() &&
        custom::behaviour::resolve() && custom::kikimora::resolve() && custom::echo::resolve() && custom::sound::resolve() &&
        custom::apparition::resolve() &&
        custom::hydra::resolve() && custom::asterion::resolve() && custom::lithobolia::resolve() &&
        custom::gremlin::resolve() && custom::bogeyman::resolve() && custom::brownie::resolve() &&
        custom::lethe::resolve() && custom::tempest::resolve() && custom::mesmer::resolve())) {
    PG_LOG("CustomGhosts inactive: game binding resolution failed");
    return false;
  }
  const std::wstring picks = std::wstring(api->dataDirectory) + L"\\custom_difficulty.txt";
  if (!(custom::registry::create_hooks() && custom::difficulty::create_hooks(picks.c_str(), &everyone_has_mod) &&
        custom::behaviour::create_hooks() && custom::echo::create_hooks() && custom::restraint::create_hooks() && custom::lethe::create_hooks() && custom::mesmer::create_hooks() &&
        custom::brownie::create_hooks() && custom::tempest::create_hooks() &&
        hooks::enable_all())) {
    PG_LOG("CustomGhosts inactive: hook installation failed");
    return false;
  }
  net::set_handler(&receive);
  api->register_service(PG_SERVICE_CUSTOM_GHOSTS, &kService);
  return true;
}

void tick() {
  using namespace pg;
  custom::registry::tick(everyone_has_mod());
  custom::difficulty::tick();
  custom::pages::tick(custom::registry::active());
  int type = 0;
  void* haunting = custom::registry::active() ? custom::registry::custom_ghost(type) : nullptr;
  void* ghost = game::is_master() ? haunting : nullptr;  // the host runs the ghost
  const bool steered = ghost && ghost_controlled();
  custom::asterion::tick(ghost, type);  // before the speed rules: it may stop the ghost
  custom::lithobolia::tick(ghost, type);
  custom::bogeyman::tick(ghost, type, steered);  // before the speed rules: its room sets its speed
  custom::brownie::tick_host(ghost, type);       // and who is looking sets the Brownie's
  custom::behaviour::tick(ghost, type, steered);
  custom::restraint::tick(ghost, type, steered);
  if (ghost && type == custom::kKikimora) custom::kikimora::tick(ghost, steered);
  else custom::kikimora::reset();
  // Every client listens while an Echo haunts the contract, and only then.
  const bool echo = haunting && type == custom::kEcho;
  custom::echo::tick(echo ? ghost : nullptr, steered, echo);
  custom::hydra::tick(haunting, type);  // and sees the Hydra's false heads
  custom::brownie::tick_all(haunting, type);  // and records which ghost haunts, for its quiet steps
  custom::lethe::tick(haunting, type);        // and forgets with the Lethe
  custom::tempest::tick(haunting, type);      // and sees the Tempest's storm
  custom::mesmer::tick(haunting, type);       // and sees the Mesmer as no one else does
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("CustomGhosts", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    pg::custom::echo::reset();
    pg::custom::hydra::reset();
    pg::custom::asterion::reset();
    pg::custom::lithobolia::reset();
    pg::custom::bogeyman::reset();
    pg::custom::brownie::reset();
    pg::custom::lethe::reset();
    pg::custom::tempest::reset();
    pg::custom::mesmer::reset();
    pg::custom::behaviour::teardown();
    pg::custom::pages::teardown();
    pg::custom::difficulty::teardown();  // while the menu still holds the custom ghosts it may name
    pg::custom::registry::teardown();
  });
}
