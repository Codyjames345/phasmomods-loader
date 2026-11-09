#pragma once

#include <string>

#include <atomic>
#include <cstddef>

// MinHook detours. The host owns the only MinHook instance (host/hooks.cpp); in a mod these calls
// go to the host (core/mod_glue.cpp), which chains a second detour on an already hooked function.
// Every detour body holds a Guard so shutdown can wait for in-flight calls.
namespace pm::hooks {

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
// Host only: patches the native site `site`. When the `expectedSize` bytes there equal `expected` (5 to
// 32 bytes, whole instructions), copies the `stubSize` bytes of `stub` to the start of a page of its own
// within rel32 range of the site, follows them with a jump back to the instruction after the replaced
// bytes, and writes a 5-byte `jmp` to the page over the site, the rest of the replaced bytes as NOPs.
// The stub must perform whatever of the replaced instructions it still needs, and position-independent
// code is required: it runs at the page's address. It resumes the game by running off its own end, or by
// jumping to page + stubSize. A stub that keeps data must place it after a jump that ends its code (to
// page + stubSize, or wherever it means to go), never where the code could run off into it. `page`
// receives the stub's first byte: the page stays readable, writable and executable for the process
// lifetime and is never freed (a thread may still be running the stub), so the host may keep the
// stub's data there. False, `page` null and nothing written, when the bytes there differ from `expected`
// (another game build), when `expected` is not whole instructions or not 5 to 32 bytes, when the stub is
// empty or longer than 4082 bytes (a page less the jump back), when the site is already patched or
// hooked, when no page can be had within rel32 range, or when the write fails.
// The patch joins the open group like a hook: rollback_group,
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
// Host only: whether a patch_site patch is in place at `site`, so it has not been written back.
bool site_patched(const void* site);
// Host only. Each mod's start is a group: rollback_group disables and removes the hooks created
// since begin_group, for a mod that did not start. rollback_to removes every hook created since
// mark() returned, newest first, so detours chained on one function unwind in order: the mods'
// hooks, all of them, when the mods are restarted. Site patches count as hooks in both.
void begin_group();
void rollback_group();
// Host only: the first hook that could not be made or switched on since begin_group (its log line, without "hooks: "),
// or empty. The host keeps a mod with one off, whatever its start answered (host/mods.cpp).
std::string group_failure();
size_t mark();
void rollback_to(size_t mark);
// Host only: writes back every site patch still in place, newest first, as a rollback does (a write
// back refused leaves that patch in place). Makes no managed calls; the host's shutdown calls it.
void restore_sites();
void shutdown();  // host only: restore_sites, disable all hooks, drain in-flight detours, uninitialise

#ifdef PM_HOOKS_TEST
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

}  // namespace pm::hooks
