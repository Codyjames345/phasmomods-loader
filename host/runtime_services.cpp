#include "runtime_services.h"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include "Photon.h"
#include "UnityObject.h"
#include "contained.h"
#include "entry_hooks.h"
#include "game_map.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_api.h"

// The runtime services' bodies: what the host contract and the host's own code get from the IL2CPP layer (managed strings,
// arrays, lists, GC holds, static fields, a class's own methods by name). The runtime's layouts stay here and in
// host/il2cpp.*; the callers name map entries. A member of the game reached here is reached through the map.
namespace pm::runtime_services {

void api_set_reference(void* obj, int32_t offset, void* value) {
  if (!obj) return;
  try {
    il2cpp::set_ref_field(obj, static_cast<size_t>(offset), value);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("suite: managed exception while storing a reference", e.ex);
  } catch (...) {
    PM_LOG("suite: exception while storing a reference");
  }
}

// The runtime services (version 2.1). Each runs its body in host::contained, which logs every
// exception it contains.

namespace {

// The class of the Type entry `key`, or null when it is unknown, unresolved or not a type.
il2cpp::Class* type_class(const char* key) {
  const PmGameEntry* e = key ? gamemap::entry(key) : nullptr;
  return e && e->kind == PM_ENTRY_TYPE ? e->klass : nullptr;
}

// A service handed an object of the wrong kind reads no layout from it. It logs that once per
// service (`refused` is the service's own flag, atomic so two threads still log once) and answers 0
// or null.
bool refuse_once(std::atomic<bool>& refused, const char* service, const char* kind) {
  if (!refused.exchange(true))
    PM_LOG("suite: a mod passed %s an object that is not %s; it answers 0 or null", service, kind);
  return false;
}
bool string_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_string(obj) || refuse_once(refused, service, "a System.String");
}
bool array_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_array(obj) || refuse_once(refused, service, "a managed array");
}
bool list_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_list(obj) || refuse_once(refused, service, "a System.Collections.Generic.List");
}

}  // namespace

void* api_string_new(const char* utf8) {
  return host::contained("suite", "making a string", static_cast<void*>(nullptr),
                         [&]() -> void* { return utf8 ? il2cpp::string_new(utf8) : nullptr; });
}
size_t api_string_utf8(const void* str, char* out, size_t size) {
  return host::contained("suite", "reading a string", size_t{0}, [&]() -> size_t {
    if (out && size) out[0] = '\0';
    if (!str || !string_given(str, "string_utf8")) return 0;
    return il2cpp::to_utf8(static_cast<const il2cpp::String*>(str), out, size);
  });
}
size_t api_array_length(const void* arr) {
  return host::contained("suite", "reading an array", size_t{0}, [&]() -> size_t {
    return arr && array_given(arr, "array_length") ? static_cast<const il2cpp::Array*>(arr)->max_length : 0;
  });
}
void* api_array_data(void* arr) {
  return host::contained("suite", "reading an array", static_cast<void*>(nullptr), [&]() -> void* {
    return arr && array_given(arr, "array_data") ? static_cast<il2cpp::Array*>(arr)->data<char>() : nullptr;
  });
}
void* api_array_new(const char* elementTypeKey, size_t length) {
  return host::contained("suite", "making an array", static_cast<void*>(nullptr), [&]() -> void* {
    il2cpp::Class* const klass = type_class(elementTypeKey);
    return klass ? il2cpp::array_new(klass, length) : nullptr;
  });
}
int32_t api_list_count(const void* list) {
  return host::contained("suite", "reading a list", int32_t{0}, [&]() -> int32_t {
    return list && list_given(list, "list_count") ? static_cast<const il2cpp::List*>(list)->size : 0;
  });
}
void* api_list_items(const void* list) {
  return host::contained("suite", "reading a list", static_cast<void*>(nullptr), [&]() -> void* {
    return list && list_given(list, "list_items") ? static_cast<const il2cpp::List*>(list)->items : nullptr;
  });
}
int32_t api_collection_count(const void* collection) {
  return host::contained("suite", "counting a collection", int32_t{0}, [&]() -> int32_t {
    static std::atomic<bool> refused{false};
    if (!collection) return 0;
    if (!il2cpp::is_counted_collection(collection)) {
      refuse_once(refused, "collection_count", "a System.Collections.Generic List, HashSet, Dictionary, Queue or Stack");
      return 0;
    }
    // As list_method: a method without code (a generic instance never compiled) is no method to call;
    // contained() does not stop an access violation.
    const il2cpp::Method* m = il2cpp::method(il2cpp::class_of(collection), "get_Count", 0);
    void* const code = m ? *reinterpret_cast<void* const*>(m) : nullptr;
    return code ? reinterpret_cast<int32_t (*)(const void*, const void*)>(code)(collection, m) : 0;
  });
}
size_t api_array_element_size(const void* arr) {
  return host::contained("suite", "reading an array", size_t{0}, [&]() -> size_t {
    return arr && array_given(arr, "array_element_size") ? il2cpp::array_element_size(arr) : 0;
  });
}
bool api_list_remove_at(void* list, int32_t index) {
  return host::contained("suite", "taking from a list", false, [&] {
    return list && list_given(list, "list_remove_at") && index >= 0 &&
           index < static_cast<const il2cpp::List*>(list)->size && list_remove_at(list, index);
  });
}
void* api_type_object(const char* typeKey) {
  return host::contained("suite", "making a type object", static_cast<void*>(nullptr),
                         [&]() -> void* { return il2cpp::type_object(type_class(typeKey)); });
}
void* api_object_new(const char* typeKey) {
  return host::contained("suite", "making an object", static_cast<void*>(nullptr), [&]() -> void* {
    il2cpp::Class* const klass = static_cast<il2cpp::Class*>(gamemap::instance_class(typeKey));
    return klass ? il2cpp::object_new(klass) : nullptr;
  });
}
bool api_is_instance(const void* obj, const char* typeKey) {
  return host::contained("suite", "checking an object's type", false,
                         [&] { return il2cpp::is_instance(obj, type_class(typeKey)); });
}
// UnityEngine.Object's implicit bool, the host contract's alive: the game's own method, reached through the map. It
// cannot go through pm::api::alive, which asks this very function.
bool api_alive(const void* unityObject) {
  return host::contained("suite", "checking an object is alive", false, [&] {
    const PmGameEntry* e = unityObject ? gamemap::entry(map::UnityObject_op_Implicit.key) : nullptr;
    return e && e->code && reinterpret_cast<bool (*)(const void*, const void*)>(e->code)(unityObject, e->method);
  });
}
const void* api_virtual_method(const void* obj, const char* methodKey) {
  return host::contained("suite", "finding a virtual method", static_cast<const void*>(nullptr), [&]() -> const void* {
    const PmGameEntry* e = methodKey ? gamemap::entry(methodKey) : nullptr;
    if (!obj || !e || e->kind != PM_ENTRY_METHOD || !e->method || !il2cpp::is_instance(obj, e->klass)) return nullptr;
    return il2cpp::virtual_method(const_cast<void*>(obj), e->method);
  });
}
uint32_t api_gc_hold(void* obj) {
  return host::contained("suite", "holding an object", uint32_t{0}, [&] { return il2cpp::gc_hold(obj); });
}
void api_gc_release(uint32_t handle) {
  try {
    il2cpp::gc_release(handle);
  } catch (...) {
    PM_LOG("suite: exception while releasing an object");
  }
}

namespace {

// The static field entry `key`: its class and FieldInfo, or null when it is unknown, unresolved, not
// a field or not static.
const PmGameEntry* static_field(const char* key) {
  const PmGameEntry* e = key ? gamemap::entry(key) : nullptr;
  const sdk::Result* r = e ? gamemap::result(key) : nullptr;
  if (!e || !r || e->kind != PM_ENTRY_FIELD || !(r->desc->flags & sdk::kStatic) || !e->klass || !e->field)
    return nullptr;
  return e;
}

// Whether a managed exception's stack from a static field access was logged: once in all, since a
// failed static constructor throws again on every access.
std::atomic<bool> g_staticStackLogged{false};

// A static field access that failed: logged once per key, with `why`. Called from catch handlers too,
// so it throws nothing.
void static_refused(const char* key, const char* why) {
  try {
    static std::mutex mutex;
    static std::set<std::string> logged;
    const std::lock_guard lock(mutex);
    if (logged.insert(key).second) PM_LOG("suite: static field %s: %s; it answers false", key, why);
  } catch (...) {
    // Logging is best effort; the caller answers false either way.
  }
}

// Runs a static field access, `access(entry)`, for the entry `key`. False, logged once per key, when
// the entry is not a resolved static field, the access refuses (a size other than the field's), or
// it throws (the class's static constructor, say).
template <class F>
bool static_access(const char* key, const char* what, F access) {
  if (!key) return false;
  try {
    const PmGameEntry* e = static_field(key);
    if (!e) {
      static_refused(key, "it is unknown, unresolved or not a static field");
      return false;
    }
    if (access(e)) return true;
    static_refused(key, "the size given is not the size of its value");
  } catch (const Il2CppExceptionWrapper& ex) {
    if (!g_staticStackLogged.exchange(true)) il2cpp::log_exception((std::string("suite: ") + what).c_str(), ex.ex);
    static_refused(key, "a managed exception was thrown");
  } catch (...) {
    static_refused(key, "an exception was thrown");
  }
  return false;
}
}  // namespace

bool api_static_get(const char* key, void* out, size_t size) {
  return out && static_access(key, "reading a static field",
                              [&](const PmGameEntry* e) { return il2cpp::static_get(e->klass, e->field, out, size); });
}
bool api_static_set(const char* key, const void* in, size_t size) {
  return in && static_access(key, "writing a static field",
                             [&](const PmGameEntry* e) { return il2cpp::static_set(e->klass, e->field, in, size); });
}

void api_log_exception(const char* where, const void* exception) {
  try {
    il2cpp::log_exception(where ? where : "mod", exception);
  } catch (...) {
    PM_LOG("suite: exception while logging a managed exception");
  }
}

namespace {
void raise_spawn_abandoned() { il2cpp::raise_argument_null("GhostController.Spawn abandoned"); }
}  // namespace

// Not contained: the exception it raises is what unwinds the spawn, up to the mod's catch around its call.
void api_abandon_ghost_spawn() {
  if (context::abandon_ghost_spawn(&raise_spawn_abandoned)) return;
  static std::atomic<bool> refused{false};
  refuse_once(refused, "abandon_ghost_spawn", "a call under a running GhostController.Spawn");
}

int32_t api_dictionary_int_entries(const void* dict, PmIntEntry* out, int32_t max) {
  return host::contained("suite", "reading a dictionary", int32_t{0}, [&]() -> int32_t {
    static std::atomic<bool> refused{false};
    if (!dict || !out || max <= 0) return 0;
    static_assert(sizeof(PmIntEntry) == sizeof(il2cpp::IntPair), "the service's pair is the runtime's");
    size_t written = 0;
    if (!il2cpp::dictionary_int_entries(dict, reinterpret_cast<il2cpp::IntPair*>(out), static_cast<size_t>(max), written))
      refuse_once(refused, "dictionary_int_entries", "a Dictionary whose keys and values are 4-byte integers or enums");
    return static_cast<int32_t>(written);
  });
}

// The game's own List<T> methods for content (host/content_types.h), called through the runtime as
// the game calls them: the method of the list's own class, its code with the list, the arguments and
// its MethodInfo.
namespace {
const il2cpp::Method* list_method(void* list, const char* name, int argc) {
  if (!list || !il2cpp::is_list(list)) return nullptr;
  const il2cpp::Method* m = il2cpp::method(il2cpp::class_of(list), name, argc);
  return m && *reinterpret_cast<void* const*>(m) ? m : nullptr;
}
}  // namespace

bool list_insert(void* list, int32_t index, void* item) {
  return host::contained("content", "adding to a list", false, [&] {
    const il2cpp::Method* m = list_method(list, "Insert", 2);
    if (!m) return false;
    reinterpret_cast<void (*)(void*, int32_t, void*, const void*)>(*reinterpret_cast<void* const*>(m))(list, index, item, m);
    return true;
  });
}
bool list_remove_at(void* list, int32_t index) {
  return host::contained("content", "taking from a list", false, [&] {
    const il2cpp::Method* m = list_method(list, "RemoveAt", 1);
    if (!m) return false;
    reinterpret_cast<void (*)(void*, int32_t, const void*)>(*reinterpret_cast<void* const*>(m))(list, index, m);
    return true;
  });
}
bool list_add_value(void* list, int32_t value) {
  return host::contained("content", "adding to a list", false, [&] {
    const il2cpp::Method* m = list_method(list, "Add", 1);
    if (!m) return false;
    reinterpret_cast<void (*)(void*, int32_t, const void*)>(*reinterpret_cast<void* const*>(m))(list, value, m);
    return true;
  });
}

// The host contract's list_add: the List's own Add of a reference, as the game calls it.
bool api_list_add(void* list, void* item) {
  return host::contained("suite", "adding to a list", false, [&] {
    if (!list || !list_given(list, "list_add")) return false;
    const il2cpp::Method* m = list_method(list, "Add", 1);
    if (!m) return false;
    reinterpret_cast<void (*)(void*, void*, const void*)>(*reinterpret_cast<void* const*>(m))(list, item, m);
    return true;
  });
}

// The host contract's copy_settings and fields_of_type: a class's own fields, read from the runtime's
// metadata, so a mod names none of them.
bool api_copy_settings(const char* typeKey, void* dst, const void* src) {
  return host::contained("suite", "copying a component's settings", false, [&] {
    il2cpp::Class* const klass = type_class(typeKey);
    if (!klass || !il2cpp::is_instance(dst, klass) || !il2cpp::is_instance(src, klass)) return false;
    if (il2cpp::copy_public_values(klass, dst, src)) return true;
    static std::atomic<bool> refused{false};
    if (!refused.exchange(true))
      PM_LOG("suite: copy_settings refused %s: the runtime cannot type one of its settings", typeKey);
    return false;
  });
}
uint32_t api_fields_of_type(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                            uint32_t max) {
  return host::contained("suite", "reading a class's fields of one type", uint32_t{0}, [&]() -> uint32_t {
    il2cpp::Class* const klass = type_class(typeKey);
    il2cpp::Class* fieldClass = type_class(fieldTypeKey);
    if (fieldClass && arrayOf) fieldClass = il2cpp::class_of_array(fieldClass);
    if (!klass || !fieldClass || !il2cpp::is_instance(obj, klass) || (max && !out)) return 0;
    return static_cast<uint32_t>(il2cpp::fields_of_type(klass, obj, fieldClass, out, max));
  });
}

// The journal's operations the game API has no wrapper for (host/content_journal.h): a collection's
// own Clear, and a sync controller's Initialise (the guess sync's or the evidence sync's), a method of
// their generic base, found on the object's class and its parents by name, as the game calls it.
bool collection_clear(void* collection) {
  return host::contained("content", "emptying a journal list", false, [&] {
    const il2cpp::Method* m = collection ? il2cpp::method(il2cpp::class_of(collection), "Clear", 0) : nullptr;
    if (!m || !*reinterpret_cast<void* const*>(m)) return false;
    reinterpret_cast<void (*)(void*, const void*)>(*reinterpret_cast<void* const*>(m))(collection, m);
    return true;
  });
}
bool sync_initialise(void* controller) {
  return host::contained("content", "having a journal's sync controller read its buttons", false, [&] {
    const il2cpp::Method* m = controller ? il2cpp::method(il2cpp::class_of(controller), "Initialise", 0) : nullptr;
    if (!m || !*reinterpret_cast<void* const*>(m)) return false;
    reinterpret_cast<void (*)(void*, const void*)>(*reinterpret_cast<void* const*>(m))(controller, m);
    return true;
  });
}
namespace {
bool has_method(void* object, const char* name) {
  return host::contained("content", "looking a journal method up", false, [&] {
    const il2cpp::Method* m = object ? il2cpp::method(il2cpp::class_of(object), name, 0) : nullptr;
    return m && *reinterpret_cast<void* const*>(m);
  });
}
}  // namespace

bool collection_has_clear(void* collection) { return has_method(collection, "Clear"); }
bool sync_has_initialise(void* controller) { return has_method(controller, "Initialise"); }

bool is_array_of(const void* obj, const char* elementTypeKey) {
  il2cpp::Class* const element = type_class(elementTypeKey);
  return obj && element && il2cpp::class_of(obj) == il2cpp::class_of_array(element);
}

bool boxed_int(const void* boxed, int32_t& out) {
  il2cpp::Class* const type = type_class(phasmo::Int32::type.desc.key);
  if (!boxed || !type || il2cpp::class_of(boxed) != type) return false;
  std::memcpy(&out, static_cast<const char*>(boxed) + sizeof(il2cpp::Object), sizeof out);
  return true;
}

void* delegate_new(const char* ctorKey, const char* invokeKey, void* target, NativeListener listener) {
  return host::contained("suite", "making a listener", static_cast<void*>(nullptr), [&]() -> void* {
    const PmGameEntry* e = ctorKey ? gamemap::entry(ctorKey) : nullptr;
    const PmGameEntry* invoke = invokeKey ? gamemap::entry(invokeKey) : nullptr;
    if (!target || !listener || !e || e->kind != PM_ENTRY_METHOD || !e->klass || !e->code || !e->method || !invoke ||
        invoke->kind != PM_ENTRY_METHOD || invoke->klass != e->klass || !invoke->method)
      return nullptr;
    const il2cpp::Method* const method = il2cpp::native_method(e->klass, invoke->method, reinterpret_cast<void*>(listener));
    il2cpp::Object* const made = method ? il2cpp::object_new(e->klass) : nullptr;
    if (!made) return nullptr;
    // The constructor (object target, IntPtr method), with its own MethodInfo last, as the game calls it.
    reinterpret_cast<void (*)(void*, void*, const void*, const void*)>(e->code)(made, target, method, e->method);
    return made;
  });
}

}  // namespace pm::runtime_services
