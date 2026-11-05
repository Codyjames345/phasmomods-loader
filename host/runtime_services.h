#pragma once

#include <cstddef>
#include <cstdint>

#include "mod_api.h"

// The runtime services' bodies (host/runtime_services.cpp): the functions of the host contract (core/mod_api.h, which
// documents each) that are the IL2CPP layer's own, and the operations the host's own code needs from it. Every game member
// they reach comes through the map. Each contains what it throws and answers as for a null object, unless it says
// otherwise. Main thread.
namespace pm::runtime_services {

void api_set_reference(void* obj, int32_t offset, void* value);
void* api_string_new(const char* utf8);
size_t api_string_utf8(const void* str, char* out, size_t size);
size_t api_array_length(const void* arr);
void* api_array_data(void* arr);
void* api_array_new(const char* elementTypeKey, size_t length);
int32_t api_list_count(const void* list);
void* api_list_items(const void* list);
int32_t api_collection_count(const void* collection);
size_t api_array_element_size(const void* arr);
bool api_list_remove_at(void* list, int32_t index);
void* api_type_object(const char* typeKey);
void* api_object_new(const char* typeKey);
bool api_is_instance(const void* obj, const char* typeKey);
// Unity's own implicit bool, reached through the map's Object.op_Implicit; false when that did not resolve.
bool api_alive(const void* unityObject);
const void* api_virtual_method(const void* obj, const char* methodKey);
uint32_t api_gc_hold(void* obj);
void api_gc_release(uint32_t handle);
bool api_static_get(const char* key, void* out, size_t size);
bool api_static_set(const char* key, const void* in, size_t size);
void api_log_exception(const char* where, const void* exception);
// Not contained: the exception it raises is what unwinds the spawn, up to the mod's catch around its call.
void api_abandon_ghost_spawn();
int32_t api_dictionary_int_entries(const void* dict, PmIntEntry* out, int32_t max);
bool api_list_add(void* list, void* item);
bool api_copy_settings(const char* typeKey, void* dst, const void* src);
uint32_t api_fields_of_type(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                            uint32_t max);

// A List<T>'s own Insert, RemoveAt and Add, for content (host/content_types.h): as the game calls them. False, touching
// nothing, for what is not a List or a List without that method's code.
bool list_insert(void* list, int32_t index, void* item);
bool list_remove_at(void* list, int32_t index);
bool list_add_value(void* list, int32_t value);
// A Dictionary<K, int>'s value of a key it holds, written in place (il2cpp::dictionary_int_set), for the content's
// type roll (host/content_types.h). False, touching nothing, for a key it lacks or what is no such dictionary.
bool dictionary_set_value(void* dict, int32_t key, int32_t value);

// The journal's operations the game API has no wrapper for (host/content_journal.h).
bool collection_clear(void* collection);
bool sync_initialise(void* controller);
bool collection_has_clear(void* collection);
bool sync_has_initialise(void* controller);

// A listener in native code: called with the delegate's target and its method when the delegate is invoked, on the
// thread that invokes it (Unity's main thread for a UI event). It runs inside the game's managed call, so it must not
// throw and should only note what happened.
using NativeListener = void (*)(void* target, const void* method);

// A delegate of the class whose constructor is the map's Method entry `ctorKey` and whose Invoke, of no arguments, is
// the entry `invokeKey` (UnityAction.ctor and UnityAction.Invoke), bound to `target` and calling `listener` when
// invoked: made by that constructor, as the game makes its own, with a method the runtime is told is an instance method
// whose code is `listener` (il2cpp::native_method). `target` must be a managed object; a UnityEvent skips a listener
// whose target is a destroyed Unity object. Null, making nothing, when either entry did not resolve or is not a method
// of that one class, when the constructor has no code, or when `target` or `listener` is null. Main thread.
void* delegate_new(const char* ctorKey, const char* invokeKey, void* target, NativeListener listener);

// Whether `obj` is exactly a T[] for the Type entry `elementTypeKey`. False for null, another class or an entry that did
// not resolve. It logs nothing, since it may be asked of what another client sent.
bool is_array_of(const void* obj, const char* elementTypeKey);
// The int a boxed System.Int32 holds, as an RPC's arguments carry it. False, leaving `out` alone, for anything else.
bool boxed_int(const void* boxed, int32_t& out);

}  // namespace pm::runtime_services
