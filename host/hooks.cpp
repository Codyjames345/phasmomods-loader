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
  bool patched;   // false: a twin reached through the patch the same create_shared made at `at`
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

void* first_detour(void* target) {
  auto first = g_first.find(target);
  return first == g_first.end() ? nullptr : first->second;
}

// How many hooked functions `detour` runs first for: more than one for a hook on a method's twins.
size_t runs_first_for(void* detour) {
  size_t n = 0;
  for (const auto& [target, first] : g_first) n += first == detour;
  return n;
}

bool repeated(void* target, void* detour, const char* name) {
  for (const Created& c : g_created) {
    if (c.target == target && c.detour == detour) {
      PG_LOG("hooks: %s is already hooked with this detour", name);
      return true;
    }
  }
  return false;
}

void remove_since(size_t start);

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
  if (repeated(target, detour, name)) return false;
  void* earlier = first_detour(target);
  void* at = earlier ? earlier : target;
  // Chaining onto a detour that runs first on every twin of a method would run this hook on all of
  // them too, which a hook on one body must not do.
  if (earlier && runs_first_for(earlier) > 1) {
    PG_LOG("hooks: %s refused at %p: every twin of this method is hooked as one, so a hook on this body "
           "alone would run on all of them; hook the method by entry with every twin",
           name, target);
    return false;
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

bool create_shared(std::span<void* const> bodies, void* detour, void** original, const char* name) {
  if (bodies.empty() || !detour) {
    PG_LOG("hooks: %s has no target", name);
    return false;
  }
  std::lock_guard lock(g_mutex);
  for (void* body : bodies) {
    if (!body) {
      PG_LOG("hooks: %s has no target", name);
      return false;
    }
    if (repeated(body, detour, name)) return false;
  }
  // Every body must lead with the same detour, one that runs on exactly these bodies (an earlier
  // hook on all of them), or with none. Otherwise this hook would skip a detour that runs on some
  // body, or run through one that a hook on a single body placed.
  void* earlier = first_detour(bodies[0]);
  bool shared = !earlier || runs_first_for(earlier) == bodies.size();
  for (void* body : bodies) shared = shared && first_detour(body) == earlier;
  if (!shared) {
    PG_LOG("hooks: %s refused at %p: a hook on some of this method's twins only is already there, so a hook on "
           "every twin would mix the two",
           name, bodies[0]);
    return false;
  }
  const size_t start = g_created.size();
  if (earlier) {
    // That detour runs first on every body, so one patch on it puts this detour ahead on all.
    MH_STATUS s = MH_CreateHook(earlier, detour, original);
    if (s != MH_OK) {
      PG_LOG("hooks: create %s at %p failed: %s", name, earlier, MH_StatusToString(s));
      return false;
    }
    for (size_t i = 0; i < bodies.size(); ++i) g_created.push_back({bodies[i], earlier, detour, earlier, i == 0, false});
  } else {
    // Each body gets its own patch; only the first body's trampoline is `original`, as twins are the
    // same behaviour.
    for (size_t i = 0; i < bodies.size(); ++i) {
      MH_STATUS s = MH_CreateHook(bodies[i], detour, i == 0 ? original : nullptr);
      if (s != MH_OK) {
        PG_LOG("hooks: create %s at %p failed: %s", name, bodies[i], MH_StatusToString(s));
        remove_since(start);
        return false;
      }
      g_created.push_back({bodies[i], bodies[i], detour, nullptr, true, false});
    }
  }
  for (void* body : bodies) g_first[body] = detour;
  PG_LOG("hooks: created %s at %p on %zu bodies%s", name, bodies[0], bodies.size(),
         earlier ? ", chained ahead of an earlier detour" : "");
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
  PG_LOG("hooks: removed %zu mod hook(s)", removed);
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
