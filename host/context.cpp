#include <cstring>
#include <vector>

#include "all.h"
#include "entry_hooks.h"
#include "log.h"
#include "mod_api.h"

namespace pg::context {
namespace detail {
namespace {

std::vector<Tracked>& tracked() {
  static std::vector<Tracked> list;  // a function's static, so PG_TRACK may register at load
  return list;
}

}  // namespace

void add(const Tracked& t) {
  for (const Tracked& existing : tracked())
    if (std::strcmp(existing.key, t.key) == 0) return;
  tracked().push_back(t);
}

}  // namespace detail

// PG_TRACK(phasmo::Type::Method) at namespace scope tracks that game method from the host's load on.
#define PG_TRACK_JOIN(a, b) a##b
#define PG_TRACK_NAME(line) PG_TRACK_JOIN(pg_tracked_, line)
#define PG_TRACK(M) [[maybe_unused]] static const bool PG_TRACK_NAME(__LINE__) = (::pg::context::track<M>(), true)

// The game methods the host tracks for caller context. None yet: stage 2 adds the first, the
// weighted ghost-type roll.

bool install(const PgGameEntry* (*lookup)(const char* key)) {
  bool ok = true;
  for (detail::Tracked& t : detail::tracked()) {
    if (t.installed) continue;
    const PgGameEntry* e = lookup(t.key);
    if (!e || e->kind != PG_ENTRY_METHOD || !e->code) {
      PG_LOG("context: %s did not resolve; calls of it are not tracked", t.key);
      continue;
    }
    if (!entry_hooks::hook(e->code, {e->twins, e->twinCount}, t.detour, t.original, t.key, true)) {
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

}  // namespace pg::context
