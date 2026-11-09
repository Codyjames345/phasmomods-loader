#pragma once

#include <cstddef>
#include <cstdint>

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

}  // namespace pm::api
