#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pg::sdk {

// The game's types as the resolver sees them, built from the runtime's metadata in the host and from
// the Cpp2IL dump offline. Every type string here is already canonical (see canonical_type).
//
// Beside each type string, an index into GameModel::types says which type it names when the source
// knows it, -1 otherwise. It names the type itself, by reference or not ("Player" and "Player&"), and
// is -1 for any other form (an array, a generic). The canonical short name alone cannot tell apart two
// types that share it, so an "@Key" type in an entry needs the index whenever the name is not unique.

struct ModelField {
  std::string name, type;
  int typeIndex = -1;
  int32_t offset = 0;  // as il2cpp_field_get_offset reports: a value type's instance fields include the
                       // 0x10 object header, so Vector3.x is at 0x10
  bool isStatic = false;
  bool hasLiteral = false;  // an enum value or a constant: literal holds it
  int64_t literal = 0;
};

struct ModelMethod {
  std::string name, ret;
  std::vector<std::string> params;
  int retIndex = -1;
  std::vector<int> paramIndex;  // one per param
  bool isStatic = false;
  uint64_t rva = 0;
  const void* runtime = nullptr;  // the MethodInfo* in the host, null offline
};

struct ModelType {
  // name is kept as the runtime writes it: compiler-generated names ("<>c__DisplayClass5_0") as written,
  // a generic definition without its arity, never canonicalised.
  std::string assembly, ns, name;
  int declaring = -1;  // index of the declaring type for a nested type
  bool isEnum = false;
  bool isValueType = false;  // a struct or an enum
  std::vector<ModelField> fields;
  std::vector<ModelMethod> methods;
  void* runtime = nullptr;  // the Il2CppClass* in the host, null offline
};

struct GameModel {
  std::vector<ModelType> types;
};

}  // namespace pg::sdk
