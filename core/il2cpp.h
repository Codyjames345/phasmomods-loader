#pragma once

#include <cstddef>
#include <cstdint>

// Minimal IL2CPP runtime access through GameAssembly.dll's exported API.
// Layouts below match Unity 2022.3 / metadata v31 (the build this mod targets).
// IL2CPP raises managed exceptions as this C++ type (libil2cpp's Il2CppExceptionWrapper; same name in
// the global namespace, so a catch matches it). Its first member is the managed exception.
struct Il2CppExceptionWrapper {
  void* ex;
};

namespace pg::sdk {
struct RuntimeApi;
}

namespace il2cpp {

struct Object {
  void* klass;
  void* monitor;
};

struct String {
  Object obj;
  int32_t length;
  char16_t chars[1];
};

struct Array {
  Object obj;
  void* bounds;
  uintptr_t max_length;
  // element data starts at +0x20
  template <class T>
  T* data() { return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + 0x20); }
};

// System.Collections.Generic.List<T> for reference types.
struct List {
  Object obj;
  Array* items;
  int32_t size;
};

struct Vector3 {
  float x, y, z;
};
struct Quaternion {
  float x, y, z, w;
};

using Class = void;
using Image = void;
using Method = void;  // const MethodInfo*; methodPointer is its first member
using Field = void;

/** Resolve exports and attach only if the runtime is ready within the shared startup deadline. */
bool init(void* gameAssembly, void* stopEvent, unsigned long long deadline);
/** Resolve exports only, for a mod started on the main thread once the runtime is running. */
bool resolve_exports(void* gameAssembly);
void detach();                  // detach the thread that called init
/** The exports the game model reads (sdk/runtime), after resolve_exports; false if one is missing. */
bool runtime_api(void* gameAssembly, pg::sdk::RuntimeApi& out);

Image* image(const char* fileName);  // e.g. "Assembly-CSharp.dll"
Class* cls(const char* fileName, const char* ns, const char* name);
const Method* method(Class* klass, const char* name, int argc);  // searches parents
void* method_ptr(Class* klass, const char* name, int argc);
/** The override of a virtual `method` that `obj`'s class runs (what a C# virtual call reaches). */
const Method* virtual_method(void* obj, const Method* method);
/** Resolve a named instance field through runtime metadata, including inherited fields. */
Field* field_info(Class* klass, const char* name);
/** Read a managed reference through the runtime field API; null metadata fails closed. */
void* reference_field(void* obj, Field* field);
/** Copy a value-type field (a struct or primitive) into `out` through the runtime field API. */
void read_field(void* obj, Field* field, void* out);
/** Write a managed reference through the runtime field API and its GC barrier. */
void set_reference_field(void* obj, Field* field, void* value);
// Static field whose declared type name equals typeName (fields are obfuscated).
void* static_field_by_type(Class* klass, const char* typeName);
/** Copy the static `field` of `klass` into `out`, after running the class's static constructor if it
 *  has not run. False, reading nothing, when `size` is not the size of the field's value (a
 *  pointer's for a reference). Game time on the main thread only: never while the game starts. */
bool static_get(Class* klass, Field* field, void* out, size_t size);
/** Store `size` bytes at `in` into the static `field` of `klass` (a reference through the GC write
 *  barrier, `in` pointing at it), after running the class's static constructor if it has not run.
 *  False, writing nothing, on a size mismatch. Game time on the main thread only. */
bool static_set(Class* klass, Field* field, const void* in, size_t size);
String* string_new(const char* utf8);
// A managed exception's stack trace as the runtime formats it (empty if unavailable).
void format_stack_trace(const void* exception, char* out, size_t size);
// Logs a caught managed exception: its class, message and stack trace (one line per frame).
void log_exception(const char* where, const void* exception);
Object* object_new(Class* klass);
// A strong GC handle (0 for null): the runtime keeps the object while a native pointer to it is kept
// (its GC does not move objects, so the pointer stays valid). Release it when the pointer is dropped.
uint32_t gc_hold(void* object);
void gc_release(uint32_t handle);
// A managed object kept across frames by native code, with its strong GC handle. The GC does not scan
// this DLL's memory, and Unity keeps only a weak handle on most engine objects' wrappers (GameObject,
// Transform; on a MonoBehaviour only until it is destroyed). An unheld wrapper can be collected while
// native code still points at it: Unity can then no longer clear its native pointer when the object is
// destroyed, and the alive check reports a destroyed object as alive.
// It reads as the raw pointer it replaces (assigned, tested, compared and passed as a void*), and is
// moved, never copied. Assigning the object it already holds keeps its handle.
// This copy stays for the host's own code (and the bundled mods' raw IL2CPP paths) until stage 2b
// moves them onto the game API. Mod code keeps objects in pg::api::Held (sdk/api/held.h), which
// holds them through the host contract.
class Held {
 public:
  Held() = default;
  Held(std::nullptr_t) {}
  Held(void* object) : object_(object), handle_(gc_hold(object)) {}
  Held(Held&& other) noexcept : object_(other.object_), handle_(other.handle_) {
    other.object_ = nullptr;
    other.handle_ = 0;
  }
  Held& operator=(Held&& other) noexcept {
    if (this != &other) {
      gc_release(handle_);
      object_ = other.object_;
      handle_ = other.handle_;
      other.object_ = nullptr;
      other.handle_ = 0;
    }
    return *this;
  }
  Held& operator=(void* object) {
    if (object != object_) {
      gc_release(handle_);
      object_ = object;
      handle_ = gc_hold(object);
    }
    return *this;
  }
  Held& operator=(std::nullptr_t) { return *this = static_cast<void*>(nullptr); }
  Held(const Held&) = delete;
  Held& operator=(const Held&) = delete;
  ~Held() { gc_release(handle_); }
  operator void*() const { return object_; }
  template <typename T>
  explicit operator T*() const {  // static_cast<T*>(held), as from the void* it replaced
    return static_cast<T*>(object_);
  }
  void* get() const { return object_; }

 private:
  void* object_ = nullptr;
  uint32_t handle_ = 0;
};
Array* array_new(Class* elementClass, uintptr_t length);
Class* class_of(const void* obj);
Class* class_of_array(Class* elementClass);  // T[] for element class T
Class* element_class(Class* arrayClass);    // T for T[]
Object* type_object(Class* klass);          // System.Type for a class (typeof(T))
const char* class_name(Class* klass);
/** True when obj is non-null and its runtime class is named `name` (a check on offset reads). */
bool is_class(const void* obj, const char* name);
/** True when obj is non-null and its class is `klass` or derives from it (the class's parents). */
bool is_instance(const void* obj, Class* klass);
/** True when obj is a System.String, a managed array (any rank), or a System.Collections.Generic.List<T>. */
bool is_string(const void* obj);
bool is_array(const void* obj);
bool is_list(const void* obj);
/** True when obj is non-null and its class, or a class it derives from, is named `name`. */
bool is_kind(const void* obj, const char* name);
void* resolve_icall(const char* name);
// Store a managed reference into a heap object through the GC write barrier
// (the runtime may use incremental GC).
void set_ref_field(void* obj, size_t offset, void* value);

// Copy a managed string into a UTF-8 buffer, truncated at a character boundary (pg::utf16_to_utf8).
// Returns the whole string's length in UTF-8 bytes, without the NUL; a null string is empty.
size_t to_utf8(const String* s, char* out, size_t outSize);

// Throw a managed System.ArgumentNullException from native code. It unwinds like any managed
// exception, through managed frames, to the nearest C++ catch.
[[noreturn]] void raise_argument_null(const char* argument);

/** Bind a managed method's native code to `out` by name and argument count; false if missing. */
template <class T>
inline bool bind(T& out, Class* klass, const char* name, int argc) {
  out = klass ? reinterpret_cast<T>(method_ptr(klass, name, argc)) : nullptr;
  return out != nullptr;
}

template <class T>
inline T field(const void* obj, size_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const char*>(obj) + offset);
}

template <class T>
inline void set_field(void* obj, size_t offset, T value) {
  *reinterpret_cast<T*>(static_cast<char*>(obj) + offset) = value;
}

}  // namespace il2cpp
