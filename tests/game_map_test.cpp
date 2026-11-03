// The host's game map resolution against a fake runtime: it stays inside its share of the startup
// deadline and stops when startup is cancelled, resolves icalls through the runtime and layers through
// the scene engine, and publishes each kind of entry as mods (and, for exports, the host) receive it.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "GhostInfo.h"
#include "check.h"
#include "game_api.h"
#include "sdk_image_fixture.h"

namespace {
unsigned long long clockMs = 0;
unsigned long long tickPerClass = 0;  // how far the clock moves for each class read
int stopAfter = -1;                   // the class read after which a stop is requested, -1 never
bool stopRequested = false;
int waits = 0;                        // how often the stop event has been checked
int stopAtWait = -1;                  // the check that finds a stop requested, -1 none
int classesRead = 0;
int apiRequests = 0;
void* icallAnswer = nullptr;          // what the runtime's il2cpp_resolve_icall answers
std::vector<std::string> icallsAsked;
std::vector<std::string> messages;

ULONGLONG WINAPI tick_count() { return clockMs; }
DWORD WINAPI wait_event(HANDLE, DWORD) {
  return stopRequested || ++waits == stopAtWait ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
}  // namespace

#define GetTickCount64 tick_count
#define WaitForSingleObject wait_event
#include "../host/game_map.cpp"
#undef GetTickCount64
#undef WaitForSingleObject

namespace {

using namespace pm::sdk;

// Nineteen images of 200 empty classes each, named as the map's assemblies.
struct FakeImage {
  std::string name;
  int classes[200] = {};
};
FakeImage g_images[] = {{"Assembly-CSharp.dll"},     {"UnityEngine.AnimationModule.dll"}, {"UnityEngine.CoreModule.dll"},
                        {"UnityEngine.UI.dll"},      {"mscorlib.dll"},                    {"PhotonUnityNetworking.dll"},
                        {"PhotonRealtime.dll"},      {"UnityEngine.PhysicsModule.dll"},   {"UnityEngine.AIModule.dll"},
                        {"UnityEngine.AudioModule.dll"}, {"DOTween.dll"},                 {"Micosmo.SensorToolkit.dll"},
                        {"Unity.Services.Vivox.dll"},  {"UnityEngine.UIModule.dll"},     {"Unity.TextMeshPro.dll"},
                        {"Unity.InputSystem.dll"},     {"Photon3Unity3D.dll"},           {"DoozyRuntime.dll"},
                        {"com.rlabrecque.steamworks.net.dll"}};
void* g_assemblies[] = {&g_images[0], &g_images[1], &g_images[2], &g_images[3],  &g_images[4],  &g_images[5],
                        &g_images[6], &g_images[7], &g_images[8], &g_images[9], &g_images[10], &g_images[11],
                        &g_images[12], &g_images[13], &g_images[14], &g_images[15], &g_images[16], &g_images[17],
                        &g_images[18]};
int g_domain = 0;

// A module the size of its headers' SizeOfImage, with no function table.
struct FakeModule {
  alignas(16) uint8_t bytes[0x400] = {};
  FakeModule() {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage = sizeof bytes;
  }
} g_module;

FakeImage* image_of(const void* p) { return static_cast<FakeImage*>(const_cast<void*>(p)); }

void reset() {
  clockMs = 0;
  tickPerClass = 0;
  stopAfter = -1;
  stopRequested = false;
  waits = 0;
  stopAtWait = -1;
  classesRead = apiRequests = 0;
  icallAnswer = nullptr;
  icallsAsked.clear();
  messages.clear();
  pm::gamemap::forget_published();
}

bool logged(const char* text) {
  for (const std::string& m : messages)
    if (m.find(text) != std::string::npos) return true;
  return false;
}

int count_logged(const char* text) {
  int n = 0;
  for (const std::string& m : messages) n += m.find(text) != std::string::npos;
  return n;
}

// Every map entry has a result, and none resolved.
bool all_unresolved(const char* detail) {
  for (const EntryDesc* e : pm::map::kAllEntries) {
    const Result* r = pm::gamemap::result(e->key);
    if (!r || r->status == Status::Resolved) return false;
    if (detail && r->detail.find(detail) == std::string::npos) return false;
  }
  return true;
}

void* module() { return g_module.bytes; }

void builds_and_resolves_within_budget() {
  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(classesRead == 3800);
  CHECK(all_unresolved(nullptr) && pm::gamemap::result("GhostAI")->status == Status::Missing);
  CHECK(pm::gamemap::entry("GhostAI") == nullptr);  // unresolved: mods receive nothing
  CHECK(!logged("abandoned"));
  CHECK(logged(std::format("game map: 0/{} resolved in 0 ms", std::size(pm::map::kAllEntries)).c_str()));
}

void stop_during_build_abandons_promptly() {
  reset();
  stopAfter = 10;
  CHECK(!pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(classesRead == 10);
  CHECK(all_unresolved("startup was cancelled"));
  CHECK(count_logged("abandoned") == 1);
  CHECK(logged(std::format("game map: 0/{} resolved", std::size(pm::map::kAllEntries)).c_str()));
}

void spent_budget_reads_nothing() {
  reset();
  clockMs = 50001;
  CHECK(!pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(apiRequests == 0 && classesRead == 0);
  CHECK(all_unresolved("the startup budget ran out"));
  CHECK(count_logged("abandoned") == 1);
}

// The search for the native sites reads all of UnityPlayer's .text, so it checks for a stop first:
// a stop found by that last check, after UnityPlayer was looked at, still resolves nothing.
void stop_before_the_native_sites_resolves_nothing() {
  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  const int checks = waits;
  reset();
  stopAtWait = checks;
  CHECK(!pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(classesRead == 3800);
  CHECK(logged("the loaded UnityPlayer cannot be read"));  // the check after UnityPlayer was looked at
  CHECK(all_unresolved("startup was cancelled"));
}

void slow_build_leaves_the_hooks_budget() {
  reset();
  tickPerClass = 100;  // 3800 classes would take 380 s
  CHECK(!pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  // It stops at the first check with less than 10 s left, so the hooks keep their share.
  CHECK(clockMs >= 50000 && clockMs < 50100);
  CHECK(all_unresolved("the startup budget ran out"));
}

// What mods receive through the host contract: each resolved entry's runtime class, method and
// field, its code and twins as addresses in the loaded module, its offset or value; nothing for an
// entry that did not resolve or that the map lacks.
void entry_carries_the_resolved_result() {
  int klass = 0, methodInfo = 0, fieldInfo = 0;
  ModelType type;
  type.runtime = &klass;
  type.methods.push_back({});
  type.methods[0].runtime = &methodInfo;
  type.fields.push_back({});
  type.fields[0].runtime = &fieldInfo;
  pm::gamemap::g_model = {};
  pm::gamemap::g_model.types.push_back(type);
  Result ghost{.desc = &pm::map::GhostAI, .status = Status::Resolved, .type = 0};
  Result unAppear{.desc = &pm::map::GhostAI_UnAppear, .status = Status::Resolved, .type = 0, .member = 0,
                  .rva = 0x100, .twins = {0x200, 0x300}};
  Result state{.desc = &pm::map::GhostAI_state, .status = Status::Resolved, .type = 0, .member = 0, .offset = 0x40};
  Result hunting{.desc = &pm::map::GhostState_hunting, .status = Status::Resolved, .value = 2};
  Result awake{.desc = &pm::map::GhostAI_Awake, .status = Status::Missing};
  pm::gamemap::g_resolution.results = {ghost, unAppear, state, hunting, awake};
  reset();
  CHECK(pm::gamemap::publish(0x10000));

  const PmGameEntry* g = pm::gamemap::entry("GhostAI");
  CHECK(g && g->kind == PM_ENTRY_TYPE && g->klass == &klass);
  const PmGameEntry* m = pm::gamemap::entry("GhostAI.UnAppear");
  CHECK(m && m->kind == PM_ENTRY_METHOD && m->klass == &klass && m->method == &methodInfo);
  CHECK(m && m->code == reinterpret_cast<void*>(0x10100) && m->twinCount == 2);
  CHECK(m && m->twinCount == 2 && m->twins[0] == reinterpret_cast<void*>(0x10200) &&
        m->twins[1] == reinterpret_cast<void*>(0x10300));
  const PmGameEntry* f = pm::gamemap::entry("GhostAI.state");
  CHECK(f && f->kind == PM_ENTRY_FIELD && f->offset == 0x40 && f->field == &fieldInfo && !f->code);
  const PmGameEntry* v = pm::gamemap::entry("GhostState.hunting");
  CHECK(v && v->kind == PM_ENTRY_ENUM_VALUE && v->value == 2);
  CHECK(pm::gamemap::entry("GhostAI.Awake") == nullptr);
  CHECK(pm::gamemap::entry("No.SuchEntry") == nullptr);
}

// A method result at rva 0 has no code of its own: mods receive no address for it, never the
// module's base, where a call would run the PE header.
// The services look a key up on every call, through an index built with the published entries: it answers as a walk of
// the results would, for every key, a key it lacks, one that did not resolve, and the first of a key listed twice.
void keys_are_found_through_the_index() {
  pm::gamemap::forget_published();
  pm::gamemap::g_model = {};
  Result ghost{.desc = &pm::map::GhostAI, .status = Status::Resolved};
  Result hunting{.desc = &pm::map::GhostState_hunting, .status = Status::Resolved, .value = 2};
  Result missing{.desc = &pm::map::GhostAI_UnAppear, .status = Status::Missing};
  Result again{.desc = &pm::map::GhostState_hunting, .status = Status::Resolved, .value = 9};
  pm::gamemap::g_resolution = {};
  pm::gamemap::g_resolution.results = {ghost, hunting, missing, again};
  CHECK(pm::gamemap::result("GhostAI") == &pm::gamemap::g_resolution.results[0]);  // before publishing too
  CHECK(!pm::gamemap::entry("GhostAI"));
  CHECK(pm::gamemap::publish(0x10000));
  CHECK(pm::gamemap::g_index.size() == 3);
  const PmGameEntry* g = pm::gamemap::entry("GhostAI");
  CHECK(g && g == &pm::gamemap::g_published[0].entry);
  const PmGameEntry* h = pm::gamemap::entry("GhostState.hunting");
  CHECK(h && h->value == 2);
  CHECK(!pm::gamemap::entry("GhostAI.UnAppear"));  // known, and not resolved
  CHECK(pm::gamemap::result("GhostAI.UnAppear") == &pm::gamemap::g_resolution.results[2]);
  CHECK(pm::gamemap::result("GhostState.hunting") == &pm::gamemap::g_resolution.results[1]);
  CHECK(!pm::gamemap::entry("No.SuchKey"));
  CHECK(!pm::gamemap::result("No.SuchKey"));
  CHECK(!pm::gamemap::entry(nullptr));
  // A key made at run time finds its entry by its text, not its address.
  const std::string made = std::string("Ghost") + "AI";
  CHECK(pm::gamemap::entry(made.c_str()) == g);
  pm::gamemap::forget_published();
  CHECK(pm::gamemap::g_index.empty());
}

void entry_without_code_has_no_address() {
  ModelType type;
  type.methods.push_back({});
  pm::gamemap::g_model = {};
  pm::gamemap::g_model.types.push_back(type);
  Result awake{.desc = &pm::map::GhostAI_Awake, .status = Status::Resolved, .type = 0, .member = 0, .rva = 0};
  pm::gamemap::g_resolution.results = {awake};
  reset();
  CHECK(pm::gamemap::publish(0x10000));
  const PmGameEntry* m = pm::gamemap::entry("GhostAI.Awake");
  CHECK(m && m->kind == PM_ENTRY_METHOD && m->code == nullptr);
}

// The worker never asks the runtime for an icall: its table is unlocked and the engine's registration
// is not known to be done. At activation, on the main thread, each icall is resolved and published;
// one the runtime does not answer then is logged as missing, once.
void icall_resolves_at_activation() {
  // Every icall entry of the map, in its order.
  std::vector<std::string> icalls;
  for (const EntryDesc* d : pm::map::kAllEntries)
    if (d->kind == Kind::Icall) icalls.emplace_back(d->name);
  const std::string count = std::to_string(icalls.size());
  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(icallsAsked.empty());
  CHECK(pm::gamemap::entry("Animator.SetBoolID") == nullptr);
  CHECK(!logged("Animator.SetBoolID"));  // not final yet

  int setBoolID = 0;
  icallAnswer = &setBoolID;
  pm::gamemap::resolve_icalls();
  CHECK(icallsAsked == icalls);
  const PmGameEntry* e = pm::gamemap::entry("Animator.SetBoolID");
  CHECK(e && e->kind == PM_ENTRY_ICALL && e->code == &setBoolID && !e->method);
  CHECK(pm::gamemap::result("Animator.SetBoolID")->status == Status::Resolved);
  CHECK(logged(("game map: " + count + "/" + count + " icalls resolved on the main thread").c_str()));

  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  pm::gamemap::resolve_icalls();  // the runtime does not answer
  CHECK(pm::gamemap::entry("Animator.SetBoolID") == nullptr);
  CHECK(count_logged("game map: MISSING Animator.SetBoolID") == 1);
  CHECK(logged(("game map: 0/" + count + " icalls resolved on the main thread").c_str()));
}

// What the fake runtime makes of an instance, and what it was asked for.
pm::sdk::InflatedMethod inflateAnswer;
bool inflateWorks = true, inflateThrows = false, askedOfMethod = false;
std::vector<void*> inflateArgs;
bool fake_inflate(void*, const void*, bool ofMethod, void* const* args, size_t count, pm::sdk::InflatedMethod& out) {
  askedOfMethod = ofMethod;
  inflateArgs.assign(args, args + count);
  if (inflateThrows) throw Il2CppExceptionWrapper{nullptr};
  out = inflateAnswer;
  return inflateWorks;
}

// An instance is made on the main thread and published as a method; one without code stays unresolved, whatever the
// runtime hands back for it: its stub and its invoker that raises. Nothing else about it is trusted.
void inflated_resolves_at_activation() {
  constexpr uint32_t kCode = 0x1000, kInvoker = 0x1100, kMissingInvoker = 0x1200;
  ImageBuilder b;
  const uint8_t code[] = {0x48, 0x8B, 0xC1, 0xC3};
  const uint8_t invoker[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3};
  const uint8_t missing[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xCA, 0x33, 0xD2, 0xE8, 0x32, 0x02, 0x00, 0x00, 0xCC};
  b.function(kCode, code);
  b.function(kInvoker, invoker);
  b.function(kMissingInvoker, missing);
  std::vector<uint8_t> image = b.build();
  const auto at = [&](uint32_t rva) { return static_cast<const void*>(image.data() + rva); };

  int definitionClass = 0, definitionMethod = 0, argClass = 0, inflatedClass = 0, inflatedMethod = 0;
  const auto prepare = [&](const EntryDesc& desc) {
    ModelType owner;
    owner.runtime = &definitionClass;
    ModelMethod m;
    m.runtime = &definitionMethod;
    owner.methods.push_back(m);
    ModelType arg;
    arg.runtime = &argClass;
    pm::gamemap::g_model = {};
    pm::gamemap::g_model.types = {owner, arg};
    Result r{.desc = &desc, .status = Status::Missing, .detail = std::string(kCheckedAtStartup), .type = 0, .member = 0};
    r.typeArgs = {1};
    pm::gamemap::g_resolution.results = {r};
    reset();
    pm::gamemap::g_inflate = &fake_inflate;
    inflateWorks = true;
    inflateThrows = false;
    inflateAnswer = {&inflatedMethod, at(kCode), at(kInvoker), &inflatedClass};
    CHECK(pm::gamemap::publish(reinterpret_cast<uintptr_t>(image.data())));
    CHECK(pm::gamemap::entry(desc.key) == nullptr);  // not before the main thread asks
  };
  const EntryDesc& ofClass = pm::map::InputControl_Vector2_ReadValue;
  const EntryDesc& ofMethod = pm::map::Sensor_GetDetectedComponents;

  prepare(ofClass);
  pm::gamemap::resolve_inflated();
  const PmGameEntry* e = pm::gamemap::entry(ofClass.key);
  CHECK(e && e->kind == PM_ENTRY_METHOD && e->code == at(kCode) && e->method == &inflatedMethod && e->klass == &inflatedClass);
  CHECK(pm::gamemap::result(ofClass.key)->status == Status::Resolved);
  CHECK(!askedOfMethod && inflateArgs == std::vector<void*>{&argClass});
  CHECK(logged("game map: 1/1 generic instances resolved on the main thread"));

  prepare(ofMethod);
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofMethod.key) && askedOfMethod);

  // A missing instance: the runtime hands back a stub and an invoker that never returns.
  prepare(ofClass);
  inflateAnswer.invoker = at(kMissingInvoker);
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr);
  CHECK(pm::gamemap::result(ofClass.key)->status == Status::Missing);
  CHECK(!checked_at_startup(*pm::gamemap::result(ofClass.key)));
  CHECK(logged("this game build has no code for the instance"));
  CHECK(logged("game map: 0/1 generic instances resolved on the main thread"));

  prepare(ofClass);  // code or an invoker outside the module, or none
  inflateAnswer.code = &definitionClass;
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr);
  prepare(ofClass);
  inflateAnswer.invoker = nullptr;
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr);

  prepare(ofClass);  // the runtime will not make it
  inflateWorks = false;
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr && logged("the runtime would not make the instance"));

  prepare(ofClass);  // or throws: never out of the host's activation
  inflateThrows = true;
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr && logged("managed exception while making a generic instance"));

  pm::gamemap::g_inflate = nullptr;
  prepare(ofClass);  // a runtime that gave no way to make one
  pm::gamemap::g_inflate = nullptr;
  pm::gamemap::resolve_inflated();
  CHECK(pm::gamemap::entry(ofClass.key) == nullptr);
  pm::gamemap::g_model = {};
  pm::gamemap::g_resolution = {};
  reset();
}

// An export's address is in the loaded module; a static field carries its FieldInfo like any field.
void exports_and_statics_are_published() {
  static constexpr EntryDesc kExport{.key = "GameAssembly.il2cpp_init", .kind = Kind::Export, .name = "il2cpp_init"};
  int klass = 0, fieldInfo = 0;
  ModelType type;
  type.runtime = &klass;
  type.fields.push_back({});
  type.fields[0].runtime = &fieldInfo;
  type.fields[0].isStatic = true;
  pm::gamemap::g_model = {};
  pm::gamemap::g_model.types.push_back(type);
  Result exported{.desc = &kExport, .status = Status::Resolved, .rva = 0x3844C0};
  Result instance{.desc = &pm::map::LevelController_instance, .status = Status::Resolved, .type = 0, .member = 0};
  pm::gamemap::g_resolution.results = {exported, instance};
  reset();
  CHECK(pm::gamemap::publish(0x10000000));

  const PmGameEntry* x = pm::gamemap::entry("GameAssembly.il2cpp_init");
  CHECK(x && x->kind == PM_ENTRY_EXPORT && x->code == reinterpret_cast<void*>(0x103844C0));
  CHECK(pm::api::host_only::game_entry("GameAssembly.il2cpp_init") == x);  // the host's own code sees it
  CHECK(pm::gamemap::mod_entry("GameAssembly.il2cpp_init") == nullptr);  // mods never see an export
  const PmGameEntry* s = pm::gamemap::entry("LevelController.instance");
  CHECK(s && s->kind == PM_ENTRY_FIELD && s->field == &fieldInfo && s->klass == &klass && !s->code);
  CHECK(pm::gamemap::mod_entry("LevelController.instance") == s);

  pm::gamemap::g_resolution.results = {};
  reset();
  CHECK(pm::gamemap::publish(0x10000000));
  CHECK(pm::api::host_only::game_entry("GameAssembly.il2cpp_init") == nullptr);  // unresolved: nothing
}

// A field within an embedded struct (GhostInfo.identityType, inside GhostInfo's GhostTraits) is
// published at its place in the class, with no FieldInfo of its own, and the game API's Field, bound
// to what the host contract gives mods, reads and writes the class's object there.
void within_field_is_published_and_read() {
  int klass = 0;
  ModelType type;
  type.runtime = &klass;
  pm::gamemap::g_model = {};
  pm::gamemap::g_model.types.push_back(type);
  Result identity{.desc = &pm::map::GhostInfo_identityType, .status = Status::Resolved, .type = 0, .member = -1,
                  .offset = 0x28};
  pm::gamemap::g_resolution.results = {identity};
  reset();
  CHECK(pm::gamemap::publish(0x10000));
  const PmGameEntry* e = pm::gamemap::mod_entry("GhostInfo.identityType");
  CHECK(e && e->kind == PM_ENTRY_FIELD && e->offset == 0x28 && e->field == nullptr && e->klass == &klass);

  PmHostApi host{};
  host.version = PMMOD_API_VERSION;
  host.game_entry = &pm::gamemap::mod_entry;
  std::string missing;
  pm::api::bind(&host, missing);
  alignas(8) unsigned char info[0x40] = {};
  const int32_t custom = 31;
  std::memcpy(info + 0x28, &custom, sizeof custom);
  CHECK(phasmo::GhostInfo::identityType.get(info) == 31);
  phasmo::GhostInfo::identityType.set(info, 20);
  int32_t stored = 0;
  std::memcpy(&stored, info + 0x28, sizeof stored);
  CHECK(stored == 20);
  pm::api::bind(nullptr, missing);
}

// Mods keep the published entries, and their twins, for the process lifetime. A second resolve or
// publish is refused and logged: every entry stays where and as it was published.
void publish_twice_is_refused() {
  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  int setBoolID = 0;
  icallAnswer = &setBoolID;
  pm::gamemap::resolve_icalls();
  const PmGameEntry* const kept = pm::gamemap::entry("Animator.SetBoolID");
  CHECK(kept && kept->code == &setBoolID);

  messages.clear();
  icallAnswer = nullptr;
  CHECK(!pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(count_logged("game map: already resolved; a second resolve is refused") == 1);
  CHECK(classesRead == 3800);  // nothing was read again
  CHECK(!pm::gamemap::publish(0x20000));
  CHECK(count_logged("game map: already published; a second publish is refused") == 1);
  CHECK(pm::gamemap::entry("Animator.SetBoolID") == kept && kept->code == &setBoolID);
}

// A scene engine whose NameToLayer knows one layer, `g_layerName` at 11, and whose PropertyToID gives a
// property its name's length plus 100.
const char* g_layerName = "PlayerView";
const pm::sdk::SceneEngine kEngine{
    [] { return int32_t{1}; },
    [](void*, const char*, const char**) { return static_cast<void*>(nullptr); },
    [](const char* name, const char**) { return std::string(name) == g_layerName ? int32_t{11} : int32_t{-1}; },
    [](const void*, const char*, int32_t*, const char**) { return false; },
    [](const void* obj) { return obj != nullptr; },
    [](void*) { return uint32_t{0}; },
    [](uint32_t) {},
    [](const char* name, int32_t* id, const char**) {
      *id = static_cast<int32_t>(100 + std::strlen(name));
      return true;
    },
};

// Marks the entries `keys` resolved, as if the runtime had them.
void mark_resolved(std::initializer_list<const char*> keys) {
  for (Result& r : pm::gamemap::g_resolution.results)
    for (const char* key : keys)
      if (std::string(r.desc->key) == key) {
        r.status = Status::Resolved;
        r.detail.clear();
      }
}

// The worker leaves the scene facts to the main thread, and does not log them as failures. At
// activation a layer resolves by its name, a shader property to its name's id, and a well-formed Scene
// entry is published to be checked where it is used; a layer no layer has is logged once and stays
// unresolved, and a shader property stays unresolved without Shader.PropertyToID.
void scene_facts_resolve_at_activation() {
  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  CHECK(pm::gamemap::entry("Layer.PlayerView") == nullptr && pm::gamemap::entry("Scene.JournalBookContent") == nullptr);
  CHECK(!logged("Layer.PlayerView") && !logged("Scene.JournalBookContent"));

  mark_resolved({"Transform", "Transform.Find", "SceneManager.GetActiveScene", "LayerMask", "LayerMask.NameToLayer",
                 "Shader.PropertyToID"});
  g_layerName = "PlayerView";
  pm::gamemap::resolve_scene_facts(&kEngine);
  const PmGameEntry* layer = pm::gamemap::entry("Layer.PlayerView");
  CHECK(layer && layer->kind == PM_ENTRY_LAYER && layer->value == 11);
  const PmGameEntry* scene = pm::gamemap::entry("Scene.JournalBookContent");
  CHECK(scene && scene->kind == PM_ENTRY_SCENE && pm::gamemap::mod_entry("Scene.JournalBookContent") == scene);
  const PmGameEntry* tint = pm::gamemap::entry("ShaderProperty.CandleFlame.TintColour");
  CHECK(tint && tint->kind == PM_ENTRY_SHADER_PROPERTY && tint->value == 100 + 11);  // "_TintColour"
  CHECK(pm::gamemap::mod_entry("ShaderProperty.CandleFlame.TintColour") == tint);
  CHECK(logged("game map: 1/3 layers and 5/5 shader properties resolved on the main thread"));
  int root = 0;
  CHECK(pm::gamemap::scene_find("Scene.JournalBookContent", &root) == nullptr);  // the engine finds nothing
  CHECK(pm::gamemap::scene_find("Layer.PlayerView", &root) == nullptr);          // not a Scene entry
  int32_t hash = 5;
  CHECK(!pm::gamemap::animator_param("Scene.JournalBookContent", &root, &hash) && hash == 5);

  reset();
  CHECK(pm::gamemap::resolve(module(), nullptr, nullptr, 60000));
  mark_resolved({"LayerMask", "LayerMask.NameToLayer"});
  g_layerName = "Renamed";
  pm::gamemap::resolve_scene_facts(&kEngine);
  CHECK(pm::gamemap::entry("Layer.PlayerView") == nullptr);
  CHECK(count_logged("game map: Layer.PlayerView: no layer is named PlayerView") == 1);
  CHECK(pm::gamemap::entry("ShaderProperty.CandleFlame.TintColour") == nullptr);
  CHECK(pm::gamemap::result("ShaderProperty.CandleFlame.TintColour")->detail == "Shader.PropertyToID unresolved");
  CHECK(logged("game map: 0/3 layers and 0/5 shader properties resolved on the main thread"));
}

// An optional AnimatorParam entry for the dependency test: the map has none yet.
inline constexpr EntryDesc kTestParam{.key = "Test.isOpened", .kind = pm::sdk::Kind::AnimatorParam,
                                      .flags = pm::sdk::kOptional, .name = "isOpened",
                                      .scene = "Resources/PCPlayer", .component = "BookController"};

// A pending scene fact, as the worker leaves it.
Result pending(const EntryDesc& d) { return {.desc = &d, .status = Status::Missing, .detail = std::string(kCheckedInScene)}; }
Result resolved(const EntryDesc& d) { return {.desc = &d, .status = Status::Resolved}; }

// Publishes `results`, then resolves the scene facts among them.
void activate(std::vector<Result> results) {
  pm::gamemap::g_model = {};
  pm::gamemap::g_resolution.results = std::move(results);
  reset();
  CHECK(pm::gamemap::publish(0x10000));
  pm::gamemap::resolve_scene_facts(&kEngine);
}

// A scene fact fails closed: it is published only when every engine entry the host needs to check it
// resolved, and otherwise stays missing, naming the first one that did not, logged once.
void scene_facts_need_their_engine_entries() {
  using namespace pm::map;
  activate({pending(Scene_JournalBookContent), resolved(Transform), resolved(Transform_Find),
            pending(SceneManager_GetActiveScene)});
  CHECK(pm::gamemap::entry("Scene.JournalBookContent") == nullptr);
  CHECK(pm::gamemap::result("Scene.JournalBookContent")->detail == "SceneManager.GetActiveScene unresolved");
  CHECK(count_logged("game map: MISSING Scene.JournalBookContent (SceneManager.GetActiveScene unresolved)") == 1);
  activate({pending(Scene_JournalBookContent), resolved(Transform), resolved(Transform_Find),
            resolved(SceneManager_GetActiveScene)});
  CHECK(pm::gamemap::entry("Scene.JournalBookContent") != nullptr);

  activate({pending(kTestParam), resolved(Animator), resolved(Animator_get_parameters),
            resolved(AnimatorControllerParameter), resolved(AnimatorControllerParameter_m_Name)});
  CHECK(pm::gamemap::entry("Test.isOpened") == nullptr);
  CHECK(count_logged("game map: MISSING Test.isOpened (Animator.StringToHash unresolved)") == 1);
  activate({pending(kTestParam), resolved(Animator), resolved(Animator_get_parameters), resolved(Animator_StringToHash),
            resolved(AnimatorControllerParameter), resolved(AnimatorControllerParameter_m_Name)});
  const PmGameEntry* param = pm::gamemap::entry("Test.isOpened");
  CHECK(param && param->kind == PM_ENTRY_ANIMATOR_PARAM);

  activate({pending(Layer_PlayerView), resolved(LayerMask)});
  CHECK(pm::gamemap::entry("Layer.PlayerView") == nullptr);
  CHECK(count_logged("game map: MISSING Layer.PlayerView (LayerMask.NameToLayer unresolved)") == 1);
}

// A pattern's bytes, a wildcard as 0.
std::vector<uint8_t> bytes_of(std::string_view pattern) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < pattern.size(); i += 3)
    out.push_back(pattern[i] == '?' ? 0 : static_cast<uint8_t>(std::stoi(std::string(pattern.substr(i, 2)), nullptr, 16)));
  return out;
}

// Native sites resolve over the loaded UnityPlayer module, and are published as addresses in it.
// The module is mapped as a loaded one is, its code readable and executable. Without a UnityPlayer,
// or with one whose code is not readable code (here a heap copy, which is not executable), every
// native site stays missing, saying so.
void native_sites_resolve_over_unity_player() {
  ImageBuilder b;
  b.function(0x2000, bytes_of(pm::map::UnityPlayer_SteerSite.pattern));
  b.function(0x3000, bytes_of(pm::map::UnityPlayer_GetInternalAgent.pattern));
  const std::vector<uint8_t> image = b.build();
  auto* const loaded =
      static_cast<uint8_t*>(VirtualAlloc(nullptr, image.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  CHECK(loaded != nullptr);
  if (!loaded) return;
  std::memcpy(loaded, image.data(), image.size());
  DWORD old = 0;
  CHECK(VirtualProtect(loaded, image.size(), PAGE_EXECUTE_READ, &old));
  reset();
  CHECK(pm::gamemap::resolve(module(), loaded, nullptr, 60000));
  const PmGameEntry* steer = pm::gamemap::entry("UnityPlayer.SteerSite");
  CHECK(steer && steer->kind == PM_ENTRY_NATIVE_SITE && steer->code == loaded + 0x2000);
  CHECK(pm::api::host_only::game_entry("UnityPlayer.SteerSite") == steer);
  const PmGameEntry* agent = pm::gamemap::entry("UnityPlayer.NavMeshAgent.GetInternalAgent");
  CHECK(agent && agent->kind == PM_ENTRY_NATIVE_SITE && agent->code == loaded + 0x3000);
  // Only the host acts on a native site: the host contract never gives one to a mod, as with an export.
  CHECK(pm::gamemap::mod_entry("UnityPlayer.SteerSite") == nullptr);
  CHECK(pm::gamemap::mod_entry("UnityPlayer.NavMeshAgent.GetInternalAgent") == nullptr);
  CHECK(logged(std::format("game map: 2/{} resolved", std::size(pm::map::kAllEntries)).c_str()));

  void* const unreadable[] = {nullptr, const_cast<uint8_t*>(image.data())};
  for (void* unityPlayer : unreadable) {
    reset();
    CHECK(pm::gamemap::resolve(module(), unityPlayer, nullptr, 60000));
    CHECK(pm::gamemap::entry("UnityPlayer.SteerSite") == nullptr);
    CHECK(logged("game map: MISSING UnityPlayer.SteerSite (no UnityPlayer image)"));
    CHECK(logged("game map: the loaded UnityPlayer cannot be read; every native site stays missing"));
  }
  VirtualFree(loaded, 0, MEM_RELEASE);
}

}  // namespace

namespace il2cpp {
bool runtime_api(void*, pm::sdk::RuntimeApi& api) {
  ++apiRequests;
  api.domain_get = [] { return static_cast<void*>(&g_domain); };
  api.domain_get_assemblies = [](void*, size_t* count) {
    *count = sizeof g_assemblies / sizeof g_assemblies[0];
    return g_assemblies;
  };
  api.assembly_get_image = [](void* a) { return a; };
  api.image_get_name = [](void* i) { return image_of(i)->name.c_str(); };
  api.image_get_class_count = [](const void*) { return size_t{200}; };
  api.image_get_class = [](const void* i, size_t k) { return static_cast<void*>(&image_of(i)->classes[k]); };
  api.class_get_name = [](void*) {
    ++classesRead;
    clockMs += tickPerClass;
    if (classesRead == stopAfter) stopRequested = true;
    return "Empty";
  };
  api.class_get_namespace = [](void*) { return ""; };
  api.class_get_declaring_type = [](void*) { return static_cast<void*>(nullptr); };
  api.class_is_enum = [](const void*) { return false; };
  api.class_is_valuetype = [](const void*) { return false; };
  api.class_enum_basetype = [](void*) { return static_cast<const void*>(nullptr); };
  api.class_from_type = [](const void*) { return static_cast<void*>(nullptr); };
  api.class_get_parent = [](void*) { return static_cast<void*>(nullptr); };
  api.class_get_interfaces = [](void*, void**) { return static_cast<void*>(nullptr); };
  api.class_is_interface = [](const void*) { return false; };
  api.class_get_fields = [](void*, void**) { return static_cast<void*>(nullptr); };
  api.class_get_methods = [](void*, void**) { return static_cast<const void*>(nullptr); };
  api.method_get_name = [](const void*) { return ""; };
  api.method_get_param_count = [](const void*) { return 0u; };
  api.method_get_param = [](const void*, uint32_t) { return static_cast<const void*>(nullptr); };
  api.method_get_return_type = [](const void*) { return static_cast<const void*>(nullptr); };
  api.method_get_flags = [](const void*, uint32_t*) { return 0u; };
  api.field_get_name = [](void*) { return ""; };
  api.field_get_offset = [](void*) { return size_t{0}; };
  api.field_get_flags = [](void*) { return 0; };
  api.field_get_type = [](void*) { return static_cast<const void*>(nullptr); };
  api.field_static_get_value = [](void*, void*) {};
  api.type_get_type = [](const void*) { return 0; };
  api.type_get_name = [](const void*) { return static_cast<char*>(nullptr); };
  api.free_memory = [](void*) {};
  api.inflate = [](void*, const void*, bool, void* const*, size_t, pm::sdk::InflatedMethod&) { return false; };
  api.resolve_icall = [](const char* name) {
    icallsAsked.emplace_back(name);
    return icallAnswer;
  };
  return true;
}
void log_exception(const char* where, const void*) { messages.emplace_back(where); }
}  // namespace il2cpp

namespace pm::log {
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof line, format, args);
  va_end(args);
  messages.emplace_back(line);
}
}  // namespace pm::log

int main() {
  builds_and_resolves_within_budget();
  stop_during_build_abandons_promptly();
  stop_before_the_native_sites_resolves_nothing();
  spent_budget_reads_nothing();
  slow_build_leaves_the_hooks_budget();
  entry_carries_the_resolved_result();
  keys_are_found_through_the_index();
  entry_without_code_has_no_address();
  inflated_resolves_at_activation();
  icall_resolves_at_activation();
  exports_and_statics_are_published();
  within_field_is_published_and_read();
  publish_twice_is_refused();
  scene_facts_resolve_at_activation();
  scene_facts_need_their_engine_entries();
  native_sites_resolve_over_unity_player();
  return g_failures ? 1 : 0;
}
