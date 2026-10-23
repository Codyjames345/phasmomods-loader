#pragma once

#include <atomic>
#include <cstddef>

// MinHook detours. The host owns the only MinHook instance (host/hooks.cpp); in a mod these calls
// go to the host (core/mod_glue.cpp), which chains a second detour on an already hooked function.
// Every detour body holds a Guard so shutdown can wait for in-flight calls.
namespace pg::hooks {

bool init();  // host only
// Refuses a repeat. When `detour` already runs first for `target` (one detour hooked on a method's
// twins, then chained onto), nothing is patched and `original` is left as it is.
bool create(void* target, void* detour, void** original, const char* name);
bool enable_all();  // enables the hooks created since the open group began
// Host only. Each mod's start is a group: rollback_group disables and removes the hooks created
// since begin_group, for a mod that did not start. rollback_to removes every hook created since
// mark() returned, newest first, so detours chained on one function unwind in order: the mods'
// hooks, all of them, when the mods are restarted.
void begin_group();
void rollback_group();
size_t mark();
void rollback_to(size_t mark);
void shutdown();  // host only: disable all hooks, drain in-flight detours, uninitialise

extern std::atomic<int> g_inflight;
struct Guard {
  Guard() { g_inflight.fetch_add(1, std::memory_order_acq_rel); }
  ~Guard() { g_inflight.fetch_sub(1, std::memory_order_acq_rel); }
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};

}  // namespace pg::hooks
