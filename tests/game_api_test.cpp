// The game API (sdk/api/game_api.h) and the phasmo:: wrappers against the fake game backend
// (sdk/fake): calls, field and static field reads and writes, icalls, enum values, what a mod
// requires, what an entry that did not resolve does, and the runtime services (sdk/api/runtime.h): strings, arrays, lists, type
// objects, object creation, alive checks, virtual calls and GC holds, and the scene and asset facts:
// paths found and kept per scene load, layers by name and Animator parameters checked on first use.
// Run: build\game_api_test.exe
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Animator.h"
#include "Camera.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "Scene.h"
#include "UnityPlayer.h"
#include "UnityWorld.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "utf16.h"

// Hooks' detours hold a hooks::Guard; a mod's glue counts them, a test does here.
namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;

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
inline constexpr pm::sdk::EntryDesc kOptionalCount{.key = "Test.optionalCount", .kind = pm::sdk::Kind::Method,
                                                   .flags = pm::sdk::kOptional, .owner = "GhostAI",
                                                   .name = "OptionalCount"};
constexpr api::Method<kOptionalCount, int32_t(void*)> optionalCount{};

// An optional GhostAI method returning a number, for virtual_call's refusals.
inline constexpr pm::sdk::EntryDesc kTestCount{.key = "Test.count", .kind = pm::sdk::Kind::Method,
                                               .flags = pm::sdk::kOptional, .owner = "GhostAI", .name = "Count"};
constexpr api::Method<kTestCount, int32_t(void*)> testCount{};
int g_countCalls = 0;
int32_t fake_count(void*, const void*) {
  ++g_countCalls;
  return 7;
}

// An optional type no fake provides, so it never resolves.
inline constexpr pm::sdk::EntryDesc kOptionalType{.key = "Test.OptionalType", .kind = pm::sdk::Kind::Type,
                                                  .flags = pm::sdk::kOptional, .name = "OptionalType"};
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

// The static storage of LevelController.instance, and of an optional static int no fake gives.
void* g_levelInstance = nullptr;
inline constexpr pm::sdk::EntryDesc kOptionalStatic{.key = "Test.optionalStatic", .kind = pm::sdk::Kind::Field,
                                                    .flags = pm::sdk::kStatic | pm::sdk::kOptional,
                                                    .owner = "LevelController", .type = "int"};
constexpr api::StaticField<kOptionalStatic, int32_t> optionalStatic{};

// A layer and an Animator parameter for the scene fact tests, both optional, so the other tests bind
// without them.
inline constexpr pm::sdk::EntryDesc kTestLayer{.key = "Test.layer", .kind = pm::sdk::Kind::Layer,
                                               .flags = pm::sdk::kOptional, .name = "NoSuchLayer"};
constexpr api::Layer<kTestLayer> testLayer{};
inline constexpr pm::sdk::EntryDesc kTestParam{.key = "Test.isOpened", .kind = pm::sdk::Kind::AnimatorParam,
                                               .flags = pm::sdk::kOptional, .name = "isOpened",
                                               .scene = "Resources/PCPlayer", .component = "BookController"};
constexpr api::AnimatorParam<kTestParam> testParam{};

void* g_sawAnimator = nullptr;
int32_t g_sawId = 0;
bool g_sawValue = false;
void fake_set_bool_id(void* animator, int32_t id, bool value) {  // an icall takes no MethodInfo
  g_sawAnimator = animator;
  g_sawId = id;
  g_sawValue = value;
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
  g_levelInstance = nullptr;
  fake::static_field(map::LevelController_instance, &g_levelInstance, sizeof g_levelInstance, true);
  fake::icall(map::Animator_SetBoolID, reinterpret_cast<void*>(&fake_set_bool_id));
  if (std::string(except) != map::Scene_JournalBookContent.key) fake::scene_fact(map::Scene_JournalBookContent);
  std::vector<std::string> layers(32);
  layers[0] = "Default";
  layers[5] = "UI";
  layers[11] = "PlayerView";
  fake::layer_names(layers);
  fake::layer(map::Layer_PlayerView);
}

bool bound() {
  std::string missing;
  const bool ok = api::bind(fake::host(), missing);
  return ok && missing.empty();
}

// The fake host fills every function of the contract it claims: each member from `log` to the end
// of PmHostApi is a function pointer, and none is null.
void fake_host_fills_every_function() {
  const PmHostApi* host = fake::host();
  const auto* bytes = reinterpret_cast<const unsigned char*>(host);
  for (size_t at = offsetof(PmHostApi, log); at < sizeof(PmHostApi); at += sizeof(void*)) {
    void* slot = nullptr;
    std::memcpy(&slot, bytes + at, sizeof slot);
    CHECK(slot != nullptr);
  }
}

// A mod names the effect: the agent and the direction reach the host as they were asked, and nothing of how it is done.
// The fake shows each ask, a null agent's question among none of them.
void steering_an_agent_reaches_the_host() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(phasmo::NavMeshAgent::steer.available());
  CHECK(fake::steer_asks().empty());
  int agent = 0;
  phasmo::NavMeshAgent::steer(&agent, 0.6f, -0.8f);
  phasmo::NavMeshAgent::steer(&agent, 3.0f, 3.0f);  // the host says what a direction is
  phasmo::NavMeshAgent::steer(nullptr, 1.0f, 0.0f);
  CHECK(fake::steer_asks().size() == 2);
  if (fake::steer_asks().size() == 2) {
    CHECK(fake::steer_asks()[0].agent == &agent && fake::steer_asks()[0].dx == 0.6f && fake::steer_asks()[0].dz == -0.8f);
    CHECK(fake::steer_asks()[1].dx == 3.0f && fake::steer_asks()[1].dz == 3.0f);
  }
}

// The host cannot steer in this build: the mod hears it when it asks, so it stays inactive.
void steering_the_host_refuses_is_unavailable() {
  fake_everything_but("");
  CHECK(bound());
  fake::refuse_steering();
  CHECK(!phasmo::NavMeshAgent::steer.available());
  int agent = 0;
  phasmo::NavMeshAgent::steer(&agent, 1.0f, 0.0f);
  CHECK(fake::steer_asks().empty());
}

// The entries the host steers with are its own: the mod binds and steers without any of them, requires none (see
// only_called_entries_are_required), and is never given a native site even where the game has one.
void steering_needs_no_entry_of_the_mod() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(phasmo::NavMeshAgent::steer.available());
  int agent = 0;
  phasmo::NavMeshAgent::steer(&agent, 1.0f, 0.0f);
  CHECK(fake::steer_asks().size() == 1);
  unsigned char code = 0;
  fake::native_site(map::UnityPlayer_SteerSite, &code);
  CHECK(fake::host()->game_entry(map::UnityPlayer_SteerSite.key) == nullptr);
  CHECK(fake::host_entry(map::UnityPlayer_SteerSite.key) != nullptr);
}

// Before bind there is no host to ask.
void steering_before_bind_is_unavailable() {
  fake_everything_but("");
  std::string missing;
  api::bind(nullptr, missing);
  CHECK(!phasmo::NavMeshAgent::steer.available());
  int agent = 0;
  phasmo::NavMeshAgent::steer(&agent, 1.0f, 0.0f);
  CHECK(fake::steer_asks().empty());
}

void call_reaches_fake() {
  fake_everything_but("");
  CHECK(bound());
  int ghost = 0;
  phasmo::GhostAI::UnAppear(static_cast<void*>(&ghost));
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
  phasmo::GhostAI::agent.set(&obj, pm::api::Ref<pm::map::NavMeshAgent>::unchecked(&agent));
  CHECK(fake::barrier_writes() == 1);
  CHECK(phasmo::GhostAI::agent.get(&obj) == &agent);
}

void enum_value() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(phasmo::GhostState::hunting() == 2);
}

std::vector<std::string> required_keys() {
  const char* const* keys = nullptr;
  const size_t n = api::used_required(&keys);
  return std::vector<std::string>(keys, keys + n);
}

// Exactly the required entries this binary's code uses: GhostAI.LookAtPlayer, declared in GhostAI.h
// and never called here, is not among them, nor the optional entries the tests call.
void only_called_entries_are_required() {
  CHECK((required_keys() == std::vector<std::string>{"Animator.SetBoolID", "Camera", "GhostAI", "GhostAI.UnAppear",
                                                     "GhostAI.agent", "GhostAI.state", "GhostState.hunting",
                                                     "Layer.PlayerView", "LevelController.instance",
                                                     "Scene.JournalBookContent"}));
  const char* const* first = nullptr;
  const char* const* again = nullptr;
  CHECK(api::used_required(&first) == api::used_required(&again) && first == again);  // kept, not rebuilt
}

// Naming an entry's use or a wrapper's class registers nothing; only a member that code calls does.
// MSVC instantiates a class template's static data members along with the class, so a registration
// kept in Use would happen here although nothing calls the entry. (An entry declared in an unnamed
// namespace still registers when called: the virtual_call and static field tests resolve this
// file's own.)
void registration_needs_a_call() {
  static_cast<void>(sizeof(api::Use<map::GhostAI_LookAtPlayer>));
  static_cast<void>(sizeof(phasmo::GhostAI::TemporarilyStopWander));
  const std::vector<std::string> required = required_keys();
  CHECK(std::find(required.begin(), required.end(), "GhostAI.LookAtPlayer") == required.end());
  CHECK(std::find(required.begin(), required.end(), "GhostAI.TemporarilyStopWander") == required.end());
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

// Two threads that call an unresolved optional entry at the same moment log its line once between
// them. Each round binds again, which makes the line due once more.
void log_once_is_atomic() {
  fake_everything_but("");
  int wrong = 0;
  for (int round = 0; round < 2000; ++round) {
    CHECK(bound());
    g_lines.clear();
    std::atomic<int> ready{0};
    const auto call = [&] {
      ready.fetch_add(1);
      while (ready.load() < 2) {
      }
      static_cast<void>(optionalCount(nullptr));
    };
    std::thread a(call), b(call);
    a.join();
    b.join();
    wrong += count_logged("Test.optionalCount is not available in this game build") != 1;
  }
  CHECK(wrong == 0);
}

void unresolved_required_call_does_nothing() {
  fake_everything_but(map::GhostAI_UnAppear.key);
  std::string missing;
  CHECK(!api::bind(fake::host(), missing));
  g_lines.clear();
  g_sawGhost = nullptr;
  int ghost = 0;
  phasmo::GhostAI::UnAppear(static_cast<void*>(&ghost));
  CHECK(g_sawGhost == nullptr);
  CHECK(count_logged("GhostAI.UnAppear is not available in this game build") == 1);
}

int g_detourCalls = 0;
void unappear_detour(api::Original<phasmo::GhostAI::UnAppear> original, void* ghost) {
  ++g_detourCalls;
  original(ghost);
}

// What goes under a mod's own hook, from outside its detour: the chain's next, with the entry's MethodInfo.
int g_underHook = 0, g_entered = 0;
const void* g_underMethod = nullptr;
void under_body(void*, const void* method) {
  ++g_underHook;
  g_underMethod = method;
}
void entered_body(void*, const void*) { ++g_entered; }
bool under_host_hook(const char*, uint32_t, void*, void** original, const char*) {
  *original = reinterpret_cast<void*>(&under_body);
  return true;
}
void under_detour(api::Original<phasmo::GhostAI::UnAppear>, void*) {}

void below_hook_goes_under_the_mods_own_hook() {
  fake::reset();
  std::string missing;
  api::bind(fake::host(), missing);
  api::below_hook<phasmo::GhostAI::UnAppear>()(nullptr);  // unresolved: nothing runs
  CHECK(g_underHook == 0 && g_entered == 0);
  fake::reset();
  fake::method(map::GhostAI_UnAppear, reinterpret_cast<void*>(&entered_body));
  PmHostApi host = *fake::host();
  host.hook_body = &under_host_hook;
  api::bind(&host, missing);
  const PmGameEntry* e = api::Use<map::GhostAI_UnAppear>::entry();
  CHECK(api::hook<phasmo::GhostAI::UnAppear>(&under_detour, "under", api::Twins::CanonicalOnly));
  api::below_hook<phasmo::GhostAI::UnAppear>()(nullptr);
  CHECK(g_underHook == 1 && g_entered == 0);
  CHECK(e && g_underMethod == e->method);
  fake::reset();
  api::bind(fake::host(), missing);
}

void hook_by_entry() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(api::hook<phasmo::GhostAI::UnAppear>(&unappear_detour, "test"));
  CHECK(fake::hooked() == std::vector<std::string>{"GhostAI.UnAppear"});
  CHECK(!api::inside<phasmo::GhostAI::UnAppear>());
  // The game calling the hooked body reaches the detour, and its original reaches the method.
  using W = std::remove_cvref_t<decltype(phasmo::GhostAI::UnAppear)>;
  using Entry = void (*)(void*, const void*);
  int ghost = 0;
  g_sawGhost = nullptr;
  reinterpret_cast<Entry>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0))(&ghost, nullptr);
  CHECK(g_detourCalls == 1 && g_sawGhost == &ghost);
  CHECK(!api::hook<phasmo::GhostAI::UnAppear>(nullptr, "no detour"));
  CHECK(count_logged("GhostAI.UnAppear: the hook was given no detour; nothing was hooked") == 1);
}

// The shared UTF-16 to UTF-8 encoder the host's string_utf8 uses: a surrogate pair becomes one
// character, an unpaired surrogate becomes U+FFFD, and a short buffer is cut at a character boundary
// while the full length is still returned.
void utf16_encoder() {
  char out[16];
  CHECK(pm::utf16_to_utf8(u"\xD83D\xDE00", out, sizeof out) == 4 && std::string(out) == "\xF0\x9F\x98\x80");
  CHECK(pm::utf16_to_utf8(u"a\xD800z", out, sizeof out) == 5 && std::string(out) == "a\xEF\xBF\xBDz");
  CHECK(pm::utf16_to_utf8(u"ടഴ", out, 5) == 6 && std::string(out) == "ട");  // 3 bytes each: the second does not fit
  CHECK(pm::utf16_to_utf8(u"ab", nullptr, 0) == 2);
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

  // A List grows through its own Add (the host contract's list_add) and answers what it holds.
  int d = 0;
  CHECK(list.contains(&c) && !list.contains(&d));
  CHECK(list.add(&d) && list.size() == 4 && list[3] == &d && list.contains(&d));
  CHECK(!api::List{nullptr}.add(&d));
  CHECK(!api::List{fake::array(sizeof(void*), 1)}.add(&d));  // an array is not a List
}

// A collection's count comes through its own Count, whatever kind it is; a List of structs is read with
// the stride the runtime gives for its element type, never an assumed one; an item leaves a List through
// its own RemoveAt.
void collection_count_struct_lists_and_remove() {
  fake_everything_but("");
  CHECK(bound());
  CHECK(api::count(fake::collection(5)) == 5);  // a HashSet
  int a = 0, b = 0, c = 0;
  const api::List list{fake::list({&a, &b, &c})};
  CHECK(api::count(list.raw) == 3);
  CHECK(api::count(nullptr) == 0 && api::count(fake::array(4, 2)) == 0);  // an array is no collection

  struct Heard {  // the test's own struct, of 24 bytes: the stride is read, not assumed
    void* who;
    float metres;
    int32_t pad[3];
  };
  const api::List heard{fake::struct_list(sizeof(Heard), 2)};
  const api::ValueArray items = heard.values();
  CHECK(items.stride() == sizeof(Heard) && items.size() == 2);
  static_cast<Heard*>(items.at(1))->who = &b;
  CHECK(static_cast<Heard*>(items.at(1)) == static_cast<Heard*>(items.at(0)) + 1);
  CHECK(static_cast<const Heard*>(items.at(1))->who == &b);
  g_lines.clear();
  CHECK(items.at(2) == nullptr);  // past the end: nothing touched, logged once
  CHECK(api::ValueArray{nullptr}.size() == 0 && api::ValueArray{list.raw}.size() == 0);  // a List is no array

  CHECK(list.remove_at(1) && list.size() == 2 && list[0] == &a && list[1] == &c);
  CHECK(!list.remove_at(2) && list.size() == 2);  // out of range: nothing removed
  CHECK(!api::List{nullptr}.remove_at(0));
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

// A class's settings copy as the host reads them from the metadata: its own public value fields, a
// reference inside one through the barrier, nothing else. Its fields of one type read in layout order.
void settings_copy_and_fields_of_type() {
  fake_everything_but("");
  fake::declare_fields(kGhostClass, {{.offset = 0x40, .size = 4, .publicValue = true},
                                     {.offset = 0x48, .size = 16, .publicValue = true, .references = {8}},
                                     {.offset = 0x58, .size = 4},  // private, or a reference: not a setting
                                     {.offset = 0x68, .size = 8, .type = kCameraClass, .array = true},
                                     {.offset = 0x60, .size = 8, .type = kCameraClass, .array = true},
                                     {.offset = 0x70, .size = 8, .type = kCameraClass}});
  CHECK(bound());
  alignas(8) unsigned char from[0x80] = {}, to[0x80] = {};
  std::memcpy(from, &kGhostClass, sizeof(void*));
  std::memcpy(to, &kDerivedClass, sizeof(void*));  // a derived class takes its parent's settings
  int curve = 0;
  void* curveRef = &curve;
  const int32_t a = 7, b = 9, hidden = 5;
  std::memcpy(from + 0x40, &a, 4);
  std::memcpy(from + 0x48, &b, 4);
  std::memcpy(from + 0x50, &curveRef, 8);
  std::memcpy(from + 0x58, &hidden, 4);
  const int writes = fake::barrier_writes();
  CHECK(phasmo::GhostAI::type.copy_settings(to, from));
  int32_t got = 0;
  void* gotRef = nullptr;
  std::memcpy(&got, to + 0x40, 4);
  CHECK(got == 7);
  std::memcpy(&got, to + 0x48, 4);
  std::memcpy(&gotRef, to + 0x50, 8);
  CHECK(got == 9 && gotRef == &curve && fake::barrier_writes() == writes + 1);
  std::memcpy(&got, to + 0x58, 4);
  CHECK(got == 0);  // not a public value: left alone
  void* camera[1] = {kCameraClass};
  CHECK(!phasmo::GhostAI::type.copy_settings(camera, from) && !phasmo::GhostAI::type.copy_settings(nullptr, from));

  int first = 0, second = 0, single = 0;
  void* refs[] = {&first, &second, &single};
  std::memcpy(from + 0x60, &refs[0], 8);
  std::memcpy(from + 0x68, &refs[1], 8);
  std::memcpy(from + 0x70, &refs[2], 8);
  void* out[4] = {};
  CHECK((phasmo::GhostAI::type.fields_of<phasmo::Camera::type>(from, true, out, 4)) == 2);
  CHECK(out[0] == &first && out[1] == &second);  // by offset, not as declared
  CHECK((phasmo::GhostAI::type.fields_of<phasmo::Camera::type>(from, true, out, 1)) == 2 && out[0] == &first);
  CHECK((phasmo::GhostAI::type.fields_of<phasmo::Camera::type>(from, false, out, 4)) == 1 && out[0] == &single);
  CHECK((phasmo::GhostAI::type.fields_of<phasmo::Camera::type>(camera, true, out, 4)) == 0);
}

void virtual_call_reaches_override() {
  fake_everything_but("");
  fake::override_method(map::GhostAI_UnAppear, kDerivedClass, reinterpret_cast<void*>(&fake_unappear_override));
  CHECK(bound());
  void* derived[1] = {kDerivedClass};
  void* base[1] = {kGhostClass};
  g_sawGhost = g_overrideGhost = nullptr;
  phasmo::GhostAI::UnAppear.virtual_call(static_cast<void*>(derived));
  CHECK(g_overrideGhost == derived && g_sawGhost == nullptr);
  CHECK(g_overrideMethod != nullptr);  // the override's own method info
  g_overrideGhost = nullptr;
  phasmo::GhostAI::UnAppear.virtual_call(static_cast<void*>(base));  // no override: the declared body
  CHECK(g_sawGhost == base && g_overrideGhost == nullptr);
  g_sawGhost = nullptr;
  phasmo::GhostAI::UnAppear(static_cast<void*>(derived));  // a plain call is never virtual
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

void static_field_get_set() {
  fake_everything_but("");
  CHECK(bound());
  int level = 0;
  CHECK(phasmo::LevelController::instance.get() == nullptr);
  phasmo::LevelController::instance.set(static_cast<void*>(&level));
  CHECK(g_levelInstance == &level);                         // in the class's static storage
  CHECK(fake::barrier_writes() == 1);                       // a reference goes through the write barrier
  CHECK(phasmo::LevelController::instance.get() == &level);
  // read tells a null from a read that failed, as get does not.
  void* read = nullptr;
  CHECK(phasmo::LevelController::instance.read(read) && read == &level);
  fake::static_field(pm::map::LevelController_instance, &g_levelInstance, sizeof(int32_t), true);
  read = nullptr;
  CHECK(!phasmo::LevelController::instance.read(read) && !read);  // the host refused it
  fake::forget(pm::map::LevelController_instance);
  bound();
  CHECK(!phasmo::LevelController::instance.read(read));  // it did not resolve
}

// An unresolved static field reads the default and writes nothing, logged once; one the host cannot
// read (here its size differs, as for a class it could not initialise) does the same.
void static_field_unresolved_returns_default() {
  fake_everything_but("");
  CHECK(bound());
  g_lines.clear();
  CHECK(!optionalStatic.available());
  CHECK(optionalStatic.get() == 0);
  optionalStatic.set(5);
  CHECK(optionalStatic.get() == 0);
  CHECK(count_logged("Test.optionalStatic is not available in this game build") == 1);
  CHECK(g_lines.size() == 1);

  int64_t wide = 9;
  fake::static_field(kOptionalStatic, &wide, sizeof wide);  // an int64 where the API reads an int32
  CHECK(bound());
  g_lines.clear();
  CHECK(optionalStatic.available());
  CHECK(optionalStatic.get() == 0);
  optionalStatic.set(5);
  CHECK(wide == 9);
  CHECK(count_logged("Test.optionalStatic: the host could not read or write the static field") == 1);
  CHECK(g_lines.size() == 1);
}

void icall_reaches_fake() {
  fake_everything_but("");
  CHECK(bound());
  int animator = 0;
  CHECK(phasmo::Animator::SetBoolID.available());
  phasmo::Animator::SetBoolID(&animator, 42, true);
  CHECK(g_sawAnimator == &animator && g_sawId == 42 && g_sawValue);
}


// A path that is not under the object it is looked up from answers null, and says so once for each
// scene load; a null root answers null without a search.
void missing_scene_path_is_null_and_logged_once() {
  fake_everything_but("");
  CHECK(bound());
  int canvas = 0;
  g_lines.clear();
  CHECK(phasmo::Journal::BookContent.available());
  CHECK(phasmo::Journal::BookContent.find(&canvas) == nullptr);
  CHECK(phasmo::Journal::BookContent.find(&canvas) == nullptr);
  CHECK(phasmo::Journal::BookContent.find(nullptr) == nullptr);
  CHECK(count_logged("Scene.JournalBookContent: no Content/GhostHuntingBookContent under the given object") == 1);
  CHECK(g_lines.size() == 1);
  fake::load_scene();
  CHECK(phasmo::Journal::BookContent.find(&canvas) == nullptr);
  CHECK(count_logged("Scene.JournalBookContent: no Content/GhostHuntingBookContent under the given object") == 2);
}

// A path found is kept until the next scene load, or until the object found is destroyed or another
// root is given; an entry the build lacks answers null without a search.
void scene_path_found_and_kept_per_scene() {
  fake_everything_but("");
  CHECK(bound());
  int canvas = 0, other = 0, book = 0, otherBook = 0;
  fake::child(&canvas, "Content/GhostHuntingBookContent", &book);
  fake::child(&other, "Content/GhostHuntingBookContent", &otherBook);
  CHECK(phasmo::Journal::BookContent.find(&canvas) == &book);
  CHECK(phasmo::Journal::BookContent.find(&canvas) == &book);
  CHECK(fake::finds() == 1);
  CHECK(fake::held() == 2);  // the root and what was found, kept for the scene
  g_lines.clear();
  CHECK(phasmo::Journal::BookContent.find(nullptr) == nullptr);  // no root: no search, nothing logged
  CHECK(phasmo::Journal::BookContent.find(&canvas) == &book);   // and the kept answer stays
  CHECK(fake::finds() == 1 && fake::held() == 2 && g_lines.empty());
  fake::load_scene();
  CHECK(phasmo::Journal::BookContent.find(&canvas) == &book && fake::finds() == 2);
  fake::destroy(&book);
  CHECK(phasmo::Journal::BookContent.find(&canvas) == nullptr && fake::finds() == 3);
  CHECK(phasmo::Journal::BookContent.find(&other) == &otherBook && fake::finds() == 4);

  fake_everything_but(map::Scene_JournalBookContent.key);  // the build lacks it
  CHECK(!bound());
  g_lines.clear();
  CHECK(phasmo::Journal::BookContent.find(&canvas) == nullptr && fake::finds() == 0);
  CHECK(count_logged("Scene.JournalBookContent is not available in this game build") == 1);
}

// A miss says what failed: a root that is not a Transform, a NameToLayer that did not resolve, an
// object that is not an Animator.
void misses_say_what_failed() {
  fake_everything_but("");
  fake::scene_fact(kTestParam);
  CHECK(bound());
  int stranger = 0;
  fake::wrong_type(&stranger);
  g_lines.clear();
  CHECK(phasmo::Journal::BookContent.find(&stranger) == nullptr);
  CHECK(count_logged("Scene.JournalBookContent: the root is not a Transform; it answers null") == 1);
  CHECK(testParam.hash(&stranger) == 0);
  CHECK(count_logged("Test.isOpened: the object is not an Animator; it answers 0") == 1);
  fake::layer_names({});  // a NameToLayer that did not resolve
  fake::layer(kTestLayer);
  CHECK(count_logged("Test.layer: LayerMask.NameToLayer unresolved") == 1);
  CHECK(count_logged("no layer is named") == 0);
  CHECK(g_lines.size() == 3);
}

// A layer resolves by its name through LayerMask.NameToLayer; one no layer has stays unresolved and
// answers -1, never 0, which is the Default layer.
void layer_by_name() {
  fake_everything_but("");  // its NameToLayer knows Default (0), UI (5) and PlayerView (11)
  g_lines.clear();
  fake::layer(kTestLayer);  // resolved as the host does when it activates
  CHECK(bound());
  CHECK(phasmo::Layers::PlayerView.available() && phasmo::Layers::PlayerView.index() == 11);
  CHECK(!testLayer.available() && testLayer.index() == -1);
  CHECK(count_logged("no layer is named NoSuchLayer") == 1);
  CHECK(count_logged("Test.layer is not available in this game build") == 1);
}

// An Animator parameter is checked against the Animator's own parameters the first time that Animator
// is asked; one it lacks answers 0, logged once per scene load.
void animator_param_checked() {
  fake_everything_but("");
  fake::scene_fact(kTestParam);
  CHECK(bound());
  int book = 0, hands = 0;
  fake::parameters(&book, {{"isVROpened", 7}, {"isOpened", 1234}});
  fake::parameters(&hands, {{"Selecting", 9}});
  g_lines.clear();
  CHECK(testParam.available());
  CHECK(testParam.hash(&book) == 1234 && testParam.hash(&book) == 1234);
  CHECK(fake::parameter_reads() == 1);  // checked on first use, not on every call
  CHECK(testParam.hash(&hands) == 0 && testParam.hash(&hands) == 0);
  CHECK(testParam.hash(nullptr) == 0);
  CHECK(count_logged("Test.isOpened: the Animator has no parameter isOpened") == 1);
  CHECK(g_lines.size() == 1);
  fake::load_scene();
  CHECK(testParam.hash(&book) == 1234 && fake::parameter_reads() == 3);
}

}  // namespace

int main() {
  below_hook_goes_under_the_mods_own_hook();
  pm::log::set_sink(&capture, "");
  fake_host_fills_every_function();
  steering_an_agent_reaches_the_host();
  steering_the_host_refuses_is_unavailable();
  steering_needs_no_entry_of_the_mod();
  steering_before_bind_is_unavailable();
  call_reaches_fake();
  field_get_set();
  reference_set_uses_barrier();
  enum_value();
  only_called_entries_are_required();
  registration_needs_a_call();
  bind_names_missing_required();
  unresolved_optional_call_returns_default();
  log_once_is_atomic();
  unresolved_required_call_does_nothing();
  hook_by_entry();
  utf16_encoder();
  utf8_round_trip_with_surrogates();
  array_and_list_access();
  collection_count_struct_lists_and_remove();
  type_object_and_make();
  is_follows_parents();
  settings_copy_and_fields_of_type();
  virtual_call_reaches_override();
  virtual_call_refuses_null_and_wrong_type();
  wrong_kind_reads_nothing();
  alive_false_for_destroyed();
  held_uses_contract_handles();
  static_field_get_set();
  static_field_unresolved_returns_default();
  icall_reaches_fake();
  missing_scene_path_is_null_and_logged_once();
  scene_path_found_and_kept_per_scene();
  misses_say_what_failed();
  layer_by_name();
  animator_param_checked();
  return g_failures ? 1 : 0;
}
