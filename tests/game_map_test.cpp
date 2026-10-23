// The host's game map resolution against a fake runtime: it stays inside its share of the startup
// deadline and stops when startup is cancelled.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "check.h"

namespace {
unsigned long long clockMs = 0;
unsigned long long tickPerClass = 0;  // how far the clock moves for each class read
int stopAfter = -1;                   // the class read after which a stop is requested, -1 never
bool stopRequested = false;
int classesRead = 0;
int apiRequests = 0;
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
  CHECK(!logged("abandoned"));
  CHECK(logged("game map: 0/23 resolved in 0 ms"));
}

void stop_during_build_abandons_promptly() {
  reset();
  stopAfter = 10;
  CHECK(!pg::gamemap::resolve(module(), nullptr, 60000));
  CHECK(classesRead == 10);
  CHECK(all_unresolved("startup was cancelled"));
  CHECK(count_logged("abandoned") == 1);
  CHECK(logged("game map: 0/23 resolved"));
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
  return g_failures ? 1 : 0;
}
