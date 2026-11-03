#include <windows.h>

#include <cstdint>

#include "contract.h"
#include "flames.h"
#include "hooks.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"

// Spectral Flame, an eighth evidence type, as a mod of the suite (core/mod_api.h): a flame set down
// where the ghost spends its time slowly turns blue. It registers the evidence through the content
// layer (sdk/api/content.h), which numbers it, lets ghost types list it and gives it its journal row
// while a custom ghost that gives it is shown. It acts only in a contract where some ghost type carries
// it (Custom Ghosts adds those); otherwise the game is as it was. The content layer puts such ghosts
// into a contract, and numbers the evidence, only while this client's mods match the host's list, the host's one
// answer for every mod, held through a contract (sdk/README.md, "The host's list"); this mod
// keeps no such state of its own. While they do not, the contract is plain vanilla.
namespace {

constexpr const char* kId = "phasmomods.spectral-flame";
constexpr PmModInfo kInfo{PMMOD_API_VERSION, kId, "Spectral Flame", 5, 0};

bool g_running = false;
int32_t g_on = -1;  // the evidence the flames were last on with, for the log

bool start(const PmHostApi* api) {
  using namespace pm;
  if (!mod::bind(api, &kInfo)) return false;
  if (!flame::contract::register_evidence()) {
    PM_LOG("SpectralFlame inactive: the host refused its evidence");
    return false;
  }
  flame::flames::resolve();
  if (!(flame::flames::create_hooks() && hooks::enable_all())) {
    PM_LOG("SpectralFlame inactive: hook installation failed");
    return false;
  }
  net::set_handler(&flame::flames::receive);
  return true;
}

void tick() {
  using namespace pm;
  flame::flames::Frame frame;
  frame.evidence = flame::contract::flame_evidence();
  frame.enabled = frame.evidence >= 0;
  frame.now = GetTickCount64();
  if (frame.enabled && frame.evidence != g_on)
    PM_LOG("SpectralFlame: on for this contract (evidence %d)", static_cast<int>(frame.evidence));
  g_on = frame.evidence;
  flame::flames::tick(frame);
}

}  // namespace

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("SpectralFlame", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pmmod_tick() { pm::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pmmod_stop() {
  pm::mod::guarded_stop(g_running, [] {
    g_on = -1;
    pm::flame::flames::teardown();
  });
}
