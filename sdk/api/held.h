#pragma once

#include <cstddef>
#include <cstdint>

#include "ref.h"

namespace pm::api {

namespace detail {

// The host contract's gc_hold and gc_release (core/mod_api.h, version 2.1); 0 and nothing before
// bind.
uint32_t gc_hold(void* object);
void gc_release(uint32_t handle);

}  // namespace detail

/// A managed object a mod keeps across frames, with a strong GC handle from the host contract's
/// gc_hold. This is how a mod keeps one: the GC does not scan a mod's memory, and Unity keeps only a
/// weak handle on most engine objects' wrappers (GameObject, Transform; a MonoBehaviour only until it
/// is destroyed). An unheld wrapper can be collected while the mod still points at it, and Unity
/// then cannot clear its native pointer when the object is destroyed, so alive() would report a
/// destroyed object as alive. (host/il2cpp.h keeps the host's own copy, il2cpp::Held, for the host's
/// code; mod code uses this one.)
///
/// It reads as the raw pointer it replaces (assigned, tested, compared and passed as a void*), and
/// is moved, never copied. Assigning the object it already holds keeps its handle. A Held made
/// before pm::api::bind holds nothing (its handle is 0), so make it after bind. Main thread.
/// @ingroup runtime
/// @since Contract 2.1
/// @code
/// static pm::api::Held g_ghost;
/// g_ghost = ghost;  // kept across frames
/// if (pm::api::alive(g_ghost)) phasmo::GhostAI::UnAppear(g_ghost);
/// @endcode
class Held {
 public:
  /// Holds nothing.
  Held() = default;
  /// Holds nothing, as a null pointer.
  Held(std::nullptr_t) {}
  /// Holds `object` with a strong GC handle.
  /// @param object The managed object to keep; null holds nothing.
  Held(void* object) : object_(object), handle_(detail::gc_hold(object)) {}
  /// Takes over `other`'s object and handle; `other` then holds nothing.
  /// @param other The Held to move from.
  Held(Held&& other) noexcept : object_(other.object_), handle_(other.handle_) {
    other.object_ = nullptr;
    other.handle_ = 0;
  }
  /// Releases what this holds, then takes over `other`'s object and handle; `other` then holds nothing.
  /// @param other The Held to move from.
  /// @return This Held.
  Held& operator=(Held&& other) noexcept {
    if (this != &other) {
      detail::gc_release(handle_);
      object_ = other.object_;
      handle_ = other.handle_;
      other.object_ = nullptr;
      other.handle_ = 0;
    }
    return *this;
  }
  /// Holds `object` instead, releasing what this held. Assigning the object already held keeps its handle.
  /// @param object The managed object to keep; null holds nothing.
  /// @return This Held.
  Held& operator=(void* object) {
    if (object != object_) {
      detail::gc_release(handle_);
      object_ = object;
      handle_ = detail::gc_hold(object);
    }
    return *this;
  }
  /// Releases what this holds, and holds nothing.
  /// @return This Held.
  Held& operator=(std::nullptr_t) { return *this = static_cast<void*>(nullptr); }
  Held(const Held&) = delete;
  Held& operator=(const Held&) = delete;
  ~Held() { detail::gc_release(handle_); }
  /// The held object, as the raw pointer it replaces.
  /// @return The object, or null. Check pm::api::alive before using an engine object kept from an
  ///   earlier frame.
  operator void*() const { return object_; }
  /// The held object as a T*: static_cast<T*>(held), as from the void* it replaced.
  /// @return The object, or null.
  template <typename T>
  explicit operator T*() const {
    return static_cast<T*>(object_);
  }
  /// The held object.
  /// @return The object, or null.
  void* get() const { return object_; }

 private:
  void* object_ = nullptr;
  uint32_t handle_ = 0;
};

/// A Held of a typed handle (ref.h): keeps a managed object of the map's type D across frames with a
/// strong GC handle, exactly as Held does, and gives it back as a Ref<D>. It reads as the Ref it replaces:
/// assigned a Ref<D> or a Ref of a type derived from D, tested, and passed where a Ref<D>, a Ref of one of
/// D's base types or an Object is taken. It is moved, never copied. Assigning the object it already holds
/// keeps its handle. Make it after pm::api::bind, as a Held. Main thread.
/// @tparam D A map entry of kind Type.
/// @ingroup runtime
/// @code
/// static pm::api::HeldRef<pm::map::GhostAI> g_ghost;
/// g_ghost = ghost;  // a Ref<pm::map::GhostAI>, kept across frames
/// if (g_ghost && pm::api::alive(g_ghost.get().raw())) { ... }
/// @endcode
template <const sdk::EntryDesc& D>
class HeldRef {
 public:
  /// Holds nothing.
  HeldRef() = default;
  /// Holds nothing, as a null handle.
  HeldRef(std::nullptr_t) {}
  /// Holds `object` with a strong GC handle.
  /// @tparam S The handle's type: D or a type derived from it.
  /// @param object The object to keep; a null handle holds nothing.
  template <const sdk::EntryDesc& S>
    requires(derives_from<S, D>())
  HeldRef(Ref<S> object) : held_(object.raw()) {}
  /// Takes over `other`'s object and handle; `other` then holds nothing.
  /// @param other The HeldRef to move from.
  HeldRef(HeldRef&& other) noexcept = default;
  /// Releases what this holds, then takes over `other`'s object and handle; `other` then holds nothing.
  /// @param other The HeldRef to move from.
  /// @return This HeldRef.
  HeldRef& operator=(HeldRef&& other) noexcept = default;
  /// Holds `object` instead, releasing what this held. Assigning the object already held keeps its handle.
  /// @tparam S The handle's type: D or a type derived from it.
  /// @param object The object to keep; a null handle holds nothing.
  /// @return This HeldRef.
  template <const sdk::EntryDesc& S>
    requires(derives_from<S, D>())
  HeldRef& operator=(Ref<S> object) {
    held_ = object.raw();
    return *this;
  }
  /// Releases what this holds, and holds nothing.
  /// @return This HeldRef.
  HeldRef& operator=(std::nullptr_t) {
    held_ = nullptr;
    return *this;
  }
  /// The held object.
  /// @return The handle, or a null one. Check pm::api::alive before using an engine object kept from an
  ///   earlier frame.
  Ref<D> get() const { return Ref<D>::unchecked(held_.get()); }
  /// The held object, as the handle of D or of one of its base types.
  /// @tparam B D or a type D derives from.
  /// @return The handle, or a null one.
  template <const sdk::EntryDesc& B>
    requires(derives_from<D, B>())
  operator Ref<B>() const {
    return get();
  }
  /// The held object, as a handle of an unnamed type (for pm::api::as).
  /// @return The handle, or a null one.
  operator Object() const { return get(); }
  /// Whether it holds an object.
  /// @return True when it holds one.
  explicit operator bool() const { return held_.get() != nullptr; }

 private:
  Held held_;
};

}  // namespace pm::api
