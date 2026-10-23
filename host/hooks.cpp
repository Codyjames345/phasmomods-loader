#include "hooks.h"

#include <windows.h>

#include <MinHook.h>

#include <map>
#include <mutex>
#include <vector>

#include "log.h"

namespace pg::hooks {

std::atomic<int> g_inflight{0};

namespace {

struct Created {
  void* target;   // the game function
  void* at;       // what MinHook patched: the target, or the detour that ran first before this one
  void* detour;
  void* earlier;  // the detour that ran first for the target before this one, or null
  bool patched;   // false: `at` was already patched to run this detour, so nothing new was patched
  bool enabled;
};

bool g_initialised = false;
std::mutex g_mutex;
// Each hooked game function and the detour that currently runs first for it. A second hook on the
// same function hooks that detour instead, so the newest detour runs first and its trampoline
// reaches the one before it.
std::map<void*, void*> g_first;
std::vector<Created> g_created;  // in creation order
size_t g_groupStart = 0;         // hooks from here on belong to the open group

// The hook MinHook placed at `at`, or null.
const Created* patched_at(void* at) {
  for (const Created& c : g_created)
    if (c.patched && c.at == at) return &c;
  return nullptr;
}

}  // namespace

bool init() {
  MH_STATUS s = MH_Initialize();
  if (s != MH_OK) {
    PG_LOG("hooks: MH_Initialize failed: %s", MH_StatusToString(s));
    return false;
  }
  g_initialised = true;
  return true;
}

bool create(void* target, void* detour, void** original, const char* name) {
  if (!target || !detour) {
    PG_LOG("hooks: %s has no target", name);
    return false;
  }
  std::lock_guard lock(g_mutex);
  for (const Created& c : g_created) {
    if (c.target == target && c.detour == detour) {
      PG_LOG("hooks: %s is already hooked with this detour", name);
      return false;
    }
  }
  auto first = g_first.find(target);
  void* earlier = first == g_first.end() ? nullptr : first->second;
  void* at = earlier ? earlier : target;
  // One detour can run first for several functions (a method's twins, hooked by entry), and MinHook
  // patches it once: follow such a patch to the detour that now runs first. When that is this
  // detour, it already runs for the target and nothing more is patched.
  while (const Created* ahead = patched_at(at)) {
    if (ahead->detour == detour) {
      g_first[target] = detour;
      g_created.push_back({target, at, detour, earlier, false, false});
      PG_LOG("hooks: %s at %p already runs through its detour", name, target);
      return true;
    }
    at = ahead->detour;
  }
  MH_STATUS s = MH_CreateHook(at, detour, original);
  if (s != MH_OK) {
    PG_LOG("hooks: create %s at %p failed: %s", name, at, MH_StatusToString(s));
    return false;
  }
  g_first[target] = detour;
  g_created.push_back({target, at, detour, earlier, true, false});
  if (at == target) PG_LOG("hooks: created %s at %p", name, target);
  else PG_LOG("hooks: created %s at %p, chained ahead of an earlier detour", name, target);
  return true;
}

bool enable_all() {
  std::lock_guard lock(g_mutex);
  bool queued = false;
  for (size_t i = g_groupStart; i < g_created.size(); ++i) {
    if (g_created[i].enabled || !g_created[i].patched) continue;
    if (MH_QueueEnableHook(g_created[i].at) != MH_OK) return false;
    queued = true;
  }
  MH_STATUS s = queued ? MH_ApplyQueued() : MH_OK;
  if (s != MH_OK) {
    PG_LOG("hooks: enable failed: %s", MH_StatusToString(s));
    return false;
  }
  for (size_t i = g_groupStart; i < g_created.size(); ++i) g_created[i].enabled = true;
  return true;
}

void begin_group() {
  std::lock_guard lock(g_mutex);
  g_groupStart = g_created.size();
}

namespace {

// Newest first, so a detour chained onto another is removed before the one it chained onto.
void remove_since(size_t start) {
  while (g_created.size() > start) {
    const Created c = g_created.back();
    g_created.pop_back();
    if (c.patched) {
      if (c.enabled) MH_DisableHook(c.at);
      MH_RemoveHook(c.at);
    }
    if (c.earlier) g_first[c.target] = c.earlier;
    else g_first.erase(c.target);
  }
  if (g_groupStart > start) g_groupStart = start;
}

}  // namespace

void rollback_group() {
  std::lock_guard lock(g_mutex);
  remove_since(g_groupStart);  // only the open group adds hooks, so nothing has chained onto these
  PG_LOG("hooks: removed the hooks of a mod that did not start");
}

size_t mark() {
  std::lock_guard lock(g_mutex);
  return g_created.size();
}

void rollback_to(size_t mark) {
  std::lock_guard lock(g_mutex);
  const size_t removed = g_created.size() > mark ? g_created.size() - mark : 0;
  remove_since(mark);
  PG_LOG("hooks: removed %zu hook(s)", removed);
}

void shutdown() {
  if (!g_initialised) return;
  MH_DisableHook(MH_ALL_HOOKS);
  // A detour that entered before disabling may still be running on the main thread.
  for (int i = 0; i < 500 && g_inflight.load(std::memory_order_acquire) > 0; ++i) Sleep(10);
  Sleep(100);  // let returning detours leave their epilogues
  MH_Uninitialize();
  g_initialised = false;
  PG_LOG("hooks: shut down (inflight=%d)", g_inflight.load());
}

}  // namespace pg::hooks
