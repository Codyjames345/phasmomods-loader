#pragma once

// The runtime services of the game API: managed strings, arrays and lists, and the alive check of
// Unity objects. They go through the host contract's runtime services (core/mod_api.h, version
// 2.1), so a mod never needs the runtime's object layouts; only the host knows them. Type objects,
// object creation, instance checks and virtual calls are members of the wrappers in game_api.h
// (Type::type_object, make, make_array and is; Method::virtual_call), and GC holds are held.h.
//
// Threading: Unity's main thread, like the rest of the game API. Before pm::api::bind every service
// answers as for a null object (empty, 0, null or false).
//
// Lifetime: a managed string, array or list a mod reads here is the runtime's memory. Never keep a
// raw pointer to one, or a view over it (Array, RefArray, List), across frames: keep the object in a
// pm::api::Held and make the view again when it is needed. An element reference from Array and the
// array behind a List are valid only until the next managed call: a List replaces its backing array
// when it grows, so take items() again after any game call, and never store either.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "game_api.h"

namespace pm::api {

namespace detail {

// The host contract's array_length, array_data, list_count and list_items, or 0 and null before
// bind or for a null argument.
size_t array_length(const void* arr);
void* array_data(void* arr);
size_t list_count(const void* list);
void* list_items(const void* list);
// The host contract's list_add, or false before bind or for a null list.
bool list_add(void* list, void* item);
// The host contract's collection_count, array_element_size and list_remove_at, or 0 and false before
// bind or for null.
size_t collection_count(const void* collection);
size_t array_element_size(const void* arr);
bool list_remove_at(void* list, int32_t index);
// Logs, once per process, that an array or list index was out of range and the access was ignored.
void report_out_of_range();
// Stores the managed reference `value` at byte `offset` of `obj` through the host's set_reference.
void store_reference(void* obj, size_t offset, void* value);
size_t dictionary_int_entries(const void* dict, PmIntEntry* out, size_t max);

}  // namespace detail

/// The UTF-8 text of the managed string `str` (System.String); a surrogate pair becomes one
/// character and an unpaired surrogate U+FFFD. Empty for a null `str` and before bind.
std::string to_utf8(const void* str);

/// Whether the managed string `str` reads exactly as `text`, with no allocation, for what runs on every RPC. False for a null
/// `str`, before bind, and for a `text` of 64 bytes or more.
bool string_is(const void* str, std::string_view text);

/// A new managed string holding the UTF-8 text `text`, or null before bind. Nothing else refers to
/// it yet: keep it in a pm::api::Held past the current frame.
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

/// A view over a managed array of value elements whose layout the game holds (a struct of the game's,
/// as a List<T> of structs keeps them), valid for the current frame. size() is its length and stride()
/// the bytes each element takes, both read from the runtime (the host contract's array_element_size),
/// never assumed. at(i) is element i in place: read its fields with the struct's field entries, whose
/// offsets are the unboxed ones (Field::get(at(i))). It is null at or past size(), logged once per
/// process, and for an array the runtime cannot size. Valid only until the next managed call. A null
/// `raw` has size 0.
struct ValueArray {
  void* raw;

  size_t size() const { return stride() ? detail::array_length(raw) : 0; }
  size_t stride() const { return detail::array_element_size(raw); }

  void* at(size_t i) const {
    if (i < size())
      if (char* data = static_cast<char*>(detail::array_data(raw))) return data + i * stride();
    detail::report_out_of_range();
    return nullptr;
  }
};

/// @brief The entries of a managed System.Collections.Generic.Dictionary<K, int> whose K is a 4-byte value such as an
/// enum (the game's Dictionary<GhostType, int> of weights, say), in the dictionary's own order, without its free
/// slots.
/// @param dict The dictionary. Null, or an object of another kind (logged once by the host): 0.
/// @param out Where the entries go: the key as a number and the value.
/// @param max At most this many are written.
/// @return How many were written. Read from the runtime's own layout of the dictionary, never assumed.
/// @pre Main thread. Valid until the dictionary changes; nothing is kept.
/// @note This client only. Before bind: 0.
/// @since Contract 2.2
/// @code
/// PmIntEntry weights[64];
/// const size_t n = pm::api::dictionary_entries(dict, weights, 64);
/// @endcode
inline size_t dictionary_entries(const void* dict, PmIntEntry* out, size_t max) {
  return detail::dictionary_int_entries(dict, out, max);
}

/// The number of items in a managed System.Collections.Generic List, HashSet, Dictionary, Queue or
/// Stack, through the collection's own Count (the host contract's collection_count); 0 for null, before
/// bind and for any other object (logged once by the host).
inline size_t count(const void* collection) { return detail::collection_count(collection); }

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

  /// Whether the List holds `item` (the same reference), as its Contains finds a reference.
  bool contains(const void* item) const {
    for (size_t i = 0, n = size(); i < n; ++i)
      if (RefArray{items()}[i] == item) return true;
    return false;
  }

  /// Adds the reference `item` at its end through the List's own Add, as the game's code does (the
  /// host contract's list_add): the List may replace its backing array, so take items() again after
  /// it. False, touching nothing, before bind, for a null `raw` or an object that is not a List.
  bool add(void* item) const { return detail::list_add(raw, item); }

  /// Removes item i through the List's own RemoveAt, as the game's code does (the host contract's
  /// list_remove_at); take items() again after it. False, touching nothing, before bind, for a null
  /// `raw`, an object that is not a List or an index at or past size().
  bool remove_at(size_t i) const {
    return i < size() && detail::list_remove_at(raw, static_cast<int32_t>(i));
  }

  /// A List of structs' backing array, as a ValueArray (indexes below size() only).
  ValueArray values() const { return ValueArray{items()}; }
};

}  // namespace pm::api
