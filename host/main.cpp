#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#include "build_check.h"
#include "contained.h"
#include "Photon.h"
#include "game_api.h"
#include "game_map.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "startup.h"
#include "suite.h"

namespace {

HANDLE g_stop = nullptr;
HMODULE g_gameAssembly = nullptr;
HMODULE g_unityPlayer = nullptr;
unsigned long long g_deadline = 0;
// Aside: the player chose to play without mods, and no hook of the loader's is left in the game.
enum class Activation { Dormant, Waiting, Installing, Active, Aside, Failed };
std::atomic<Activation> g_activation{Activation::Dormant};
bool g_suiteStarted = false;  // main thread only
pm::build::Hashes g_build;    // the worker writes it before it hands over to the main thread (g_activation)

// Shutdown handshake: the worker asks, the main thread releases what it owns.
std::atomic<bool> g_shutdownRequested{false};
std::atomic<bool> g_mainReleased{false};

using LateUpdate_t = void (*)(void* self, const void* method);
LateUpdate_t g_lateUpdate = nullptr;

/** Release main-thread state without freeing code that a detour might still use. */
void main_thread_release() {
  if (g_suiteStarted) pm::suite::stop();
  g_suiteStarted = false;
  g_mainReleased = true;
}

/**
 * Takes the frame hook off once no mod will run, so the game runs with none of the loader's hooks. This runs inside that
 * very detour, past its call of the original: only Unity's main thread calls the method, nothing is left to run in its
 * trampoline, and the relay it came through is never freed (host/hooks.cpp), so returning from here goes straight back to
 * Unity. The DLL stays pinned and the worker stays parked, as in any session, and neither touches the game.
 */
void stand_aside() {
  g_activation = Activation::Aside;  // no tick from here on, whatever follows
  try {
    main_thread_release();
  } catch (...) {
    // Nothing that stops here has to finish for the hook to go, and a suite that threw is not asked to stop again at shutdown.
    PM_LOG("ModHost: stopping the suite failed part way; its hook still comes off");
    g_suiteStarted = false;
    g_mainReleased = true;
  }
  pm::hooks::rollback_to(0);
  PM_LOG("ModHost: no mod runs this session, so no hook of the loader's stays in the game");
}

/** Activate at the first frame boundary, then run the consent notice and the selected mods. */
void main_thread_tick() {
  if (g_shutdownRequested) {
    if (!g_mainReleased) main_thread_release();
    return;
  }
  auto expected = Activation::Waiting;
  if (g_activation.compare_exchange_strong(expected, Activation::Installing)) {
    if (!pm::startup::permitted(g_stop, g_deadline, "main-thread")) {
      g_activation = Activation::Failed;
      return;
    }
    if (!pm::suite::start(g_gameAssembly, g_unityPlayer, g_build)) {
      PM_LOG("ModHost inactive: main-thread initialization failed");
      g_activation = Activation::Failed;
      return;
    }
    g_suiteStarted = true;
    expected = Activation::Installing;
    if (!g_activation.compare_exchange_strong(expected, Activation::Active)) {
      main_thread_release();
      return;
    }
    PM_LOG("ModHost active");
  }
  if (g_activation == Activation::Active && !pm::suite::tick()) stand_aside();
}

/** Preserve the original frame callback and contain managed exceptions at the detour boundary. */
void detour_late_update(void* self, const void* method) {
  pm::hooks::Guard guard;
  g_lateUpdate(self, method);
  try {
    main_thread_tick();
    return;
  } catch (const Il2CppExceptionWrapper& e) {
    pm::host::log_managed("tick: managed exception caught at the frame boundary", e.ex);
  } catch (...) {
    PM_LOG("tick: unknown exception caught at the frame boundary");
  }
  if (g_activation == Activation::Installing || g_activation == Activation::Failed) {
    if (g_suiteStarted) pm::suite::stop();
    g_suiteStarted = false;
    g_activation = Activation::Failed;
    PM_LOG("ModHost inactive: main-thread initialization threw an exception");
  }
}

/**
 * The one hook before the player answers the consent notice, once the map has resolved what it needs: the frame, without
 * which the notice cannot show. Everything else waits for "I agree" (host/mod_session.h).
 */
bool install_hooks() {
  const PmGameEntry* lateUpdate = pm::api::host_only::game_entry(pm::map::PhotonHandler_LateUpdate.key);
  return lateUpdate && pm::hooks::create(lateUpdate->code, reinterpret_cast<void*>(&detour_late_update),
                           reinterpret_cast<void**>(&g_lateUpdate), "PhotonHandler.LateUpdate") &&
         pm::hooks::enable_all();
}

/** Complete startup within one budget and retain detour storage for the game process lifetime. */
DWORD WINAPI worker(void*) {
  g_deadline = GetTickCount64() + 60000;
  pm::log::open();
  PM_LOG("ModHost loading");
  PM_LOG("PhasmoMods version %s", PM_SUITE_VERSION);
  g_activation = Activation::Dormant;
  g_shutdownRequested = false;
  g_mainReleased = false;
  bool hooksOwned = false;
  try {
    const bool modulesReady = pm::startup::wait(g_stop, g_deadline, "module", [&] {
      g_gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
      g_unityPlayer = GetModuleHandleW(L"UnityPlayer.dll");
      return g_gameAssembly && g_unityPlayer;
    });
    if (modulesReady) {
      // Any build runs: these say only what the notice tells the player, and how often it asks (host/build_check.h).
      g_build = {pm::build::hash_module(g_gameAssembly, "GameAssembly.dll"), pm::build::hash_module(g_unityPlayer, "UnityPlayer.dll")};
      // The one thing no build gets past: the layouts below are compiled in for one engine and metadata version, and code
      // built for another can crash the game past any catch (core/engine_version.h).
      const pm::engine::Versions versions = pm::build::running_versions();
      if (!pm::engine::known(versions)) {
        PM_LOG("ModHost inactive: %s", pm::engine::refusal(versions).c_str());
      } else if (!il2cpp::init(g_gameAssembly, g_stop, g_deadline)) {  // pm:not-game-data: the runtime's own startup
        PM_LOG("ModHost inactive: runtime initialization failed");
      } else if (pm::startup::permitted(g_stop, g_deadline, "resolution")) {
        // An unresolved entry switches off only the mods that need it, never the host.
        pm::gamemap::resolve(g_gameAssembly, g_unityPlayer, g_stop, g_deadline);
        if (pm::startup::permitted(g_stop, g_deadline, "hooks")) {
          HMODULE pinned = nullptr;
          if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                 reinterpret_cast<LPCWSTR>(&worker), &pinned)) {
            PM_LOG("ModHost inactive: native module lifetime pin failed");
          } else if (!(hooksOwned = pm::hooks::init())) {
            PM_LOG("ModHost inactive: hook initialization failed");
          } else if (!install_hooks()) {
            PM_LOG("ModHost inactive: hook installation failed");
          } else {
            g_activation = Activation::Waiting;
            PM_LOG("ModHost waiting for the main thread");
            const bool completed = pm::startup::wait(g_stop, g_deadline, "main-thread", [] {
              const Activation now = g_activation;
              return now == Activation::Active || now == Activation::Aside || now == Activation::Failed;
            });
            if (!completed) {
              auto pending = g_activation.load();
              while ((pending == Activation::Waiting || pending == Activation::Installing) &&
                     !g_activation.compare_exchange_strong(pending, Activation::Failed)) {}
            }
          }
        }
      }
    }
  } catch (...) {
    PM_LOG("ModHost inactive: native startup threw an exception");
    g_activation = Activation::Failed;
  }

  // The game's quit (il2cpp_shutdown) waits for every thread attached to the runtime, so the worker
  // leaves it before waiting: from here on it makes no managed calls; the main thread does the work.
  il2cpp::detach();  // pm:not-game-data: the runtime's own shutdown
  const bool active = g_activation == Activation::Active || g_activation == Activation::Aside;
  if (active) WaitForSingleObject(g_stop, INFINITE);
  g_shutdownRequested = true;
  if (active) {
    for (int i = 0; i < 300 && !g_mainReleased; ++i) Sleep(10);
    if (!g_mainReleased) PM_LOG("shutdown: main thread did not tick; state retained until process exit");
  }
  g_activation = Activation::Failed;
  if (hooksOwned) {
    pm::hooks::restore_sites();  // the game's own bytes at every native site a mod patched
    const auto result = MH_DisableHook(MH_ALL_HOOKS);
    if (result != MH_OK) PM_LOG("shutdown: hook disable failed (%d); native code retained", result);
    // Disabled detours can still be returning. Keep their trampolines and pinned DLL alive.
  }
  PM_LOG("ModHost stopped; native module retained until process exit");
  pm::log::close();
  // Keep the event valid for PM_Shutdown callers for the same process lifetime.
  return 0;
}

}  // namespace

/** Request gameplay shutdown; the pinned native module is never hot-unloaded. */
extern "C" __declspec(dllexport) DWORD WINAPI PM_Shutdown(void*) {
  return g_stop && SetEvent(g_stop) ? 0 : ERROR_INVALID_HANDLE;
}

/** Schedule startup only when both the cancellation event and worker allocation succeed. */
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stop) return FALSE;
    HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (!t) {
      CloseHandle(g_stop);
      g_stop = nullptr;
      return FALSE;
    }
    CloseHandle(t);
  }
  return TRUE;
}
