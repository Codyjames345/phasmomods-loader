#pragma once

#include <atomic>
#include <cstddef>

// MinHook detours. The host owns the only MinHook instance (host/hooks.cpp); in a mod these calls
// go to the host (core/mod_glue.cpp), which chains a second detour on an already hooked function.
// Every detour body holds a Guard so shutdown can wait for in-flight calls.
namespace pg::hooks {

bool init();  // host only
// Hooks the one function `target` (a game method's body, never its twins with it): each target has
// its own chain, in which the newest detour runs first and its `original` reaches the one before it,
// or the target. The host chains through a relay of its own per hook, never by patching a detour, so
// a detour hooked on several targets carries no later hook on one of them over to the others (its
// one `original` still reaches only the target it was hooked on last). Refuses a repeat of the same
// detour on the same target.
// Refuses a target whose first 5 bytes touch a site patch in place.
bool create(void* target, void* detour, void** original, const char* name);
bool enable_all();  // enables the hooks created since the open group began
// Host only: the host contract's patch_site (core/mod_api.h says what it writes and when it
// refuses) at the native site `site`. The patch joins the open group like a hook: rollback_group,
// rollback_to and shutdown write the site's own bytes back, unless something else has overwritten the
// patch since (logged, left as it is). Each write, and each write back, runs with every other thread
// of the process suspended, as MinHook's enabling does, and is refused (retried briefly first) while
// one of them is stopped inside the replaced bytes. A write back so refused leaves the patch in place
// and recorded, and a later rollback that covers it (a group's, the mods' restart) or shutdown tries
// again. Every patch takes a new page (a 64 KB reservation), so a site patched again after a rollback
// takes another; pages are never freed.
// Every refusal and failure is logged under `name`; protections changed for the write are restored.
bool patch_site(void* site, const void* expected, size_t expectedSize, const void* stub, size_t stubSize, void** page,
                const char* name);
// Host only. Each mod's start is a group: rollback_group disables and removes the hooks created
// since begin_group, for a mod that did not start. rollback_to removes every hook created since
// mark() returned, newest first, so detours chained on one function unwind in order: the mods'
// hooks, all of them, when the mods are restarted. Site patches count as hooks in both.
void begin_group();
void rollback_group();
size_t mark();
void rollback_to(size_t mark);
void shutdown();  // host only: disable all hooks and site patches, drain in-flight detours, uninitialise

#ifdef PG_HOOKS_TEST
// tests/hooks_test.cpp only: while set, patch_site finds no page within rel32 range of a site.
extern bool g_noPageNear;
#endif

extern std::atomic<int> g_inflight;
struct Guard {
  Guard() { g_inflight.fetch_add(1, std::memory_order_acq_rel); }
  ~Guard() { g_inflight.fetch_sub(1, std::memory_order_acq_rel); }
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};

}  // namespace pg::hooks
