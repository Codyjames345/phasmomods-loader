#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "dump_model.h"

using namespace pg::sdk;

namespace {

const std::filesystem::path kRoot = std::filesystem::path(PG_SOURCE_DIR) / "tests" / "fixtures" / "diffable";

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

}  // namespace

int main() {
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
