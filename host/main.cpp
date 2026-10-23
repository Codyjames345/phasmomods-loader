#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#include "build_check.h"
#include "game.h"
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
enum class Activation { Dormant, Waiting, Installing, Active, Failed };
std::atomic<Activation> g_activation{Activation::Dormant};
bool g_suiteStarted = false;  // main thread only

// Shutdown handshake: the worker asks, the main thread releases what it owns.
std::atomic<bool> g_shutdownRequested{false};
std::atomic<bool> g_mainReleased{false};

using LateUpdate_t = void (*)(void* self, const void* method);
LateUpdate_t g_lateUpdate = nullptr;

/** Release main-thread state without freeing code that a detour might still use. */
void main_thread_release() {
  if (g_suiteStarted) pg::suite::stop();
  g_suiteStarted = false;
  g_mainReleased = true;
}

/** Activate at the first frame boundary, then run the consent notice and the selected mods. */
void main_thread_tick() {
  if (g_shutdownRequested) {
    if (!g_mainReleased) main_thread_release();
    return;
  }
  auto expected = Activation::Waiting;
  if (g_activation.compare_exchange_strong(expected, Activation::Installing)) {
    if (!pg::startup::permitted(g_stop, g_deadline, "main-thread")) {
      g_activation = Activation::Failed;
      return;
    }
    if (!pg::suite::start(g_gameAssembly, g_unityPlayer)) {
      PG_LOG("ModHost inactive: main-thread initialization failed");
      g_activation = Activation::Failed;
      return;
    }
    g_suiteStarted = true;
    expected = Activation::Installing;
    if (!g_activation.compare_exchange_strong(expected, Activation::Active)) {
      main_thread_release();
      return;
    }
    PG_LOG("ModHost active");
  }
  if (g_activation == Activation::Active) pg::suite::tick();
}

/** Preserve the original frame callback and contain managed exceptions at the detour boundary. */
void detour_late_update(void* self, const void* method) {
  pg::hooks::Guard guard;
  g_lateUpdate(self, method);
  try {
    main_thread_tick();
    return;
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("tick: managed exception caught at the frame boundary", e.ex);
  } catch (...) {
    PG_LOG("tick: unknown exception caught at the frame boundary");
  }
  if (g_activation == Activation::Installing || g_activation == Activation::Failed) {
    if (g_suiteStarted) pg::suite::stop();
    g_suiteStarted = false;
    g_activation = Activation::Failed;
    PG_LOG("ModHost inactive: main-thread initialization threw an exception");
  }
}

/** Create the host's hooks only after both audited binaries have been verified. */
bool install_hooks() {
  return pg::hooks::create(pg::game::photon_handler_late_update(), reinterpret_cast<void*>(&detour_late_update),
                           reinterpret_cast<void**>(&g_lateUpdate), "PhotonHandler.LateUpdate") &&
         pg::suite::create_hooks() && pg::hooks::enable_all();
}

/** Complete startup within one budget and retain detour storage for the game process lifetime. */
DWORD WINAPI worker(void*) {
  g_deadline = GetTickCount64() + 60000;
  pg::log::open();
  PG_LOG("ModHost loading");
  PG_LOG("PhasmoMods version %s", PG_SUITE_VERSION);
  g_activation = Activation::Dormant;
  g_shutdownRequested = false;
  g_mainReleased = false;
  bool hooksOwned = false;
  try {
    const bool modulesReady = pg::startup::wait(g_stop, g_deadline, "module", [&] {
      g_gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
      g_unityPlayer = GetModuleHandleW(L"UnityPlayer.dll");
      return g_gameAssembly && g_unityPlayer;
    });
    if (modulesReady) {
      const bool gameOk = pg::build::verify_game_assembly(g_gameAssembly);
      const bool unityOk = pg::build::verify_unity_player(g_unityPlayer);
      if (!gameOk || !unityOk) {
        PG_LOG("ModHost inactive: unsupported or unreadable game binaries; nothing hooked");
      } else if (!il2cpp::init(g_gameAssembly, g_stop, g_deadline)) {
        PG_LOG("ModHost inactive: runtime initialization failed");
      } else if (pg::startup::permitted(g_stop, g_deadline, "resolution")) {
        // An unresolved entry switches off only the mods that need it, never the host.
        pg::gamemap::resolve(g_gameAssembly, g_stop, g_deadline);
        if (!(pg::game::resolve() && pg::suite::resolve())) {
          PG_LOG("ModHost inactive: game binding resolution failed");
        } else if (pg::startup::permitted(g_stop, g_deadline, "hooks")) {
          HMODULE pinned = nullptr;
          if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                 reinterpret_cast<LPCWSTR>(&worker), &pinned)) {
            PG_LOG("ModHost inactive: native module lifetime pin failed");
          } else if (!(hooksOwned = pg::hooks::init())) {
            PG_LOG("ModHost inactive: hook initialization failed");
          } else if (!install_hooks()) {
            PG_LOG("ModHost inactive: hook installation failed");
          } else {
            g_activation = Activation::Waiting;
            PG_LOG("ModHost waiting for the main thread");
            const bool completed = pg::startup::wait(g_stop, g_deadline, "main-thread", [] {
              return g_activation == Activation::Active || g_activation == Activation::Failed;
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
    PG_LOG("ModHost inactive: native startup threw an exception");
    g_activation = Activation::Failed;
  }

  // The game's quit (il2cpp_shutdown) waits for every thread attached to the runtime, so the worker
  // leaves it before waiting: from here on it makes no managed calls; the main thread does the work.
  il2cpp::detach();
  const bool active = g_activation == Activation::Active;
  if (active) WaitForSingleObject(g_stop, INFINITE);
  g_shutdownRequested = true;
  if (active) {
    for (int i = 0; i < 300 && !g_mainReleased; ++i) Sleep(10);
    if (!g_mainReleased) PG_LOG("shutdown: main thread did not tick; state retained until process exit");
  }
  g_activation = Activation::Failed;
  if (hooksOwned) {
    const auto result = MH_DisableHook(MH_ALL_HOOKS);
    if (result != MH_OK) PG_LOG("shutdown: hook disable failed (%d); native code retained", result);
    // Disabled detours can still be returning. Keep their trampolines and pinned DLL alive.
  }
  PG_LOG("ModHost stopped; native module retained until process exit");
  pg::log::close();
  // Keep the event valid for PG_Shutdown callers for the same process lifetime.
  return 0;
}

}  // namespace

/** Request gameplay shutdown; the pinned native module is never hot-unloaded. */
extern "C" __declspec(dllexport) DWORD WINAPI PG_Shutdown(void*) {
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
