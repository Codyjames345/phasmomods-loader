// The host contract's alive (host/runtime_services.cpp) is the game's own Object.op_Implicit, reached through the map: it
// once went round through pm::api::alive, which asks this very function, and the first alive check of the game overflowed
// the stack. And is_array_of, which reads what another client sent, and delegate_new, the loader's listeners on the
// game's events. Run: build\runtime_services_test.exe
#include <cstring>
#include <string>

#include "../host/il2cpp.cpp"
#include "../host/runtime_services.cpp"
#include "check.h"
#include "entry_hooks.h"
#include "game_map.h"

namespace {
PmGameEntry g_implicit{}, g_byte{};
bool g_mapped = true;
int g_calls = 0;
const void* g_seenObject = nullptr;
const void* g_seenMethod = nullptr;
bool g_answer = true;
int g_method = 0, g_byteClass = 0, g_byteArrayClass = 0, g_otherClass = 0;

bool op_implicit(const void* object, const void* method) {
  ++g_calls;
  g_seenObject = object;
  g_seenMethod = method;
  return g_answer;
}

PmGameEntry* g_ctor = nullptr;  // UnityAction.ctor, when a test gives it
PmGameEntry* g_invoke = nullptr;  // UnityAction.Invoke, when a test gives it
}  // namespace

namespace pm::gamemap {
const PmGameEntry* entry(const char* key) {
  const std::string_view k(key);
  if (k == "UnityAction.ctor") return g_ctor;
  if (k == "UnityAction.Invoke") return g_invoke;
  if (k == map::UnityObject_op_Implicit.key) return g_mapped ? &g_implicit : nullptr;
  return k == "Byte" && g_mapped ? &g_byte : nullptr;
}
const sdk::Result* result(std::string_view) { return nullptr; }
}  // namespace pm::gamemap
namespace pm::context {
bool abandon_ghost_spawn(void (*)()) { return false; }
}  // namespace pm::context

namespace {

void alive_asks_the_game_once() {
  g_implicit.kind = PM_ENTRY_METHOD;
  g_implicit.code = reinterpret_cast<void*>(&op_implicit);
  g_implicit.method = &g_method;
  int object = 0;
  g_answer = true;
  CHECK(pm::runtime_services::api_alive(&object));
  CHECK(g_calls == 1 && g_seenObject == &object && g_seenMethod == &g_method);
  g_answer = false;  // a destroyed object
  CHECK(!pm::runtime_services::api_alive(&object));
  CHECK(g_calls == 2);
  CHECK(!pm::runtime_services::api_alive(nullptr));
  CHECK(g_calls == 2);
  g_mapped = false;  // a build without the entry: nothing is alive, and nothing is called
  CHECK(!pm::runtime_services::api_alive(&object));
  CHECK(g_calls == 2);
  g_mapped = true;
  g_implicit.code = nullptr;
  CHECK(!pm::runtime_services::api_alive(&object));
  CHECK(g_calls == 2);
}

void only_the_exact_array_is_one() {
  g_byte.kind = PM_ENTRY_TYPE;
  g_byte.klass = &g_byteClass;
  il2cpp::p_il2cpp_array_class_get = [](void* element, uint32_t) -> void* { return element == &g_byteClass ? &g_byteArrayClass : &g_otherClass; };
  il2cpp::p_il2cpp_object_get_class = [](void* obj) -> void* { return *static_cast<void**>(obj); };
  void* bytes = &g_byteArrayClass;
  void* others = &g_otherClass;
  CHECK(pm::runtime_services::is_array_of(&bytes, "Byte"));
  CHECK(!pm::runtime_services::is_array_of(&others, "Byte"));
  CHECK(!pm::runtime_services::is_array_of(nullptr, "Byte"));
  CHECK(!pm::runtime_services::is_array_of(&bytes, "Int32"));  // an entry that did not resolve
  g_mapped = false;
  CHECK(!pm::runtime_services::is_array_of(&bytes, "Byte"));
  g_mapped = true;
}

// A delegate to native code is made by the delegate's own constructor, the map's entry, as the game makes its own: with
// a MethodInfo copied from the delegate's Invoke, whose code is the listener and which is no static method, so the
// constructor binds it to the target.
void delegate_is_made_by_its_own_constructor() {
  struct Ctor {
    void* made;
    void* target;
    const uint8_t* method;
    const void* ctorInfo;
    int calls;
  };
  static Ctor seen{};
  static int delegateClass = 0, ctorInfo = 0, otherClass = 0;
  static uint8_t invoke[0x58];
  static uint8_t object[0x50];
  for (size_t i = 0; i < sizeof invoke; ++i) invoke[i] = static_cast<uint8_t>(i);
  invoke[0x4C] = 0x16;  // flags with METHOD_ATTRIBUTE_STATIC (0x10): the copy must say instance
  invoke[0x4D] = 0x01;
  PmGameEntry ctor{};
  ctor.kind = PM_ENTRY_METHOD;
  ctor.klass = &delegateClass;
  ctor.method = &ctorInfo;
  ctor.code = reinterpret_cast<void*>(+[](void* made, void* target, const void* method, const void* info) {
    seen = {made, target, static_cast<const uint8_t*>(method), info, seen.calls + 1};
  });
  g_ctor = &ctor;
  PmGameEntry invokeEntry{};
  invokeEntry.kind = PM_ENTRY_METHOD;
  invokeEntry.klass = &delegateClass;
  invokeEntry.method = invoke;
  g_invoke = &invokeEntry;
  il2cpp::p_il2cpp_object_new = [](void* klass) -> void* { return klass == &delegateClass ? object : nullptr; };
  const pm::runtime_services::NativeListener listener = [](void*, const void*) {};
  int target = 0;
  const auto make = [](const char* ctorKey, const char* invokeKey, void* to, pm::runtime_services::NativeListener with) {
    return pm::runtime_services::delegate_new(ctorKey, invokeKey, to, with);
  };
  void* made = make("UnityAction.ctor", "UnityAction.Invoke", &target, listener);
  CHECK(made == object && seen.calls == 1 && seen.made == object && seen.target == &target && seen.ctorInfo == &ctorInfo);
  const uint8_t* method = seen.method;
  CHECK(method && method != invoke);
  if (method) {
    void* code = nullptr;
    std::memcpy(&code, method, sizeof code);
    CHECK(code == reinterpret_cast<void*>(listener));  // methodPointer
    std::memcpy(&code, method + 8, sizeof code);
    CHECK(code == reinterpret_cast<void*>(listener));  // virtualMethodPointer, which the constructor binds
    CHECK(method[0x4C] == 0x06 && method[0x4D] == 0x01);  // no longer static, the rest of the flags kept
    CHECK(std::memcmp(method + 0x10, invoke + 0x10, 0x4C - 0x10) == 0);  // the name, class, types: Invoke's
    CHECK(std::memcmp(method + 0x4E, invoke + 0x4E, 0x58 - 0x4E) == 0);
  }
  // One MethodInfo per class and listener, kept: a second delegate shares it.
  CHECK(make("UnityAction.ctor", "UnityAction.Invoke", &target, listener) == object);
  CHECK(seen.calls == 2 && seen.method == method);
  // Nothing is made without a target or a listener, for an entry that did not resolve or has no code, or for an Invoke
  // that did not resolve or is another class's.
  CHECK(!make("UnityAction.ctor", "UnityAction.Invoke", nullptr, listener));
  CHECK(!make("UnityAction.ctor", "UnityAction.Invoke", &target, nullptr));
  CHECK(!make("Unknown.ctor", "UnityAction.Invoke", &target, listener));
  CHECK(!make("UnityAction.ctor", "Unknown.Invoke", &target, listener));  // the map's Invoke did not resolve
  ctor.code = nullptr;
  CHECK(!make("UnityAction.ctor", "UnityAction.Invoke", &target, listener));
  ctor.code = reinterpret_cast<void*>(+[](void*, void*, const void*, const void*) { ++seen.calls; });
  ctor.klass = &otherClass;  // an Invoke of another class
  CHECK(!make("UnityAction.ctor", "UnityAction.Invoke", &target, listener));
  ctor.klass = &delegateClass;
  invokeEntry.method = nullptr;
  CHECK(!make("UnityAction.ctor", "UnityAction.Invoke", &target, listener));
  CHECK(seen.calls == 2);
  g_ctor = nullptr;
  g_invoke = nullptr;
}

}  // namespace

int main() {
  alive_asks_the_game_once();
  only_the_exact_array_is_one();
  delegate_is_made_by_its_own_constructor();
  return g_failures ? 1 : 0;
}
