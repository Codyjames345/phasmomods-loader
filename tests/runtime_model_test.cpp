// The runtime model against a fake IL2CPP API, and its agreement with the dump model on one class
// written both ways.
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "dump_model.h"
#include "resolver.h"
#include "runtime_model.h"

using namespace pm::sdk;

namespace {

// Il2CppTypeEnum values and metadata flags the model reads.
constexpr int kBool = 0x02, kI1 = 0x04, kU1 = 0x05, kI4 = 0x08, kR4 = 0x0C, kString = 0x0E, kValueType = 0x11,
              kClass = 0x12, kVar = 0x13, kGenericInst = 0x15, kSzArray = 0x1D;
constexpr int kFieldStatic = 0x10, kFieldLiteral = 0x40;
// FieldAccessMask values.
constexpr int kPrivate = 1, kPrivateProtected = 2, kInternal = 3, kProtected = 4, kProtectedInternal = 5, kPublic = 6;
constexpr uint32_t kMethodStatic = 0x10;

struct FakeClass;

struct FakeType {
  int kind;
  FakeClass* klass;  // what il2cpp_class_from_type gives
  std::string name;  // what il2cpp_type_get_name gives
};

struct FakeMethod {
  uintptr_t pointer;  // MethodInfo's first member: its code
  std::string name;
  const FakeType* ret;
  std::vector<const FakeType*> params;
  uint32_t flags = 0;
};

struct FakeField {
  std::string name;
  const FakeType* type;
  size_t offset = 0;
  int flags = 0;
  int64_t value = 0;  // a literal's value, stored little-endian as the runtime writes it
  size_t size = 0;    // how many bytes il2cpp_field_static_get_value writes
};

struct FakeClass {
  std::string ns, name;
  FakeClass* declaring = nullptr;
  bool isEnum = false, isValueType = false;
  const FakeType* enumBase = nullptr;
  std::vector<FakeField> fields;
  std::vector<FakeMethod> methods;
  FakeClass* parent = nullptr;          // null: System.Object's place, which the model leaves out
  std::vector<FakeClass*> interfaces;
  bool isInterface = false;
};

struct FakeImage {
  std::string name;
  std::vector<FakeClass*> classes;
};

struct FakeAssembly {
  FakeImage* image;
};

// The world the fake API serves.
struct World {
  std::vector<void*> assemblies;
  int strings = 0;               // type names handed out and not yet freed
  std::vector<std::string> read;  // fields whose static value was read
};
World* g_world = nullptr;
int g_domain = 0;

template <class T, class U>
T* as(U* p) {
  return static_cast<T*>(const_cast<void*>(static_cast<const void*>(p)));
}

template <class T>
T* next(std::vector<T>& items, void** iter) {
  const size_t i = *iter ? static_cast<size_t>(static_cast<T*>(*iter) - items.data()) + 1 : 0;
  if (i >= items.size()) return nullptr;
  *iter = &items[i];
  return &items[i];
}

RuntimeApi fake_api() {
  RuntimeApi api;
  api.domain_get = [] { return static_cast<void*>(&g_domain); };
  api.domain_get_assemblies = [](void*, size_t* count) {
    *count = g_world->assemblies.size();
    return g_world->assemblies.data();
  };
  api.assembly_get_image = [](void* a) { return static_cast<void*>(as<FakeAssembly>(a)->image); };
  api.image_get_name = [](void* i) { return as<FakeImage>(i)->name.c_str(); };
  api.image_get_class_count = [](const void* i) { return as<FakeImage>(i)->classes.size(); };
  api.image_get_class = [](const void* i, size_t k) { return static_cast<void*>(as<FakeImage>(i)->classes[k]); };
  api.class_get_name = [](void* c) { return as<FakeClass>(c)->name.c_str(); };
  api.class_get_namespace = [](void* c) { return as<FakeClass>(c)->ns.c_str(); };
  api.class_get_declaring_type = [](void* c) { return static_cast<void*>(as<FakeClass>(c)->declaring); };
  api.class_is_enum = [](const void* c) { return as<FakeClass>(c)->isEnum; };
  api.class_is_valuetype = [](const void* c) { return as<FakeClass>(c)->isValueType; };
  api.class_enum_basetype = [](void* c) { return static_cast<const void*>(as<FakeClass>(c)->enumBase); };
  api.class_from_type = [](const void* t) { return static_cast<void*>(as<FakeType>(t)->klass); };
  api.class_get_parent = [](void* c) { return static_cast<void*>(as<FakeClass>(c)->parent); };
  api.class_is_interface = [](const void* c) { return as<FakeClass>(c)->isInterface; };
  api.class_get_interfaces = [](void* c, void** iter) {
    FakeClass** i = next(as<FakeClass>(c)->interfaces, iter);
    return static_cast<void*>(i ? *i : nullptr);
  };
  api.class_get_fields = [](void* c, void** iter) { return static_cast<void*>(next(as<FakeClass>(c)->fields, iter)); };
  api.class_get_methods = [](void* c, void** iter) {
    return static_cast<const void*>(next(as<FakeClass>(c)->methods, iter));
  };
  api.method_get_name = [](const void* m) { return as<FakeMethod>(m)->name.c_str(); };
  api.method_get_param_count = [](const void* m) { return static_cast<uint32_t>(as<FakeMethod>(m)->params.size()); };
  api.method_get_param = [](const void* m, uint32_t i) {
    return static_cast<const void*>(as<FakeMethod>(m)->params[i]);
  };
  api.method_get_return_type = [](const void* m) { return static_cast<const void*>(as<FakeMethod>(m)->ret); };
  api.method_get_flags = [](const void* m, uint32_t* impl) {
    if (impl) *impl = 0;
    return as<FakeMethod>(m)->flags;
  };
  api.field_get_name = [](void* f) { return as<FakeField>(f)->name.c_str(); };
  api.field_get_offset = [](void* f) { return as<FakeField>(f)->offset; };
  api.field_get_flags = [](void* f) { return as<FakeField>(f)->flags; };
  api.field_get_type = [](void* f) { return static_cast<const void*>(as<FakeField>(f)->type); };
  api.field_static_get_value = [](void* f, void* out) {
    const FakeField* field = as<FakeField>(f);
    g_world->read.push_back(field->name);
    std::memcpy(out, &field->value, field->size);
  };
  api.type_get_type = [](const void* t) { return as<FakeType>(t)->kind; };
  api.type_get_name = [](const void* t) {
    ++g_world->strings;
    return _strdup(as<FakeType>(t)->name.c_str());
  };
  api.free_memory = [](void* p) {
    --g_world->strings;
    std::free(p);
  };
  return api;
}

// A module the size of its headers' SizeOfImage, for the rvas.
struct FakeModule {
  alignas(16) uint8_t bytes[0x400] = {};
  static constexpr uint32_t kSize = 0x10000;
  FakeModule() {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage = kSize;
  }
  uintptr_t base() const { return reinterpret_cast<uintptr_t>(bytes); }
};

const FakeType kInt{kI4, nullptr, "System.Int32"};
const FakeType kSByte{kI1, nullptr, "System.SByte"};
const FakeType kByte{kU1, nullptr, "System.Byte"};
const FakeType kBoolean{kBool, nullptr, "System.Boolean"};
const FakeType kChar{0x03, nullptr, "System.Char"};
const FakeType kLong{0x0A, nullptr, "System.Int64"};
const FakeType kFloat{kR4, nullptr, "System.Single"};
const FakeType kStringType{kString, nullptr, "System.String"};
const FakeType kVoid{0x01, nullptr, "System.Void"};

// Two classes and what they nest: GhostAI (with its enum GhostState and a struct Pair) and the
// generic Holder`1, in Assembly-CSharp, and a class in an assembly the model does not ask for.
struct Game {
  FakeModule module;
  FakeClass ghost, state, pair, holder, closure, other;
  FakeType ghostType{kClass, &ghost, "GhostAI"};
  FakeType ghostRef{kClass, &ghost, "GhostAI&"};
  FakeType stateType{kValueType, &state, "GhostAI/GhostState"};
  FakeType pairType{kValueType, &pair, "GhostAI/Pair"};
  FakeType otherType{kClass, &other, "Elsewhere.Other"};
  FakeType listType{kGenericInst, nullptr, "System.Collections.Generic.List<UnityEngine.Vector3>"};
  FakeImage csharp{"Assembly-CSharp.dll", {}}, firstpass{"Assembly-CSharp-firstpass.dll", {}};
  FakeAssembly assemblies[2] = {{&csharp}, {&firstpass}};
  World world;

  Game() {
    const uintptr_t base = module.base();
    ghost = {"Game", "GhostAI"};
    ghost.fields = {{"state", &stateType, 0x18},
                    {"path", &listType, 0x20},
                    {"instance", &ghostType, 0x8, kFieldStatic},
                    {"other", &otherType, 0x28},
                    {"limit", &kInt, 0, kFieldStatic | kFieldLiteral, 50, 4},
                    {"scale", &kFloat, 0, kFieldStatic | kFieldLiteral, 0x3F000000, 4},
                    {"awake", &kBoolean, 0, kFieldStatic | kFieldLiteral, 1, 1},
                    {"tag", &kStringType, 0, kFieldStatic | kFieldLiteral}};
    ghost.methods = {{base + 0x1000, "Awake", &kVoid, {}},
                     {base + 0x1100, "Find", &stateType, {&ghostRef, &kInt}, kMethodStatic},
                     {base + module.kSize + 0x40, "Shared", &kVoid, {}},
                     {base - 0x40, "Below", &kVoid, {}},
                     {0, "Abstract", &kVoid, {}},
                     {base + 0x1200, ".ctor", &kVoid, {}}};
    // An enum's literals and its value__ field, which the dump does not list.
    state = {"", "GhostState", &ghost, true, true, &kSByte};
    state.fields = {{"value__", &kSByte, 0x10},
                    {"none", &stateType, 0, kFieldStatic | kFieldLiteral, -1, 1},
                    {"idle", &stateType, 0, kFieldStatic | kFieldLiteral, 0, 1},
                    {"hunting", &stateType, 0, kFieldStatic | kFieldLiteral, 2, 1}};
    pair = {"", "Pair", &ghost, false, true};
    pair.fields = {{"first", &kInt, 0x10}, {"second", &ghostType, 0x18}};
    holder = {"Game", "Holder`1"};
    holder.methods = {{base + 0x1300, "Get", &kVoid, {}}};
    closure = {"", "<>c__DisplayClass5_0", &holder};
    other = {"Elsewhere", "Other"};
    other.methods = {{base + 0x1400, "Run", &kVoid, {}}};
    csharp.classes = {&ghost, &state, &pair, &holder, &closure};
    firstpass.classes = {&other};
    world.assemblies = {&assemblies[0], &assemblies[1]};
    g_world = &world;
  }
};

const std::vector<std::string> kCSharp{"Assembly-CSharp"};

GameModel load(Game& game) {
  GameModel model;
  CHECK(load_runtime(fake_api(), game.module.base(), kCSharp, model));
  CHECK(game.world.strings == 0);  // every type name the runtime handed out was freed
  return model;
}

int find(const GameModel& m, const std::string& name) {
  for (size_t i = 0; i < m.types.size(); ++i)
    if (m.types[i].name == name) return static_cast<int>(i);
  return -1;
}

const ModelField* field(const ModelType& t, const std::string& name) {
  for (const ModelField& f : t.fields)
    if (f.name == name) return &f;
  return nullptr;
}

const ModelMethod* method(const ModelType& t, const std::string& name) {
  for (const ModelMethod& m : t.methods)
    if (m.name == name) return &m;
  return nullptr;
}

void enumerates_types_methods_fields() {
  Game game;
  const GameModel m = load(game);
  CHECK(m.types.size() == 5);
  const int ghost = find(m, "GhostAI"), state = find(m, "GhostState"), pair = find(m, "Pair");
  const int holder = find(m, "Holder"), closure = find(m, "<>c__DisplayClass5_0");
  CHECK(ghost >= 0 && state >= 0 && pair >= 0 && holder >= 0 && closure >= 0);
  CHECK(find(m, "Other") < 0);  // its assembly was not asked for
  if (ghost < 0 || state < 0 || pair < 0 || holder < 0 || closure < 0) return;
  const ModelType& g = m.types[ghost];
  CHECK(g.assembly == "Assembly-CSharp" && g.ns == "Game" && g.declaring == -1 && g.runtime == &game.ghost);
  CHECK(!g.isEnum && !g.isValueType);
  // Nested types name their declaring type and take its namespace.
  CHECK(m.types[state].declaring == ghost && m.types[state].ns == "Game");
  CHECK(m.types[pair].declaring == ghost && m.types[pair].isValueType && !m.types[pair].isEnum);
  CHECK(m.types[closure].declaring == holder && m.types[closure].ns == "Game");
  CHECK(m.types[state].isEnum && m.types[state].isValueType);
  // A generic definition keeps its arity apart from its name ("Holder`1"); every other type has none.
  CHECK(m.types[holder].genericParams == 1 && g.genericParams == 0 && m.types[closure].genericParams == 0);

  const ModelMethod* awake = method(g, "Awake");
  CHECK(awake && awake->rva == 0x1000 && !awake->isStatic && awake->ret == "void");
  CHECK(awake && awake->runtime == &game.ghost.methods[0]);
  const ModelMethod* find = method(g, "Find");
  CHECK(find && find->rva == 0x1100 && find->isStatic && find->ret == "GhostState" && find->retIndex == state);
  CHECK(find && (find->params == std::vector<std::string>{"GhostAI&", "int"}));
  CHECK(find && (find->paramIndex == std::vector<int>{ghost, -1}));
  const ModelMethod* ctor = method(g, ".ctor");
  CHECK(ctor && ctor->rva == 0x1200);
  CHECK(method(m.types[holder], "Get") && method(m.types[holder], "Get")->rva == 0x1300);

  const ModelField* st = field(g, "state");
  CHECK(st && st->offset == 0x18 && !st->isStatic && !st->hasLiteral && st->typeIndex == state);
  CHECK(st && st->runtime != nullptr && std::string(fake_api().field_get_name(st->runtime)) == "state");
  const ModelField* instance = field(g, "instance");
  CHECK(instance && instance->offset == 0x8 && instance->isStatic && instance->typeIndex == ghost);
  // A class outside the model's assemblies is no model type.
  const ModelField* other = field(g, "other");
  CHECK(other && other->type == "Other" && other->typeIndex == -1);
  // A value type's instance offsets are kept as the runtime reports them, with the object header.
  CHECK(m.types[pair].fields.size() == 2 && m.types[pair].fields[0].offset == 0x10);
  CHECK(m.types[pair].fields.size() == 2 && m.types[pair].fields[1].typeIndex == ghost);
}

void enum_literals_read_through_static_value() {
  Game game;
  const GameModel m = load(game);
  const int state = find(m, "GhostState"), ghost = find(m, "GhostAI");
  if (state < 0 || ghost < 0) return;
  const ModelType& s = m.types[state];
  // value__ is left out, as the dump leaves it out.
  CHECK(s.fields.size() == 3 && field(s, "value__") == nullptr);
  const ModelField* none = field(s, "none");
  const ModelField* hunting = field(s, "hunting");
  // An sbyte enum's -1 is read as one byte and sign-extended.
  CHECK(none && none->hasLiteral && none->isStatic && none->literal == -1 && none->offset == 0);
  CHECK(hunting && hunting->literal == 2 && hunting->type == "GhostState" && hunting->typeIndex == state);

  // Constants: an integer's number is read; a float's, a bool's and a string's are left 0 and never read.
  const ModelType& g = m.types[ghost];
  const ModelField* limit = field(g, "limit");
  CHECK(limit && limit->hasLiteral && limit->isStatic && limit->literal == 50);
  for (const char* name : {"scale", "awake", "tag"}) {
    const ModelField* f = field(g, name);
    CHECK(f && f->hasLiteral && f->isStatic && f->literal == 0);
  }
  CHECK((game.world.read == std::vector<std::string>{"limit", "none", "idle", "hunting"}));
}

void byte_enum_is_zero_extended() {
  Game game;
  game.state.enumBase = &kByte;
  game.state.fields[1].value = 200;
  const GameModel m = load(game);
  const int state = find(m, "GhostState");
  if (state < 0) return;
  const ModelField* none = field(m.types[state], "none");
  CHECK(none && none->literal == 200);
}

void pointer_outside_module_gets_zero_rva() {
  Game game;
  const GameModel m = load(game);
  const int ghost = find(m, "GhostAI");
  if (ghost < 0) return;
  for (const char* name : {"Shared", "Below", "Abstract"}) {
    const ModelMethod* mm = method(m.types[ghost], name);
    CHECK(mm && mm->rva == 0);
  }
}

void types_are_canonical() {
  Game game;
  const GameModel m = load(game);
  const int ghost = find(m, "GhostAI");
  if (ghost < 0) return;
  const ModelField* path = field(m.types[ghost], "path");
  CHECK(path && path->type == "List<Vector3>" && path->typeIndex == -1);
  const ModelField* st = field(m.types[ghost], "state");
  CHECK(st && st->type == "GhostState");
}

void bad_module_or_missing_assembly_fails() {
  Game game;
  GameModel model;
  CHECK(!load_runtime(fake_api(), game.module.base(), std::vector<std::string>{"NoSuchAssembly"}, model));
  game.module.bytes[0] = 0;  // no MZ
  CHECK(!load_runtime(fake_api(), game.module.base(), kCSharp, model));
  RuntimeApi partial = fake_api();
  partial.class_enum_basetype = nullptr;
  game.module = FakeModule();
  CHECK(!load_runtime(partial, game.module.base(), kCSharp, model));
}

// An assembly named as optional that the runtime has not loaded is left out of the model; a required one still fails it.
void a_missing_optional_assembly_is_left_out() {
  Game game;
  GameModel model;
  const std::vector<std::string> named{"Assembly-CSharp", "NoSuchAssembly"};
  const std::vector<std::string> optional{"NoSuchAssembly"};
  CHECK(load_runtime(fake_api(), game.module.base(), named, model, {}, optional));
  CHECK(find(model, "GhostAI") >= 0);
  for (const ModelType& t : model.types) CHECK(t.assembly == "Assembly-CSharp");
  GameModel again;
  CHECK(!load_runtime(fake_api(), game.module.base(), named, again, {}, std::vector<std::string>{"Assembly-CSharp"}));
}

// The same class, written once as the runtime serves it and once as Cpp2IL dumps it
// (tests/fixtures/parity), gives the same model.
struct Probe {
  FakeModule module;
  FakeClass probe, mode, cell;
  FakeClass monoBehaviour{"UnityEngine", "MonoBehaviour"};  // Probe's parent, outside the model's images
  // Bases outside the model's images: a generic interface (named with its arity, as the runtime names
  // it) and UnityEngine.Object, which the dump writes as `object`.
  FakeClass comparable{"System", "IComparable`1"};
  FakeClass unityObject{"UnityEngine", "Object"};
  FakeClass iprobe, inner, sensor, handle, oneFace;
  // Generic definitions, whose methods IL2CPP gives no code (the code-gen module's method table holds 0):
  // Box`1 with its nested Lid, Crate, a generic class whose name lost its arity (as the obfuscator leaves
  // some), so only its base list shows its type parameter, and Bare, whose declaration shows none at all.
  FakeClass box, lid, crate, bare;
  FakeType probeType{kClass, &probe, "Game.Probe"};
  FakeType probeRef{kClass, &probe, "Game.Probe&"};
  FakeType modeType{kValueType, &mode, "Game.Probe/Mode"};
  FakeType cellType{kValueType, &cell, "Game.Probe/Cell"};
  FakeType listType{kGenericInst, nullptr, "System.Collections.Generic.List<UnityEngine.Vector3>"};
  FakeType vector3{kValueType, nullptr, "UnityEngine.Vector3"};
  FakeType typeParameter{kVar, nullptr, "T"};
  FakeType parameterArray{kSzArray, nullptr, "T[]"};
  FakeType modeArray{kSzArray, nullptr, "Game.Probe/Mode[]"};
  FakeImage image{"Assembly-CSharp.dll", {}};
  FakeAssembly assembly{&image};
  World world;

  Probe() {
    const uintptr_t base = module.base();
    probe = {"Game", "Probe"};
    probe.parent = &monoBehaviour;
    iprobe = {"", "IProbe", &probe};
    iprobe.isInterface = true;
    inner = {"", "Inner", &probe};
    sensor = {"", "Sensor", &probe};
    sensor.parent = &inner;
    sensor.interfaces = {&iprobe, &comparable};
    oneFace = {"", "OneFace", &probe};
    oneFace.interfaces = {&iprobe};
    handle = {"", "Handle", &probe};
    handle.parent = &unityObject;
    mode = {"", "Mode", &probe, true, true, &kSByte};
    // The access bits (FieldAccessMask) are the parity dump's modifiers: 6 public, 1 private.
    mode.fields = {{"value__", &kSByte, 0x10, kPublic},
                   {"off", &modeType, 0, kFieldStatic | kFieldLiteral | kPublic, -1, 1},
                   {"low", &modeType, 0, kFieldStatic | kFieldLiteral | kPublic, 0, 1},
                   {"high", &modeType, 0, kFieldStatic | kFieldLiteral | kPublic, 5, 1}};
    cell = {"", "Cell", &probe, false, true};
    cell.fields = {{"count", &kInt, 0x10, kPublic}, {"owner", &probeType, 0x18, kPublic}};
    probe.fields = {{"awake", &kBoolean, 0x18, kPublic},
                    {"mode", &modeType, 0x19, kPrivate},
                    {"path", &listType, 0x20, kPrivate},
                    {"cell", &cellType, 0x28, kPrivate},
                    {"shared", &kInt, 0x40, kInternal},
                    {"family", &kInt, 0x44, kProtected},
                    {"either", &kInt, 0x48, kProtectedInternal},
                    {"both", &kInt, 0x4C, kPrivateProtected},
                    {"instance", &probeType, 0x0, kFieldStatic | kPublic},
                    {"limit", &kInt, 0, kFieldStatic | kFieldLiteral | kPrivate, 50, 4},
                    {"scale", &kFloat, 0, kFieldStatic | kFieldLiteral | kPrivate, 0x3727C5AC, 4},
                    {"turn", &kFloat, 0, kFieldStatic | kFieldLiteral | kPrivate, 0x43B40000, 4},
                    {"ready", &kBoolean, 0, kFieldStatic | kFieldLiteral | kPrivate, 1, 1},
                    {"start", &modeType, 0, kFieldStatic | kFieldLiteral | kPrivate, 5, 1},
                    {"letter", &kChar, 0, kFieldStatic | kFieldLiteral | kPrivate, 65, 2},
                    {"big", &kLong, 0, kFieldStatic | kFieldLiteral | kPrivate, -5, 8},
                    {"nothing", &probeType, 0, kFieldStatic | kFieldLiteral | kPrivate},
                    {"tag", &kStringType, 0, kFieldStatic | kFieldLiteral | kPublic}};
    probe.methods = {{base + 0x1200, "Find", &kBoolean, {&probeRef, &modeType}},
                     {base + 0x1300, "Pick", &modeType, {&vector3}, kMethodStatic},
                     {0, "Index", &kInt, {&parameterArray}, kMethodStatic},  // a generic method
                     {base + 0x1680, "Count", &kInt, {&modeArray}, kMethodStatic},
                     {0, "Wait", &kVoid, {&kInt}},
                     {base + 0x1400, ".ctor", &kVoid, {}},
                     {base + 0x1500, ".cctor", &kVoid, {}, kMethodStatic}};
    box = {"Game", "Box`1"};
    box.methods = {{0, "Put", &kVoid, {&typeParameter}}, {0, "Clear", &kVoid, {}}};
    lid = {"", "Lid", &box};
    lid.methods = {{0, "Close", &kVoid, {}}};
    crate = {"Game", "Crate"};
    crate.interfaces = {&comparable};
    crate.methods = {{0, "Open", &kVoid, {}}};
    bare = {"Game", "Bare"};
    bare.methods = {{0, "Shut", &kVoid, {}}};
    image.classes = {&probe, &mode, &cell, &iprobe, &inner, &sensor, &oneFace, &handle, &box, &lid, &crate, &bare};
    world.assemblies = {&assembly};
    g_world = &world;
  }
};

// The type an index names, by its name and its declaring type's, so models in different orders compare.
std::string type_key(const GameModel& m, int index) {
  if (index < 0) return "-";
  const ModelType& t = m.types[index];
  return t.assembly + "|" + t.ns + "|" + type_key(m, t.declaring) + "/" + t.name;
}

void same_model(const GameModel& a, const GameModel& b) {
  CHECK(a.types.size() == b.types.size());
  std::map<std::string, int> inB;
  for (size_t i = 0; i < b.types.size(); ++i) inB[type_key(b, static_cast<int>(i))] = static_cast<int>(i);
  for (size_t i = 0; i < a.types.size(); ++i) {
    const ModelType& ta = a.types[i];
    const std::string key = type_key(a, static_cast<int>(i));
    const auto it = inB.find(key);
    CHECK(it != inB.end());
    if (it == inB.end()) {
      std::printf("  only in the runtime model: %s\n", key.c_str());
      continue;
    }
    const ModelType& tb = b.types[it->second];
    CHECK(ta.isEnum == tb.isEnum && ta.isValueType == tb.isValueType && ta.isInterface == tb.isInterface);
    CHECK(ta.genericParams == tb.genericParams);
    CHECK(ta.fields.size() == tb.fields.size());
    for (size_t f = 0; f < ta.fields.size() && f < tb.fields.size(); ++f) {
      const ModelField& fa = ta.fields[f];
      const ModelField& fb = tb.fields[f];
      const bool same = fa.name == fb.name && fa.type == fb.type && type_key(a, fa.typeIndex) == type_key(b, fb.typeIndex) &&
                        fa.offset == fb.offset && fa.isStatic == fb.isStatic && fa.hasLiteral == fb.hasLiteral &&
                        fa.literal == fb.literal && fa.access == fb.access;
      CHECK(same);
      if (!same) std::printf("  field %s.%s differs\n", ta.name.c_str(), fa.name.c_str());
    }
    CHECK(ta.bases == tb.bases && ta.baseArities == tb.baseArities);
    if (ta.bases != tb.bases || ta.baseArities != tb.baseArities) std::printf("  bases of %s differ\n", ta.name.c_str());
    CHECK(ta.methods.size() == tb.methods.size());
    for (size_t k = 0; k < ta.methods.size() && k < tb.methods.size(); ++k) {
      const ModelMethod& ma = ta.methods[k];
      const ModelMethod& mb = tb.methods[k];
      bool same = ma.name == mb.name && ma.ret == mb.ret && ma.params == mb.params && ma.isStatic == mb.isStatic &&
                  ma.rva == mb.rva && type_key(a, ma.retIndex) == type_key(b, mb.retIndex) &&
                  ma.paramIndex.size() == mb.paramIndex.size();
      for (size_t p = 0; same && p < ma.paramIndex.size(); ++p)
        same = type_key(a, ma.paramIndex[p]) == type_key(b, mb.paramIndex[p]);
      CHECK(same);
      if (!same) std::printf("  method %s.%s differs\n", ta.name.c_str(), ma.name.c_str());
    }
  }
}

void runtime_matches_dump() {
  Probe probe;
  GameModel runtime;
  CHECK(load_runtime(fake_api(), probe.module.base(), kCSharp, runtime));
  GameModel dump;
  std::string error;
  size_t unparsed = 99;
  const auto root = std::filesystem::path(PM_SOURCE_DIR) / "tests" / "fixtures" / "parity";
  CHECK(load_dump(root, kCSharp, dump, error, &unparsed));
  CHECK(unparsed == 0);
  CHECK(runtime.types.size() == 12);
  // The code-gen module's table, by the fixture's method tokens: rows 0x6000001 upward. A row of 0 is a method
  // with no code of its own, though the dump writes an address for all but Wait.
  const MethodTables tables{{"Assembly-CSharp",
                             {0x1200, 0x1300, 0, 0x1400, 0x1500, /* Index */ 0, 0x1680, /* Put */ 0,
                              /* Close */ 0, /* Clear */ 0, /* Open */ 0, /* Shut */ 0}}};
  CHECK(method(dump.types[find(dump, "Bare")], "Shut")->rva == 0x1A00);  // the dump alone cannot tell
  // The dump writes "class Box", without its type parameter: the file's name, Box`1.cs, gives it.
  CHECK(dump.types[find(dump, "Box")].genericParams == 1 && dump.types[find(dump, "Bare")].genericParams == 0);
  apply_method_tables(tables, dump);
  same_model(runtime, dump);
  // A generic definition's methods have no address in either model, though the dump writes one for each:
  // a generic method, the methods of a generic class and of the class nested in it, and those of generic
  // classes whose names lost their arity, one even with nothing in its declaration to show it. A plain
  // method of an array of a model type keeps its own.
  for (const GameModel* m : {&runtime, &dump}) {
    const auto rva = [&](const char* type, const char* name) {
      const int t = find(*m, type);
      const ModelMethod* mm = t >= 0 ? method(m->types[t], name) : nullptr;
      return mm ? mm->rva : ~uint64_t{0};
    };
    CHECK(rva("Probe", "Index") == 0 && rva("Box", "Put") == 0 && rva("Box", "Clear") == 0);
    CHECK(rva("Lid", "Close") == 0 && rva("Crate", "Open") == 0 && rva("Bare", "Shut") == 0);
    CHECK(rva("Probe", "Count") == 0x1680 && rva("Probe", "Pick") == 0x1300);
  }
  // And both read the risky cases as meant, not merely alike.
  for (const GameModel* m : {&runtime, &dump}) {
    const int sensor = find(*m, "Sensor"), handle = find(*m, "Handle"), iprobe = find(*m, "IProbe");
    const int probeType = find(*m, "Probe");
    CHECK(sensor >= 0 && handle >= 0 && iprobe >= 0 && probeType >= 0);
    if (sensor < 0 || handle < 0 || iprobe < 0 || probeType < 0) continue;
    CHECK((m->types[sensor].bases == std::vector<std::string>{"Inner", "IProbe", "IComparable"}));
    CHECK((m->types[sensor].baseArities == std::vector<int>{0, 0, 1}));  // IComparable<Probe>
    CHECK((m->types[handle].bases == std::vector<std::string>{"Object"}));
    CHECK((m->types[probeType].bases == std::vector<std::string>{"MonoBehaviour"}));
    CHECK(m->types[iprobe].isInterface && !m->types[sensor].isInterface);
    const ModelType& p = m->types[probeType];
    const auto access = [&](const char* name) {
      for (const ModelField& f : p.fields)
        if (f.name == name) return f.access;
      return Visibility::Any;
    };
    CHECK(access("shared") == Visibility::Internal && access("family") == Visibility::Protected);
    CHECK(access("either") == Visibility::ProtectedInternal && access("both") == Visibility::PrivateProtected);
    CHECK(access("awake") == Visibility::Public && access("mode") == Visibility::Private);
  }
  // An exact field list (kExactFields) judges both models alike: Probe declares eight instance fields,
  // its statics and constants not among them, so the full list finds it and one short finds nothing.
  static constexpr const char* kAll[] = {"bool", "Mode", "List<Vector3>", "Cell", "int", "int", "int", "int"};
  static constexpr const char* kShort[] = {"bool", "Mode", "List<Vector3>", "Cell", "int", "int", "int"};
  constexpr EntryDesc all{.key = "All", .flags = kExactFields, .assembly = "Assembly-CSharp", .ns = "Game",
                          .fieldTypes = kAll};
  constexpr EntryDesc oneShort{.key = "Short", .flags = kExactFields, .assembly = "Assembly-CSharp", .ns = "Game",
                               .fieldTypes = kShort};
  // kFieldOrder also reads the offsets: the list in memory order is found, the same types in another order are not.
  static constexpr const char* kSwapped[] = {"Mode", "bool", "List<Vector3>", "Cell", "int", "int", "int", "int"};
  constexpr EntryDesc inOrder{.key = "InOrder", .flags = kExactFields | kFieldOrder, .assembly = "Assembly-CSharp",
                              .ns = "Game", .fieldTypes = kAll};
  constexpr EntryDesc swapped{.key = "Swapped", .flags = kExactFields | kFieldOrder, .assembly = "Assembly-CSharp",
                              .ns = "Game", .fieldTypes = kSwapped};
  const EntryDesc* const exact[] = {&all, &oneShort, &inOrder, &swapped};
  for (const GameModel* m : {&runtime, &dump}) {
    const Resolution r = resolve(exact, *m, nullptr);
    const Result* a = r.find("All");
    const Result* s = r.find("Short");
    CHECK(a && a->status == Status::Resolved && a->type == find(*m, "Probe"));
    CHECK(s && s->status == Status::Missing);
    const Result* o = r.find("InOrder");
    const Result* w = r.find("Swapped");
    CHECK(o && o->status == Status::Resolved && o->type == find(*m, "Probe"));
    CHECK(w && w->status == Status::Missing);
  }
  // A type found as the one interface another declares reads both models' base lists alike, and a base no model
  // type has (Sensor's IComparable) decides nothing in either.
  constexpr EntryDesc owner{.key = "Owner", .name = "Probe", .assembly = "Assembly-CSharp", .ns = "Game"};
  constexpr EntryDesc face{.key = "Face", .owner = "Owner", .name = "OneFace"};
  constexpr EntryDesc declared{.key = "Declared", .owner = "Owner", .within = "Face"};
  constexpr EntryDesc sensorType{.key = "SensorType", .owner = "Owner", .name = "Sensor"};
  constexpr EntryDesc unknownBase{.key = "UnknownBase", .owner = "Owner", .within = "SensorType"};
  // A generic instance's method, of a generic class and a generic method: both models find the definition and the
  // type argument, and neither claims the instance, whose code only the running game has.
  static constexpr const char* kT[] = {"T"};
  static constexpr const char* kTArray[] = {"T[]"};
  static constexpr const char* kModeArg[] = {"Mode"};
  static constexpr const char* kNoArg[] = {"NoSuchType"};
  constexpr EntryDesc boxType{.key = "BoxType", .name = "Box", .assembly = "Assembly-CSharp", .ns = "Game"};
  constexpr EntryDesc ofClass{.key = "OfClass", .kind = Kind::Inflated, .owner = "BoxType", .name = "Put", .type = "void",
                              .params = kT, .typeArgs = kModeArg};
  constexpr EntryDesc ofMethod{.key = "OfMethod", .kind = Kind::Inflated, .flags = kStatic | kGenericMethod,
                               .owner = "Owner", .name = "Index", .type = "int", .params = kTArray, .typeArgs = kModeArg};
  constexpr EntryDesc noArg{.key = "NoArg", .kind = Kind::Inflated, .owner = "BoxType", .name = "Put", .params = kT,
                            .typeArgs = kNoArg};
  const EntryDesc* const shapes[] = {&owner, &face, &declared, &sensorType, &unknownBase, &boxType, &ofClass, &ofMethod, &noArg};
  for (const GameModel* m : {&runtime, &dump}) {
    const Resolution r = resolve(shapes, *m, nullptr);
    const Result* d = r.find("Declared");
    CHECK(d && d->status == Status::Resolved && d->type == find(*m, "IProbe"));
    CHECK(r.find("UnknownBase")->status == Status::Missing);
    for (const char* key : {"OfClass", "OfMethod"}) {
      const Result* i = r.find(key);
      CHECK(i && checked_at_startup(*i) && i->status != Status::Resolved);
      CHECK(i && i->typeArgs == std::vector<int>{find(*m, "Mode")});
      CHECK(i && i->member >= 0 && i->address == 0);
    }
    CHECK(r.find("OfClass")->type == find(*m, "Box") && r.find("OfMethod")->type == find(*m, "Probe"));
    CHECK(!checked_at_startup(*r.find("NoArg")));
  }
  // Both read a value only for an integer or enum constant: a char written as 65 stays 0.
  const int probeType = find(dump, "Probe");
  if (probeType < 0) return;
  const ModelType& p = dump.types[probeType];
  CHECK(field(p, "big") && field(p, "big")->literal == -5);
  CHECK(field(p, "start") && field(p, "start")->literal == 5);
  CHECK(field(p, "letter") && field(p, "letter")->literal == 0);
  CHECK(field(p, "nothing") && field(p, "nothing")->literal == 0);
}

}  // namespace

int main() {
  enumerates_types_methods_fields();
  enum_literals_read_through_static_value();
  byte_enum_is_zero_extended();
  pointer_outside_module_gets_zero_rva();
  types_are_canonical();
  bad_module_or_missing_assembly_fails();
  a_missing_optional_assembly_is_left_out();
  runtime_matches_dump();
  return g_failures ? 1 : 0;
}
