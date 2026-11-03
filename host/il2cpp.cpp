#include "il2cpp.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "log.h"
#include "runtime_model.h"
#include "startup.h"
#include "utf16.h"

namespace il2cpp {
namespace {

#define API(ret, name, args) using name##_t = ret(*) args; name##_t p_##name = nullptr;
API(void*, il2cpp_domain_get, ())
API(void**, il2cpp_domain_get_assemblies, (void* domain, size_t* count))
API(void*, il2cpp_assembly_get_image, (void* assembly))
API(const char*, il2cpp_image_get_name, (void* image))
API(void*, il2cpp_class_from_name, (void* image, const char* ns, const char* name))
API(const void*, il2cpp_class_get_method_from_name, (void* klass, const char* name, int argc))
API(void*, il2cpp_class_get_parent, (void* klass))
API(void*, il2cpp_class_get_fields, (void* klass, void** iter))
API(void*, il2cpp_class_get_field_from_name, (void* klass, const char* name))
API(void, il2cpp_field_get_value, (void* obj, void* field, void* value))
API(void, il2cpp_field_set_value, (void* obj, void* field, void* value))
API(const void*, il2cpp_field_get_type, (void* field))
API(char*, il2cpp_type_get_name, (const void* type))
API(void, il2cpp_field_static_get_value, (void* field, void* value))
API(void, il2cpp_field_static_set_value, (void* field, void* value))
API(void, il2cpp_runtime_class_init, (void* klass))
API(void*, il2cpp_class_from_type, (const void* type))
API(bool, il2cpp_class_is_valuetype, (const void* klass))
API(bool, il2cpp_class_is_enum, (const void* klass))
API(int32_t, il2cpp_class_value_size, (void* klass, uint32_t* align))
API(void*, il2cpp_thread_attach, (void* domain))
API(void, il2cpp_thread_detach, (void* thread))
API(void*, il2cpp_string_new, (const char* str))
API(void*, il2cpp_object_new, (void* klass))
API(void*, il2cpp_array_new, (void* elementClass, uintptr_t length))
API(void*, il2cpp_object_get_class, (void* obj))
API(const char*, il2cpp_class_get_name, (void* klass))
API(void*, il2cpp_resolve_icall, (const char* name))
API(void, il2cpp_free, (void* ptr))
API(void*, il2cpp_get_corlib, ())
API(void*, il2cpp_array_class_get, (void* elementClass, uint32_t rank))
API(const void*, il2cpp_class_get_type, (void* klass))
API(void*, il2cpp_type_get_object, (const void* type))
API(void, il2cpp_gc_wbarrier_set_field, (void* obj, void** field, void* value))
API(void*, il2cpp_get_exception_argument_null, (const char* arg))
API(void, il2cpp_raise_exception, (void* exception))
API(const void*, il2cpp_object_get_virtual_method, (void* obj, const void* method))
API(void, il2cpp_format_stack_trace, (const void* exception, char* output, int size))
API(uint32_t, il2cpp_gchandle_new, (void* obj, bool pinned))
API(void, il2cpp_gchandle_free, (uint32_t handle))
API(void*, il2cpp_class_get_element_class, (void* klass))
API(const char*, il2cpp_class_get_namespace, (void* klass))
API(int, il2cpp_class_get_rank, (const void* klass))
API(int, il2cpp_field_get_flags, (void* field))
API(size_t, il2cpp_field_get_offset, (void* field))
API(void*, il2cpp_class_from_system_type, (void* type))
API(void*, il2cpp_method_get_object, (const void* method, void* refClass))
API(const void*, il2cpp_method_get_from_reflection, (const void* method))
API(const void*, il2cpp_class_get_methods, (void* klass, void** iter))
API(const char*, il2cpp_method_get_name, (const void* method))
API(uint32_t, il2cpp_method_get_param_count, (const void* method))
#undef API

void* g_thread = nullptr;

template <class T>
bool resolve(HMODULE module, T& out, const char* name) {
  out = reinterpret_cast<T>(GetProcAddress(module, name));
  if (!out) PM_LOG("il2cpp: missing export %s", name);
  return out != nullptr;
}

}  // namespace

/** Resolve the runtime's exported API (no waiting, no thread attachment). */
bool resolve_exports(void* gameAssembly) {
  auto m = static_cast<HMODULE>(gameAssembly);
  bool ok = true;
#define R(name) ok &= resolve(m, p_##name, #name);
  R(il2cpp_domain_get) R(il2cpp_domain_get_assemblies) R(il2cpp_assembly_get_image)
  R(il2cpp_image_get_name) R(il2cpp_class_from_name) R(il2cpp_class_get_method_from_name)
  R(il2cpp_class_get_parent) R(il2cpp_class_get_fields) R(il2cpp_field_get_type)
  R(il2cpp_class_get_field_from_name) R(il2cpp_field_get_value) R(il2cpp_field_set_value)
  R(il2cpp_type_get_name) R(il2cpp_field_static_get_value) R(il2cpp_thread_attach)
  R(il2cpp_thread_detach) R(il2cpp_string_new) R(il2cpp_object_new) R(il2cpp_array_new)
  R(il2cpp_object_get_class) R(il2cpp_class_get_name) R(il2cpp_resolve_icall) R(il2cpp_free)
  R(il2cpp_get_corlib) R(il2cpp_gc_wbarrier_set_field) R(il2cpp_array_class_get) R(il2cpp_class_get_type) R(il2cpp_type_get_object)
  R(il2cpp_get_exception_argument_null) R(il2cpp_raise_exception) R(il2cpp_object_get_virtual_method)
  R(il2cpp_gchandle_new) R(il2cpp_gchandle_free) R(il2cpp_class_get_element_class)
  R(il2cpp_class_get_namespace) R(il2cpp_class_get_rank) R(il2cpp_field_static_set_value)
  R(il2cpp_runtime_class_init) R(il2cpp_class_from_type) R(il2cpp_class_is_valuetype) R(il2cpp_class_is_enum) R(il2cpp_class_value_size)
  R(il2cpp_field_get_flags) R(il2cpp_field_get_offset) R(il2cpp_class_from_system_type) R(il2cpp_method_get_object)
  R(il2cpp_method_get_from_reflection) R(il2cpp_class_get_methods) R(il2cpp_method_get_name) R(il2cpp_method_get_param_count)
#undef R
  if (!ok) return false;
  p_il2cpp_format_stack_trace = reinterpret_cast<decltype(p_il2cpp_format_stack_trace)>(
      GetProcAddress(m, "il2cpp_format_stack_trace"));  // optional: diagnostics only
  return true;
}

/** The exports the game model reads: those already resolved, and the metadata readers only it uses. */
bool runtime_api(void* gameAssembly, pm::sdk::RuntimeApi& out) {
  auto m = static_cast<HMODULE>(gameAssembly);
  out.domain_get = p_il2cpp_domain_get;
  out.domain_get_assemblies = p_il2cpp_domain_get_assemblies;
  out.assembly_get_image = p_il2cpp_assembly_get_image;
  out.image_get_name = p_il2cpp_image_get_name;
  out.class_get_name = p_il2cpp_class_get_name;
  out.class_get_fields = p_il2cpp_class_get_fields;
  out.field_get_type = p_il2cpp_field_get_type;
  out.field_static_get_value = p_il2cpp_field_static_get_value;
  out.type_get_name = p_il2cpp_type_get_name;
  out.free_memory = p_il2cpp_free;
  out.resolve_icall = p_il2cpp_resolve_icall;
  out.inflate = &inflate;
  bool ok = true;
#define R(field, name) ok &= resolve(m, out.field, #name);
  R(image_get_class_count, il2cpp_image_get_class_count) R(image_get_class, il2cpp_image_get_class)
  R(class_get_namespace, il2cpp_class_get_namespace) R(class_get_declaring_type, il2cpp_class_get_declaring_type)
  R(class_is_enum, il2cpp_class_is_enum) R(class_is_valuetype, il2cpp_class_is_valuetype)
  R(class_enum_basetype, il2cpp_class_enum_basetype) R(class_from_type, il2cpp_class_from_type)
  R(class_get_parent, il2cpp_class_get_parent) R(class_get_interfaces, il2cpp_class_get_interfaces)
  R(class_is_interface, il2cpp_class_is_interface)
  R(class_get_methods, il2cpp_class_get_methods) R(method_get_name, il2cpp_method_get_name)
  R(method_get_param_count, il2cpp_method_get_param_count) R(method_get_param, il2cpp_method_get_param)
  R(method_get_return_type, il2cpp_method_get_return_type) R(method_get_flags, il2cpp_method_get_flags)
  R(field_get_name, il2cpp_field_get_name) R(field_get_offset, il2cpp_field_get_offset)
  R(field_get_flags, il2cpp_field_get_flags) R(type_get_type, il2cpp_type_get_type)
#undef R
  return ok;
}

/** Resolve the runtime API and attach this worker only while startup is still permitted. */
bool init(void* gameAssembly, void* stopEvent, unsigned long long deadline) {
  if (!pm::startup::permitted(stopEvent, deadline, "runtime")) return false;
  if (!resolve_exports(gameAssembly)) return false;
  // When loaded at process start the runtime is not up yet. il2cpp_get_corlib is a plain read
  // of il2cpp_defaults.corlib (no lazy initialisation), set once il2cpp_init has loaded images.
  if (!pm::startup::wait(stopEvent, deadline, "runtime", [] { return p_il2cpp_get_corlib() != nullptr; }))
    return false;
  g_thread = p_il2cpp_thread_attach(p_il2cpp_domain_get());
  if (!g_thread) PM_LOG("ModHost inactive: runtime thread attachment failed");
  return g_thread != nullptr;
}

void detach() {
  if (g_thread) p_il2cpp_thread_detach(g_thread);
  g_thread = nullptr;
}

Image* image(const char* fileName) {
  size_t count = 0;
  void** assemblies = p_il2cpp_domain_get_assemblies(p_il2cpp_domain_get(), &count);
  for (size_t i = 0; i < count; ++i) {
    void* img = p_il2cpp_assembly_get_image(assemblies[i]);
    if (img && std::strcmp(p_il2cpp_image_get_name(img), fileName) == 0) return img;
  }
  return nullptr;
}

Class* cls(const char* fileName, const char* ns, const char* name) {
  void* img = image(fileName);
  void* klass = img ? p_il2cpp_class_from_name(img, ns, name) : nullptr;
  if (!klass) PM_LOG("il2cpp: class %s.%s not found in %s", ns, name, fileName);
  return klass;
}

const Method* method(Class* klass, const char* name, int argc) {
  for (void* k = klass; k; k = p_il2cpp_class_get_parent(k)) {
    if (const void* m = p_il2cpp_class_get_method_from_name(k, name, argc)) return m;
  }
  PM_LOG("il2cpp: method %s/%d not found on %s", name, argc,
         klass ? p_il2cpp_class_get_name(klass) : "(null)");
  return nullptr;
}

void* method_ptr(Class* klass, const char* name, int argc) {
  const Method* m = method(klass, name, argc);
  return m ? *reinterpret_cast<void* const*>(m) : nullptr;
}

namespace {

// Unity 2022.3's MethodInfo (metadata v29 and later, x64), as a delegate's constructor reads it (decompiled):
// { methodPointer, virtualMethodPointer, invoker_method, name, klass, return_type, parameters, rgctx_data or
// methodMetadataHandle, genericMethod or genericContainerHandle, token (uint32), flags (uint16), iflags (uint16),
// slot (uint16), parameters_count (uint8), bit fields (uint8) }. The constructor takes the code from
// virtualMethodPointer and asks whether the method is static from flags' 0x10 (METHOD_ATTRIBUTE_STATIC); with
// parameters_count 0 a static method would be invoked without its target.
constexpr size_t kMethodInfoSize = 0x58;
constexpr size_t kMethodPointer = 0x00, kVirtualMethodPointer = 0x08, kMethodFlags = 0x4C;
constexpr uint16_t kMethodStatic = 0x10;

}  // namespace

const Method* native_method(Class* delegateClass, const Method* invoke, void* code) {
  if (!delegateClass || !invoke || !code) return nullptr;
  // Made once per delegate class and code, and kept for the process's life: a delegate holds the pointer for as long
  // as it lives, which the host does not decide.
  static std::map<std::pair<Class*, void*>, std::unique_ptr<uint8_t[]>> made;
  if (const auto it = made.find({delegateClass, code}); it != made.end()) return it->second.get();
  auto info = std::make_unique<uint8_t[]>(kMethodInfoSize);
  // The delegate's own Invoke gives its name, class, return type and parameters, so the runtime reads a whole
  // method wherever it looks; only its code and that it is an instance method are the host's.
  std::memcpy(info.get(), invoke, kMethodInfoSize);
  std::memcpy(info.get() + kMethodPointer, &code, sizeof code);
  std::memcpy(info.get() + kVirtualMethodPointer, &code, sizeof code);
  uint16_t flags = 0;
  std::memcpy(&flags, info.get() + kMethodFlags, sizeof flags);
  flags = static_cast<uint16_t>(flags & ~kMethodStatic);
  std::memcpy(info.get() + kMethodFlags, &flags, sizeof flags);
  const Method* const out = info.get();
  made.emplace(std::make_pair(delegateClass, code), std::move(info));
  return out;
}

const Method* virtual_method(void* obj, const Method* method) {
  return obj && method ? p_il2cpp_object_get_virtual_method(obj, method) : nullptr;
}

/** Keep instance bindings tied to field metadata rather than managed object offsets. */
Field* field_info(Class* klass, const char* name) {
  for (void* k = klass; k; k = p_il2cpp_class_get_parent(k)) {
    if (void* field = p_il2cpp_class_get_field_from_name(k, name)) return field;
  }
  PM_LOG("il2cpp: field %s not found on %s", name, klass ? p_il2cpp_class_get_name(klass) : "(null)");
  return nullptr;
}

/** A missing object or binding cannot be dereferenced while the lobby is loading. */
void* reference_field(void* obj, Field* field) {
  void* value = nullptr;
  if (obj && field) p_il2cpp_field_get_value(obj, field, &value);
  return value;
}

bool is_class(const void* obj, const char* name) {
  const char* actual = obj ? class_name(class_of(obj)) : nullptr;
  return actual && std::strcmp(actual, name) == 0;
}

bool is_instance(const void* obj, Class* klass) {
  if (!obj || !klass) return false;
  for (void* k = class_of(obj); k; k = p_il2cpp_class_get_parent(k))
    if (k == klass) return true;
  return false;
}

bool is_string(const void* obj) {
  static Class* const string = cls("mscorlib.dll", "System", "String");
  return obj && string && class_of(obj) == string;  // System.String is sealed
}

bool is_array(const void* obj) { return obj && p_il2cpp_class_get_rank(class_of(obj)) > 0; }

bool is_list(const void* obj) {
  // A constructed List<T> keeps the generic definition's name and namespace.
  Class* const klass = obj ? class_of(obj) : nullptr;
  return klass && std::strcmp(p_il2cpp_class_get_name(klass), "List`1") == 0 &&
         std::strcmp(p_il2cpp_class_get_namespace(klass), "System.Collections.Generic") == 0;
}

bool is_counted_collection(const void* obj) {
  Class* const klass = obj ? class_of(obj) : nullptr;
  if (!klass || std::strcmp(p_il2cpp_class_get_namespace(klass), "System.Collections.Generic") != 0) return false;
  const char* const name = p_il2cpp_class_get_name(klass);
  for (const char* counted : {"List`1", "HashSet`1", "Dictionary`2", "Queue`1", "Stack`1"})
    if (std::strcmp(name, counted) == 0) return true;
  return false;
}

size_t array_element_size(const void* arr) {
  void* const element = is_array(arr) ? p_il2cpp_class_get_element_class(class_of(arr)) : nullptr;
  if (!element) return 0;
  if (!p_il2cpp_class_is_valuetype(element)) return sizeof(void*);
  const int32_t size = p_il2cpp_class_value_size(element, nullptr);
  return size > 0 ? static_cast<size_t>(size) : 0;
}

bool is_kind(const void* obj, const char* name) {
  for (void* k = obj ? class_of(obj) : nullptr; k; k = p_il2cpp_class_get_parent(k))
    if (std::strcmp(p_il2cpp_class_get_name(k), name) == 0) return true;
  return false;
}

void read_field(void* obj, Field* field, void* out) {
  if (obj && field) p_il2cpp_field_get_value(obj, field, out);
}

/**
 * Store through IL2CPP's write barrier. For a reference-type field this build's
 * il2cpp_field_set_value stores `value` itself (GameAssembly passes it straight to
 * the barrier), unlike il2cpp_field_get_value, which writes through its pointer argument.
 */
void set_reference_field(void* obj, Field* field, void* value) {
  if (obj && field) p_il2cpp_field_set_value(obj, field, value);
}

void* static_field_by_type(Class* klass, const char* typeName) {
  void* iter = nullptr;
  while (void* f = p_il2cpp_class_get_fields(klass, &iter)) {
    char* tn = p_il2cpp_type_get_name(p_il2cpp_field_get_type(f));
    bool match = tn && std::strcmp(tn, typeName) == 0;
    if (tn) p_il2cpp_free(tn);
    if (!match) continue;
    void* value = nullptr;
    p_il2cpp_field_static_get_value(f, &value);
    return value;
  }
  return nullptr;
}

namespace {

// The bytes a field's value takes: a pointer for a managed reference, the value's own size for a
// value type. 0 when the runtime does not know the field's class.
size_t value_size(Field* field, bool& reference) {
  void* const klass = p_il2cpp_class_from_type(p_il2cpp_field_get_type(field));
  if (!klass) return 0;
  reference = !p_il2cpp_class_is_valuetype(klass);
  if (reference) return sizeof(void*);
  const int32_t size = p_il2cpp_class_value_size(klass, nullptr);
  return size > 0 ? static_cast<size_t>(size) : 0;
}

}  // namespace

// The size is checked first: the field type's layout is known from the metadata the game map read at
// startup, and a refused access must not run the owner's static constructor.
bool static_get(Class* klass, Field* field, void* out, size_t size) {
  if (!klass || !field || !out) return false;
  bool reference = false;
  if (value_size(field, reference) != size) return false;
  p_il2cpp_runtime_class_init(klass);  // runs the static constructor once, as the game's first use does
  p_il2cpp_field_static_get_value(field, out);
  return true;
}

bool static_set(Class* klass, Field* field, const void* in, size_t size) {
  if (!klass || !field || !in) return false;
  bool reference = false;
  if (value_size(field, reference) != size) return false;
  p_il2cpp_runtime_class_init(klass);
  // As il2cpp_field_set_value (set_reference_field): a reference is passed itself, and stored through
  // the GC write barrier; a value type's bytes are passed by address. This build's static store
  // (GameAssembly) takes the same path.
  void* const value = reference ? *static_cast<void* const*>(in) : const_cast<void*>(in);
  p_il2cpp_field_static_set_value(field, value);
  return true;
}

void format_stack_trace(const void* exception, char* out, size_t size) {
  if (size) out[0] = 0;
  if (exception && p_il2cpp_format_stack_trace && size) p_il2cpp_format_stack_trace(exception, out, static_cast<int>(size));
}

void log_exception(const char* where, const void* exception) {
  char message[192] = "";
  // System.Exception: _message at +0x18 (after the object header and _className).  pm:not-game-data: the runtime's own layout
  if (exception) to_utf8(field<String*>(exception, 0x18), message, sizeof message);
  PM_LOG("%s: %s: %s", where, exception ? class_name(class_of(exception)) : "?", message);
  char stack[2048];
  format_stack_trace(exception, stack, sizeof stack);
  for (char* line = stack; line && *line;) {
    char* next = std::strchr(line, '\n');
    if (next) *next++ = 0;
    if (*line) PM_LOG("%s:   %s", where, line);
    line = next;
  }
}

String* string_new(const char* utf8) { return static_cast<String*>(p_il2cpp_string_new(utf8)); }
Object* object_new(Class* klass) { return static_cast<Object*>(p_il2cpp_object_new(klass)); }
Class* element_class(Class* arrayClass) {
  return arrayClass ? static_cast<Class*>(p_il2cpp_class_get_element_class(arrayClass)) : nullptr;
}
uint32_t gc_hold(void* object) { return object ? p_il2cpp_gchandle_new(object, false) : 0; }
void gc_release(uint32_t handle) {
  // A Held in a static object is released by its destructor when the process exits, after the game's
  // il2cpp_shutdown: the exiting process keeps the handle instead of calling into the stopped runtime.
  using ShutdownInProgress = BOOLEAN(NTAPI*)();
  static const auto exiting = reinterpret_cast<ShutdownInProgress>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlDllShutdownInProgress"));
  if (handle && !(exiting && exiting())) p_il2cpp_gchandle_free(handle);
}
Array* array_new(Class* elementClass, uintptr_t length) {
  return static_cast<Array*>(p_il2cpp_array_new(elementClass, length));
}
Object* type_object(Class* klass) {
  return klass ? static_cast<Object*>(p_il2cpp_type_get_object(p_il2cpp_class_get_type(klass))) : nullptr;
}
Class* class_of_array(Class* elementClass) { return p_il2cpp_array_class_get(elementClass, 1); }
Class* class_of(const void* obj) { return p_il2cpp_object_get_class(const_cast<void*>(obj)); }
const char* class_name(Class* klass) { return p_il2cpp_class_get_name(klass); }
void set_ref_field(void* obj, size_t offset, void* value) {
  p_il2cpp_gc_wbarrier_set_field(obj, reinterpret_cast<void**>(static_cast<char*>(obj) + offset), value);
}

namespace {

// ECMA-335 FieldAttributes (II.23.1.5).
constexpr int kFieldAccessMask = 0x7, kFieldPublic = 0x6, kFieldStatic = 0x10, kFieldLiteral = 0x40;

bool instance_field(void* field) { return !(p_il2cpp_field_get_flags(field) & (kFieldStatic | kFieldLiteral)); }

// How deep structs may nest inside a copied setting. A real nesting this deep is not expected; past it
// the copy is refused rather than left without the write barrier.
constexpr int kMaxNesting = 8;

// The offsets, relative to the struct's place in its owner, of each managed reference the struct
// `klass` holds (and the structs nested in it hold), appended to `refs`. False when the metadata does
// not say (a field type the runtime cannot resolve) or the nesting is deeper than kMaxNesting. A
// primitive (System.Int32, Boolean, Single and the like) declares an instance `m_value` of its own type,
// and an enum's one instance field is a primitive: neither holds a reference, so the walk stops there.
bool struct_references(Class* klass, size_t at, int depth, std::vector<size_t>& refs) {
  if (depth > kMaxNesting) return false;
  void* iter = nullptr;
  while (void* f = p_il2cpp_class_get_fields(klass, &iter)) {
    if (!instance_field(f)) continue;
    void* const type = p_il2cpp_class_from_type(p_il2cpp_field_get_type(f));
    if (!type) return false;
    if (type == klass) continue;  // a primitive's own m_value
    // A struct's field offset counts the object header a boxed struct has.
    const size_t inner = at + p_il2cpp_field_get_offset(f) - sizeof(Object);
    if (!p_il2cpp_class_is_valuetype(type)) refs.push_back(inner);
    else if (!struct_references(type, inner, depth + 1, refs)) return false;
  }
  return true;
}

}  // namespace

bool copy_public_values(Class* klass, void* dst, const void* src) {
  struct Setting {
    size_t at, size;
  };
  std::vector<Setting> settings;
  std::vector<size_t> refs;  // within the settings, stored through the barrier after the bytes
  void* iter = nullptr;
  while (void* f = p_il2cpp_class_get_fields(klass, &iter)) {
    if (!instance_field(f) || (p_il2cpp_field_get_flags(f) & kFieldAccessMask) != kFieldPublic) continue;
    void* const type = p_il2cpp_class_from_type(p_il2cpp_field_get_type(f));
    if (!type) return false;  // a public field the runtime cannot type: refused, nothing copied
    if (!p_il2cpp_class_is_valuetype(type)) continue;
    const int32_t size = p_il2cpp_class_value_size(type, nullptr);
    const size_t at = p_il2cpp_field_get_offset(f);
    if (size <= 0 || !struct_references(type, at, 0, refs)) return false;
    settings.push_back({at, static_cast<size_t>(size)});
  }
  for (const Setting& s : settings)
    std::memcpy(static_cast<char*>(dst) + s.at, static_cast<const char*>(src) + s.at, s.size);
  for (size_t at : refs)  // the references the bytes carried, stored as the GC needs
    set_ref_field(dst, at, *reinterpret_cast<void* const*>(static_cast<const char*>(src) + at));
  return true;
}

bool dictionary_int_entries(const void* dict, IntPair* out, size_t max, size_t& written) {
  written = 0;
  Class* const klass = dict ? class_of(dict) : nullptr;
  if (!klass || std::strcmp(p_il2cpp_class_get_namespace(klass), "System.Collections.Generic") != 0 ||
      std::strcmp(p_il2cpp_class_get_name(klass), "Dictionary`2") != 0)
    return false;
  void* const entriesField = p_il2cpp_class_get_field_from_name(klass, "_entries");
  void* const countField = p_il2cpp_class_get_field_from_name(klass, "_count");
  if (!entriesField || !countField) return false;
  // The entries' class comes from the constructed class's own field, so an empty dictionary is judged too.
  void* const entriesType = p_il2cpp_class_from_type(p_il2cpp_field_get_type(entriesField));
  Class* const entry = entriesType ? element_class(entriesType) : nullptr;
  void* const hashField = entry ? p_il2cpp_class_get_field_from_name(entry, "hashCode") : nullptr;
  void* const keyField = entry ? p_il2cpp_class_get_field_from_name(entry, "key") : nullptr;
  void* const valueField = entry ? p_il2cpp_class_get_field_from_name(entry, "value") : nullptr;
  if (!hashField || !keyField || !valueField) return false;
  // A number an int holds as it is: a System.Int32 or UInt32, or an enum stored in 4 bytes. A float or
  // another 4-byte struct is no int.
  const auto integer = [](void* f) {
    void* const type = p_il2cpp_class_from_type(p_il2cpp_field_get_type(f));
    if (!type || !p_il2cpp_class_is_valuetype(type) || p_il2cpp_class_value_size(type, nullptr) != 4) return false;
    if (p_il2cpp_class_is_enum(type)) return true;
    const char* const ns = p_il2cpp_class_get_namespace(type);
    const char* const name = p_il2cpp_class_get_name(type);
    return ns && name && std::strcmp(ns, "System") == 0 &&
           (std::strcmp(name, "Int32") == 0 || std::strcmp(name, "UInt32") == 0);
  };
  if (!integer(keyField) || !integer(valueField)) return false;
  const char* const self = static_cast<const char*>(dict);
  Array* entries = nullptr;
  std::memcpy(&entries, self + p_il2cpp_field_get_offset(entriesField), sizeof entries);
  int32_t count = 0;
  std::memcpy(&count, self + p_il2cpp_field_get_offset(countField), sizeof count);
  if (!entries || count <= 0) return true;  // an empty dictionary
  // A struct's field offset counts the object header a boxed struct has.
  const size_t hashAt = p_il2cpp_field_get_offset(hashField) - sizeof(Object);
  const size_t keyAt = p_il2cpp_field_get_offset(keyField) - sizeof(Object);
  const size_t valueAt = p_il2cpp_field_get_offset(valueField) - sizeof(Object);
  const size_t stride = array_element_size(entries);
  if (stride == 0) return false;
  const char* const data = entries->data<char>();
  const size_t n = std::min<size_t>(static_cast<size_t>(count), entries->max_length);
  for (size_t i = 0; i < n && written < max; ++i) {
    const char* const at = data + i * stride;
    int32_t hash = 0;
    std::memcpy(&hash, at + hashAt, sizeof hash);
    if (hash < 0) continue;
    std::memcpy(&out[written].key, at + keyAt, sizeof(int32_t));
    std::memcpy(&out[written].value, at + valueAt, sizeof(int32_t));
    ++written;
  }
  return true;
}

size_t fields_of_type(Class* klass, const void* obj, Class* fieldClass, void** out, size_t max) {
  struct Found {
    size_t offset;
    void* value;
  };
  std::vector<Found> found;
  void* iter = nullptr;
  while (void* f = p_il2cpp_class_get_fields(klass, &iter)) {
    if (!instance_field(f) || p_il2cpp_class_from_type(p_il2cpp_field_get_type(f)) != fieldClass) continue;
    const size_t at = p_il2cpp_field_get_offset(f);
    found.push_back({at, *reinterpret_cast<void* const*>(static_cast<const char*>(obj) + at)});
  }
  std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.offset < b.offset; });
  for (size_t i = 0; i < found.size() && i < max; ++i) out[i] = found[i].value;
  return found.size();
}

void* resolve_icall(const char* name) {
  void* p = p_il2cpp_resolve_icall(name);
  if (!p) PM_LOG("il2cpp: icall %s not found", name);
  return p;
}

namespace {

// The method at the place `method` has among the definition's own in the instance `inflated`, whose methods the
// runtime makes one for one and in order from the definition's. Null when there is none, or when the method there has
// another name or another number of parameters: a runtime that orders them otherwise fails closed.
const void* same_method_of(void* definition, const void* method, void* inflated) {
  void* iter = nullptr;
  size_t index = 0;
  bool found = false;
  while (const void* m = p_il2cpp_class_get_methods(definition, &iter)) {
    if (m == method) {
      found = true;
      break;
    }
    ++index;
  }
  if (!found) return nullptr;
  iter = nullptr;
  const void* m = nullptr;
  for (size_t i = 0; i <= index; ++i)
    if (!(m = p_il2cpp_class_get_methods(inflated, &iter))) return nullptr;
  const char* const name = p_il2cpp_method_get_name(method);
  const char* const madeName = p_il2cpp_method_get_name(m);
  const bool same = name && madeName && std::strcmp(name, madeName) == 0 &&
                    p_il2cpp_method_get_param_count(method) == p_il2cpp_method_get_param_count(m);
  return same ? m : nullptr;
}

}  // namespace

/**
 * Through the game's own reflection, which is all the runtime exports for it: RuntimeType.MakeGenericType and
 * RuntimeMethodInfo.MakeGenericMethod, by their .NET names. A managed exception they throw (a type argument the
 * definition's constraints refuse) unwinds to the caller. MethodInfo begins { methodPointer, virtualMethodPointer,
 * invoker_method }, the runtime's own layout.
 */
bool inflate(void* klass, const void* method, bool ofMethod, void* const* args, size_t count, pm::sdk::InflatedMethod& out) {
  if (!klass || !method || !args || !count) return false;
  Class* const typeClass = cls("mscorlib.dll", "System", "Type");
  Class* const owner = ofMethod ? cls("mscorlib.dll", "System.Reflection", "RuntimeMethodInfo")
                                : cls("mscorlib.dll", "System", "RuntimeType");
  const char* const maker = ofMethod ? "MakeGenericMethod" : "MakeGenericType";
  const void* const make = owner ? p_il2cpp_class_get_method_from_name(owner, maker, 1) : nullptr;
  Array* const types = typeClass ? array_new(typeClass, count) : nullptr;
  // A MethodInfo's first word is its code, and nothing stops a jump to null.
  void* const makeCode = make ? *reinterpret_cast<void* const*>(make) : nullptr;
  if (!makeCode || !types) return false;
  for (size_t i = 0; i < count; ++i) {
    Object* const type = type_object(args[i]);
    if (!type) return false;
    set_ref_field(types, 0x20 + i * sizeof(void*), type);
  }
  void* const self = ofMethod ? p_il2cpp_method_get_object(method, klass) : type_object(klass);
  if (!self) return false;
  void* const made = reinterpret_cast<void* (*)(void*, void*, const void*)>(makeCode)(self, types, make);
  if (!made) return false;
  void* const inflated = ofMethod ? klass : p_il2cpp_class_from_system_type(made);
  const void* const result = ofMethod ? p_il2cpp_method_get_from_reflection(made)
                                      : inflated ? same_method_of(klass, method, inflated) : nullptr;
  if (!result) return false;
  auto const parts = reinterpret_cast<const void* const*>(result);
  out = {result, parts[0], parts[2], inflated};
  return true;
}

void raise_argument_null(const char* argument) {
  p_il2cpp_raise_exception(p_il2cpp_get_exception_argument_null(argument));
  __assume(0);  // il2cpp_raise_exception throws and never returns
}

size_t to_utf8(const String* s, char* out, size_t outSize) {
  const std::u16string_view text = s ? std::u16string_view(s->chars, static_cast<size_t>(s->length)) : std::u16string_view();
  return pm::utf16_to_utf8(text, out, outSize);
}

}  // namespace il2cpp
