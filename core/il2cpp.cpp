#include "il2cpp.h"

#include <windows.h>

#include <cstring>

#include "log.h"
#include "startup.h"

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
#undef API

void* g_thread = nullptr;

template <class T>
bool resolve(HMODULE module, T& out, const char* name) {
  out = reinterpret_cast<T>(GetProcAddress(module, name));
  if (!out) PG_LOG("il2cpp: missing export %s", name);
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
#undef R
  if (!ok) return false;
  p_il2cpp_format_stack_trace = reinterpret_cast<decltype(p_il2cpp_format_stack_trace)>(
      GetProcAddress(m, "il2cpp_format_stack_trace"));  // optional: diagnostics only
  return true;
}

/** Resolve the runtime API and attach this worker only while startup is still permitted. */
bool init(void* gameAssembly, void* stopEvent, unsigned long long deadline) {
  if (!pg::startup::permitted(stopEvent, deadline, "runtime")) return false;
  if (!resolve_exports(gameAssembly)) return false;
  // When loaded at process start the runtime is not up yet. il2cpp_get_corlib is a plain read
  // of il2cpp_defaults.corlib (no lazy initialisation), set once il2cpp_init has loaded images.
  if (!pg::startup::wait(stopEvent, deadline, "runtime", [] { return p_il2cpp_get_corlib() != nullptr; }))
    return false;
  g_thread = p_il2cpp_thread_attach(p_il2cpp_domain_get());
  if (!g_thread) PG_LOG("ModHost inactive: runtime thread attachment failed");
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
  if (!klass) PG_LOG("il2cpp: class %s.%s not found in %s", ns, name, fileName);
  return klass;
}

const Method* method(Class* klass, const char* name, int argc) {
  for (void* k = klass; k; k = p_il2cpp_class_get_parent(k)) {
    if (const void* m = p_il2cpp_class_get_method_from_name(k, name, argc)) return m;
  }
  PG_LOG("il2cpp: method %s/%d not found on %s", name, argc,
         klass ? p_il2cpp_class_get_name(klass) : "(null)");
  return nullptr;
}

void* method_ptr(Class* klass, const char* name, int argc) {
  const Method* m = method(klass, name, argc);
  return m ? *reinterpret_cast<void* const*>(m) : nullptr;
}

const Method* virtual_method(void* obj, const Method* method) {
  return obj && method ? p_il2cpp_object_get_virtual_method(obj, method) : nullptr;
}

/** Keep instance bindings tied to field metadata rather than managed object offsets. */
Field* field_info(Class* klass, const char* name) {
  for (void* k = klass; k; k = p_il2cpp_class_get_parent(k)) {
    if (void* field = p_il2cpp_class_get_field_from_name(k, name)) return field;
  }
  PG_LOG("il2cpp: field %s not found on %s", name, klass ? p_il2cpp_class_get_name(klass) : "(null)");
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
 * il2cpp_field_set_value stores `value` itself (GameAssembly RVA 0x3A5F83 passes it straight to
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

void format_stack_trace(const void* exception, char* out, size_t size) {
  if (size) out[0] = 0;
  if (exception && p_il2cpp_format_stack_trace && size) p_il2cpp_format_stack_trace(exception, out, static_cast<int>(size));
}

void log_exception(const char* where, const void* exception) {
  char message[192] = "";
  // System.Exception: _message at +0x18 (after the object header and _className).
  if (exception) to_utf8(field<String*>(exception, 0x18), message, sizeof message);
  PG_LOG("%s: %s: %s", where, exception ? class_name(class_of(exception)) : "?", message);
  char stack[2048];
  format_stack_trace(exception, stack, sizeof stack);
  for (char* line = stack; line && *line;) {
    char* next = std::strchr(line, '\n');
    if (next) *next++ = 0;
    if (*line) PG_LOG("%s:   %s", where, line);
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

void* resolve_icall(const char* name) {
  void* p = p_il2cpp_resolve_icall(name);
  if (!p) PG_LOG("il2cpp: icall %s not found", name);
  return p;
}

void raise_argument_null(const char* argument) {
  p_il2cpp_raise_exception(p_il2cpp_get_exception_argument_null(argument));
  __assume(0);  // il2cpp_raise_exception throws and never returns
}

void to_utf8(const String* s, char* out, size_t outSize) {
  if (!outSize) return;
  size_t n = 0;
  auto emit = [&](const char* bytes, size_t count) {
    if (n + count + 1 > outSize) return false;  // never split a character
    std::memcpy(out + n, bytes, count);
    n += count;
    return true;
  };
  for (int32_t i = 0; s && i < s->length; ++i) {
    uint32_t c = s->chars[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < s->length && s->chars[i + 1] >= 0xDC00 && s->chars[i + 1] < 0xE000) {
      c = 0x10000 + ((c - 0xD800) << 10) + (s->chars[++i] - 0xDC00);
    } else if (c >= 0xD800 && c < 0xE000) {
      c = 0xFFFD;  // unpaired surrogate
    }
    char b[4];
    size_t len;
    if (c < 0x80) {
      b[0] = static_cast<char>(c), len = 1;
    } else if (c < 0x800) {
      b[0] = static_cast<char>(0xC0 | (c >> 6)), b[1] = static_cast<char>(0x80 | (c & 0x3F)), len = 2;
    } else if (c < 0x10000) {
      b[0] = static_cast<char>(0xE0 | (c >> 12)), b[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      b[2] = static_cast<char>(0x80 | (c & 0x3F)), len = 3;
    } else {
      b[0] = static_cast<char>(0xF0 | (c >> 18)), b[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      b[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3F)), b[3] = static_cast<char>(0x80 | (c & 0x3F)), len = 4;
    }
    if (!emit(b, len)) break;
  }
  out[n] = '\0';
}

}  // namespace il2cpp
