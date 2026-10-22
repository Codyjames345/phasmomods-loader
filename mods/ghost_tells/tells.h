#pragma once

// The Ghost Tells mod's shared rules, kept free of game calls so they can be tested offline: which
// ghosts it gives new tells to, and the small timers every tell is made of.
namespace pg::tells {

// GhostType values of the behaviour types the tells follow (GhostInfo +0x2C), as the game's own type
// rules do, so a Mimic copying one of them shows its tells.
constexpr int kSpirit = 0;
constexpr int kMare = 6;
constexpr int kShade = 8;
constexpr int kYurei = 10;
constexpr int kGoryo = 14;
constexpr int kOnryo = 16;

inline bool told(int type) {
  return type == kSpirit || type == kMare || type == kShade || type == kYurei || type == kGoryo || type == kOnryo;
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

/** The held decision for this contract (mod.cpp): every client in this private room runs the mod. */
bool tells_on();

}  // namespace pg::tells
