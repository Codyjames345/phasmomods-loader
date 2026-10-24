#pragma once

// The runtime services of the game API: managed strings, arrays and lists, and the alive check of
// Unity objects. They go through the host contract's runtime services (core/mod_api.h, version
// 2.1), so a mod never needs the runtime's object layouts; only the host knows them. Type objects,
// object creation, instance checks and virtual calls are members of the wrappers in game_api.h
// (Type::type_object, make, make_array and is; Method::virtual_call), and GC holds are held.h.
//
// Threading: Unity's main thread, like the rest of the game API. Before pg::api::bind every service
// answers as for a null object (empty, 0, null or false).
//
// Lifetime: a managed string, array or list a mod reads here is the runtime's memory. Never keep a
// raw pointer to one, or a view over it (Array, RefArray, List), across frames: keep the object in a
// pg::api::Held and make the view again when it is needed. An element reference from Array and the
// array behind a List are valid only until the next managed call: a List replaces its backing array
// when it grows, so take items() again after any game call, and never store either.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "game_api.h"

namespace pg::api {

namespace detail {

// The host contract's array_length, array_data, list_count and list_items, or 0 and null before
// bind or for a null argument.
size_t array_length(const void* arr);
void* array_data(void* arr);
size_t list_count(const void* list);
void* list_items(const void* list);
// Logs, once per process, that an array or list index was out of range and the access was ignored.
void report_out_of_range();
// Stores the managed reference `value` at byte `offset` of `obj` through the host's set_reference.
void store_reference(void* obj, size_t offset, void* value);

}  // namespace detail

/// The UTF-8 text of the managed string `str` (System.String); a surrogate pair becomes one
/// character and an unpaired surrogate U+FFFD. Empty for a null `str` and before bind.
std::string to_utf8(const void* str);

/// A new managed string holding the UTF-8 text `text`, or null before bind. Nothing else refers to
/// it yet: keep it in a pg::api::Held past the current frame.
void* make_string(std::string_view text);

/// UnityEngine.Object's implicit bool: false for null, for a destroyed engine object (whose managed
/// wrapper still exists) and before bind. Check it before using an engine object kept from an
/// earlier frame.
bool alive(const void* unityObject);

/// A view over a managed array of value elements (T[], with T a primitive or a struct copied whole),
/// valid for the current frame. size() is its length; operator[] gives element i in place, so it
/// can be read or written; that reference is valid only until the next managed call. An object that
/// is not an array has size 0 (the host logs that once). An index at or past size() logs once per process and gives a zeroed
/// scratch element instead, so nothing outside the array is touched. A null `raw` has size 0.
template <class T>
struct Array {
  static_assert(std::is_trivially_copyable_v<T>, "an array element is read and written as plain bytes");
  static_assert(!std::is_pointer_v<T>, "an array of managed references is a RefArray");
  void* raw;

  size_t size() const { return detail::array_length(raw); }

  T& operator[](size_t i) const {
    if (i < size())
      if (void* data = detail::array_data(raw)) return static_cast<T*>(data)[i];
    detail::report_out_of_range();
    static T scratch;
    scratch = T{};
    return scratch;
  }
};

/// A view over a managed array of references (object[], GameObject[] and the like), valid for the
/// current frame. operator[] reads element i (null past the end, logged once per process); set
/// stores one through the GC write barrier (nothing past the end, logged once per process). A null
/// `raw` has size 0.
struct RefArray {
  void* raw;

  size_t size() const { return detail::array_length(raw); }

  void* operator[](size_t i) const {
    if (i < size())
      if (void* data = detail::array_data(raw)) return static_cast<void* const*>(data)[i];
    detail::report_out_of_range();
    return nullptr;
  }

  void set(size_t i, void* value) const {
    void* const data = i < size() ? detail::array_data(raw) : nullptr;
    if (!data) {
      detail::report_out_of_range();
      return;
    }
    const size_t offset = static_cast<size_t>(static_cast<char*>(data) - static_cast<char*>(raw)) + i * sizeof(void*);
    detail::store_reference(raw, offset, value);
  }
};

/// A view over a managed System.Collections.Generic.List of references (List<GameObject> and the
/// like), valid for the current frame. size() is its count; operator[] reads item i (null past the
/// count, logged once per process). items() is the backing array, whose length may exceed the
/// count: for a List of values read it as Array<T>{list.items()}, indexes below size() only. The
/// List replaces that array when it grows, so it is valid only until the next managed call: take
/// items() again after any game call and never store it. A null `raw`, or an object that is not a
/// List (logged once by the host), has size 0.
struct List {
  void* raw;

  size_t size() const { return detail::list_count(raw); }

  void* items() const { return detail::list_items(raw); }

  void* operator[](size_t i) const {
    if (i < size()) return RefArray{items()}[i];
    detail::report_out_of_range();
    return nullptr;
  }
};

}  // namespace pg::api
