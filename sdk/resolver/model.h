#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../api/entry.h"  // Visibility; by path, for the targets that build the model without sdk/api

namespace pm::sdk {

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
  Visibility access = Visibility::Any;  // as declared; Any when the source does not say
  int64_t literal = 0;
  void* runtime = nullptr;  // the FieldInfo* in the host, null offline
};

struct ModelMethod {
  std::string name, ret;
  std::vector<std::string> params;
  int retIndex = -1;
  std::vector<int> paramIndex;  // one per param
  bool isStatic = false;
  uint64_t rva = 0;
  const void* runtime = nullptr;  // the MethodInfo* in the host, null offline
  uint32_t token = 0;             // its metadata token, read from a dump, 0 in the host
};

struct ModelType {
  // name is kept as the runtime writes it: compiler-generated names ("<>c__DisplayClass5_0") as written,
  // a generic definition without its arity, never canonicalised.
  std::string assembly, ns, name;
  int declaring = -1;  // index of the declaring type for a nested type
  bool isEnum = false;
  bool isValueType = false;  // a struct or an enum
  bool isInterface = false;
  // A generic definition's number of type parameters ("UnityEvent`1" is 1), 0 for any other type: what tells
  // UnityEvent from UnityEvent<T0>, which the name above does not. The dump knows it for a top-level type only (by
  // its file's name), and gives a nested type 0.
  int genericParams = 0;
  std::vector<ModelField> fields;
  std::vector<ModelMethod> methods;
  // The short names (canonical, without generic arguments) of the class it derives from and of the
  // interfaces it declares, as its declaration lists them. System.Object, System.ValueType and
  // System.Enum, which the dump never writes, are left out.
  std::vector<std::string> bases;
  void* runtime = nullptr;  // the Il2CppClass* in the host, null offline
};

struct GameModel {
  std::vector<ModelType> types;
  // The Cpp2IL dump writes System.Object[] and UnityEngine.Object[] alike, as Object[], so a model read from it cannot tell them
  // apart. The running game can, so there an entry's object[] means System.Object[] alone.
  bool objectArraysLookAlike = false;
};

}  // namespace pm::sdk
