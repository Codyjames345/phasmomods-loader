#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "dump_model.h"

using namespace pm::sdk;

namespace {

const std::filesystem::path kRoot = std::filesystem::path(PM_SOURCE_DIR) / "tests" / "fixtures" / "diffable";

GameModel load(const std::vector<std::string>& assemblies = {"Assembly-CSharp", "UnityEngine.CoreModule"}) {
  GameModel model;
  std::string error;
  CHECK(load_dump(kRoot, assemblies, model, error));
  CHECK(error.empty());
  return model;
}

const ModelType* find(const GameModel& m, const std::string& name) {
  for (const ModelType& t : m.types)
    if (t.name == name) return &t;
  return nullptr;
}

int index_of(const GameModel& m, const ModelType* t) { return static_cast<int>(t - m.types.data()); }

const ModelMethod* method(const ModelType& t, const std::string& name) {
  for (const ModelMethod& m : t.methods)
    if (m.name == name) return &m;
  return nullptr;
}

// A field's accessibility comes from its modifiers; an enum's values, which the dump writes with none,
// are public.
void field_access_is_read() {
  const GameModel m = load();
  const ModelType* ghost = find(m, "Ghost");
  CHECK(ghost != nullptr);
  if (!ghost) return;
  for (const ModelField& f : ghost->fields) {
    if (f.name == "awake") CHECK(f.access == pm::sdk::Visibility::Public);
    if (f.name == "seen") CHECK(f.access == pm::sdk::Visibility::Private);
  }
  const ModelType* state = find(m, "ദശസറ");
  CHECK(state && !state->fields.empty() && state->fields[0].access == pm::sdk::Visibility::Public);
}

void nested_enum_with_literals() {
  const GameModel m = load();
  const ModelType* ghost = find(m, "Ghost");
  const ModelType* state = find(m, "ദശസറ");
  CHECK(ghost && state);
  if (!ghost || !state) return;
  CHECK(state->isEnum && state->isValueType);
  CHECK(state->declaring == index_of(m, ghost));
  CHECK(state->assembly == "Assembly-CSharp" && state->ns.empty());
  CHECK(state->fields.size() == 3);
  if (state->fields.size() != 3) return;
  CHECK(state->fields[0].name == "idle" && state->fields[0].hasLiteral && state->fields[0].literal == 0);
  CHECK(state->fields[2].name == "hunting" && state->fields[2].literal == 2);
  CHECK(state->fields[1].isStatic && state->fields[1].typeIndex == index_of(m, state));
  CHECK(!ghost->isEnum && !ghost->isValueType && ghost->declaring == -1);
}

void method_rva_and_params() {
  const GameModel m = load();
  const ModelType* ghost = find(m, "Ghost");
  const ModelMethod* change = ghost ? method(*ghost, "ChangeState") : nullptr;
  CHECK(change != nullptr);
  if (!change) return;
  CHECK(change->rva == 0x1290);
  CHECK(change->ret == "void");
  CHECK((change->params == std::vector<std::string>{"ദശസറ", "PhotonObjectInteract", "bool"}));
  CHECK(!change->isStatic);
  // Only the enum is a type of the model.
  CHECK((change->paramIndex == std::vector<int>{index_of(m, find(m, "ദശസറ")), -1, -1}));
  CHECK(change->retIndex == -1);
}

void parameter_modifiers_generics_and_static() {
  const GameModel m = load();
  const ModelType* ghost = find(m, "Ghost");
  const ModelMethod* find_ = ghost ? method(*ghost, "TryFind") : nullptr;
  CHECK(find_ != nullptr);
  if (!find_) return;
  CHECK(find_->isStatic && find_->rva == 0x1500);
  CHECK((find_->params == std::vector<std::string>{"string", "Ghost&", "List<Vector3>&", "Nullable<float>",
                                                  "string"}));
  CHECK(find_->paramIndex[1] == index_of(m, ghost));  // Ghost& names Ghost
  CHECK(find_->paramIndex[2] == -1);                  // a generic names no type
  // Abstract: no address.
  const ModelMethod* counts = method(*ghost, "Counts");
  CHECK(counts && counts->rva == 0);
  CHECK(counts && counts->ret == "Dictionary<string,int>");
  // Constructors carry the runtime's names.
  const ModelMethod* ctor = method(*ghost, ".ctor");
  CHECK(ctor && ctor->ret == "void" && ctor->rva == 0x1600 && !ctor->isStatic);
  const ModelMethod* cctor = method(*ghost, ".cctor");
  CHECK(cctor && cctor->ret == "void" && cctor->rva == 0x1700 && cctor->isStatic);
  CHECK(method(*ghost, "Ghost") == nullptr);
  // A property accessor's address is not taken by anything else, and the property is no method.
  CHECK(method(*ghost, "IsAwake") == nullptr);
  const ModelMethod* getter = method(*ghost, "get_IsAwake");
  CHECK(getter && getter->rva == 0x1300 && getter->ret == "bool");
}

void field_offsets_and_static() {
  const GameModel m = load();
  const ModelType* ghost = find(m, "Ghost");
  CHECK(ghost != nullptr);
  if (!ghost) return;
  const ModelField* awake = nullptr;
  const ModelField* instance = nullptr;
  const ModelField* seen = nullptr;
  const ModelField* tag = nullptr;
  const ModelField* lines = nullptr;
  const ModelField* limit = nullptr;
  for (const ModelField& f : ghost->fields) {
    if (f.name == "awake") awake = &f;
    if (f.name == "instance") instance = &f;
    if (f.name == "seen") seen = &f;
    if (f.name == "tag") tag = &f;
    if (f.name == "lines") lines = &f;
    if (f.name == "limit") limit = &f;
  }
  CHECK(awake && awake->offset == 0xD8 && !awake->isStatic && awake->type == "bool");
  CHECK(instance && instance->isStatic && instance->type == "Ghost" && instance->typeIndex == index_of(m, ghost));
  CHECK(seen && seen->offset == 0xE0 && seen->type == "Dictionary<string,Nullable<int>>" && seen->typeIndex == -1);
  CHECK(tag && tag->isStatic && tag->hasLiteral && tag->type == "string");  // its "//" is no comment
  CHECK(lines && lines->isStatic && lines->hasLiteral);  // a constant holding a line break
  CHECK(limit && limit->hasLiteral && limit->literal == 50);
  CHECK(awake && instance && !awake->hasLiteral && !instance->hasLiteral);
  CHECK(ghost->fields.size() == 6);
  // The members after a constant that spans lines are still read.
  CHECK(method(*ghost, "ChangeState") != nullptr && method(*ghost, ".cctor") != nullptr);
  // A method whose default string argument holds a line break is read whole, and the members after it too.
  const ModelMethod* escape = method(*ghost, "Escape");
  CHECK(escape && escape->isStatic && escape->params.size() == 3 && escape->rva == 0x1340);
  CHECK(method(*ghost, "get_IsAwake") != nullptr);
}

void namespace_from_path() {
  const GameModel m = load();
  const ModelType* camera = find(m, "Camera");
  CHECK(camera != nullptr);
  if (!camera) return;
  CHECK(camera->ns == "UnityEngine" && camera->assembly == "UnityEngine.CoreModule");
  const ModelMethod* main = method(*camera, "get_main");
  CHECK(main && main->isStatic && main->ret == "Camera" && main->retIndex == index_of(m, camera));
}

void compiler_generated_nested_class() {
  const GameModel m = load();
  const ModelType* closure = find(m, "<>c__DisplayClass5_0");
  const ModelType* ghost = find(m, "Ghost");
  CHECK(closure != nullptr);
  if (!closure || !ghost) return;
  CHECK(closure->declaring == index_of(m, ghost));
  CHECK(closure->fields.size() == 2 && closure->fields[0].name == "<>4__this");
  CHECK(closure->fields[0].offset == 0x10 && closure->fields[0].typeIndex == index_of(m, ghost));
  // The nested type's members do not leak into the outer type, nor the outer's into it.
  CHECK(closure->methods.size() == 1 && method(*closure, "<Wait>b__0"));
  CHECK((closure->methods[0].params == std::vector<std::string>{"int&"}));
}

void struct_is_a_value_type() {
  const GameModel m = load();
  const ModelType* pair = find(m, "Pair");
  CHECK(pair && pair->isValueType && !pair->isEnum);
  // Offsets include the object header, as the runtime reports them (the dump prints 0x0 and 0x8).
  CHECK(pair && pair->fields.size() == 2 && pair->fields[0].offset == 0x10 && pair->fields[1].offset == 0x18);
}

void only_the_named_assemblies_are_read() {
  const GameModel m = load({"UnityEngine.CoreModule"});
  CHECK(m.types.size() == 1 && find(m, "Camera"));
  CHECK(find(m, "Ghost") == nullptr && find(m, "Ignored") == nullptr);
}

void unreadable_root_reports_error() {
  GameModel model;
  std::string error;
  const std::filesystem::path missing = kRoot / "no-such-folder";
  CHECK(!load_dump(missing, std::vector<std::string>{"Assembly-CSharp"}, model, error));
  CHECK(error.find("no-such-folder") != std::string::npos);

  error.clear();
  CHECK(!load_dump(kRoot, std::vector<std::string>{"NoSuchAssembly"}, model, error));
  CHECK(error.find("NoSuchAssembly") != std::string::npos);
}

void nothing_is_left_unparsed() {
  GameModel m;
  std::string error;
  size_t unparsed = 99;
  CHECK(load_dump(kRoot, std::vector<std::string>{"Assembly-CSharp", "UnityEngine.CoreModule"}, m, error, &unparsed));
  CHECK(unparsed == 0);
}

// The dump writes System's primitive structs with their C# keywords ("public struct float"), where the
// runtime names the class Single. The model names them as the runtime does, and a field spelled with
// the keyword still names that type.
void system_primitive_is_named_as_the_runtime_names_it() {
  const GameModel m = load({"mscorlib"});
  const ModelType* single = find(m, "Single");
  CHECK(single && single->ns == "System" && single->isValueType);
  CHECK(!find(m, "float"));
  if (!single) return;
  for (const ModelField& f : single->fields)
    if (f.name == "m_value") CHECK(f.type == "float" && f.typeIndex == index_of(m, single));
}

}  // namespace

// An image laid out by RVA with what read_method_tables reads: the PE headers' image base, and where a test puts them,
// module names, records and tables.
struct TableImage {
  static constexpr uint64_t kBase = 0x180000000;
  static constexpr uint32_t kLfanew = 0x40;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(0x4000);
  TableImage() {
    put<uint32_t>(0x3C, kLfanew);
    put<uint64_t>(kLfanew + 24 + 24, kBase);
  }
  template <class T>
  void put(uint32_t at, T value) {
    std::memcpy(bytes.data() + at, &value, sizeof value);
  }
  void name(uint32_t at, const std::string& text) { std::memcpy(bytes.data() + at, text.data(), text.size()); }
  void record(uint32_t at, uint32_t nameAt, uint32_t count, uint64_t tableVa) {
    put<uint64_t>(at, kBase + nameAt);
    put<uint32_t>(at + 8, count);
    put<uint64_t>(at + 16, tableVa);
  }
};

void the_tables_are_read_by_module_name() {
  TableImage image;
  image.name(0x1000, "Assembly-CSharp.dll");
  image.record(0x2000, 0x1000, 3, TableImage::kBase + 0x3000);
  image.put<uint64_t>(0x3000, TableImage::kBase + 0x1200);
  image.put<uint64_t>(0x3008, 0);
  image.put<uint64_t>(0x3010, TableImage::kBase + 0x1300);
  MethodTables tables;
  std::string error;
  const std::vector<std::string> names{"Assembly-CSharp"};
  CHECK(read_method_tables(image.bytes, names, tables, error));
  CHECK((tables["Assembly-CSharp"] == std::vector<uint64_t>{0x1200, 0, 0x1300}));
}

void a_missing_module_is_an_error() {
  TableImage image;
  image.name(0x1000, "Assembly-CSharp.dll");
  image.record(0x2000, 0x1000, 3, TableImage::kBase + 0x3000);
  MethodTables tables;
  std::string error;
  const std::vector<std::string> names{"Assembly-CSharp", "PhotonRealtime"};
  CHECK(!read_method_tables(image.bytes, names, tables, error));
  CHECK(error.find("no code-gen module for PhotonRealtime") != std::string::npos);
  CHECK(tables.empty());
  CHECK(!read_method_tables(std::vector<uint8_t>(0x10), names, tables, error));
}

void several_modules_are_an_error() {
  TableImage image;
  image.name(0x1000, "Assembly-CSharp.dll");
  image.record(0x2000, 0x1000, 1, TableImage::kBase + 0x3000);
  image.record(0x2100, 0x1000, 1, TableImage::kBase + 0x3000);
  MethodTables tables;
  std::string error;
  const std::vector<std::string> names{"Assembly-CSharp"};
  CHECK(!read_method_tables(image.bytes, names, tables, error));
  CHECK(error.find("several code-gen modules for Assembly-CSharp") != std::string::npos);
}

// What only looks like a module: a longer name ending in the assembly's, no count, a table leaving the image, or a
// table holding something other than addresses.
void a_look_alike_is_no_module() {
  TableImage image;
  image.name(0x1000, "NotAssembly-CSharp.dll");
  image.record(0x2000, 0x1003, 1, TableImage::kBase + 0x3000);
  image.name(0x1100, "Assembly-CSharp.dll");
  image.record(0x2100, 0x1100, 0, TableImage::kBase + 0x3000);
  image.record(0x2200, 0x1100, 0x1000, TableImage::kBase + 0x3F00);
  image.record(0x2300, 0x1100, 1, TableImage::kBase + 0x3100);
  image.put<uint64_t>(0x3100, 0x1234);  // no address of the image
  image.record(0x2400, 0x1100, 1, TableImage::kBase + 0x3000);
  image.put<uint64_t>(0x3000, TableImage::kBase + 0x1200);
  MethodTables tables;
  std::string error;
  const std::vector<std::string> names{"Assembly-CSharp"};
  CHECK(read_method_tables(image.bytes, names, tables, error));
  CHECK((tables["Assembly-CSharp"] == std::vector<uint64_t>{0x1200}));
  // The longer name alone answers to nothing.
  TableImage longer;
  longer.name(0x1000, "NotAssembly-CSharp.dll");
  longer.record(0x2000, 0x1003, 1, TableImage::kBase + 0x3000);
  CHECK(!read_method_tables(longer.bytes, names, tables, error));
}

// The address comes from the method's row of its own assembly's table, whatever the dump wrote.
void a_method_takes_its_row() {
  GameModel model;
  model.types.resize(2);
  model.types[0].assembly = "A";
  model.types[1].assembly = "B";
  const auto add = [](ModelType& type, uint32_t token, uint64_t dumpRva) {
    ModelMethod m;
    m.token = token;
    m.rva = dumpRva;
    type.methods.push_back(m);
  };
  add(model.types[0], 0x06000001, 0x111);  // the table's address replaces the dump's
  add(model.types[0], 0x06000002, 0x222);  // row 0: no code of its own, though the dump gives one
  add(model.types[0], 0x06000003, 0x333);  // past the table
  add(model.types[0], 0, 0x444);           // no token
  add(model.types[0], 0x04000001, 0x555);  // not a method's token
  add(model.types[1], 0x06000001, 0x666);  // no table for this assembly
  apply_method_tables({{"A", {0x1200, 0}}}, model);
  CHECK(model.types[0].methods[0].rva == 0x1200);
  for (size_t i = 1; i < 5; ++i) CHECK(model.types[0].methods[i].rva == 0);
  CHECK(model.types[1].methods[0].rva == 0);
}

int main() {
  the_tables_are_read_by_module_name();
  a_missing_module_is_an_error();
  several_modules_are_an_error();
  a_look_alike_is_no_module();
  a_method_takes_its_row();
  system_primitive_is_named_as_the_runtime_names_it();
  field_access_is_read();
  nested_enum_with_literals();
  nothing_is_left_unparsed();
  method_rva_and_params();
  parameter_modifiers_generics_and_static();
  field_offsets_and_static();
  namespace_from_path();
  compiler_generated_nested_class();
  struct_is_a_value_type();
  only_the_named_assemblies_are_read();
  unreadable_root_reports_error();
  return g_failures ? 1 : 0;
}
