#pragma once

#include <cstdint>

// When a dead investigator may watch through the ghost's eyes ("haunting"), and what that costs them.
// Pure, testable offline.
//  - Holding the key for kHoldMs toggles it, only while dead and while there is a ghost to watch.
//  - While haunting they cannot move or touch anything; after it they still cannot touch anything
//    for kLockMs, so they cannot spot the ghost and then point it out by throwing things at it.
//  - Haunting ends by itself when the ghost or the contract goes (or they are no longer dead).
namespace pg::afterlife {

constexpr uint64_t kHoldMs = 800;
constexpr uint64_t kLockMs = 60000;

class Rules {
 public:
  /** One frame. Returns true when haunting started or stopped. */
  bool step(bool dead, bool ghostPresent, bool keyDown, uint64_t now) {
    bool changed = false;
    if (haunting_ && (!dead || !ghostPresent)) {
      stop(now);
      changed = true;
    }
    if (!keyDown) {
      held_ = false;
      fired_ = false;
      return changed;
    }
    if (!held_) held_ = true, heldSince_ = now;
    if (fired_ || now - heldSince_ < kHoldMs || !dead || !ghostPresent) return changed;
    fired_ = true;  // one toggle per hold
    if (haunting_) stop(now);
    else haunting_ = true;
    return true;
  }

  bool haunting() const { return haunting_; }
  bool touch_locked(uint64_t now) const { return haunting_ || now < lockedUntil_; }
  /** Whole seconds left until they may touch things again (0 when they may). */
  unsigned lock_seconds(uint64_t now) const {
    if (haunting_ || now >= lockedUntil_) return 0;
    return static_cast<unsigned>((lockedUntil_ - now + 999) / 1000);
  }
  void reset() { *this = Rules{}; }

 private:
  void stop(uint64_t now) {
    haunting_ = false;
    lockedUntil_ = now + kLockMs;
  }

  bool haunting_ = false;
  bool fired_ = false;
  bool held_ = false;
  uint64_t heldSince_ = 0;
  uint64_t lockedUntil_ = 0;
};

}  // namespace pg::afterlife
