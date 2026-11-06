// The host's static field access (host/il2cpp.cpp) against fake runtime exports: a size other than
// the field's value is refused before the class is initialised, so a refused access never runs a
// static constructor; an accepted one initialises the class first, then reads or writes, a reference
// passed itself and a value by address. And the host's dictionary reader (dictionary_int_entries) over
// fake classes: what it reads, and every shape it refuses. And inflate, the generic instances' way in, over fake
// reflection: the method it finds, what it hands the game's maker, and every null it leaves the entry unresolved for.
// Run: build\il2cpp_static_test.exe
#include <string>
#include <vector>

#include "../host/il2cpp.cpp"
#include "check.h"
#include "runtime_model.h"

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

// A static constructor that throws: the managed exception leaves static_get and static_set as it
// came, for the caller to contain (host/runtime_services.cpp's static_access), and nothing is read or written.
void throwing_static_constructor_escapes_untouched() {
  g_fieldClass = &g_intClass;
  static int managed = 0;  // the managed exception
  il2cpp::p_il2cpp_runtime_class_init = [](void*) {
    g_calls.emplace_back("init");
    throw Il2CppExceptionWrapper{&managed};
  };
  for (const bool reading : {true, false}) {
    g_calls.clear();
    int32_t value = 3;
    void* caught = nullptr;
    try {
      if (reading) il2cpp::static_get(&g_owner, &g_fieldInfo, &value, sizeof value);
      else il2cpp::static_set(&g_owner, &g_fieldInfo, &value, sizeof value);
    } catch (const Il2CppExceptionWrapper& e) {
      caught = e.ex;
    }
    CHECK(caught == &managed);
    CHECK((g_calls == std::vector<std::string>{"init"}));
    CHECK(value == 3);
  }
}

// Fake classes and fields for the dictionary reader: an object's class is its first member, a field's type
// is the field itself, whose class is `type`.
struct FakeClass;
struct FakeField {
  size_t offset;
  FakeClass* type;
};
struct FakeClass {
  const char* ns;
  const char* name;
  bool valueType = false, isEnum = false;
  int32_t size = 8;
  std::vector<std::pair<const char*, FakeField*>> fields;
  FakeClass* element = nullptr;  // an array class's element class
  int rank = 0;
};

FakeClass g_int{"System", "Int32", true, false, 4};
FakeClass g_uint{"System", "UInt32", true, false, 4};
FakeClass g_float{"System", "Single", true, false, 4};
FakeClass g_long{"System", "Int64", true, false, 8};
FakeClass g_pair{"", "Pair", true, false, 4};  // a 4-byte struct
FakeClass g_ghostType{"", "GhostType", true, true, 4};
// The entry struct, boxed offsets: hashCode, next, key, value.
FakeField g_hash{0x10, &g_int}, g_next{0x14, &g_int}, g_key{0x18, &g_ghostType}, g_value{0x1C, &g_int};
FakeClass g_entry{"", "Entry", true, false, 16, {{"hashCode", &g_hash}, {"next", &g_next}, {"key", &g_key}, {"value", &g_value}}};
FakeClass g_entries{"", "Entry[]", false, false, 8, {}, &g_entry, 1};
FakeField g_entriesField{0x10, &g_entries}, g_countField{0x18, &g_int};
FakeClass g_dictionary{"System.Collections.Generic", "Dictionary`2", false, false, 8,
                       {{"_entries", &g_entriesField}, {"_count", &g_countField}}};
FakeClass g_list{"System.Collections.Generic", "List`1"};

struct Entry {
  int32_t hash, next, key, value;
};
struct FakeEntries {
  void* klass = &g_entries;
  void* monitor = nullptr;
  void* bounds = nullptr;
  uintptr_t length = 0;
  Entry items[4] = {};
};
struct FakeDictionary {
  void* klass = &g_dictionary;
  void* monitor = nullptr;
  FakeEntries* entries = nullptr;
  int32_t count = 0;
};

FakeClass* fake(void* k) { return static_cast<FakeClass*>(k); }

void install_classes() {
  il2cpp::p_il2cpp_object_get_class = [](void* obj) { return *static_cast<void**>(obj); };
  il2cpp::p_il2cpp_class_get_namespace = [](void* k) { return fake(k)->ns; };
  il2cpp::p_il2cpp_class_get_name = [](void* k) { return fake(k)->name; };
  il2cpp::p_il2cpp_class_get_field_from_name = [](void* k, const char* name) -> void* {
    for (const auto& [n, f] : fake(k)->fields)
      if (std::string(n) == name) return f;
    return nullptr;
  };
  il2cpp::p_il2cpp_field_get_offset = [](void* f) { return static_cast<FakeField*>(f)->offset; };
  il2cpp::p_il2cpp_field_get_type = [](void* f) -> const void* { return f; };
  il2cpp::p_il2cpp_class_from_type = [](const void* t) -> void* { return static_cast<const FakeField*>(t)->type; };
  il2cpp::p_il2cpp_class_is_valuetype = [](const void* k) { return static_cast<const FakeClass*>(k)->valueType; };
  il2cpp::p_il2cpp_class_is_enum = [](const void* k) { return static_cast<const FakeClass*>(k)->isEnum; };
  il2cpp::p_il2cpp_class_value_size = [](void* k, uint32_t*) { return fake(k)->size; };
  il2cpp::p_il2cpp_class_get_element_class = [](void* k) -> void* { return fake(k)->element; };
  il2cpp::p_il2cpp_class_get_rank = [](const void* k) { return static_cast<const FakeClass*>(k)->rank; };
}

FakeEntries g_items;
FakeDictionary g_dict;

void reset_dictionary() {
  g_key.type = &g_ghostType;
  g_value.type = &g_int;
  g_items = FakeEntries{};
  g_items.length = 4;
  g_items.items[0] = {100, -1, 3, 0};    // weight 0: still an entry
  g_items.items[1] = {-1, -1, 9, 50};    // a free slot
  g_items.items[2] = {101, -1, 5, 10};
  g_items.items[3] = {102, -1, 9, 20};
  g_dict = FakeDictionary{};
  g_dict.entries = &g_items;
  g_dict.count = 4;
}

// The used slots in array order, at most max; an empty dictionary is read as one with no entries.
void dictionary_entries_are_read() {
  install_classes();
  reset_dictionary();
  il2cpp::IntPair out[8] = {};
  size_t written = 99;
  CHECK(il2cpp::dictionary_int_entries(&g_dict, out, 8, written));
  CHECK(written == 3);
  CHECK(out[0].key == 3 && out[0].value == 0 && out[1].key == 5 && out[1].value == 10 && out[2].key == 9 &&
        out[2].value == 20);
  CHECK(il2cpp::dictionary_int_entries(&g_dict, out, 2, written) && written == 2);
  g_dict.count = 2;  // the count bounds the slots read, whatever the array's length
  CHECK(il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 1);
  g_dict.entries = nullptr;
  g_dict.count = 0;
  CHECK(il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 0);
  reset_dictionary();
  g_value.type = &g_uint;  // a UInt32 value is a number too
  CHECK(il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 3);
  install();
}

// Refused, with nothing written: another class, and a key or value that is no 4-byte integer or enum.
void dictionary_shapes_are_refused() {
  install_classes();
  il2cpp::IntPair out[8] = {};
  size_t written = 99;
  reset_dictionary();
  g_dict.klass = &g_list;
  CHECK(!il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 0);
  CHECK(!il2cpp::dictionary_int_entries(nullptr, out, 8, written) && written == 0);
  for (FakeClass* value : {&g_float, &g_pair, &g_long}) {
    reset_dictionary();
    g_value.type = value;
    written = 99;
    CHECK(!il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 0);
    g_dict.entries = nullptr;  // judged by its class even when empty
    CHECK(!il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 0);
  }
  reset_dictionary();
  g_key.type = &g_float;
  CHECK(!il2cpp::dictionary_int_entries(&g_dict, out, 8, written) && written == 0);
  reset_dictionary();
  install();
}

}  // namespace

namespace {

// A runtime method as inflate reads it: { code, virtual code, invoker }, then what the fake exports answer about it.
struct FakeMethod {
  void* code = nullptr;
  void* virtualCode = nullptr;
  void* invoker = nullptr;
  const char* name = "";
  uint32_t params = 0;
};
struct FakeTypes {
  il2cpp::Array array{};
  void* items[4]{};
};

int g_image = 0, g_assembly = 0, g_definition = 0, g_instance = 0, g_madeType = 0, g_methodObject = 0, g_made = 0;
int g_typeClass = 0, g_runtimeType = 0, g_runtimeMethod = 0, g_argument = 0;
void* g_assemblies[1] = {&g_assembly};
FakeTypes g_types;
int g_codeWord = 0, g_invokerWord = 0;
FakeMethod g_first{nullptr, nullptr, nullptr, "First", 0};
FakeMethod g_wanted{nullptr, nullptr, nullptr, "Wanted", 1};
FakeMethod g_madeFirst{nullptr, nullptr, nullptr, "First", 0};
FakeMethod g_madeWanted{&g_codeWord, nullptr, &g_invokerWord, "Wanted", 1};
FakeMethod g_genericMethod{&g_codeWord, nullptr, &g_invokerWord, "Generic", 0};
FakeMethod g_make{};
std::vector<const FakeMethod*> g_definitionMethods, g_instanceMethods;

// What the fake maker was last given, and what it answers.
void* g_makeSelf = nullptr;
void* g_makeTypes = nullptr;
const void* g_makeMethod = nullptr;
void* g_makeAnswer = nullptr;
int g_makeCalls = 0;
void* fake_make(void* self, void* types, const void* method) {
  ++g_makeCalls;
  g_makeSelf = self;
  g_makeTypes = types;
  g_makeMethod = method;
  return g_makeAnswer;
}

// Whether each export finds something.
bool g_noMakerMethod = false, g_noTypeObject = false, g_noMethodObject = false, g_noArray = false;

void install_inflate() {
  namespace i = il2cpp;
  g_makeAnswer = &g_madeType;
  g_makeCalls = 0;
  g_noMakerMethod = g_noTypeObject = g_noMethodObject = g_noArray = false;
  g_make = {reinterpret_cast<void*>(&fake_make), nullptr, nullptr, "Make", 1};
  g_definitionMethods = {&g_first, &g_wanted};
  g_instanceMethods = {&g_madeFirst, &g_madeWanted};
  i::p_il2cpp_domain_get = []() -> void* { return &g_image; };
  i::p_il2cpp_domain_get_assemblies = [](void*, size_t* count) {
    *count = 1;
    return g_assemblies;
  };
  i::p_il2cpp_assembly_get_image = [](void*) -> void* { return &g_image; };
  i::p_il2cpp_image_get_name = [](void*) { return "mscorlib.dll"; };
  i::p_il2cpp_class_from_name = [](void*, const char*, const char* name) -> void* {
    if (std::strcmp(name, "Type") == 0) return &g_typeClass;
    if (std::strcmp(name, "RuntimeType") == 0) return &g_runtimeType;
    return std::strcmp(name, "RuntimeMethodInfo") == 0 ? &g_runtimeMethod : nullptr;
  };
  i::p_il2cpp_class_get_method_from_name = [](void* klass, const char* name, int argc) -> const void* {
    if (g_noMakerMethod || argc != 1) return nullptr;
    const bool maker = (klass == &g_runtimeType && std::strcmp(name, "MakeGenericType") == 0) ||
                       (klass == &g_runtimeMethod && std::strcmp(name, "MakeGenericMethod") == 0);
    return maker ? &g_make : nullptr;
  };
  i::p_il2cpp_array_new = [](void* element, uintptr_t length) -> void* {
    if (g_noArray || element != &g_typeClass || length > 4) return nullptr;
    g_types = {};
    g_types.array.max_length = length;
    return &g_types;
  };
  i::p_il2cpp_gc_wbarrier_set_field = [](void*, void** at, void* value) { *at = value; };
  // A class's type, and a type's object, are the class itself here.
  i::p_il2cpp_class_get_type = [](void* klass) -> const void* { return klass; };
  i::p_il2cpp_type_get_object = [](const void* type) -> void* { return g_noTypeObject ? nullptr : const_cast<void*>(type); };
  i::p_il2cpp_method_get_object = [](const void*, void*) -> void* { return g_noMethodObject ? nullptr : &g_methodObject; };
  i::p_il2cpp_method_get_from_reflection = [](const void* made) -> const void* { return made == &g_made ? &g_genericMethod : nullptr; };
  i::p_il2cpp_class_from_system_type = [](void* type) -> void* { return type == &g_madeType ? &g_instance : nullptr; };
  i::p_il2cpp_class_get_methods = [](void* klass, void** iter) -> const void* {
    const std::vector<const FakeMethod*>& list = klass == &g_definition ? g_definitionMethods : g_instanceMethods;
    const size_t at = reinterpret_cast<size_t>(*iter);
    if (at >= list.size()) return nullptr;
    *iter = reinterpret_cast<void*>(at + 1);
    return list[at];
  };
  i::p_il2cpp_method_get_name = [](const void* m) { return static_cast<const FakeMethod*>(m)->name; };
  i::p_il2cpp_method_get_param_count = [](const void* m) { return static_cast<const FakeMethod*>(m)->params; };
}

bool inflate_class(pm::sdk::InflatedMethod& out) {
  void* const args[] = {&g_argument};
  return il2cpp::inflate(&g_definition, &g_wanted, false, args, 1, out);
}

void inflate_finds_the_same_method_of_the_instance() {
  install_inflate();
  pm::sdk::InflatedMethod out;
  CHECK(inflate_class(out));
  CHECK(out.method == &g_madeWanted && out.code == &g_codeWord && out.invoker == &g_invokerWord && out.klass == &g_instance);
  // The game's maker ran on the definition's type object, with an array of the arguments' type objects.
  CHECK(g_makeCalls == 1 && g_makeSelf == &g_definition && g_makeMethod == &g_make && g_makeTypes == &g_types);
  CHECK(g_types.array.max_length == 1 && g_types.items[0] == &g_argument);
}

void inflate_makes_a_generic_method() {
  install_inflate();
  g_makeAnswer = &g_made;
  void* const args[] = {&g_argument};
  pm::sdk::InflatedMethod out;
  CHECK(il2cpp::inflate(&g_definition, &g_wanted, true, args, 1, out));
  CHECK(out.method == &g_genericMethod && out.code == &g_codeWord && out.klass == &g_definition);
  CHECK(g_makeSelf == &g_methodObject && g_types.items[0] == &g_argument);
}

// Each of these leaves the entry unresolved. The last two would otherwise call the wrong method through a wrong signature.
void inflate_fails_closed() {
  void* const args[] = {&g_argument};
  pm::sdk::InflatedMethod out;
  install_inflate();
  CHECK(!il2cpp::inflate(nullptr, &g_wanted, false, args, 1, out));
  CHECK(!il2cpp::inflate(&g_definition, nullptr, false, args, 1, out));
  CHECK(!il2cpp::inflate(&g_definition, &g_wanted, false, nullptr, 1, out));
  CHECK(!il2cpp::inflate(&g_definition, &g_wanted, false, args, 0, out));
  CHECK(g_makeCalls == 0);

  g_noMakerMethod = true;
  CHECK(!inflate_class(out));
  install_inflate();
  g_make.code = nullptr;  // a maker with no code is never jumped to
  CHECK(!inflate_class(out));
  CHECK(g_makeCalls == 0);
  install_inflate();
  g_noArray = true;
  CHECK(!inflate_class(out));
  install_inflate();
  g_noTypeObject = true;
  CHECK(!inflate_class(out));
  CHECK(g_makeCalls == 0);
  install_inflate();
  g_noMethodObject = true;
  CHECK(!il2cpp::inflate(&g_definition, &g_wanted, true, args, 1, out));
  CHECK(g_makeCalls == 0);
  install_inflate();
  g_makeAnswer = nullptr;
  CHECK(!inflate_class(out));
  install_inflate();
  g_makeAnswer = &g_definition;  // a type the runtime knows no class for
  CHECK(!inflate_class(out));
  install_inflate();
  CHECK(!il2cpp::inflate(&g_definition, &g_madeFirst, false, args, 1, out));  // not among the definition's own
  install_inflate();
  g_instanceMethods = {&g_madeFirst};  // the instance has fewer methods
  CHECK(!inflate_class(out));
  install_inflate();
  g_instanceMethods = {&g_madeWanted, &g_madeFirst};  // in another order: another name at its place
  CHECK(!inflate_class(out));
  install_inflate();
  static FakeMethod otherCount{&g_codeWord, nullptr, &g_invokerWord, "Wanted", 2};
  g_instanceMethods = {&g_madeFirst, &otherCount};  // an overload at its place
  CHECK(!inflate_class(out));
}

}  // namespace

int main() {
  install();
  size_mismatch_never_initialises();
  accepted_access_initialises_first();
  throwing_static_constructor_escapes_untouched();
  dictionary_entries_are_read();
  dictionary_shapes_are_refused();
  inflate_finds_the_same_method_of_the_instance();
  inflate_makes_a_generic_method();
  inflate_fails_closed();
  return g_failures ? 1 : 0;
}
