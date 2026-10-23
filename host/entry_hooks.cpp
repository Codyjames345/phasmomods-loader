#include "entry_hooks.h"

#include <algorithm>
#include <vector>

namespace pg::entry_hooks {

bool hook(void* canonical, std::span<void* const> twins, void* detour, void** original, const char* name,
          bool allTwins) {
  if (!allTwins) return hooks::create(canonical, detour, original, name);
  std::vector<void*> bodies{canonical};
  for (void* twin : twins)  // a twin folded onto another body, or listed twice, is one body
    if (twin && std::find(bodies.begin(), bodies.end(), twin) == bodies.end()) bodies.push_back(twin);
  return hooks::create_shared(bodies, detour, original, name);
}

}  // namespace pg::entry_hooks
