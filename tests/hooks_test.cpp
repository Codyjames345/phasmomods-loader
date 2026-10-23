// Offline tests for the host's hooks (host/hooks.cpp) on local functions, with the real MinHook:
// chaining, refused repeats, per-mod groups and rolling a group back; hooks by entry over a method's
// twins (host/entry_hooks.cpp) and the caller context (host/context.cpp). Run: build\hooks_test.exe
#include <windows.h>

#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "entry_hooks.h"
#include "hooks.h"
#include "log.h"
#include "mod_api.h"

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

std::vector<std::string> g_log;
void capture(const char* line) { g_log.push_back(line); }
bool logged(const char* text) {
  for (const std::string& line : g_log)
    if (line.find(text) != std::string::npos) return true;
  return false;
}

// Twins: the same behaviour at two addresses, as the obfuscator makes them. Every body here differs
// in bytes from every other (its own constant, or the extra volatile zero), so the linker cannot fold
// two of them into one address.
__declspec(noinline) int twin_a(int x) {
  volatile int y = x;
  return y + 11;
}
__declspec(noinline) int twin_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 11;
}
__declspec(noinline) int solo_a(int x) {
  volatile int y = x;
  return y + 12;
}
__declspec(noinline) int solo_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 12;
}
__declspec(noinline) int folded(int x) {
  volatile int y = x;
  return y + 13;
}

// Pairs for the refused mixes of twin policies, one pair per case.
__declspec(noinline) int all_first_a(int x) {
  volatile int y = x;
  return y + 14;
}
__declspec(noinline) int all_first_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 14;
}
__declspec(noinline) int one_first_a(int x) {
  volatile int y = x;
  return y + 15;
}
__declspec(noinline) int one_first_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 15;
}
__declspec(noinline) int raw_first_a(int x) {
  volatile int y = x;
  return y + 16;
}
__declspec(noinline) int raw_first_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 16;
}

__declspec(noinline) int dup_a(int x) {
  volatile int y = x;
  return y + 18;
}
__declspec(noinline) int dup_b(int x) {
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 18;
}

Fn g_origTwin = nullptr, g_origTwin2 = nullptr, g_origSolo = nullptr, g_origSolo2 = nullptr, g_origFolded = nullptr;
__declspec(noinline) int detour_twin(int x) { return g_origTwin(x) * 100; }
__declspec(noinline) int detour_twin2(int x) { return g_origTwin2(x) + 5; }
__declspec(noinline) int detour_solo(int x) { return g_origSolo(x) * 100; }
__declspec(noinline) int detour_solo2(int x) { return g_origSolo2(x) + 7; }
__declspec(noinline) int detour_folded(int x) { return g_origFolded(x) * 100; }
// Each case's first hook, and the later hook that is refused (it never runs, so it never calls on).
Fn g_origAllFirst = nullptr, g_origOneFirst = nullptr, g_origRawFirst = nullptr, g_origDup = nullptr,
   g_origRefused = nullptr;
__declspec(noinline) int detour_all_first(int x) { return g_origAllFirst(x) * 100; }
__declspec(noinline) int detour_one_first(int x) { return g_origOneFirst(x) * 100; }
__declspec(noinline) int detour_raw_first(int x) { return g_origRawFirst(x) * 100; }
__declspec(noinline) int detour_dup(int x) { return g_origDup(x) * 100; }
__declspec(noinline) int detour_refused(int x) { return g_origRefused(x) + 9; }

bool hook_entry(Fn canonical, std::initializer_list<void*> twins, Fn detour, Fn* original, const char* name, bool all) {
  const std::vector<void*> list(twins);
  return pg::entry_hooks::hook(p(canonical), std::span<void* const>(list), p(detour),
                               reinterpret_cast<void**>(original), name, all);
}

void twin_calls_reach_detour() {
  CHECK(p(twin_a) != p(twin_b));
  CHECK(hook_entry(twin_a, {p(twin_b)}, detour_twin, &g_origTwin, "twin", true));
  CHECK(pg::hooks::enable_all());
  CHECK(call(twin_a, 1) == 1200);
  CHECK(call(twin_b, 1) == 1200);  // entered through the twin, continued on the canonical chain
}

// A second detour on the same entry chains ahead of the first on every twin, and rolls back cleanly.
void second_hook_chains_on_twins() {
  const size_t before = pg::hooks::mark();
  CHECK(hook_entry(twin_a, {p(twin_b)}, detour_twin2, &g_origTwin2, "twin 2", true));
  CHECK(pg::hooks::enable_all());
  CHECK(g_origTwin2 != nullptr);
  CHECK(call(twin_a, 1) == 1205);
  CHECK(call(twin_b, 1) == 1205);
  pg::hooks::rollback_to(before);
  CHECK(call(twin_a, 1) == 1200);
  CHECK(call(twin_b, 1) == 1200);
}

// CanonicalOnly hooks stay on the canonical body, and a second one chains ahead of the first.
void canonical_only_leaves_twin() {
  CHECK(hook_entry(solo_a, {p(solo_b)}, detour_solo, &g_origSolo, "solo", false));
  CHECK(pg::hooks::enable_all());
  CHECK(call(solo_a, 1) == 1300);
  CHECK(call(solo_b, 1) == 13);
  CHECK(hook_entry(solo_a, {p(solo_b)}, detour_solo2, &g_origSolo2, "solo 2", false));
  CHECK(pg::hooks::enable_all());
  CHECK(call(solo_a, 1) == 1307);
  CHECK(call(solo_b, 1) == 13);
}

// A hook that would mix twin policies on one method is refused with nothing patched: it would run
// on bodies its policy excludes, or skip a detour that runs on one of them.
void refused(bool hooked, size_t before, const char* why) {
  CHECK(!hooked);
  CHECK(pg::hooks::mark() == before);
  CHECK(logged(why));
  CHECK(g_origRefused == nullptr);  // no success with the original left unset
}

void all_twins_then_canonical_only_refused() {
  CHECK(hook_entry(all_first_a, {p(all_first_b)}, detour_all_first, &g_origAllFirst, "all first", true));
  CHECK(pg::hooks::enable_all());
  g_log.clear();
  const size_t before = pg::hooks::mark();
  refused(hook_entry(all_first_a, {p(all_first_b)}, detour_refused, &g_origRefused, "only after all", false), before,
          "only after all refused");
  // A raw hook on the twin is refused the same way.
  refused(pg::hooks::create(p(all_first_b), p(detour_refused), reinterpret_cast<void**>(&g_origRefused), "raw on twin"),
          before, "raw on twin refused");
  CHECK(pg::hooks::enable_all());
  CHECK(call(all_first_a, 1) == 1500 && call(all_first_b, 1) == 1500);
}

void canonical_only_then_all_twins_refused() {
  CHECK(hook_entry(one_first_a, {p(one_first_b)}, detour_one_first, &g_origOneFirst, "one first", false));
  CHECK(pg::hooks::enable_all());
  g_log.clear();
  const size_t before = pg::hooks::mark();
  refused(hook_entry(one_first_a, {p(one_first_b)}, detour_refused, &g_origRefused, "all after one", true), before,
          "all after one refused");
  CHECK(pg::hooks::enable_all());
  CHECK(call(one_first_a, 1) == 1600 && call(one_first_b, 1) == 16);
}

void raw_on_twin_then_all_twins_refused() {
  CHECK(pg::hooks::create(p(raw_first_b), p(detour_raw_first), reinterpret_cast<void**>(&g_origRawFirst), "raw first"));
  CHECK(pg::hooks::enable_all());
  g_log.clear();
  const size_t before = pg::hooks::mark();
  refused(hook_entry(raw_first_a, {p(raw_first_b)}, detour_refused, &g_origRefused, "all after raw", true), before,
          "all after raw refused");
  CHECK(pg::hooks::enable_all());
  CHECK(call(raw_first_a, 1) == 17 && call(raw_first_b, 1) == 1700);
}

// create_shared takes each body once. [A, A] on top of a detour that runs first on {A, B} matches
// that detour's count of bodies, yet would leave B without the new detour.
void repeated_body_refused() {
  CHECK(hook_entry(dup_a, {p(dup_b)}, detour_dup, &g_origDup, "dup", true));
  CHECK(pg::hooks::enable_all());
  g_log.clear();
  const size_t before = pg::hooks::mark();
  void* const twice[] = {p(dup_a), p(dup_a)};
  refused(pg::hooks::create_shared(twice, p(detour_refused), reinterpret_cast<void**>(&g_origRefused), "twice"),
          before, "twice refused");
  CHECK(pg::hooks::enable_all());
  CHECK(call(dup_a, 1) == 1900 && call(dup_b, 1) == 1900);
}

void folded_twin_hooked_once() {
  g_log.clear();
  const size_t before = pg::hooks::mark();
  CHECK(hook_entry(folded, {p(folded), p(folded)}, detour_folded, &g_origFolded, "folded", true));
  CHECK(pg::hooks::mark() - before == 1);
  CHECK(!logged("already hooked"));
  CHECK(pg::hooks::enable_all());
  CHECK(call(folded, 1) == 1400);
}

// The caller context, on a tracked test method (the game's convention: its MethodInfo comes last).
constexpr pg::sdk::EntryDesc kTestF{.key = "Test.F", .kind = pg::sdk::Kind::Method};
constexpr pg::api::Method<kTestF, int(int)> TestF{};

int g_depthInside = -1, g_depthElsewhere = -1;
__declspec(noinline) int tracked(int x, const void*) {
  g_depthInside = pg::context::depth("Test.F");
  std::thread([] { g_depthElsewhere = pg::context::depth("Test.F"); }).join();
  if (x < 0) throw std::runtime_error("thrown inside the tracked method");
  volatile int y = x;
  return y + 1;
}
int call_tracked(int x) {
  int (*volatile through)(int, const void*) = tracked;
  return through(x, nullptr);
}

const PgGameEntry* fake_lookup(const char* key) {
  static PgGameEntry entry{};
  entry.kind = PG_ENTRY_METHOD;
  entry.code = reinterpret_cast<void*>(&tracked);
  return std::string(key) == "Test.F" ? &entry : nullptr;
}

void depth_inside_tracked_call() {
  CHECK(pg::context::depth("Test.F") == 0);
  pg::context::track<TestF>();
  CHECK(pg::context::install(&fake_lookup));
  CHECK(pg::hooks::enable_all());
  CHECK(call_tracked(1) == 2);
  CHECK(g_depthInside == 1);
  CHECK(g_depthElsewhere == 0);  // another thread never sees this one's depth
  CHECK(pg::context::depth("Test.F") == 0);
  g_depthInside = -1;
  bool caught = false;
  try {
    call_tracked(-1);
  } catch (const std::runtime_error&) {
    caught = true;
  }
  CHECK(caught);
  CHECK(g_depthInside == 1);
  CHECK(pg::context::depth("Test.F") == 0);  // the exception unwound through the counter
  CHECK(pg::context::depth("Test.Untracked") == 0);
}

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

  pg::log::set_sink(&capture, "");
  twin_calls_reach_detour();
  second_hook_chains_on_twins();
  canonical_only_leaves_twin();
  all_twins_then_canonical_only_refused();
  canonical_only_then_all_twins_refused();
  raw_on_twin_then_all_twins_refused();
  repeated_body_refused();
  folded_twin_hooked_once();
  depth_inside_tracked_call();

  shutdown();
  CHECK(call(target, 1) == 2 && call(other, 1) == 5);
  return g_failures ? 1 : 0;
}
