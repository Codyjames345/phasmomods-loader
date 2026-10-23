// The game API (sdk/api/game_api.h) and the phasmo:: wrappers against the fake game backend
// (sdk/fake): calls, field reads and writes, enum values, what a mod requires, what an entry that
// did not resolve does, and the runtime services (sdk/api/runtime.h): strings, arrays, lists, type
// objects, object creation, alive checks, virtual calls and GC holds. Run: build\game_api_test.exe
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Camera.h"
#include "GhostAI.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "utf16.h"

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

// An optional GhostAI method returning a number, for virtual_call's refusals.
inline constexpr pg::sdk::EntryDesc kTestCount{.key = "Test.count", .kind = pg::sdk::Kind::Method,
                                               .flags = pg::sdk::kOptional, .owner = "GhostAI", .name = "Count"};
constexpr api::Method<kTestCount, int32_t(void*)> testCount{};
int g_countCalls = 0;
int32_t fake_count(void*, const void*) {
  ++g_countCalls;
  return 7;
}

// An optional type no fake provides, so it never resolves.
inline constexpr pg::sdk::EntryDesc kOptionalType{.key = "Test.OptionalType", .kind = pg::sdk::Kind::Type,
                                                  .flags = pg::sdk::kOptional, .name = "OptionalType"};
constexpr api::Type<kOptionalType> optionalType{};

// Class pointers for the fake: GhostAI, a class derived from it, and Camera.
int g_ghostClass = 0, g_derivedClass = 0, g_cameraClass = 0;
void* const kGhostClass = &g_ghostClass;
void* const kDerivedClass = &g_derivedClass;
void* const kCameraClass = &g_cameraClass;

const void* g_overrideMethod = nullptr;
void* g_overrideGhost = nullptr;
void fake_unappear_override(void* ghost, const void* method) {
  g_overrideGhost = ghost;
  g_overrideMethod = method;
}

// A managed object as far as the fields below reach: GhostAI.state at 0x40, GhostAI.agent at 0x48.
struct FakeObject {
  alignas(8) uint8_t bytes[0x60] = {};
};

// Every entry this binary calls, except those `except` names, so bind can succeed.
void fake_everything_but(const char* except) {
  fake::reset();
  if (std::string(except) != map::GhostAI_UnAppear.key)
    fake::method(map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear), kGhostClass);
  fake::field(map::GhostAI_state, 0x40);
  fake::field(map::GhostAI_agent, 0x48);
  fake::value(map::GhostState_hunting, 2);
  fake::type(map::GhostAI, kGhostClass);
  fake::type(map::Camera, kCameraClass);
  fake::parent(kDerivedClass, kGhostClass);
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

// The shared UTF-16 to UTF-8 encoder the host's string_utf8 uses: a surrogate pair becomes one
// character, an unpaired surrogate becomes U+FFFD, and a short buffer is cut at a character boundary
// while the full length is still returned.
void utf16_encoder() {
  char out[16];
  CHECK(pg::utf16_to_utf8(u"\xD83D\xDE00", out, sizeof out) == 4 && std::string(out) == "\xF0\x9F\x98\x80");
  CHECK(pg::utf16_to_utf8(u"a\xD800z", out, sizeof out) == 5 && std::string(out) == "a\xEF\xBF\xBDz");
  CHECK(pg::utf16_to_utf8(u"ടഴ", out, 5) == 6 && std::string(out) == "ട");  // 3 bytes each: the second does not fit
  CHECK(pg::utf16_to_utf8(u"ab", nullptr, 0) == 2);
}

void utf8_round_trip_with_surrogates() {
  fake_everything_but("");
  CHECK(bound());
  for (const char* text : {"ടഴദബ", "😀", "a😀ടz", ""}) {
    void* s = api::make_string(text);
    CHECK(s != nullptr);
    CHECK(api::to_utf8(s) == text);
  }
  CHECK(api::to_utf8(nullptr).empty());
}

void array_and_list_access() {
  fake_everything_but("");
  CHECK(bound());
  const api::Array<int32_t> numbers{fake::array(sizeof(int32_t), 3)};
  CHECK(numbers.size() == 3);
  for (size_t i = 0; i < numbers.size(); ++i) numbers[i] = static_cast<int32_t>(10 * (i + 1));
  CHECK(numbers[0] == 10 && numbers[1] == 20 && numbers[2] == 30);

  int a = 0, b = 0, c = 0;
  const api::List list{fake::list({&a, &b, &c})};
  CHECK(list.size() == 3);
  CHECK(list[0] == &a && list[1] == &b && list[2] == &c);

  const api::RefArray refs{list.items()};
  CHECK(refs.size() >= 3 && refs[1] == &b);
  refs.set(1, &c);  // a reference store goes through the GC write barrier
  CHECK(fake::barrier_writes() == 1 && list[1] == &c);

  const api::Array<int32_t> none{nullptr};
  CHECK(none.size() == 0 && api::List{nullptr}.size() == 0);
}

void type_object_and_make() {
  fake_everything_but("");
  CHECK(bound());
  void* const type = phasmo::GhostAI::type.type_object();
  CHECK(type != nullptr && type == phasmo::GhostAI::type.type_object());  // one System.Type per class
  void* const ghost = phasmo::GhostAI::type.make();
  CHECK(ghost != nullptr && phasmo::GhostAI::type.is(ghost) && !phasmo::Camera::type.is(ghost));
  const api::RefArray cameras{phasmo::Camera::type.make_array(4)};
  CHECK(cameras.size() == 4 && cameras[3] == nullptr);

  g_lines.clear();
  CHECK(optionalType.type_object() == nullptr);
  CHECK(optionalType.make() == nullptr);
  CHECK(optionalType.make_array(2) == nullptr);
  CHECK(!optionalType.is(ghost));
  CHECK(count_logged("Test.OptionalType is not available in this game build") == 1);
}

void is_follows_parents() {
  fake_everything_but("");
  CHECK(bound());
  void* derived[1] = {kDerivedClass};
  void* camera[1] = {kCameraClass};
  CHECK(phasmo::GhostAI::type.is(derived));
  CHECK(!phasmo::GhostAI::type.is(camera));
  CHECK(!phasmo::GhostAI::type.is(nullptr));
}

void virtual_call_reaches_override() {
  fake_everything_but("");
  fake::override_method(map::GhostAI_UnAppear, kDerivedClass, reinterpret_cast<void*>(&fake_unappear_override));
  CHECK(bound());
  void* derived[1] = {kDerivedClass};
  void* base[1] = {kGhostClass};
  g_sawGhost = g_overrideGhost = nullptr;
  phasmo::GhostAI::UnAppear.virtual_call(derived);
  CHECK(g_overrideGhost == derived && g_sawGhost == nullptr);
  CHECK(g_overrideMethod != nullptr);  // the override's own method info
  g_overrideGhost = nullptr;
  phasmo::GhostAI::UnAppear.virtual_call(base);  // no override: the declared body
  CHECK(g_sawGhost == base && g_overrideGhost == nullptr);
  g_sawGhost = nullptr;
  phasmo::GhostAI::UnAppear(derived);  // a plain call is never virtual
  CHECK(g_sawGhost == derived && g_overrideGhost == nullptr);
}

// A null object, or one not of the method's declaring type, is refused once bound: nothing is
// called, the default comes back, and it is logged once. Unbound, the declared body runs.
void virtual_call_refuses_null_and_wrong_type() {
  fake_everything_but("");
  fake::method(kTestCount, reinterpret_cast<void*>(&fake_count), kGhostClass);
  CHECK(bound());
  void* camera[1] = {kCameraClass};
  void* ghost[1] = {kGhostClass};
  g_lines.clear();
  g_countCalls = 0;
  CHECK(testCount.virtual_call(camera) == 0);
  CHECK(testCount.virtual_call(nullptr) == 0);
  CHECK(g_countCalls == 0);
  CHECK(count_logged("Test.count: virtual_call on a null object or one not of its declaring type") == 1);
  CHECK(testCount.virtual_call(ghost) == 7 && g_countCalls == 1);  // the declared body, no override

  std::string missing;
  CHECK(!api::bind(nullptr, missing));  // unbound: the declared body, as a plain call
  CHECK(testCount.virtual_call(camera) == 7 && g_countCalls == 2);
}

// A service given an object of the wrong kind reads nothing from it and logs that once.
void wrong_kind_reads_nothing() {
  fake_everything_but("");
  CHECK(bound());
  void* const text = api::make_string("abc");
  void* const list = fake::list({text});
  void* const arr = fake::array(sizeof(int32_t), 2);
  g_lines.clear();
  CHECK(api::Array<int32_t>{text}.size() == 0 && api::RefArray{list}.size() == 0);
  CHECK(api::List{text}.size() == 0 && api::List{arr}.items() == nullptr);
  CHECK(api::to_utf8(list).empty() && api::to_utf8(arr).empty());
  CHECK(count_logged("is not a string") == 1);
  CHECK(count_logged("is not an array") == 1);
  CHECK(count_logged("is not a List") == 1);
  CHECK(api::to_utf8(text) == "abc");
  CHECK(fake::host()->string_utf8(text, nullptr, 8) == 3);  // a null `out` is size 0
}

void alive_false_for_destroyed() {
  fake_everything_but("");
  CHECK(bound());
  void* const ghost = phasmo::GhostAI::type.make();
  CHECK(api::alive(ghost));
  fake::destroy(ghost);
  CHECK(!api::alive(ghost));
  CHECK(!api::alive(nullptr));
}

void held_uses_contract_handles() {
  fake_everything_but("");
  CHECK(bound());
  int a = 0, b = 0;
  {
    api::Held kept = &a;
    CHECK(kept == &a && fake::held() == 1);
    kept = &a;  // the object it already holds keeps its handle
    CHECK(fake::held() == 1);
    kept = &b;
    CHECK(kept == &b && fake::held() == 1);
    api::Held moved = std::move(kept);
    CHECK(!kept && moved == &b && fake::held() == 1);
  }
  CHECK(fake::held() == 0);
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
  utf16_encoder();
  utf8_round_trip_with_surrogates();
  array_and_list_access();
  type_object_and_make();
  is_follows_parents();
  virtual_call_reaches_override();
  virtual_call_refuses_null_and_wrong_type();
  wrong_kind_reads_nothing();
  alive_false_for_destroyed();
  held_uses_contract_handles();
  return g_failures ? 1 : 0;
}
