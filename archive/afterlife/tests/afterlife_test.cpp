// Offline tests for when a dead investigator may haunt with the ghost (mods/afterlife/haunt_rules.h).
// Run: build\afterlife_test.exe
#include "haunt_rules.h"

#include "check.h"

using namespace pg::afterlife;

namespace {

/** Hold the key from `from` for `ms`, one frame every 50 ms; returns the time after letting go. */
uint64_t hold(Rules& r, bool dead, bool ghost, uint64_t from, uint64_t ms) {
  uint64_t t = from;
  for (; t <= from + ms; t += 50) r.step(dead, ghost, true, t);
  r.step(dead, ghost, false, t);
  return t;
}

void a_long_hold_toggles_only_for_the_dead() {
  Rules r;
  uint64_t t = hold(r, false, true, 0, 2000);  // alive: nothing
  CHECK(!r.haunting() && !r.touch_locked(t));
  t = hold(r, true, true, t, kHoldMs / 2);     // too short
  CHECK(!r.haunting());
  t = hold(r, true, false, t, 2000);           // no ghost to watch
  CHECK(!r.haunting());
  t = hold(r, true, true, t, 3000);            // one toggle per hold, however long
  CHECK(r.haunting() && r.touch_locked(t) && r.lock_seconds(t) == 0);
  t = hold(r, true, true, t, kHoldMs);
  CHECK(!r.haunting());
  CHECK(r.touch_locked(t) && r.lock_seconds(t) == 60);  // a minute of touching nothing after
  CHECK(r.touch_locked(t + kLockMs - 1000) && !r.touch_locked(t + kLockMs + 100));
}

void holding_on_toggles_once() {
  Rules r;
  for (uint64_t t = 0; t <= 5000; t += 50) {
    r.step(true, true, true, t);
    CHECK(r.haunting() == (t >= kHoldMs));  // on once the hold is long enough, and it stays on
  }
}

void haunting_ends_with_the_ghost() {
  Rules r;
  uint64_t t = hold(r, true, true, 0, kHoldMs);
  CHECK(r.haunting());
  CHECK(r.step(true, false, false, t + 10));  // the ghost is gone (the contract ended)
  CHECK(!r.haunting() && r.touch_locked(t + 20));
  r.reset();
  CHECK(!r.touch_locked(t + 20));
}

}  // namespace

int main() {
  a_long_hold_toggles_only_for_the_dead();
  holding_on_toggles_once();
  haunting_ends_with_the_ghost();
  std::printf(g_failures ? "%d check(s) failed\n" : "all afterlife tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
