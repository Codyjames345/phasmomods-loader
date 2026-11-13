#pragma once

/// @file
/// @brief Typed handles to the game's managed objects: pm::api::Ref<D> for an object of the mapped type D,
/// pm::api::Object for one of a type the map does not name.
///
/// A handle is one pointer, as the void* it replaces, so it costs nothing to pass or copy. Its type is the
/// compiler's to check: a Ref of one type is not a Ref of an unrelated one, so passing a Player where a
/// ghost is expected does not compile. As C# allows, a handle converts by itself to a handle of any base
/// type the map names (each Type entry's `base`, sdk/api/entry.h, which the offline check proves) and to
/// Object; going the other way, down to a derived type, asks the runtime first, through pm::api::as
/// (game_api.h). A raw reference becomes a Ref only through Ref::unchecked; Object takes one through
/// constructors, and both handles convert back to a void*, while the typed-handle migration is
/// unfinished: those bridge conversions are transitional and leave in its part 5.
///
/// A handle is valid for the current frame only, as the reference it holds: keep an object across frames
/// in a pm::api::HeldRef (held.h).
/// @ingroup runtime

#include <cstddef>
#include <type_traits>

#include "entry.h"

namespace pm::api {

class Object;
template <const sdk::EntryDesc& D>
class Ref;

namespace detail {
// Whether a type is one of the handles of this header: true for Ref<D> for any D and for Object,
// with cv and references stripped. (The wrapper templates in game_api.h use it the same way.)
template <class T>
struct HandleTrait : std::false_type {};
template <const sdk::EntryDesc& D>
struct HandleTrait<Ref<D>> : std::true_type {};
template <>
struct HandleTrait<Object> : std::true_type {};
template <class T>
inline constexpr bool is_handle_v = HandleTrait<std::remove_cvref_t<T>>::value;
}  // namespace detail

/// Whether the map's type D is the type B or derives from it, following each Type entry's `base` from D.
/// Compile-time data only: no game is asked. A type the map gives no base derives from nothing else here,
/// even when the game's class does, so the answer is false for a base the map does not record.
/// @tparam D A map entry of kind Type.
/// @tparam B A map entry of kind Type.
/// @return True when B is D or one of the bases the map records for it.
template <const sdk::EntryDesc& D, const sdk::EntryDesc& B>
constexpr bool derives_from() {
  static_assert(D.kind == sdk::Kind::Type && B.kind == sdk::Kind::Type, "derives_from compares two Type entries");
  for (const sdk::EntryDesc* t = &D; t; t = t->base)
    if (t == &B) return true;
  return false;
}

/// A handle to a managed object of a type the map does not name, or of any type when the code does not
/// care which. Every Ref converts to one by itself. Get a Ref back from it with pm::api::as.
/// @ingroup runtime
class Object {
 public:
  /// A null handle.
  constexpr Object() = default;
  /// A null handle.
  constexpr Object(std::nullptr_t) {}
  /// transitional: removed in part 5. Wraps a raw reference, so code that still keeps a game object
  /// as a void* compiles where a wrapper's parameter is now an Object. New code gives a handle or
  /// calls Object::unchecked.
  /// @param object A managed object, or null.
  constexpr Object(void* object) : object_(object) {}
  /// transitional: removed in part 5. As the void* one, for a const raw reference; the const is
  /// dropped, the GC not needing it.
  /// @param object A managed object, or null.
  constexpr Object(const void* object) : object_(const_cast<void*>(object)) {}
  /// transitional: removed in part 5. Wraps any other class that reads as a raw reference, as
  /// pm::api::Held (held.h) does, so code that still keeps a game object in one compiles where a
  /// wrapper's parameter is now an Object. Only class types: a raw pointer other than the void*
  /// and const void* above never becomes an Object. A handle never comes this way, so a Ref's own
  /// conversion to Object stays its only one, with no ambiguity. New code gives a handle.
  /// @tparam T A class type with an implicit conversion to void*, not one of the handles.
  /// @param t The managed object to wrap, or null.
  template <class T>
    requires(std::is_class_v<T> && !detail::is_handle_v<T> && std::is_convertible_v<const T&, void*>)
  Object(const T& t) : object_(static_cast<void*>(t)) {}
  /// Wraps a raw managed reference, as the host contract and the runtime services give one. Nothing is
  /// checked: the caller vouches that `object` is a managed object or null.
  /// @param object A managed object, or null.
  /// @return The handle.
  static constexpr Object unchecked(void* object) {
    Object o;
    o.object_ = object;
    return o;
  }
  /// The raw reference, for code that still takes a void*.
  /// @return The object, or null.
  constexpr void* raw() const { return object_; }
  /// Whether the handle is not null. It says nothing about whether an engine object is destroyed: ask
  /// pm::api::alive for that.
  /// @return True for a handle to an object.
  constexpr explicit operator bool() const { return object_ != nullptr; }
  /// Whether two handles name the same object (or are both null).
  /// @param other The other handle; a Ref converts to one.
  /// @return True for the same object.
  constexpr bool operator==(const Object& other) const { return object_ == other.object_; }
  /// Whether the handle is null.
  /// @return True for a null handle.
  constexpr bool operator==(std::nullptr_t) const { return object_ == nullptr; }
  /// transitional: removed in part 5. The raw reference an Object used to be, so code that assigns a
  /// wrapper's answer to a void* or passes one to the host contract compiles while a wrapper gives
  /// handles. New code keeps a handle.
  /// @return The object, or null.
  constexpr operator void*() const { return object_; }

 private:
  void* object_ = nullptr;
};

/// A handle to a managed object of the map's type D (a Type entry) or of a class derived from it. It
/// converts by itself to a Ref of each base type the map records for D (see derives_from) and to Object,
/// never to an unrelated Ref or down to a derived one: pm::api::as checks and converts down. Comparing two
/// handles of any types compares the objects they name.
/// @tparam D A map entry of kind Type.
/// @ingroup runtime
/// @code
/// pm::api::Ref<pm::map::GhostAI> ghost = ...;
/// pm::api::Ref<pm::map::MonoBehaviour> behaviour = ghost;  // GhostAI derives from MonoBehaviour
/// pm::api::Ref<pm::map::GhostAI> again = pm::api::as<pm::map::GhostAI>(behaviour);  // asks the runtime
/// @endcode
template <const sdk::EntryDesc& D>
class Ref {
  static_assert(D.kind == sdk::Kind::Type, "Ref names a map entry of kind Type");

 public:
  /// The map entry of the handle's type.
  static constexpr const sdk::EntryDesc& desc = D;

  /// A null handle.
  constexpr Ref() = default;
  /// A null handle.
  constexpr Ref(std::nullptr_t) {}
  /// Wraps a raw managed reference the caller knows is a D, as the host contract and the runtime services
  /// give one. Nothing is checked: a reference of another type makes a handle that lies. When the type is
  /// not known, wrap it with Object::unchecked and convert it with pm::api::as instead.
  /// @param object A managed object of type D or a type derived from it, or null.
  /// @return The handle.
  static constexpr Ref unchecked(void* object) {
    Ref r;
    r.object_ = object;
    return r;
  }
  /// The raw reference, for code that still takes a void*.
  /// @return The object, or null.
  constexpr void* raw() const { return object_; }
  /// Whether the handle is not null. It says nothing about whether an engine object is destroyed: ask
  /// pm::api::alive for that.
  /// @return True for a handle to an object.
  constexpr explicit operator bool() const { return object_ != nullptr; }
  /// The same object as a handle of the base type B, which the map records D derives from.
  /// @tparam B A Type entry that D is or derives from.
  /// @return The handle of type B to the same object.
  template <const sdk::EntryDesc& B>
    requires(derives_from<D, B>())
  constexpr operator Ref<B>() const {
    return Ref<B>::unchecked(object_);
  }
  /// The same object as a handle of an unnamed type.
  /// @return The Object handle to the same object.
  constexpr operator Object() const { return Object::unchecked(object_); }
  /// Whether two handles name the same object (or are both null), whatever their types.
  /// @tparam O The other handle's Type entry.
  /// @param other The other handle.
  /// @return True for the same object.
  template <const sdk::EntryDesc& O>
  constexpr bool operator==(const Ref<O>& other) const {
    return object_ == other.raw();
  }
  /// Whether the handle is null.
  /// @return True for a null handle.
  constexpr bool operator==(std::nullptr_t) const { return object_ == nullptr; }
  /// transitional: removed in part 5. The raw reference a Ref used to be, so code that assigns a
  /// wrapper's answer to a void* or passes one to a void* parameter compiles while a wrapper gives
  /// handles. New code keeps a handle; nothing here gives Ref a constructor from a void*, which stays
  /// the point of the handles.
  /// @return The object, or null.
  constexpr operator void*() const { return object_; }

 private:
  void* object_ = nullptr;
};

namespace detail {
// A Type entry for the layout checks below, of no game type.
inline constexpr sdk::EntryDesc kRefLayout{.key = "pm.api.RefLayout"};
}  // namespace detail

// A handle is the pointer it replaces, and is copied as one: it fits where the void* did (a field, an argument, a
// host contract slot) and a Field or array element can hold it.
static_assert(sizeof(Ref<detail::kRefLayout>) == sizeof(void*) && sizeof(Object) == sizeof(void*),
              "a handle is one pointer");
static_assert(std::is_trivially_copyable_v<Ref<detail::kRefLayout>> && std::is_trivially_copyable_v<Object>,
              "a handle is copied as plain bytes");

}  // namespace pm::api
