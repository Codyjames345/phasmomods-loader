#include <utility>
#include <vector>

#include "check.h"
#include "il2cpp.h"

namespace {
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
int holdCalls = 0;
int live() {
  int count = 0;
  for (void* object : holds) count += object != nullptr;
  return count;
}
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
int a = 0, b = 0;
void* const kA = &a;
void* const kB = &b;
void* passed(void* value) { return value; }
}  // namespace

namespace il2cpp {
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  ++holdCalls;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}  // namespace il2cpp

namespace {
/** A Held stands in for the raw pointer it replaces: assigned, tested, compared and passed as one. */
void reads_as_a_pointer() {
  il2cpp::Held kept;
  CHECK(!kept && kept == nullptr && live() == 0);
  kept = kA;
  CHECK(kept && kept == kA && passed(kept) == kA && kept.get() == kA && held(kA));
  il2cpp::Held other = kA;
  CHECK(kept == other);
  kept = nullptr;
  CHECK(!kept && held(kA));  // `other` still holds it
  other = nullptr;
  CHECK(live() == 0);
}
/** Assigning the object it already holds keeps the one handle (a field set every frame). */
void same_object_keeps_its_handle() {
  il2cpp::Held kept = kA;
  const int before = holdCalls;
  for (int frame = 0; frame < 3; ++frame) kept = kA;
  CHECK(holdCalls == before && live() == 1);
  kept = kB;
  CHECK(held(kB) && !held(kA) && live() == 1);
}
/** Moves hand the one handle over; destruction and clearing release it. */
void ownership_moves_and_ends() {
  {
    il2cpp::Held first = kA;
    il2cpp::Held second = std::move(first);
    CHECK(!first && second == kA && live() == 1);
    first = std::move(second);
    CHECK(first == kA && !second && live() == 1);
  }
  CHECK(live() == 0);
  std::vector<il2cpp::Held> kept;
  kept.push_back(kA);
  kept.emplace_back(kB);
  kept.reserve(64);  // reallocation moves, it does not copy or release
  CHECK(live() == 2 && kept[0] == kA && kept[1] == kB);
  kept.erase(kept.begin());
  CHECK(live() == 1 && held(kB) && !held(kA));
  kept.clear();
  CHECK(live() == 0);
}
}  // namespace

int main() {
  reads_as_a_pointer();
  same_object_keeps_its_handle();
  ownership_moves_and_ends();
  return g_failures ? 1 : 0;
}
