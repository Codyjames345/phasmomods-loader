#pragma once

#include <string_view>

// The Ghost Tells mod's shared rules, kept free of game calls so they can be tested offline: which
// ghosts it gives new tells to, and the small timers every tell is made of.
namespace pm::tells {

// The six vanilla ghosts with new tells, in the mod's own numbering (not the game's GhostType). The
// game names each by its vanilla content id; the content layer runs a ghost's rules while the
// contract's ghost acts as it, a Mimic's copy included (mod.cpp).
enum Told : int { kSpirit, kMare, kShade, kYurei, kGoryo, kOnryo, kToldCount };

inline constexpr const char* kToldIds[kToldCount] = {"vanilla.spirit", "vanilla.mare",  "vanilla.shade",
                                                     "vanilla.yurei",  "vanilla.goryo", "vanilla.onryo"};

/** The told ghost a content id names, or -1 for any other. */
inline int told_of(std::string_view id) {
  for (int i = 0; i < kToldCount; ++i)
    if (id == kToldIds[i]) return i;
  return -1;
}

/** Reports the frame a condition becomes true. */
struct Edge {
  bool was = false;
  bool rose(bool now) {
    const bool r = now && !was;
    was = now;
    return r;
  }
};

/** A countdown in seconds of play. */
struct Timer {
  float left = 0;
  void start(float seconds) { left = seconds; }
  void stop() { left = 0; }
  bool running() const { return left > 0; }
  /** One frame; true while it still runs after this frame's time is taken off. */
  bool step(float dt) {
    if (left <= 0) return false;
    left -= dt;
    return left > 0;
  }
};

/** The told ghost whose rules run now (mod.cpp): the host runs them only while this client's mods
 *  match the host's list (sdk/README.md, "The host's list"); -1 otherwise. Main thread. */
int told_now();

}  // namespace pm::tells
