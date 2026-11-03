#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "model.h"

namespace pm::sdk {

// A generic instance's method as the runtime made it: its MethodInfo*, the code and invoker that MethodInfo holds,
// and the class it belongs to (the inflated class, or the generic method's owner).
struct InflatedMethod {
  const void* method = nullptr;
  const void* code = nullptr;
  const void* invoker = nullptr;
  void* klass = nullptr;
};

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
  void* (*class_get_parent)(void* klass) = nullptr;
  void* (*class_get_interfaces)(void* klass, void** iter) = nullptr;
  bool (*class_is_interface)(const void* klass) = nullptr;
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
  // Not read by the model: the map's icall entries resolve through it (host/game_map.cpp). It looks
  // the name up among the icalls the engine registered, and answers null for an unknown one.
  void* (*resolve_icall)(const char* name) = nullptr;
  // Not read by the model either: the map's Inflated entries resolve through it (host/game_map.cpp).
  // Makes `method` of the class `klass` for the type arguments `args` (Il2CppClass*): the class's, or
  // with ofMethod the method's own. False when the runtime would not make it. On the main thread; it
  // runs managed reflection.
  bool (*inflate)(void* klass, const void* method, bool ofMethod, void* const* args, size_t count,
                  InflatedMethod& out) = nullptr;
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
// - A type's bases are its parent's and its interfaces' short names, less System.Object,
//   System.ValueType and System.Enum, as the dump's base list gives them; an enum has none. Whether it
//   is an interface is il2cpp_class_is_interface's answer, as the dump's `interface` keyword gives it.
// - A method's rva is its code's distance from gameAssemblyBase when the code lies inside the module
//   the headers at that base describe, and 0 otherwise (no code, or shared code elsewhere).
// It fails when the headers at gameAssemblyBase are not a PE32+ image's, an export is missing, or a
// named assembly is not loaded, and as soon as `proceed`, asked before each type, says no. It reads the
// runtime's metadata and makes no managed object. On failure `out` is left as it was.
bool load_runtime(const RuntimeApi& api, uintptr_t gameAssemblyBase, std::span<const std::string> assemblies,
                  GameModel& out, const std::function<bool()>& proceed = {});

// The loaded module at base, from its first byte to SizeOfImage, read from its PE32+ headers. Empty
// when the headers are not a PE32+ image's.
std::span<const uint8_t> loaded_module(uintptr_t base);

}  // namespace pm::sdk
