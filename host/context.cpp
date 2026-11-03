#include <cstring>
#include <vector>

#include "all.h"
#include "entry_hooks.h"
#include "log.h"
#include "mod_api.h"

namespace pm::context {
namespace detail {
namespace {

std::vector<Tracked>& tracked() {
  static std::vector<Tracked> list;  // a function's static, so PM_TRACK may register at load
  return list;
}

}  // namespace

void add(const Tracked& t) {
  for (const Tracked& existing : tracked())
    if (std::strcmp(existing.key, t.key) == 0) return;
  tracked().push_back(t);
}

}  // namespace detail

// PM_TRACK(phasmo::Type::Method) at namespace scope tracks that game method in every mod session, from the
// player's "I agree" on (host/mod_session.h).
#define PM_TRACK_JOIN(a, b) a##b
#define PM_TRACK_NAME(line) PM_TRACK_JOIN(pm_tracked_, line)
#define PM_TRACK(M) [[maybe_unused]] static const bool PM_TRACK_NAME(__LINE__) = (::pm::context::track<M>(), true)

// The game methods the host tracks for caller context (pm::api::inside). A hook on each makes the answer the
// running thread's, whichever of a method's bodies the call came in through.
PM_TRACK(phasmo::GhostController::Spawn);
PM_TRACK(phasmo::GhostWanderState::Exit);
PM_TRACK(phasmo::LiftButton::NetworkedUse);

bool install(const PmGameEntry* (*lookup)(const char* key)) {
  bool ok = true;
  for (detail::Tracked& t : detail::tracked()) {
    if (t.installed) continue;
    const PmGameEntry* e = lookup(t.key);
    if (!e || e->kind != PM_ENTRY_METHOD || !e->code) {
      PM_LOG("context: %s did not resolve; calls of it are not tracked", t.key);
      continue;
    }
    const std::vector<void*> bodies = entry_hooks::bodies(e->code, {e->twins, e->twinCount});
    if (bodies.size() > api::kMaxBodies) {
      PM_LOG("context: %s has %zu bodies, more than the %u a hook covers; calls of it are not tracked", t.key,
             bodies.size(), api::kMaxBodies);
      continue;
    }
    // Every body or none: a failure removes the bodies hooked before it.
    const size_t start = hooks::mark();
    bool hooked = true;
    for (uint32_t i = 0; hooked && i < bodies.size(); ++i)
      hooked = hooks::create(bodies[i], t.entry(i), &t.originals[i], t.key);
    if (!hooked) {
      hooks::rollback_to(start);
      PM_LOG("context: %s could not be hooked; calls of it are not tracked", t.key);
      ok = false;
      continue;
    }
    t.installed = true;
  }
  return ok;
}

int32_t depth(const char* key) {
  if (!key) return 0;
  for (const detail::Tracked& t : detail::tracked())
    if (std::strcmp(t.key, key) == 0) return t.depth();
  return 0;
}

bool tracking_failed(const char* key) {
  if (!key) return false;
  for (const detail::Tracked& t : detail::tracked())
    if (std::strcmp(t.key, key) == 0) return !t.installed;
  return false;
}

bool abandon_ghost_spawn(void (*raise)()) {
  if (depth(map::GhostController_Spawn.key) <= 0) return false;
  raise();
  return true;
}

}  // namespace pm::context
