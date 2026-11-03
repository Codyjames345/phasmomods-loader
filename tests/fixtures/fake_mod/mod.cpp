// A mod for the host's offline tests (tests/mods_test.cpp) and the offline check's (gamecheck --mods).
// It calls GhostAI.UnAppear, so its pmmod_requires names that entry, and it uses the Camera type,
// which it also requires. It never runs game code: its tick only calls through when handed a ghost,
// and nothing hands it one. Built a second time with PM_FAKE_MOD_API_VERSION set to an older
// contract version, it stands in for a mod built before the host's current minor version. Built with
// PM_FAKE_MOD_ID, PM_FAKE_MOD_CHANNEL and PM_FAKE_MOD_ASKS, it is another mod, which asks the host
// whether the mod PM_FAKE_MOD_ASKS has started, as a mod does for an optional dependency. PM_FAKE_MOD_FLAGS gives it
// PmModInfo.flags, and PM_FAKE_MOD_NO_ENTRIES leaves it requiring no game entry.
#include <windows.h>

#include "Camera.h"
#include "GhostAI.h"
#include "mod_glue.h"

#ifndef PM_FAKE_MOD_API_VERSION
#define PM_FAKE_MOD_API_VERSION PMMOD_API_VERSION
#endif
#ifndef PM_FAKE_MOD_ID
#define PM_FAKE_MOD_ID "fake-mod"
#endif
#ifndef PM_FAKE_MOD_CHANNEL
#define PM_FAKE_MOD_CHANNEL 200
#endif
#ifndef PM_FAKE_MOD_FLAGS
#define PM_FAKE_MOD_FLAGS 0
#endif

namespace {

constexpr PmModInfo kInfo{PM_FAKE_MOD_API_VERSION, PM_FAKE_MOD_ID, "Fake Mod", PM_FAKE_MOD_CHANNEL, PM_FAKE_MOD_FLAGS};
bool g_running = false;
bool g_failStart = false;
const PmHostApi* g_api = nullptr;
int g_asked = -1;        // what the host answered about PM_FAKE_MOD_ASKS at the last start: 1 started, 0 not, -1 not asked
int g_askedAtStop = -1;  // and at the last stop
void* g_ghost = nullptr;

int ask() {
#ifdef PM_FAKE_MOD_ASKS
  return g_api && g_api->mod_started(PM_FAKE_MOD_ASKS) ? 1 : 0;
#else
  return -1;
#endif
}

bool start(const PmHostApi* api) {
  g_api = api;
  g_asked = ask();
  return !g_failStart;
}
void tick() {
#ifndef PM_FAKE_MOD_NO_ENTRIES
  if (g_ghost && phasmo::Camera::type.is(g_ghost)) phasmo::GhostAI::UnAppear(g_ghost);
#endif
}
void stop() { g_askedAtStop = ask(); }

}  // namespace

// What the host's load had this thread's error mode at as this DLL was loaded (mods_test).
DWORD g_errorModeAtLoad = 0;
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) g_errorModeAtLoad = GetThreadErrorMode();
  return TRUE;
}
extern "C" __declspec(dllexport) DWORD fake_mod_error_mode_at_load() { return g_errorModeAtLoad; }
// Whether its next starts fail, as a mod whose pmmod_start answers false (mods_test).
extern "C" __declspec(dllexport) void fake_mod_fail_start(bool fail) { g_failStart = fail; }
extern "C" __declspec(dllexport) int fake_mod_asked() { return g_asked; }
extern "C" __declspec(dllexport) int fake_mod_asked_at_stop() { return g_askedAtStop; }
extern "C" __declspec(dllexport) int fake_mod_ask_now() { return ask(); }

// Built with PM_FAKE_MOD_OLD_EXPORTS it exports the names of contract 2.1 and before (core/mod_exports.h), as a mod built
// before the rename does; the SDK's glue still exports its new pmmod_requires.
#ifdef PM_FAKE_MOD_OLD_EXPORTS
#define PM_FAKE_EXPORT(name) pgmod_##name  // pm:old-name: a mod built before the rename, which the host still loads
#else
#define PM_FAKE_EXPORT(name) pmmod_##name
#endif

extern "C" __declspec(dllexport) const PmModInfo* PM_FAKE_EXPORT(info)() { return &kInfo; }
extern "C" __declspec(dllexport) bool PM_FAKE_EXPORT(start)(const PmHostApi* api) {
  return pm::mod::guarded_start("Fake Mod", g_running, &start, api);
}
extern "C" __declspec(dllexport) void PM_FAKE_EXPORT(tick)() { pm::mod::guarded_tick(g_running, &tick); }
extern "C" __declspec(dllexport) void PM_FAKE_EXPORT(stop)() { pm::mod::guarded_stop(g_running, &stop); }
