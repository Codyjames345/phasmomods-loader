// The example ghost's exports (sdk/guides/adding-a-ghost.md). The SDK's glue (core/mod_glue.h) binds the host contract,
// loads the mod's string table and keeps anything thrown inside the mod.
#include "ghost.h"
#include "mod_glue.h"

namespace {

// It declares channel 0, as every mod outside the loader's own does: the loader gives it one from 64 to 255,
// derived from its id (sdk/README.md "PmModInfo"). It sends nothing.
constexpr PmModInfo kInfo{PMMOD_API_VERSION, "example.ghost", "Example Ghost", 0, 0};
bool g_running = false;
example::Beat g_beat;

bool start(const PmHostApi* api) {
  if (!pm::mod::bind(api, &kInfo)) return false;
  return example::register_knell(g_beat);
}

}  // namespace

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }
extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("ExampleGhost", g_running, &start, api);
}
extern "C" __declspec(dllexport) void pmmod_tick() {}
extern "C" __declspec(dllexport) void pmmod_stop() { pm::mod::guarded_stop(g_running, [] { g_beat = {}; }); }
