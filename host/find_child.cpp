#include "find_child.h"

#include "Transform.h"
#include "game_api.h"
#include "runtime.h"

namespace pm::host {

void* find_child(void* transform, const char* path) {
  static constexpr api::Method<map::Transform_Find, void*(void*, void*)> kFind{};
  return transform ? kFind(transform, api::make_string(path)) : nullptr;
}

}  // namespace pm::host
