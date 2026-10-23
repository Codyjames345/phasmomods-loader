// The host's static field access (core/il2cpp.cpp) against fake runtime exports: a size other than
// the field's value is refused before the class is initialised, so a refused access never runs a
// static constructor; an accepted one initialises the class first, then reads or writes, a reference
// passed itself and a value by address. Run: build\il2cpp_static_test.exe
#include <string>
#include <vector>

#include "../core/il2cpp.cpp"
#include "check.h"

namespace {

int g_fieldInfo = 0, g_owner = 0, g_referenceClass = 0, g_intClass = 0;
void* g_fieldClass = nullptr;  // the class the field's type is: g_referenceClass or g_intClass
std::vector<std::string> g_calls;
void* g_setValue = nullptr;  // what static_set_value was given
uint64_t g_stored = 0;       // what static_get_value copies out

void install() {
  il2cpp::p_il2cpp_field_get_type = [](void*) -> const void* { return &g_fieldInfo; };
  il2cpp::p_il2cpp_class_from_type = [](const void*) { return g_fieldClass; };
  il2cpp::p_il2cpp_class_is_valuetype = [](const void* k) { return k == &g_intClass; };
  il2cpp::p_il2cpp_class_value_size = [](void*, uint32_t*) { return int32_t{4}; };
  il2cpp::p_il2cpp_runtime_class_init = [](void*) { g_calls.emplace_back("init"); };
  il2cpp::p_il2cpp_field_static_get_value = [](void*, void* out) {
    g_calls.emplace_back("get");
    std::memcpy(out, &g_stored, sizeof(void*));
  };
  il2cpp::p_il2cpp_field_static_set_value = [](void*, void* value) {
    g_calls.emplace_back("set");
    g_setValue = value;
  };
}

void size_mismatch_never_initialises() {
  g_fieldClass = &g_referenceClass;
  g_calls.clear();
  int32_t small = 0;
  CHECK(!il2cpp::static_get(&g_owner, &g_fieldInfo, &small, sizeof small));
  CHECK(!il2cpp::static_set(&g_owner, &g_fieldInfo, &small, sizeof small));
  g_fieldClass = &g_intClass;
  uint64_t wide = 0;
  CHECK(!il2cpp::static_get(&g_owner, &g_fieldInfo, &wide, sizeof wide));
  g_fieldClass = nullptr;  // a field type the runtime does not know
  CHECK(!il2cpp::static_get(&g_owner, &g_fieldInfo, &wide, sizeof wide));
  CHECK(g_calls.empty());
}

void accepted_access_initialises_first() {
  g_fieldClass = &g_referenceClass;
  int object = 0;
  g_stored = reinterpret_cast<uint64_t>(&object);
  g_calls.clear();
  void* read = nullptr;
  CHECK(il2cpp::static_get(&g_owner, &g_fieldInfo, &read, sizeof read));
  CHECK(read == &object);
  void* value = &object;
  CHECK(il2cpp::static_set(&g_owner, &g_fieldInfo, &value, sizeof value));
  CHECK(g_setValue == &object);  // a reference is passed itself
  CHECK((g_calls == std::vector<std::string>{"init", "get", "init", "set"}));

  g_fieldClass = &g_intClass;
  const int32_t number = 7;
  CHECK(il2cpp::static_set(&g_owner, &g_fieldInfo, &number, sizeof number));
  CHECK(g_setValue == &number);  // a value is passed by address
}

}  // namespace

int main() {
  install();
  size_mismatch_never_initialises();
  accepted_access_initialises_first();
  return g_failures ? 1 : 0;
}
