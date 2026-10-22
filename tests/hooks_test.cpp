// Offline tests for the host's hooks (host/hooks.cpp) on local functions, with the real MinHook:
// chaining, refused repeats, per-mod groups and rolling a group back. Run: build\hooks_test.exe
#include <windows.h>

#include "hooks.h"

#include "check.h"

namespace {

using Fn = int (*)(int);

// Distinct bodies, never inlined, so each has its own patchable prologue.
__declspec(noinline) int target(int x) {
  volatile int y = x;
  return y + 1;
}
__declspec(noinline) int other(int x) {
  volatile int y = x;
  return y * 3 + 2;
}

Fn g_origA = nullptr, g_origB = nullptr, g_origC = nullptr, g_origD = nullptr;
__declspec(noinline) int detour_a(int x) { return g_origA(x) * 10; }
__declspec(noinline) int detour_b(int x) { return g_origB(x) + 1000; }
__declspec(noinline) int detour_c(int x) { return g_origC(x) - 7; }
__declspec(noinline) int detour_d(int x) { return g_origD(x) + 500; }

// Calls through a volatile pointer, so the compiler cannot inline or fold the call.
int call(Fn f, int x) {
  Fn volatile through = f;
  return through(x);
}

void* p(Fn f) { return reinterpret_cast<void*>(f); }

}  // namespace

int main() {
  using namespace pg::hooks;
  CHECK(init());
  CHECK(call(target, 1) == 2);

  // The host's own hooks, before any mod group.
  CHECK(create(p(target), p(detour_a), reinterpret_cast<void**>(&g_origA), "a"));
  CHECK(enable_all());
  CHECK(call(target, 1) == 20);
  const size_t beforeMods = mark();

  // A mod hooks the same function: its detour runs first and reaches the earlier one.
  begin_group();
  CHECK(create(p(target), p(detour_b), reinterpret_cast<void**>(&g_origB), "b"));
  CHECK(!create(p(target), p(detour_b), reinterpret_cast<void**>(&g_origB), "b again"));  // refused
  CHECK(create(p(other), p(detour_d), reinterpret_cast<void**>(&g_origD), "d"));
  CHECK(enable_all());
  CHECK(call(target, 1) == 1020);
  CHECK(call(other, 1) == 505);

  // The mod did not start: its hooks go, and the chain is as it was.
  rollback_group();
  CHECK(call(target, 1) == 20);
  CHECK(call(other, 1) == 5);

  // The next mod chains ahead of the host's detour again, and enabling touches only its own group.
  begin_group();
  CHECK(create(p(target), p(detour_c), reinterpret_cast<void**>(&g_origC), "c"));
  CHECK(call(target, 1) == 20);  // created, not yet enabled
  CHECK(enable_all());
  CHECK(call(target, 1) == 13);
  begin_group();
  CHECK(create(p(other), p(detour_d), reinterpret_cast<void**>(&g_origD), "d again"));
  begin_group();  // that mod never enabled its hook; the next mod's enabling must not do it for it
  CHECK(enable_all());
  CHECK(call(other, 1) == 5);

  // Every mod stops (the in-game Mods panel's Apply): everything since the mark goes, newest first,
  // and the host's own hook stays. A mod started again hooks the same functions again.
  rollback_to(beforeMods);
  CHECK(call(target, 1) == 20 && call(other, 1) == 5);
  begin_group();
  CHECK(create(p(target), p(detour_c), reinterpret_cast<void**>(&g_origC), "c restarted"));
  CHECK(enable_all());
  CHECK(call(target, 1) == 13);

  shutdown();
  CHECK(call(target, 1) == 2 && call(other, 1) == 5);
  return g_failures ? 1 : 0;
}
