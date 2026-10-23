#include "entry_hooks.h"

#include <algorithm>

#include "log.h"

namespace pg::entry_hooks {

std::vector<void*> bodies(void* canonical, std::span<void* const> twins) {
  std::vector<void*> out{canonical};
  for (void* twin : twins)  // a twin folded onto another body, or listed twice, is one body
    if (twin && std::find(out.begin(), out.end(), twin) == out.end()) out.push_back(twin);
  return out;
}

bool hook_body(void* canonical, std::span<void* const> twins, uint32_t index, void* detour, void** original,
               const char* name) {
  const std::vector<void*> all = bodies(canonical, twins);
  if (index >= all.size()) {
    PG_LOG("hooks: %s refused: the method has no body %u (it has %zu)", name, index, all.size());
    return false;
  }
  return hooks::create(all[index], detour, original, name);
}

bool hook(void* canonical, std::span<void* const> twins, void* detour, void** original, const char* name,
          bool allTwins) {
  const std::vector<void*> all = bodies(canonical, twins);
  const size_t count = allTwins ? all.size() : 1;
  const size_t start = hooks::mark();
  for (size_t i = 0; i < count; ++i) {
    // Only the canonical body's trampoline is `original`: there is one detour for every body.
    if (!hooks::create(all[i], detour, i == 0 ? original : nullptr, name)) {
      hooks::rollback_to(start);
      return false;
    }
  }
  return true;
}

}  // namespace pg::entry_hooks
