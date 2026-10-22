#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
  std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

unsigned long long clockMs = 0, modulesAt = 0, runtimeAt = 0, stopAt = 70000, startupFinishedAt = 0;
unsigned long long frameAt = 0;
bool gameHash = true, unityHash = true, startOk = true, deliverFrame = false;
bool releaseFrame = false;
bool enabled = false, stopped = false, eventOk = true, threadOk = true;
bool startThrows = false;
int tornDown = 0;
int resolved = 0, created = 0, attached = 0, starts = 0, ticks = 0, activeLogs = 0, threads = 0;
int detached = 0;
bool parkedAttached = false;  // the worker waited for shutdown while still attached to IL2CPP
std::vector<std::string> messages;
void (*frame)(void*, const void*) = nullptr;
LPTHREAD_START_ROUTINE workerEntry = nullptr;

/** Model the original game callback so the real detour can preserve it. */
void original_frame(void*, const void*) {}

/** Supply deterministic time without sleeping or launching a game. */
ULONGLONG WINAPI tick_count() { return clockMs; }
/** Make modules appear at the configured monotonic time. */
HMODULE WINAPI module_handle(LPCWSTR) { return clockMs >= modulesAt ? reinterpret_cast<HMODULE>(1) : nullptr; }
/** Advance time and deliver a frame only at an external wait boundary. */
DWORD WINAPI wait_event(HANDLE, DWORD duration) {
  if (duration == INFINITE) { startupFinishedAt = clockMs; parkedAttached = attached > detached; stopped = true; }
  else {
    const auto wakeAt = clockMs + duration;
    if (deliverFrame && enabled && frame && frameAt <= wakeAt) {
      deliverFrame = false;
      clockMs = frameAt;
      frame(nullptr, nullptr);
    }
    clockMs = wakeAt;
  }
  if (clockMs >= stopAt) stopped = true;
  return stopped ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
/** Advance shutdown grace time without a real delay. */
void WINAPI sleep_for(DWORD duration) {
  clockMs += duration;
  if (releaseFrame && enabled && frame) {
    releaseFrame = false;
    frame(nullptr, nullptr);
  }
}
/** Simulate event allocation without leaking operating system handles. */
HANDLE WINAPI create_event(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCWSTR) {
  return eventOk ? reinterpret_cast<HANDLE>(1) : nullptr;
}
/** Observe thread allocation while preventing DllMain from launching a worker. */
HANDLE WINAPI create_thread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE entry, LPVOID, DWORD, LPDWORD) {
  ++threads;
  workerEntry = entry;
  return threadOk ? reinterpret_cast<HANDLE>(2) : nullptr;
}
/** Close a synthetic handle without touching the host process. */
BOOL WINAPI close_handle(HANDLE) { return TRUE; }
/** Signal the synthetic cancellation event. */
BOOL WINAPI set_event(HANDLE) { stopped = true; return TRUE; }
/** Model lifetime pinning without changing the test executable reference count. */
BOOL WINAPI pin_module(DWORD, LPCWSTR, HMODULE* module) { *module = reinterpret_cast<HMODULE>(1); return TRUE; }
/** Observe hook disablement without writing executable memory. */
MH_STATUS WINAPI disable_hooks(LPVOID) { enabled = false; return MH_OK; }
/** Return a runtime image only after the configured initialization time. */
void* corlib() { return clockMs >= runtimeAt ? reinterpret_cast<void*>(1) : nullptr; }
/** Supply the initialized runtime domain. */
void* domain() { return reinterpret_cast<void*>(1); }
/** Count actual runtime attachment attempts. */
void* attach(void*) { ++attached; return reinterpret_cast<void*>(1); }
/** IL2CPP's shutdown waits for attached threads; count detachment. */
void detach(void*) { ++detached; }
/** Stand in for unused IL2CPP exports, which startup only resolves. */
void unused_export() {}
/** Bind the production IL2CPP resolver to offline runtime boundaries. */
FARPROC WINAPI proc_address(HMODULE, LPCSTR name) {
  if (std::strcmp(name, "il2cpp_get_corlib") == 0) return reinterpret_cast<FARPROC>(&corlib);
  if (std::strcmp(name, "il2cpp_domain_get") == 0) return reinterpret_cast<FARPROC>(&domain);
  if (std::strcmp(name, "il2cpp_thread_attach") == 0) return reinterpret_cast<FARPROC>(&attach);
  if (std::strcmp(name, "il2cpp_thread_detach") == 0) return reinterpret_cast<FARPROC>(&detach);
  return reinterpret_cast<FARPROC>(&unused_export);
}
}  // namespace

// Compile the real worker and runtime initialization with external boundaries replaced.
#define GetTickCount64 tick_count
#define GetModuleHandleW module_handle
#define WaitForSingleObject wait_event
#define Sleep sleep_for
#define CreateEventW create_event
#define CreateThread create_thread
#define CloseHandle close_handle
#define SetEvent set_event
#define GetModuleHandleExW pin_module
#define GetProcAddress proc_address
#define MH_DisableHook disable_hooks
#include "../host/main.cpp"
#include "../core/il2cpp.cpp"
#undef GetTickCount64
#undef GetModuleHandleW
#undef WaitForSingleObject
#undef Sleep
#undef CreateEventW
#undef CreateThread
#undef CloseHandle
#undef SetEvent
#undef GetModuleHandleExW
#undef GetProcAddress
#undef MH_DisableHook

namespace pg::log {
/** Keep native diagnostics available for terminal-stage assertions. */
void open() {}
/** Keep diagnostic storage available after worker completion. */
void close() {}
/** Assert that activation never precedes a successful suite start. */
void write(const char* format, ...) {
  char message[512];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof message, format, args);
  va_end(args);
  messages.emplace_back(message);
  if (std::strcmp(message, "ModHost active") == 0) {
    ++activeLogs;
    CHECK(starts == 1 && startOk);
  }
}
}
namespace pg::build {
/** Model the audited GameAssembly check at its existing boundary. */
bool verify_game_assembly(void*) { return gameHash; }
/** Model the independent audited UnityPlayer check. */
bool verify_unity_player(void*) { return unityHash; }
}
namespace pg::game {
/** Count every entry to game resolution after binary verification. */
bool resolve() { ++resolved; return true; }
/** Provide an inert hook target to the hook boundary. */
void* photon_handler_late_update() { return reinterpret_cast<void*>(1); }
}
namespace pg::hooks {
std::atomic<int> g_inflight{0};
/** Allow hook setup without modifying the host executable. */
bool init() { return true; }
/** Count hook creation without installing detours. */
bool create(void*, void* detour, void** original, const char*) {
  ++created;
  frame = reinterpret_cast<void (*)(void*, const void*)>(detour);
  *original = reinterpret_cast<void*>(&original_frame);
  return true;
}
/** Permit the test scheduler to deliver a main-thread frame. */
bool enable_all() { enabled = true; return true; }
/** Record legacy cleanup without destroying real trampolines. */
void shutdown() { enabled = false; }
}
namespace pg::suite {
/** Count resolution of the host's own bindings. */
bool resolve() { return true; }
/** Create no hooks beyond the frame hook. */
bool create_hooks() { return true; }
/** Record the main-thread start and its configured result. */
bool start(void*, void*) {
  ++starts;
  if (startThrows) throw 1;
  return startOk;
}
/** Reject suite behavior before successful main-thread initialization. */
void tick() { ++ticks; CHECK(starts == 1 && startOk); }
/** Keep shutdown independent of a live game. */
void stop() { ++tornDown; }
}

namespace {
/** Reset only external fixtures, then enter through the actual DLL attachment surface. */
void reset() {
  clockMs = modulesAt = runtimeAt = startupFinishedAt = 0;
  frameAt = 0;
  stopAt = 70000;
  gameHash = unityHash = startOk = eventOk = threadOk = true;
  deliverFrame = releaseFrame = enabled = stopped = false;
  startThrows = false;
  tornDown = 0;
  resolved = created = attached = starts = ticks = activeLogs = threads = detached = 0;
  parkedAttached = false;
  messages.clear();
  frame = nullptr;
  CHECK(DllMain(nullptr, DLL_PROCESS_ATTACH, nullptr));
  threads = 0;
}
/** Execute the worker handed to CreateThread by the production DLL entry point. */
void run_worker() { CHECK(workerEntry != nullptr); workerEntry(nullptr); }
/** Find an actionable stage diagnostic in the production log output. */
bool logged(const char* text) {
  for (const auto& message : messages) if (message.find(text) != std::string::npos) return true;
  return false;
}
}  // namespace

/** Exercise real startup rejection, deadlines, start ordering, and allocation failures. */
int main() {
  reset(); gameHash = false; run_worker();
  CHECK(resolved == 0 && created == 0 && activeLogs == 0);
  reset(); unityHash = false; run_worker();
  CHECK(resolved == 0 && created == 0 && activeLogs == 0);
  reset(); stopped = true; run_worker();
  CHECK(attached == 0 && resolved == 0 && created == 0);
  reset(); modulesAt = 100000; run_worker();
  CHECK(clockMs <= 60000 && created == 0); CHECK(logged("module timeout"));
  reset(); runtimeAt = 100000; run_worker();
  CHECK(clockMs <= 60000 && attached == 0 && created == 0); CHECK(logged("runtime timeout"));
  reset(); modulesAt = 59000; runtimeAt = 100000; run_worker();
  CHECK(clockMs <= 60000 && attached == 0); CHECK(logged("runtime timeout"));
  reset(); run_worker();
  CHECK(activeLogs == 0 && ticks == 0); CHECK(logged("main-thread timeout"));
  reset(); startOk = false; deliverFrame = true; run_worker();
  CHECK(starts == 1 && activeLogs == 0 && ticks == 0 && !enabled);
  CHECK(logged("ModHost inactive: main-thread initialization failed"));
  reset(); startThrows = true; deliverFrame = true; run_worker();
  CHECK(activeLogs == 0 && ticks == 0 && !enabled && tornDown == 0);
  CHECK(logged("main-thread initialization threw"));
  reset(); deliverFrame = true; run_worker();
  CHECK(starts == 1 && activeLogs == 1 && ticks == 1);
  // The game's own quit (il2cpp_shutdown) waits for every attached thread: the worker must not be
  // attached while it waits for shutdown, or quitting the game hangs.
  CHECK(attached == 1 && !parkedAttached && detached == 1);
  reset(); deliverFrame = releaseFrame = true; frameAt = 59999; run_worker();
  CHECK(activeLogs == 1 && ticks == 1 && tornDown == 1 && !enabled);
  CHECK(startupFinishedAt == 60000);
  CHECK(logged("startup: main-thread timeout"));
  CHECK(!logged("ModHost inactive: main-thread timeout"));
  reset(); deliverFrame = releaseFrame = true; frameAt = 99; stopAt = 100; run_worker();
  CHECK(activeLogs == 1 && ticks == 1 && tornDown == 1 && !enabled);
  CHECK(startupFinishedAt == 100);
  CHECK(logged("startup: main-thread cancelled"));
  CHECK(!logged("ModHost inactive: main-thread cancelled"));
  reset(); eventOk = false;
  CHECK(!DllMain(nullptr, DLL_PROCESS_ATTACH, nullptr)); CHECK(threads == 0);
  reset(); threadOk = false;
  CHECK(!DllMain(nullptr, DLL_PROCESS_ATTACH, nullptr));
  return failures ? 1 : 0;
}
