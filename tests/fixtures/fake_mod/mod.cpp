// A mod for the host's offline tests (tests/mods_test.cpp) and the offline check's (gamecheck --mods).
// It calls GhostAI.UnAppear, so its pgmod_requires names that entry, and it uses the Camera type,
// which it also requires. It never runs game code: its tick only calls through when handed a ghost,
// and nothing hands it one. Built a second time with PG_FAKE_MOD_API_VERSION set to an older
// contract version, it stands in for a mod built before the host's current minor version.
#include "Camera.h"
#include "GhostAI.h"
#include "mod_glue.h"

#ifndef PG_FAKE_MOD_API_VERSION
#define PG_FAKE_MOD_API_VERSION PGMOD_API_VERSION
#endif

namespace {

constexpr PgModInfo kInfo{PG_FAKE_MOD_API_VERSION, "fake-mod", "Fake Mod", 200, 0};
bool g_running = false;
void* g_ghost = nullptr;

bool start(const PgHostApi*) { return true; }
void tick() {
  if (g_ghost && phasmo::Camera::type.is(g_ghost)) phasmo::GhostAI::UnAppear(g_ghost);
}
void stop() {}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }
extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("Fake Mod", g_running, &start, api);
}
extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }
extern "C" __declspec(dllexport) void pgmod_stop() { pg::mod::guarded_stop(g_running, &stop); }
