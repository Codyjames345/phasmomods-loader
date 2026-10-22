#pragma once

#include <cstdint>

// The Echo's repeat, kept free of game calls so it can be tested offline. It repeats what players do: soon
// after a player uses a light switch or a door in its favourite room, its next interaction uses that same
// switch or door.
namespace pg::custom {

constexpr uint64_t kEchoRepeatMs = 60000;  // how long after a player's use the Echo still repeats it

/**
 * The switch or door the Echo's next interaction repeats. Only the most recent use in its room counts, for
 * kEchoRepeatMs, and it is handed out once. `Keep` holds the object between frames: il2cpp::Held in the
 * game, so the collector keeps it alive while it waits; a plain pointer in the offline tests.
 */
template <class Keep = void*>
class Repeat {
 public:
  /** A player used `object` at `now` (milliseconds); a use outside its room changes nothing. */
  void used(void* object, bool inItsRoom, uint64_t now) {
    if (!inItsRoom) return;
    object_ = object;
    at_ = now;
  }

  /** The object to repeat at `now`, once; null when nothing was used in its room within kEchoRepeatMs. */
  void* take(uint64_t now) {
    void* object = pending(now) ? static_cast<void*>(object_) : nullptr;
    object_ = nullptr;
    return object;
  }

  /** Whether a use waits to be repeated at `now`. */
  bool pending(uint64_t now) const { return object_ != nullptr && now - at_ <= kEchoRepeatMs; }

 private:
  Keep object_{};
  uint64_t at_ = 0;
};

}  // namespace pg::custom
