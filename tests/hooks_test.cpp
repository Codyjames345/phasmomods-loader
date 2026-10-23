// Offline tests for the host's hooks (host/hooks.cpp) on local functions, with the real MinHook:
// chaining, refused repeats, per-mod groups and rolling a group back; hooks by entry over a method's
// twins (host/entry_hooks.cpp), the game API's typed hooks with one chain per body (sdk/api), and the
// caller context (host/context.cpp). Run: build\hooks_test.exe
#include <windows.h>

#include <initializer_list>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "entry_hooks.h"
#include "game_api.h"
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

Fn g_origTwin = nullptr, g_origTwin2 = nullptr, g_origSolo = nullptr, g_origSolo2 = nullptr, g_origFolded = nullptr;
__declspec(noinline) int detour_twin(int x) { return g_origTwin(x) * 100; }
__declspec(noinline) int detour_twin2(int x) { return g_origTwin2(x) + 5; }
__declspec(noinline) int detour_solo(int x) { return g_origSolo(x) * 100; }
__declspec(noinline) int detour_solo2(int x) { return g_origSolo2(x) + 7; }
__declspec(noinline) int detour_folded(int x) { return g_origFolded(x) * 100; }

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

void folded_twin_hooked_once() {
  g_log.clear();
  const size_t before = pg::hooks::mark();
  CHECK(hook_entry(folded, {p(folded), p(folded)}, detour_folded, &g_origFolded, "folded", true));
  CHECK(pg::hooks::mark() - before == 1);
  CHECK(!logged("already hooked"));
  CHECK(pg::hooks::enable_all());
  CHECK(call(folded, 1) == 1400);
}

// One chain per body. A method's two bodies, with the game's convention (its MethodInfo comes last),
// and what ran for a call, in order: the detours by name, then the body that ran ("A" or "B").
std::string g_trace;
void step(const char* name) {
  if (!g_trace.empty()) g_trace += ' ';
  g_trace += name;
}

using GameFn = int (*)(int, const void*);
__declspec(noinline) int body_a(int x, const void*) {
  step("A");
  volatile int y = x;
  return y + 21;
}
__declspec(noinline) int body_b(int x, const void*) {
  step("B");
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 21;
}

// What a call entering through `body` ran.
std::string trace(GameFn body) {
  g_trace.clear();
  GameFn volatile through = body;
  CHECK(through(1, nullptr) == 22);
  return g_trace;
}

// Two mods' game API on the same method: each declares its own entry for it, so each has its own
// typed hook, as two mod DLLs do.
constexpr pg::sdk::EntryDesc kTwinMod1{.key = "Test.Twin", .kind = pg::sdk::Kind::Method};
constexpr pg::sdk::EntryDesc kTwinMod2{.key = "Test.Twin", .kind = pg::sdk::Kind::Method};
constexpr pg::api::Method<kTwinMod1, int(int)> TwinMod1{};
constexpr pg::api::Method<kTwinMod2, int(int)> TwinMod2{};
int d1(pg::api::Original<TwinMod1> original, int x) {
  step("D1");
  return original(x);
}
int d2(pg::api::Original<TwinMod2> original, int x) {
  step("D2");
  return original(x);
}

// An unmigrated mod's raw hook on one body, and a 2.0 mod's hook_entry: plain detours, each with
// one `original`.
GameFn g_origRaw = nullptr, g_origLegacy = nullptr;
__declspec(noinline) int raw_detour(int x, const void* method) {
  step("R");
  return g_origRaw(x, method);
}
__declspec(noinline) int legacy_detour(int x, const void* method) {
  step("L");
  return g_origLegacy(x, method);
}
bool raw_on(GameFn body) {
  return pg::hooks::create(reinterpret_cast<void*>(body), reinterpret_cast<void*>(&raw_detour),
                           reinterpret_cast<void**>(&g_origRaw), "raw");
}
bool legacy(bool allTwins) {
  void* const twins[] = {reinterpret_cast<void*>(&body_b)};
  return pg::entry_hooks::hook(reinterpret_cast<void*>(&body_a), twins, reinterpret_cast<void*>(&legacy_detour),
                               reinterpret_cast<void**>(&g_origLegacy), "legacy", allTwins);
}

// The test's host contract: the method entries below, hooked through the host's own per-body hooks.
std::map<std::string, PgGameEntry> g_entries;
std::vector<std::vector<void*>> g_twinLists;
void give(const char* key, void* code, std::vector<void*> twins) {
  std::vector<void*>& list = g_twinLists.emplace_back(std::move(twins));
  PgGameEntry e{};
  e.kind = PG_ENTRY_METHOD;
  e.code = code;
  e.twins = list.data();
  e.twinCount = static_cast<uint32_t>(list.size());
  g_entries[key] = e;
}
const PgGameEntry* test_entry(const char* key) {
  const auto it = g_entries.find(key);
  return it == g_entries.end() ? nullptr : &it->second;
}
bool test_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  const PgGameEntry* e = test_entry(key);
  return e && pg::entry_hooks::hook_body(e->code, {e->twins, e->twinCount}, index, detour, original, name);
}
uint32_t test_body_count(const char* key) {
  const PgGameEntry* e = test_entry(key);
  return e ? static_cast<uint32_t>(pg::entry_hooks::bodies(e->code, {e->twins, e->twinCount}).size()) : 0;
}
void test_log(const char* line) { PG_LOG("%s", line); }
bool test_enable() { return pg::hooks::enable_all(); }

PgHostApi test_host() {
  PgHostApi host{};
  host.version = PGMOD_API_VERSION;
  host.log = &test_log;
  host.enable_hooks = &test_enable;
  host.game_entry = &test_entry;
  host.hook_body = &test_hook_body;
  host.body_count = &test_body_count;
  return host;
}

// Twins A and B with different chains: an all-twins hook D1, then a CanonicalOnly hook D2 on A. A
// call through B runs D1 and then B's own code; a call through A runs D2, then D1, then A's code.
void original_follows_the_entered_body() {
  const size_t before = pg::hooks::mark();
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(pg::api::hook<TwinMod2>(&d2, "d2", pg::api::Twins::CanonicalOnly));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_b) == "D1 B");
  CHECK(trace(&body_a) == "D2 D1 A");
  pg::hooks::rollback_to(before);
  CHECK(trace(&body_a) == "A");
  CHECK(trace(&body_b) == "B");
}

// Hooks with different twin policies coexist on one method in either order, typed or not: a hook on
// one body never runs for a call that entered through another.
void mixed_policies_now_coexist() {
  const size_t before = pg::hooks::mark();
  CHECK(pg::api::hook<TwinMod2>(&d2, "d2", pg::api::Twins::CanonicalOnly));
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 D2 A");
  CHECK(trace(&body_b) == "D1 B");
  pg::hooks::rollback_to(before);

  // A 2.0 mod's all-twins hook_entry, then a CanonicalOnly hook. Its one detour runs on both
  // bodies and continues on the canonical chain (the limit hook_entry documents), yet D2 stays on A.
  CHECK(legacy(true));
  CHECK(pg::api::hook<TwinMod2>(&d2, "d2", pg::api::Twins::CanonicalOnly));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D2 L A");
  CHECK(trace(&body_b) == "L A");
  pg::hooks::rollback_to(before);

  // A CanonicalOnly hook_entry, then an all-twins hook.
  CHECK(legacy(false));
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 L A");
  CHECK(trace(&body_b) == "D1 B");
  pg::hooks::rollback_to(before);

  // An all-twins hook, then an all-twins hook_entry over it: the entry's detour leads on both.
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(legacy(true));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "L D1 A");
  CHECK(trace(&body_b) == "L D1 A");
  pg::hooks::rollback_to(before);
  CHECK(trace(&body_a) == "A" && trace(&body_b) == "B");
}

// A raw hook on a twin, before or after an all-twins hook, joins that twin's chain alone.
void raw_create_on_a_twin() {
  const size_t before = pg::hooks::mark();
  CHECK(raw_on(&body_b));
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 A");
  CHECK(trace(&body_b) == "D1 R B");
  pg::hooks::rollback_to(before);

  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(raw_on(&body_b));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 A");
  CHECK(trace(&body_b) == "R D1 B");
  pg::hooks::rollback_to(before);
  CHECK(trace(&body_a) == "A" && trace(&body_b) == "B");
}

// One mod hooks a method once: a second typed hook on it is refused and the first keeps its detour.
int d1_again(pg::api::Original<TwinMod1> original, int x) {
  step("D1 again");
  return original(x);
}
void second_typed_hook_refused() {
  const size_t before = pg::hooks::mark();
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::CanonicalOnly));
  CHECK(!pg::api::hook<TwinMod1>(&d1_again, "d1 again", pg::api::Twins::All));
  CHECK(pg::hooks::mark() - before == 1);
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 A");
  CHECK(trace(&body_b) == "B");
  pg::hooks::rollback_to(before);
}

// A method with more bodies than a typed hook has slots for: an all-twins hook is refused with a log
// line and nothing patched. A CanonicalOnly hook needs one slot and is made.
__declspec(noinline) int many_body(int x, const void*) {
  step("M");
  volatile int y = x;
  return y + 23;
}
constexpr pg::sdk::EntryDesc kMany{.key = "Test.Many", .kind = pg::sdk::Kind::Method};
constexpr pg::api::Method<kMany, int(int)> Many{};
int many_detour(pg::api::Original<Many> original, int x) {
  step("DM");
  return original(x);
}
// Never called: the twins only have to be distinct addresses.
const char g_notCode[16] = {};
std::vector<void*> eight_twins() {
  std::vector<void*> twins;
  for (int i = 1; i <= 8; ++i) twins.push_back(const_cast<char*>(g_notCode + i));
  return twins;
}

void more_than_eight_bodies_refused() {
  g_log.clear();
  const size_t before = pg::hooks::mark();
  CHECK(!pg::api::hook<Many>(&many_detour, "many", pg::api::Twins::All));
  CHECK(pg::hooks::mark() == before);
  CHECK(logged("Test.Many has 9 bodies, more than the 8 a hook covers; nothing was hooked"));
  CHECK(pg::api::hook<Many>(&many_detour, "many", pg::api::Twins::CanonicalOnly));
  CHECK(pg::hooks::mark() - before == 1);
  CHECK(pg::hooks::enable_all());
  g_trace.clear();
  GameFn volatile through = &many_body;
  CHECK(through(1, nullptr) == 24 && g_trace == "DM M");
  pg::hooks::rollback_to(before);
}

// The mods restart: every per-body hook since the mark goes, newest first, and each body's chain is
// as it was, with the host's own hook made before the mark still in place. The mods hook again.
GameFn g_origHost = nullptr;
__declspec(noinline) int host_detour(int x, const void* method) {
  step("H");
  return g_origHost(x, method);
}
void rollback_per_body() {
  CHECK(pg::hooks::create(reinterpret_cast<void*>(&body_a), reinterpret_cast<void*>(&host_detour),
                          reinterpret_cast<void**>(&g_origHost), "host"));
  CHECK(pg::hooks::enable_all());
  const size_t beforeMods = pg::hooks::mark();
  pg::hooks::begin_group();
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1", pg::api::Twins::All));
  CHECK(pg::api::hook<TwinMod2>(&d2, "d2", pg::api::Twins::CanonicalOnly));
  CHECK(raw_on(&body_b));
  CHECK(legacy(true));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "L D2 D1 H A");
  CHECK(trace(&body_b) == "L D2 D1 H A");  // hook_entry's detour continues on the canonical chain
  g_log.clear();
  pg::hooks::rollback_to(beforeMods);
  CHECK(logged("removed 6 hook(s)"));
  CHECK(trace(&body_a) == "H A");
  CHECK(trace(&body_b) == "B");
  pg::hooks::begin_group();
  CHECK(pg::api::hook<TwinMod1>(&d1, "d1 restarted", pg::api::Twins::All));
  CHECK(raw_on(&body_b));
  CHECK(pg::hooks::enable_all());
  CHECK(trace(&body_a) == "D1 H A");
  CHECK(trace(&body_b) == "R D1 B");
  pg::hooks::rollback_to(beforeMods);
  CHECK(trace(&body_a) == "H A" && trace(&body_b) == "B");
}

// The caller context, on a tracked test method with a twin (the game's convention: its MethodInfo
// comes last).
constexpr pg::sdk::EntryDesc kTestF{.key = "Test.F", .kind = pg::sdk::Kind::Method};
constexpr pg::api::Method<kTestF, int(int)> TestF{};
constexpr pg::sdk::EntryDesc kTestMany{.key = "Test.ManyTracked", .kind = pg::sdk::Kind::Method};
constexpr pg::api::Method<kTestMany, int(int)> TestMany{};

int g_depthInside = -1, g_depthElsewhere = -1;
__declspec(noinline) int tracked(int x, const void*) {
  g_depthInside = pg::context::depth("Test.F");
  std::thread([] { g_depthElsewhere = pg::context::depth("Test.F"); }).join();
  if (x < 0) throw std::runtime_error("thrown inside the tracked method");
  volatile int y = x;
  return y + 1;
}
__declspec(noinline) int tracked_twin(int x, const void*) {
  g_depthInside = pg::context::depth("Test.F");
  volatile int y = x;
  volatile int zero = 0;
  return y + zero + 1;
}
int call_tracked(GameFn body, int x) {
  GameFn volatile through = body;
  return through(x, nullptr);
}

const PgGameEntry* fake_lookup(const char* key) {
  static void* const twins[] = {reinterpret_cast<void*>(&tracked_twin)};
  static PgGameEntry entry{};
  entry.kind = PG_ENTRY_METHOD;
  entry.code = reinterpret_cast<void*>(&tracked);
  entry.twins = twins;
  entry.twinCount = 1;
  static const std::vector<void*> many = eight_twins();
  static PgGameEntry manyEntry{};
  manyEntry.kind = PG_ENTRY_METHOD;
  manyEntry.code = reinterpret_cast<void*>(&many_body);
  manyEntry.twins = many.data();
  manyEntry.twinCount = static_cast<uint32_t>(many.size());
  if (std::string(key) == "Test.F") return &entry;
  if (std::string(key) == "Test.ManyTracked") return &manyEntry;
  return nullptr;
}

void tracking_counts_every_body() {
  CHECK(pg::context::depth("Test.F") == 0);
  pg::context::track<TestF>();
  pg::context::track<TestMany>();
  g_log.clear();
  const size_t before = pg::hooks::mark();
  CHECK(pg::context::install(&fake_lookup));
  CHECK(pg::hooks::mark() - before == 2);  // one hook per body of Test.F, none on Test.ManyTracked
  CHECK(logged("Test.ManyTracked has 9 bodies, more than the 8 a hook covers"));
  CHECK(pg::hooks::enable_all());
  CHECK(call_tracked(&tracked, 1) == 2);
  CHECK(g_depthInside == 1);
  CHECK(g_depthElsewhere == 0);  // another thread never sees this one's depth
  CHECK(pg::context::depth("Test.F") == 0);
  g_depthInside = -1;
  CHECK(call_tracked(&tracked_twin, 1) == 2);
  CHECK(g_depthInside == 1);  // entered through the twin, counted all the same
  CHECK(pg::context::depth("Test.F") == 0);
  g_depthInside = -1;
  bool caught = false;
  try {
    call_tracked(&tracked, -1);
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
  folded_twin_hooked_once();

  give("Test.Twin", reinterpret_cast<void*>(&body_a), {reinterpret_cast<void*>(&body_b)});
  give("Test.Many", reinterpret_cast<void*>(&many_body), eight_twins());
  const PgHostApi host = test_host();
  std::string missing;
  CHECK(pg::api::bind(&host, missing));
  original_follows_the_entered_body();
  mixed_policies_now_coexist();
  raw_create_on_a_twin();
  second_typed_hook_refused();
  more_than_eight_bodies_refused();
  rollback_per_body();
  tracking_counts_every_body();

  shutdown();
  CHECK(call(target, 1) == 2 && call(other, 1) == 5);
  return g_failures ? 1 : 0;
}
