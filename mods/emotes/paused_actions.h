#pragma once

// The game's input actions the wheel pauses while it is open (its look, item swap and bumper actions), held so that every
// way the wheel closes gives back exactly the actions it paused, and no action the game had off is turned on.

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace pm::emotes {

/// The actions the wheel has paused and not yet given back. `Handle` keeps one action across frames: the mod uses
/// pm::api::Held, so the game's object stays pinned; it is made from the action's pointer and has get().
template <class Handle>
class PausedActions {
 public:
  /// Remembers `enabled`, the actions the caller is about to disable, each once; an action already held is not added
  /// again.
  void pause(std::span<void* const> enabled) {
    for (void* action : enabled)
      if (std::none_of(held_.begin(), held_.end(), [&](const Handle& h) { return h.get() == action; }))
        held_.emplace_back(action);
  }

  /// The actions to enable again, each once, in the order paused; the set is empty afterwards.
  std::vector<Handle> resume() {
    std::vector<Handle> back;
    back.swap(held_);
    return back;
  }

  /// Whether any action is held.
  bool paused() const { return !held_.empty(); }

 private:
  std::vector<Handle> held_;
};

}  // namespace pm::emotes
