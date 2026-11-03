#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

#include "LevelController.h"
#include "Photon.h"
#include "apparition.h"
#include "asterion.h"
#include "behaviour.h"
#include "bogeyman.h"
#include "brownie.h"
#include "content.h"
#include "echo.h"
#include "flame_net.h"
#include "game.h"
#include "ghosts.h"
#include "gremlin.h"
#include "hooks.h"
#include "hud_line.h"
#include "hydra.h"
#include "kikimora.h"
#include "lethe.h"
#include "lithobolia.h"
#include "log.h"
#include "mesmer.h"
#include "mod_glue.h"
#include "net.h"
#include "registration.h"
#include "restraint.h"
#include "runtime.h"
#include "sound.h"
#include "tempest.h"

// The custom ghosts (ghosts.h) as a mod of the suite (core/mod_api.h). Each is a custom ghost of the
// content layer (sdk/api/content.h, registration.h): the host numbers it, puts it into the game's ghost
// types, journals and custom difficulty, and keeps it away from saves and from players without it. Each
// ghost's rules are content rules on its id, which the host runs on every client while the contract's
// ghost acts as that ghost (its own type, or a Mimic's copy of it). The host runs them, and puts the
// ghosts into the game, only while this client's mods match the host's list, its one answer for every mod, held
// through a contract (sdk/README.md, "The host's list"); this mod keeps no such state of its own.
// While they do not, none of it runs: the lobby and the contract are plain vanilla.
namespace {

using namespace pm::custom;
namespace content = pm::api::content;

constexpr const char* kId = "phasmomods.custom-ghosts";
constexpr PmModInfo kInfo{PMMOD_API_VERSION, kId, "Custom Ghosts", 2, PM_MOD_RECORDS_VOICE};
bool g_running = false;
int g_acting = 0;  // the custom ghost (ghosts.h Type) whose rules run now, or 0

int type_of_user(void* user) { return static_cast<int>(reinterpret_cast<intptr_t>(user)); }

// The contract's ghost, its GhostAI, once it has spawned; else null.
void* contract_ghost() {
  void* level = phasmo::LevelController::instance.get();
  void* ghost = pm::api::alive(level) ? phasmo::LevelController::currentGhost.get(level) : nullptr;
  return pm::api::alive(ghost) ? ghost : nullptr;
}

// The ghost `type` acts as on this client while its rules run, and on the host (which runs its AI) only.
void* hosts_ghost(int type) {
  return type && type == g_acting && phasmo::PhotonNetwork::get_IsMasterClient() ? contract_ghost() : nullptr;
}

// One frame of the custom ghost `type` (0: none, which puts every rule back as it was).
void frame(int type) {
  void* haunting = type ? contract_ghost() : nullptr;
  // The host runs the ghost.
  void* ghost = haunting && phasmo::PhotonNetwork::get_IsMasterClient() ? haunting : nullptr;
  const bool steered = ghost && content::player_controlled();
  asterion::tick(ghost, type);  // before the speed rules: it may stop the ghost
  lithobolia::tick(ghost, type);
  bogeyman::tick(ghost, type, steered);  // before the speed rules: its room sets its speed
  brownie::tick_host(ghost, type);       // and who is looking sets the Brownie's
  behaviour::tick(ghost, type, steered);
  behaviour::tick_room(haunting, type);  // every client: the Draugr's room is colder while it is angered
  restraint::tick(ghost, type);
  if (ghost && type == kKikimora) kikimora::tick(ghost, steered);
  else kikimora::reset();
  // Every client listens while an Echo haunts the contract, and only then.
  const bool echo = haunting && type == kEcho;
  echo::tick(echo ? ghost : nullptr, steered, echo);
  hydra::tick(haunting, type);  // and sees the Hydra's false heads
  brownie::tick_all(haunting, type);  // and records which ghost haunts, for its steps
  lethe::tick(haunting, type);        // and forgets with the Lethe
  tempest::tick(haunting, type);      // and sees the Tempest's storm
  mesmer::tick(haunting, type);       // and sees the Mesmer as no one else does
}

// The rules of one custom ghost (PmGhostRules). `user` is its Type.
void rules_start(void* user) {
  try {
    g_acting = type_of_user(user);
    if (const Ghost* g = find(g_acting)) PM_LOG("CustomGhosts: the rules of the %s run", g->name);
  } catch (...) {
  }
}

void rules_tick(void* user) {
  try {
    if (g_running && type_of_user(user) == g_acting) frame(g_acting);
  } catch (...) {
    PM_LOG("CustomGhosts: a rule threw; its frame is skipped");
  }
}

void rules_stop(void* user) {
  try {
    if (type_of_user(user) != g_acting) return;
    g_acting = 0;
    if (g_running) frame(0);  // a stopped mod put everything back already (pmmod_stop)
  } catch (...) {
    PM_LOG("CustomGhosts: a rule threw while stopping");
  }
}

bool rules_hunt_speed(float* base, float* losScale, void* user) {
  try {
    return type_of_user(user) == g_acting && behaviour::hunt_speed(base, losScale);
  } catch (...) {
    return false;  // nothing thrown may reach the host
  }
}

// The callbacks of each ghost's definition (PmGhostDef). `user` is its Type.
bool use_ability(int type) {
  try {
    void* ghost = hosts_ghost(type);
    if (!ghost) return false;
    if (type == kEcho) return echo::speak(ghost, 0);
    if (type == kKikimora) return kikimora::hide(ghost, kikimora::nearest(ghost));
    if (type == kLithobolia) return lithobolia::throw_now(ghost);
    if (type == kGremlin) return gremlin::short_out(ghost);
  } catch (...) {
    PM_LOG("ability: exception while using a custom ghost's ability");  // nothing thrown may reach another mod
  }
  return false;
}

int32_t hud_state(int type) {
  try {
    if (!type || type != g_acting) return -1;
    if (type == kEcho) return echo::dormant_now() ? 1 : 0;
    if (type == kKikimora) return restraint::held_back(type, restraint::kInteract) ? 1 : 0;
    return behaviour::sense(type);
  } catch (...) {
    return -1;  // nothing thrown may reach another mod
  }
}

void* target(int type, std::span<const void* const> excluded) {
  try {
    if (!type || type != g_acting) return nullptr;
    if (type == kDraugr) return behaviour::target(type, excluded);
    if (type == kBogeyman) return bogeyman::target(contract_ghost(), excluded);
  } catch (...) {
    PM_LOG("CustomGhosts: a ghost's target threw");  // nothing thrown may reach another mod
  }
  return nullptr;
}

bool held_back(int type, int32_t kind) {
  try {
    return (kind == restraint::kInteract || kind == restraint::kEvent || kind == restraint::kAbility) &&
           restraint::held_back(type, static_cast<restraint::Kind>(kind));
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}

// The scent only the Black Shuck's player sees: nothing for any other ghost, or for one whose rules do not
// run.
uint32_t player_view(int type, float* xyz, uint32_t max) {
  try {
    if (type != kBlackShuck || type != g_acting) return 0;
    const int n = behaviour::scent(xyz, static_cast<int>(std::min<uint32_t>(max, std::numeric_limits<int>::max())));
    return n > 0 ? static_cast<uint32_t>(n) : 0;
  } catch (...) {
    return 0;  // nothing thrown may reach another mod
  }
}

// Where a player may steer the Black Shuck in a hunt: within its trails' corridor (behaviour.h player_move).
void player_move(int type, const float* at, float* dx, float* dz) {
  try {
    if (type == kBlackShuck && type == g_acting) behaviour::player_move(at, dx, dz);
  } catch (...) {
    // Nothing thrown may reach another mod; the direction stays the player's.
  }
}

// Its player's HUD line for a state hud_state answered on the host (hud_line.h), kept until the next call.
const char* hud_text(int type, int32_t state) {
  static std::string line;
  try {
    line = hud_line(type, state);
  } catch (...) {
    line.clear();  // nothing thrown may reach another mod
  }
  return line.empty() ? nullptr : line.c_str();
}

// Every message on the channel: the flame ghosts' ('C' 'G' 'F') to their own, the rest to the Echo.
void receive(int sender, const uint8_t* data, size_t size) {
  switch (net_msg::kind_of(data, size)) {
    case net_msg::kDecoys:
    case net_msg::kDouble: hydra::receive(sender, data, size); break;
    case net_msg::kHit: lithobolia::receive(sender, data, size); break;
    case net_msg::kStrike: tempest::receive(sender, data, size); break;
    case net_msg::kViewing:
    case net_msg::kForget: lethe::receive(sender, data, size); break;
    case net_msg::kAnger: behaviour::receive(sender, data, size); break;
    case net_msg::kSeen: brownie::receive(sender, data, size); break;
    case 0: echo::receive(sender, data, size); break;
    default: break;
  }
}

bool start(const PmHostApi* api) {
  if (!pm::mod::bind(api, &kInfo)) return false;
  content::GhostDef callbacks;
  callbacks.useAbility = [](void* user) { return use_ability(type_of_user(user)); };
  callbacks.hudState = [](void* user) { return hud_state(type_of_user(user)); };
  callbacks.heldBack = [](int32_t kind, void* user) { return held_back(type_of_user(user), kind); };
  callbacks.interactionLost = [](void* user) {
    try {
      return restraint::interaction_lost(type_of_user(user));
    } catch (...) {
      return false;  // nothing thrown may reach another mod
    }
  };
  callbacks.playerView = [](float* xyz, uint32_t max, void* user) { return player_view(type_of_user(user), xyz, max); };
  callbacks.target = [](const void* const* excluded, uint32_t count, void* user) {
    return target(type_of_user(user), std::span<const void* const>(excluded, excluded ? count : 0));
  };
  callbacks.hudLine = [](int32_t state, void* user) { return hud_text(type_of_user(user), state); };
  callbacks.playerMove = [](const float* at, float* dx, float* dz, void* user) {
    player_move(type_of_user(user), at, dx, dz);
  };
  content::GhostRules rules;
  rules.start = &rules_start;
  rules.tick = &rules_tick;
  rules.stop = &rules_stop;
  rules.huntSpeed = &rules_hunt_speed;
  if (!register_content(callbacks, rules)) {
    PM_LOG("CustomGhosts inactive: the host refused its ghosts or their rules");
    return false;
  }
  if (!(pm::game::resolve() && behaviour::resolve() && kikimora::resolve() && echo::resolve() && sound::resolve() &&
        apparition::resolve() && hydra::resolve() && asterion::resolve() && lithobolia::resolve() &&
        gremlin::resolve() && bogeyman::resolve() && brownie::resolve() && lethe::resolve() && tempest::resolve() &&
        mesmer::resolve())) {
    PM_LOG("CustomGhosts inactive: game binding resolution failed");
    return false;
  }
  if (!(behaviour::create_hooks() && echo::create_hooks() && restraint::create_hooks() && lethe::create_hooks() &&
        mesmer::create_hooks() && brownie::create_hooks() && tempest::create_hooks() && pm::hooks::enable_all())) {
    PM_LOG("CustomGhosts inactive: hook installation failed");
    return false;
  }
  pm::net::set_handler(&receive);
  return true;
}

}  // namespace

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("CustomGhosts", g_running, &start, api);
}

// The rules tick from the host's content dispatch; the mod's own tick has nothing left to do.
extern "C" __declspec(dllexport) void pmmod_tick() {
  pm::mod::guarded_tick(g_running, [] {});
}

extern "C" __declspec(dllexport) void pmmod_stop() {
  pm::mod::guarded_stop(g_running, [] {
    g_acting = 0;
    pm::custom::echo::reset();
    pm::custom::hydra::reset();
    pm::custom::asterion::reset();
    pm::custom::lithobolia::reset();
    pm::custom::bogeyman::reset();
    pm::custom::brownie::reset();
    pm::custom::lethe::reset();
    pm::custom::tempest::reset();
    pm::custom::mesmer::reset();
    pm::custom::kikimora::reset();
    pm::custom::behaviour::teardown();
  });
}
