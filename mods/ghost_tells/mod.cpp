#include <cstdint>

#include "gate.h"
#include "gate_rule.h"
#include "goryo.h"
#include "hold.h"
#include "hooks.h"
#include "log.h"
#include "mare.h"
#include "mod_glue.h"
#include "net.h"
#include "onryo.h"
#include "register.h"
#include "rolls.h"
#include "shade.h"
#include "spirit.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"
#include "yurei.h"

// Ghost Tells as a mod of the suite (core/mod_api.h): three new tells each for the Spirit, Mare, Shade,
// Yurei, Goryo and Onryo (docs/ghost-tells.md). Each ghost's tells are content rules on its vanilla id
// (sdk/api/content.h): the content layer runs them while the contract's ghost acts as that type, a
// Mimic's copy included, in a private room only, and stops them when it stops. The journal lines go
// through journal_lines. The host runs the rules and shows the lines only while this client's mods
// match the host's list, its one answer for every mod, held through a contract (sdk/README.md, "The host's
// list"); this mod keeps no such state of its own. While they do not, none of it runs: the lobby's
// pages and the contract are plain vanilla. The rules that hold the ghost back answer the content
// layer's heldBack as well as the game's own entry points (gate.h).
namespace {

using namespace pm::tells;

constexpr const char* kId = "phasmomods.ghost-tells";
constexpr PmModInfo kInfo{PMMOD_API_VERSION, kId, "Ghost Tells", 6, 0};

bool g_running = false;
int g_told = -1;  // the told ghost whose rules run now, or -1

void stop_told(int told) {
  switch (told) {
    case kSpirit: spirit::reset(); break;
    case kMare: mare::reset(); break;
    case kShade: shade::reset(); break;
    case kYurei: yurei::reset(); break;
    case kGoryo: goryo::reset(); break;
    case kOnryo: onryo::reset(); break;
    default: break;
  }
  hold::reset();
  gate::tick(nullptr);
}

// The rules of one told ghost. `user` is its Told number.
void rules_start(void* user) {
  try {
    if (g_told >= 0) stop_told(g_told);
    g_told = static_cast<int>(reinterpret_cast<intptr_t>(user));
    PM_LOG("GhostTells: on for this contract");
  } catch (...) {
  }
}

void rules_stop(void* user) {
  try {
    const int told = static_cast<int>(reinterpret_cast<intptr_t>(user));
    if (g_told == told) {
      stop_told(told);
      g_told = -1;
    }
  } catch (...) {
  }
}

void rules_tick(void* user) {
  try {
    const int told = static_cast<int>(reinterpret_cast<intptr_t>(user));
    if (!g_running || told != g_told) return;
    const float dt = world::frame_seconds();
    void* ghost = world::ghost();
    if (!ghost) {
      gate::tick(nullptr);
      return;
    }
    const bool host = world::is_master();  // the host runs the ghost
    gate::tick(host ? ghost : nullptr);
    switch (told) {
      case kSpirit:
        if (host) spirit::tick_host(ghost, dt);
        break;
      case kMare: mare::tick(ghost, host, dt); break;
      case kShade:
        if (host) shade::tick_host(ghost, dt);
        break;
      case kYurei:
        if (host) yurei::tick_host(ghost, dt);
        break;
      case kGoryo:
        if (host) goryo::tick_host(ghost, dt);
        goryo::tick_all(ghost);
        break;
      case kOnryo:
        if (host) onryo::tick_host(ghost, dt);
        onryo::tick_all(dt);
        break;
      default: break;
    }
  } catch (...) {
    PM_LOG("GhostTells: a rule threw; its frame is skipped");
  }
}

bool rules_hunt_speed(float* base, float* losScale, void* user) {
  try {
    const int told = static_cast<int>(reinterpret_cast<intptr_t>(user));
    return told == told_now() && hold::hunt_speed(base, losScale);
  } catch (...) {
    return false;
  }
}

bool rules_held_back(int32_t kind, void* user) {
  try {
    const int told = static_cast<int>(reinterpret_cast<intptr_t>(user));
    const int asked = gate_held_kind(kind);  // the fuse box as the interaction its gate holds it as
    return told == told_now() && asked >= 0 && gate::held_back(static_cast<gate::Kind>(asked));
  } catch (...) {
    return false;  // nothing thrown may reach the host
  }
}

// Messages from other clients, acted on by the host, and the host's, shown by every client.
void receive(int sender, const uint8_t* data, size_t size) {
  try {
    if (told_now() < 0) return;
    const bool fromMaster = sender == world::master_actor();
    switch (net::kind_of(data, size)) {
      case net::kName:
        if (world::is_master() && net::read_name_heard(data, size)) spirit::heard_name(sender);
        break;
      case net::kTrap:
        if (world::is_master() && net::read_trapped(data, size)) yurei::trapped();
        break;
      case net::kGoryoShow: {
        bool on = false;
        if (fromMaster && net::read_goryo_show(data, size, on)) goryo::receive_show(on);
        break;
      }
      case net::kOnryoCount: {
        int count = 0;
        if (fromMaster && net::read_onryo_count(data, size, count)) onryo::receive_count(count);
        break;
      }
      case net::kOnryoSpite: {
        int view = 0;
        float seconds = 0;
        if (fromMaster && net::read_onryo_spite(data, size, view, seconds)) onryo::receive_spite(view, seconds);
        break;
      }
      default: break;
    }
  } catch (...) {
  }
}

bool start(const PmHostApi* api) {
  using namespace pm;
  if (!mod::bind(api, &kInfo)) return false;
  pm::api::content::GhostRules rules;
  rules.start = &rules_start;
  rules.tick = &rules_tick;
  rules.stop = &rules_stop;
  rules.huntSpeed = &rules_hunt_speed;
  rules.heldBack = &rules_held_back;
  if (!register_content(rules)) {
    PM_LOG("GhostTells inactive: the host refused its rules or journal lines");
    return false;
  }
  gate::resolve();
  spirit::resolve();
  mare::resolve();
  shade::resolve();
  yurei::resolve();
  goryo::resolve();
  if (!(world::create_hooks() && gate::create_hooks() && rolls::create_hooks() && spirit::create_hooks() &&
        mare::create_hooks() && yurei::create_hooks() && onryo::create_hooks() && hooks::enable_all())) {
    PM_LOG("GhostTells inactive: hook installation failed");
    return false;
  }
  pm::net::set_handler(&receive);
  return true;
}

}  // namespace

int pm::tells::told_now() { return g_running && g_told >= 0 ? g_told : -1; }

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("GhostTells", g_running, &start, api);
}

// The rules tick from the host's content dispatch; the mod's own tick has nothing left to do.
extern "C" __declspec(dllexport) void pmmod_tick() {
  pm::mod::guarded_tick(g_running, [] {});
}

extern "C" __declspec(dllexport) void pmmod_stop() {
  pm::mod::guarded_stop(g_running, [] {
    if (g_told >= 0) stop_told(g_told);
    g_told = -1;
    world::reset();
  });
}
