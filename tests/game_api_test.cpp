// The game API (sdk/api/game_api.h) and the phasmo:: wrappers against the fake game backend
// (sdk/fake): calls, field reads and writes, enum values, what a mod requires, and what an entry that
// did not resolve does. Run: build\game_api_test.exe
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "log.h"

namespace {

namespace api = pg::api;
namespace fake = pg::fake;
namespace map = pg::map;

std::vector<std::string> g_lines;
void capture(const char* line) { g_lines.emplace_back(line); }

int count_logged(const std::string& text) {
  return static_cast<int>(std::count_if(g_lines.begin(), g_lines.end(),
                                        [&](const std::string& l) { return l.find(text) != std::string::npos; }));
}

void* g_sawGhost = nullptr;
const void* g_sawMethod = reinterpret_cast<const void*>(1);
void fake_unappear(void* ghost, const void* method) {
  g_sawGhost = ghost;
  g_sawMethod = method;
}

// An optional entry no fake provides, so it never resolves.
inline constexpr pg::sdk::EntryDesc kOptionalCount{.key = "Test.optionalCount", .kind = pg::sdk::Kind::Method,
                                                   .flags = pg::sdk::kOptional, .owner = "GhostAI",
                                                   .name = "OptionalCount"};
constexpr api::Method<kOptionalCount, int32_t(void*)> optionalCount{};

// A managed object as far as the fields below reach: GhostAI.state at 0x40, GhostAI.agent at 0x48.
struct FakeObject {
  alignas(8) uint8_t bytes[0x60] = {};
};

// Every entry this binary calls, except those `except` names, so bind can succeed.
void fake_everything_but(const char* except) {
  fake::reset();
  if (std::string(except) != map::GhostAI_UnAppear.key)
    fake::method(map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
  fake::field(map::GhostAI_state, 0x40);
  fake::field(map::GhostAI_agent, 0x48);
  fake::value(map::GhostState_hunting, 2);
}

bool bound() {
  std::string missing;
  const bool ok = api::bind(fake::host(), missing);
  return ok && missing.empty();
}

void call_reaches_fake() {
  fake_everything_but("");
  CHECK(bound());
  int ghost = 0;
  phasmo::GhostAI::UnAppear(&ghost);
  CHECK(g_sawGhost == &ghost);
  CHECK(g_sawMethod == nullptr);  // the fake's method info
  CHECK(phasmo::GhostAI::UnAppear.available());
}

void field_get_set() {
  fake_everything_but("");
  CHECK(bound());
  FakeObject obj;
  phasmo::GhostAI::state.set(&obj, 2);
  CHECK(phasmo::GhostAI::state.get(&obj) == 2);
  CHECK(*reinterpret_cast<int32_t*>(obj.bytes + 0x40) == 2);  // at the fake's offset
  CHECK(fake::barrier_writes() == 0);                          // a plain value is written directly
}

void reference_set_uses_barrier() {
  fake_everything_but("");
  CHECK(bound());
  FakeObject obj;
  int agent = 0;
  phasmo::GhostAI::agent.set(&obj, &agent);
  CHECK(fake::barrier_writes() == 1);
  CHECK(phasmo::GhostAI::agent.get(&obj) == &agent);
}

void enum_value() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(phasmo::GhostState::hunting() == 2);
}

void only_called_entries_are_required() {
  const char* const* keys = nullptr;
  const size_t n = api::used_required(&keys);
  std::vector<std::string> required(keys, keys + n);
  const auto has = [&](const char* key) { return std::find(required.begin(), required.end(), key) != required.end(); };
  CHECK(has("GhostAI.UnAppear"));
  CHECK(!has("GhostAI.LookAtPlayer"));  // declared in GhostAI.h, never called here
  CHECK(!has("Test.optionalCount"));    // called, but optional
  CHECK(std::is_sorted(required.begin(), required.end()));
}

void bind_names_missing_required() {
  fake_everything_but(map::GhostAI_UnAppear.key);
  std::string missing;
  CHECK(!api::bind(fake::host(), missing));
  CHECK(missing == "GhostAI.UnAppear");
}

void unresolved_optional_call_returns_default() {
  fake_everything_but("");
  CHECK(bound());  // an optional entry that did not resolve does not stop the mod
  g_lines.clear();
  CHECK(!optionalCount.available());
  CHECK(optionalCount(nullptr) == 0);
  CHECK(optionalCount(nullptr) == 0);
  CHECK(count_logged("Test.optionalCount is not available in this game build") == 1);
  CHECK(g_lines.size() == 1);
}

void unresolved_required_call_does_nothing() {
  fake_everything_but(map::GhostAI_UnAppear.key);
  std::string missing;
  CHECK(!api::bind(fake::host(), missing));
  g_lines.clear();
  g_sawGhost = nullptr;
  int ghost = 0;
  phasmo::GhostAI::UnAppear(&ghost);
  CHECK(g_sawGhost == nullptr);
  CHECK(count_logged("GhostAI.UnAppear is not available in this game build") == 1);
}

void hook_by_entry() {
  fake_everything_but("");
  CHECK(bound());
  void* original = nullptr;
  CHECK(api::hook<phasmo::GhostAI::UnAppear>(reinterpret_cast<void*>(&fake_unappear), &original, "test"));
  CHECK(fake::hooked() == std::vector<std::string>{"GhostAI.UnAppear"});
  CHECK(!api::inside<phasmo::GhostAI::UnAppear>());
}

}  // namespace

int main() {
  pg::log::set_sink(&capture, "");
  call_reaches_fake();
  field_get_set();
  reference_set_uses_barrier();
  enum_value();
  only_called_entries_are_required();
  bind_names_missing_required();
  unresolved_optional_call_returns_default();
  unresolved_required_call_does_nothing();
  hook_by_entry();
  return g_failures ? 1 : 0;
}
