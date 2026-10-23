#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "model.h"

namespace pg::sdk {

// The IL2CPP exports the runtime model reads, as GameAssembly.dll exports them. The handles are the
// runtime's metadata (Il2CppDomain*, Il2CppAssembly*, Il2CppImage*, Il2CppClass*, MethodInfo*,
// FieldInfo*, Il2CppType*), never managed objects.
struct RuntimeApi {
  void* (*domain_get)() = nullptr;
  void** (*domain_get_assemblies)(void* domain, size_t* count) = nullptr;
  void* (*assembly_get_image)(void* assembly) = nullptr;
  const char* (*image_get_name)(void* image) = nullptr;
  size_t (*image_get_class_count)(const void* image) = nullptr;
  void* (*image_get_class)(const void* image, size_t index) = nullptr;
  const char* (*class_get_name)(void* klass) = nullptr;
  const char* (*class_get_namespace)(void* klass) = nullptr;
  void* (*class_get_declaring_type)(void* klass) = nullptr;
  bool (*class_is_enum)(const void* klass) = nullptr;
  bool (*class_is_valuetype)(const void* klass) = nullptr;
  const void* (*class_enum_basetype)(void* klass) = nullptr;
  void* (*class_from_type)(const void* type) = nullptr;
  void* (*class_get_fields)(void* klass, void** iter) = nullptr;
  const void* (*class_get_methods)(void* klass, void** iter) = nullptr;
  const char* (*method_get_name)(const void* method) = nullptr;
  uint32_t (*method_get_param_count)(const void* method) = nullptr;
  const void* (*method_get_param)(const void* method, uint32_t index) = nullptr;
  const void* (*method_get_return_type)(const void* method) = nullptr;
  uint32_t (*method_get_flags)(const void* method, uint32_t* implFlags) = nullptr;
  const char* (*field_get_name)(void* field) = nullptr;
  size_t (*field_get_offset)(void* field) = nullptr;
  int (*field_get_flags)(void* field) = nullptr;
  const void* (*field_get_type)(void* field) = nullptr;
  void (*field_static_get_value)(void* field, void* value) = nullptr;
  int (*type_get_type)(const void* type) = nullptr;
  char* (*type_get_name)(const void* type) = nullptr;  // freed with free_memory
  void (*free_memory)(void* memory) = nullptr;
};

// Builds the game model from the running game's metadata, for the named assemblies (image names
// without ".dll"), and agrees with load_dump on the same build:
// - A type's name is the runtime's, less a generic arity ("List`1" is "List"); a nested type takes its
//   outermost declaring type's namespace, as the dump's folders give it.
// - Member type strings are canonical. typeIndex, retIndex and paramIndex name the exact class a type
//   is (by reference or not) when it is a class or struct in the model, and are -1 otherwise.
// - Offsets are il2cpp_field_get_offset's: a value type's instance fields include the object header.
// - A literal field is static. An integer or enum constant's literal is its number, read through
//   il2cpp_field_static_get_value; any other constant's literal is 0 (its value is never read, so no
//   string is made). A constant has no storage, and its offset is 0.
// - An enum's instance field value__ is left out: the dump does not list it.
// - A method's rva is its code's distance from gameAssemblyBase when the code lies inside the module
//   the headers at that base describe, and 0 otherwise (no code, or shared code elsewhere).
// It fails when the headers at gameAssemblyBase are not a PE32+ image's, an export is missing, or a
// named assembly is not loaded. It reads the runtime's metadata and makes no managed object.
bool load_runtime(const RuntimeApi& api, uintptr_t gameAssemblyBase, std::span<const std::string> assemblies,
                  GameModel& out);

// The loaded module at base, from its first byte to SizeOfImage, read from its PE32+ headers. Empty
// when the headers are not a PE32+ image's.
std::span<const uint8_t> loaded_module(uintptr_t base);

}  // namespace pg::sdk
