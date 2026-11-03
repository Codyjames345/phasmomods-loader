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
bool suiteDone = false;              // what suite::tick answers: true while it has something left to do
int rolledBack = 0, rolledBackTo = -1;  // calls of hooks::rollback_to, and the last mark
int sitesRestored = 0;              // calls of hooks::restore_sites
bool restoredWhileEnabled = false;  // the last one came before the hooks were disabled
int mapped = 0, mappedInOrder = 0;  // map resolutions; those attached and before the game bindings
bool mapOk = true;
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
#include "../host/il2cpp.cpp"
pm::build::Hashes startedWith;
pm::engine::Versions runningVersions{"2022.3.40f1", 31};
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

namespace pm::log {
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
namespace pm::build {
/** Model the libraries' hashes: empty when one cannot be read. */
std::string hash_module(void*, const char* label) {
  const bool read = std::strcmp(label, "GameAssembly.dll") == 0 ? gameHash : unityHash;
  return read ? std::string("hash of ") + label : std::string();
}
/** Model the game's files: what its engine and metadata versions read as. */
engine::Versions running_versions() { return runningVersions; }
}
namespace pm::gamemap {
/** Count map resolution and whether it ran attached, before the game bindings, with UnityPlayer. */
bool resolve(void* gameAssembly, void* unityPlayer, void* stopEvent, unsigned long long deadline) {
  ++mapped;
  CHECK(gameAssembly == g_gameAssembly && unityPlayer == g_unityPlayer && unityPlayer != nullptr);
  CHECK(stopEvent == g_stop && deadline == g_deadline);
  if (attached > detached && resolved == 0) ++mappedInOrder;
  return mapOk;
}
}
namespace pm::game {
/** Count every entry to game resolution after binary verification. */
bool resolve() { ++resolved; return true; }
}
namespace pm::api::host_only {
/** Provide an inert hook target, PhotonHandler.LateUpdate's, to the hook boundary. */
const PmGameEntry* game_entry(const char*) {
  static const PmGameEntry entry{.kind = PM_ENTRY_METHOD, .code = reinterpret_cast<void*>(1)};
  return &entry;
}
}
namespace pm::hooks {
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
/** Record that the site patches were written back, and whether the hooks were still enabled then. */
void restore_sites() {
  ++sitesRestored;
  restoredWhileEnabled = enabled;
}
/** Take every hook since the mark off: with the mark 0, the frame's. */
void rollback_to(size_t mark) {
  ++rolledBack;
  rolledBackTo = static_cast<int>(mark);
  if (mark == 0) enabled = false;
}
}
namespace pm::suite {
/** Count resolution of the host's own bindings. */
bool resolve() { return true; }
/** Record the main-thread start and its configured result. */
bool start(void*, void*, const build::Hashes& game) {
  ++starts;
  startedWith = game;
  if (startThrows) throw 1;
  return startOk;
}
/** Reject suite behavior before successful main-thread initialization. */
bool tick() { ++ticks; CHECK(starts == 1 && startOk); return !suiteDone; }
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
  mapOk = true;
  mapped = mappedInOrder = 0;
  tornDown = 0;
  resolved = created = attached = starts = ticks = activeLogs = threads = detached = 0;
  sitesRestored = 0;
  parkedAttached = restoredWhileEnabled = false;
  messages.clear();
  frame = nullptr;
  runningVersions = {"2022.3.40f1", 31};
  suiteDone = false;
  rolledBack = 0;
  rolledBackTo = -1;
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

/** The host stays out: it never attaches to the runtime, maps nothing and hooks nothing, and the log names both versions. */
void stays_inactive(const pm::engine::Versions& versions, const char* found) {
  reset();
  runningVersions = versions;
  deliverFrame = true;
  run_worker();
  CHECK(attached == 0 && mapped == 0 && created == 0 && starts == 0 && ticks == 0 && activeLogs == 0 && !enabled);
  CHECK(logged("ModHost inactive: this loader's layouts were written for Unity 2022.3.x and metadata version 31"));
  CHECK(logged(found));
}

// Another Unity, or one its files do not say, reaches none of the layouts compiled in for this one.
void unknown_engine_version_stays_inactive() {
  stays_inactive({"6000.0.23f1", 31}, "this game is on Unity 6000.0.23f1 and metadata version 31");
  stays_inactive({"2022.2.9f1", 31}, "this game is on Unity 2022.2.9f1");
  stays_inactive({"", 31}, "this game is on Unity (unknown)");
}

void unknown_metadata_version_stays_inactive() {
  stays_inactive({"2022.3.40f1", 32}, "and metadata version 32");
  stays_inactive({"2022.3.40f1", -1}, "and metadata version (unknown)");
}

// Only the frame is hooked before the player answers: the suite makes every other hook on "I agree", so the worker makes
// exactly one. "Play without mods" takes that off as well, from inside the frame, and stops the suite; no later frame ticks,
// and a shutdown finds the main thread's part done, with no wait for a frame that will never come.
void declined_leaves_no_hook_in_the_game() {
  reset();
  deliverFrame = true;
  run_worker();
  CHECK(created == 1 && rolledBack == 0);  // a session going on keeps it, whatever the player has yet to answer
  reset();
  suiteDone = true;
  deliverFrame = releaseFrame = true;
  run_worker();
  CHECK(created == 1 && ticks == 1 && tornDown == 1);
  CHECK(rolledBack == 1 && rolledBackTo == 0 && !enabled);
  CHECK(logged("no hook of the loader's stays in the game"));
  CHECK(!logged("main thread did not tick"));
  CHECK(attached == 1 && !parkedAttached && detached == 1);  // parked as in any session, off the runtime
}

// A patch on the branch keeps the layouts: the host runs, as on any other new build.
void a_patch_on_the_branch_runs() {
  reset();
  runningVersions = {"2022.3.62f2", 31};
  deliverFrame = releaseFrame = true;
  run_worker();
  CHECK(mapped == 1 && starts == 1 && activeLogs == 1);
}
}  // namespace

/** Exercise real startup rejection, deadlines, start ordering, and allocation failures. */
int main() {
  // Whatever build this is, and whether or not its libraries can be hashed, the host goes on: the map decides what
  // runs, and the suite gets the hashes for the notice.
  reset(); deliverFrame = releaseFrame = true; run_worker();
  CHECK(mapped == 1 && starts == 1 && activeLogs == 1);
  CHECK(startedWith.gameAssembly == "hash of GameAssembly.dll" && startedWith.unityPlayer == "hash of UnityPlayer.dll");
  reset(); gameHash = false; deliverFrame = releaseFrame = true; run_worker();
  CHECK(mapped == 1 && starts == 1 && activeLogs == 1);
  CHECK(startedWith.gameAssembly.empty() && startedWith.unityPlayer == "hash of UnityPlayer.dll");
  reset(); unityHash = false; deliverFrame = releaseFrame = true; run_worker();
  CHECK(mapped == 1 && starts == 1 && activeLogs == 1);
  CHECK(startedWith.gameAssembly == "hash of GameAssembly.dll" && startedWith.unityPlayer.empty());
  unknown_engine_version_stays_inactive();
  unknown_metadata_version_stays_inactive();
  a_patch_on_the_branch_runs();
  declined_leaves_no_hook_in_the_game();
  reset(); stopped = true; run_worker();
  CHECK(attached == 0 && resolved == 0 && created == 0);
  reset(); modulesAt = 100000; run_worker();
  CHECK(clockMs <= 60000 && created == 0); CHECK(logged("module timeout"));
  reset(); runtimeAt = 100000; run_worker();
  CHECK(clockMs <= 60000 && attached == 0 && created == 0 && mapped == 0); CHECK(logged("runtime timeout"));
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
  // The map resolves once, on the attached worker, before the game bindings.
  CHECK(mapped == 1 && mappedInOrder == 1);
  // Shutdown writes every site patch back, once, before it disables the hooks.
  CHECK(sitesRestored == 1 && restoredWhileEnabled && !enabled);
  // A map that does not resolve leaves the host active: only the mods that need it stay off.
  reset(); mapOk = false; deliverFrame = true; run_worker();
  CHECK(mapped == 1 && starts == 1 && activeLogs == 1 && ticks == 1);
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
