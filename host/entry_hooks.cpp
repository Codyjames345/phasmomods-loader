#include "entry_hooks.h"

#include <algorithm>
#include <vector>

#include "log.h"

namespace pg::entry_hooks {

bool hook(void* canonical, std::span<void* const> twins, void* detour, void** original, const char* name,
          bool allTwins) {
  const size_t before = hooks::mark();
  if (!hooks::create(canonical, detour, original, name)) return false;
  if (!allTwins) return true;
  std::vector<void*> done{canonical};
  for (void* twin : twins) {
    if (!twin || std::find(done.begin(), done.end(), twin) != done.end()) continue;  // folded or repeated
    done.push_back(twin);
    // The twin's own trampoline is never called: the detour's `original` continues on the canonical chain.
    if (!hooks::create(twin, detour, nullptr, name)) {
      PG_LOG("hooks: %s could not hook every twin; none of its hooks stay", name);
      hooks::rollback_to(before);
      return false;
    }
  }
  return true;
}

}  // namespace pg::entry_hooks
