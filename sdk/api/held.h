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
class Held {
 public:
  Held() = default;
  Held(std::nullptr_t) {}
  Held(void* object) : object_(object), handle_(detail::gc_hold(object)) {}
  Held(Held&& other) noexcept : object_(other.object_), handle_(other.handle_) {
    other.object_ = nullptr;
    other.handle_ = 0;
  }
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
  Held& operator=(void* object) {
    if (object != object_) {
      detail::gc_release(handle_);
      object_ = object;
      handle_ = detail::gc_hold(object);
    }
    return *this;
  }
  Held& operator=(std::nullptr_t) { return *this = static_cast<void*>(nullptr); }
  Held(const Held&) = delete;
  Held& operator=(const Held&) = delete;
  ~Held() { detail::gc_release(handle_); }
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

}  // namespace pm::api
