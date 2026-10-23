// The host's game map resolution against a fake runtime: it stays inside its share of the startup
// deadline and stops when startup is cancelled, resolves icalls through the runtime, and publishes
// each kind of entry as mods (and, for exports, the host) receive it.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "check.h"
#include "game_api.h"

namespace {
unsigned long long clockMs = 0;
unsigned long long tickPerClass = 0;  // how far the clock moves for each class read
int stopAfter = -1;                   // the class read after which a stop is requested, -1 never
bool stopRequested = false;
int classesRead = 0;
int apiRequests = 0;
void* icallAnswer = nullptr;          // what the runtime's il2cpp_resolve_icall answers
std::vector<std::string> icallsAsked;
std::vector<std::string> messages;

ULONGLONG WINAPI tick_count() { return clockMs; }
DWORD WINAPI wait_event(HANDLE, DWORD) { return stopRequested ? WAIT_OBJECT_0 : WAIT_TIMEOUT; }
}  // namespace

#define GetTickCount64 tick_count
#define WaitForSingleObject wait_event
#include "../host/game_map.cpp"
#undef GetTickCount64
#undef WaitForSingleObject

namespace {

using namespace pg::sdk;

// Two images of 200 empty classes each, named as the map's assemblies.
struct FakeImage {
  std::string name;
  int classes[200] = {};
};
FakeImage g_images[] = {{"Assembly-CSharp.dll"}, {"UnityEngine.CoreModule.dll"}};
void* g_assemblies[] = {&g_images[0], &g_images[1]};
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
  classesRead = apiRequests = 0;
  icallAnswer = nullptr;
  icallsAsked.clear();
  messages.clear();
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
  for (const EntryDesc* e : pg::map::kAllEntries) {
    const Result* r = pg::gamemap::result(e->key);
    if (!r || r->status == Status::Resolved) return false;
    if (detail && r->detail.find(detail) == std::string::npos) return false;
  }
  return true;
}

void* module() { return g_module.bytes; }

void builds_and_resolves_within_budget() {
  reset();
  CHECK(pg::gamemap::resolve(module(), nullptr, 60000));
  CHECK(classesRead == 400);
  CHECK(all_unresolved(nullptr) && pg::gamemap::result("GhostAI")->status == Status::Missing);
  CHECK(pg::gamemap::entry("GhostAI") == nullptr);  // unresolved: mods receive nothing
  CHECK(!logged("abandoned"));
  CHECK(logged("game map: 0/27 resolved in 0 ms"));
}

void stop_during_build_abandons_promptly() {
  reset();
  stopAfter = 10;
  CHECK(!pg::gamemap::resolve(module(), nullptr, 60000));
  CHECK(classesRead == 10);
  CHECK(all_unresolved("startup was cancelled"));
  CHECK(count_logged("abandoned") == 1);
  CHECK(logged("game map: 0/27 resolved"));
}

void spent_budget_reads_nothing() {
  reset();
  clockMs = 50001;
  CHECK(!pg::gamemap::resolve(module(), nullptr, 60000));
  CHECK(apiRequests == 0 && classesRead == 0);
  CHECK(all_unresolved("the startup budget ran out"));
  CHECK(count_logged("abandoned") == 1);
}

void slow_build_leaves_the_hooks_budget() {
  reset();
  tickPerClass = 150;  // 400 classes would take 60 s
  CHECK(!pg::gamemap::resolve(module(), nullptr, 60000));
  // It stops at the first check with less than 10 s left, so the hooks keep their share.
  CHECK(clockMs >= 50000 && clockMs < 50150);
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
  pg::gamemap::g_model = {};
  pg::gamemap::g_model.types.push_back(type);
  Result ghost{.desc = &pg::map::GhostAI, .status = Status::Resolved, .type = 0};
  Result unAppear{.desc = &pg::map::GhostAI_UnAppear, .status = Status::Resolved, .type = 0, .member = 0,
                  .rva = 0x100, .twins = {0x200, 0x300}};
  Result state{.desc = &pg::map::GhostAI_state, .status = Status::Resolved, .type = 0, .member = 0, .offset = 0x40};
  Result hunting{.desc = &pg::map::GhostState_hunting, .status = Status::Resolved, .value = 2};
  Result awake{.desc = &pg::map::GhostAI_Awake, .status = Status::Missing};
  pg::gamemap::g_resolution.results = {ghost, unAppear, state, hunting, awake};
  pg::gamemap::publish(0x10000);

  const PgGameEntry* g = pg::gamemap::entry("GhostAI");
  CHECK(g && g->kind == PG_ENTRY_TYPE && g->klass == &klass);
  const PgGameEntry* m = pg::gamemap::entry("GhostAI.UnAppear");
  CHECK(m && m->kind == PG_ENTRY_METHOD && m->klass == &klass && m->method == &methodInfo);
  CHECK(m && m->code == reinterpret_cast<void*>(0x10100) && m->twinCount == 2);
  CHECK(m && m->twinCount == 2 && m->twins[0] == reinterpret_cast<void*>(0x10200) &&
        m->twins[1] == reinterpret_cast<void*>(0x10300));
  const PgGameEntry* f = pg::gamemap::entry("GhostAI.state");
  CHECK(f && f->kind == PG_ENTRY_FIELD && f->offset == 0x40 && f->field == &fieldInfo && !f->code);
  const PgGameEntry* v = pg::gamemap::entry("GhostState.hunting");
  CHECK(v && v->kind == PG_ENTRY_ENUM_VALUE && v->value == 2);
  CHECK(pg::gamemap::entry("GhostAI.Awake") == nullptr);
  CHECK(pg::gamemap::entry("No.SuchEntry") == nullptr);
}

// A method result at rva 0 has no code of its own: mods receive no address for it, never the
// module's base, where a call would run the PE header.
void entry_without_code_has_no_address() {
  ModelType type;
  type.methods.push_back({});
  pg::gamemap::g_model = {};
  pg::gamemap::g_model.types.push_back(type);
  Result awake{.desc = &pg::map::GhostAI_Awake, .status = Status::Resolved, .type = 0, .member = 0, .rva = 0};
  pg::gamemap::g_resolution.results = {awake};
  pg::gamemap::publish(0x10000);
  const PgGameEntry* m = pg::gamemap::entry("GhostAI.Awake");
  CHECK(m && m->kind == PG_ENTRY_METHOD && m->code == nullptr);
}

// The worker never asks the runtime for an icall: its table is unlocked and the engine's registration
// is not known to be done. At activation, on the main thread, each icall is resolved and published;
// one the runtime does not answer then is logged as missing, once.
void icall_resolves_at_activation() {
  reset();
  CHECK(pg::gamemap::resolve(module(), nullptr, 60000));
  CHECK(icallsAsked.empty());
  CHECK(pg::gamemap::entry("Animator.SetBoolID") == nullptr);
  CHECK(!logged("Animator.SetBoolID"));  // not final yet

  int setBoolID = 0;
  icallAnswer = &setBoolID;
  pg::gamemap::resolve_icalls();
  CHECK((icallsAsked == std::vector<std::string>{"UnityEngine.Animator::SetBoolID"}));
  const PgGameEntry* e = pg::gamemap::entry("Animator.SetBoolID");
  CHECK(e && e->kind == PG_ENTRY_ICALL && e->code == &setBoolID && !e->method);
  CHECK(pg::gamemap::result("Animator.SetBoolID")->status == Status::Resolved);
  CHECK(logged("game map: 1/1 icalls resolved on the main thread"));

  reset();
  CHECK(pg::gamemap::resolve(module(), nullptr, 60000));
  pg::gamemap::resolve_icalls();  // the runtime does not answer
  CHECK(pg::gamemap::entry("Animator.SetBoolID") == nullptr);
  CHECK(count_logged("game map: MISSING Animator.SetBoolID") == 1);
  CHECK(logged("game map: 0/1 icalls resolved on the main thread"));
}

// An export's address is in the loaded module; a static field carries its FieldInfo like any field.
// The host reaches an export through pg::api::Export, which exists only in host code.
void exports_and_statics_are_published() {
  int klass = 0, fieldInfo = 0;
  ModelType type;
  type.runtime = &klass;
  type.fields.push_back({});
  type.fields[0].runtime = &fieldInfo;
  type.fields[0].isStatic = true;
  pg::gamemap::g_model = {};
  pg::gamemap::g_model.types.push_back(type);
  Result shutdown{.desc = &pg::map::GameAssembly_il2cpp_shutdown, .status = Status::Resolved, .rva = 0x3844C0};
  Result instance{.desc = &pg::map::LevelController_instance, .status = Status::Resolved, .type = 0, .member = 0};
  pg::gamemap::g_resolution.results = {shutdown, instance};
  pg::gamemap::publish(0x10000000);

  const PgGameEntry* x = pg::gamemap::entry("GameAssembly.il2cpp_shutdown");
  CHECK(x && x->kind == PG_ENTRY_EXPORT && x->code == reinterpret_cast<void*>(0x103844C0));
  constexpr pg::api::Export<pg::map::GameAssembly_il2cpp_shutdown> shutdownExport{};
  CHECK(shutdownExport.available() && shutdownExport.address() == x->code);
  CHECK(pg::gamemap::mod_entry("GameAssembly.il2cpp_shutdown") == nullptr);  // mods never see an export
  const PgGameEntry* s = pg::gamemap::entry("LevelController.instance");
  CHECK(s && s->kind == PG_ENTRY_FIELD && s->field == &fieldInfo && s->klass == &klass && !s->code);
  CHECK(pg::gamemap::mod_entry("LevelController.instance") == s);

  pg::gamemap::g_resolution.results = {};
  pg::gamemap::publish(0x10000000);
  CHECK(!shutdownExport.available() && shutdownExport.address() == nullptr);
}

}  // namespace

namespace il2cpp {
bool runtime_api(void*, pg::sdk::RuntimeApi& api) {
  ++apiRequests;
  api.domain_get = [] { return static_cast<void*>(&g_domain); };
  api.domain_get_assemblies = [](void*, size_t* count) {
    *count = 2;
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
  api.resolve_icall = [](const char* name) {
    icallsAsked.emplace_back(name);
    return icallAnswer;
  };
  return true;
}
void log_exception(const char* where, const void*) { messages.emplace_back(where); }
}  // namespace il2cpp

namespace pg::log {
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof line, format, args);
  va_end(args);
  messages.emplace_back(line);
}
}  // namespace pg::log

int main() {
  builds_and_resolves_within_budget();
  stop_during_build_abandons_promptly();
  spent_budget_reads_nothing();
  slow_build_leaves_the_hooks_budget();
  entry_carries_the_resolved_result();
  entry_without_code_has_no_address();
  icall_resolves_at_activation();
  exports_and_statics_are_published();
  return g_failures ? 1 : 0;
}
