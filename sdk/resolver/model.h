#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pg::sdk {

// The game's types as the resolver sees them, built from the runtime's metadata in the host and from
// the Cpp2IL dump offline. Every type string here is already canonical (see canonical_type).

struct ModelField {
  std::string name, type;
  int32_t offset = 0;
  bool isStatic = false;
  bool hasLiteral = false;  // an enum value or a constant: literal holds it
  int64_t literal = 0;
};

struct ModelMethod {
  std::string name, ret;
  std::vector<std::string> params;
  bool isStatic = false;
  uint64_t rva = 0;
  const void* runtime = nullptr;  // the MethodInfo* in the host, null offline
};

struct ModelType {
  std::string assembly, ns, name;
  int declaring = -1;  // index of the declaring type for a nested type
  bool isEnum = false;
  std::vector<ModelField> fields;
  std::vector<ModelMethod> methods;
  void* runtime = nullptr;  // the Il2CppClass* in the host, null offline
};

struct GameModel {
  std::vector<ModelType> types;
};

}  // namespace pg::sdk
