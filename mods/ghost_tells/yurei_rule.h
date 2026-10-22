#pragma once

#include <vector>

#include "tells.h"

// The Yurei's tells, kept free of game calls so they can be tested offline. "Smudging the Yurei's
// place of death will trap it temporarily": when its incense trap starts it shuts the doors of its room
// on itself and keeps them shut while trapped; the door its ability slams is held locked for a moment
// with the game's hunt lock; and a door left open in its room draws its ability.
namespace pg::tells {

constexpr float kYureiTrapSeconds = 90.0f;   // the game's own trap (TemporarilyStopWander(90))
constexpr float kYureiReseal = 3.0f;         // seconds a door may stand open while it is trapped
constexpr float kYureiHold = 3.0f;           // seconds its ability's door stays locked
constexpr float kYureiDraw = 20.0f;          // seconds a door stands open in its room before it draws the ability
constexpr float kYureiDrawCooldown = 60.0f;

/** Its own room's doors while its incense trap lasts. */
struct Seal {
  Timer trap;
  bool first = false;
  std::vector<float> openFor;
  void start() {
    trap.start(kYureiTrapSeconds);
    first = true;
    openFor.clear();
  }
  bool active() const { return trap.running(); }
  /** One frame with each door's `open` state; the doors to shut now. */
  std::vector<int> step(const std::vector<bool>& open, float dt) {
    std::vector<int> shut;
    if (!trap.running()) return shut;
    openFor.resize(open.size(), 0.0f);
    for (size_t i = 0; i < open.size(); ++i) {
      openFor[i] = open[i] ? openFor[i] + dt : 0.0f;
      if (open[i] && (first || openFor[i] >= kYureiReseal)) {
        shut.push_back(static_cast<int>(i));
        openFor[i] = 0.0f;
      }
    }
    first = false;
    trap.step(dt);
    return shut;
  }
};

/** The door its ability slammed, locked until kYureiHold has passed. */
struct Hold {
  Timer t;
  void lock() { t.start(kYureiHold); }
  bool pending() const { return t.running(); }
  /** One frame; true on the frame the door is to be unlocked. */
  bool step(float dt) { return t.running() && !t.step(dt); }
};

/** A door of its current room left open long enough draws its ability, now and then. */
struct Draw {
  std::vector<float> openFor;
  float cooldown = 0;
  /** One frame with each door's `open` state; the door to draw it to, or -1. */
  int step(const std::vector<bool>& open, bool mayAct, float dt) {
    cooldown -= dt;
    openFor.resize(open.size(), 0.0f);
    int longest = -1;
    for (size_t i = 0; i < open.size(); ++i) {
      openFor[i] = open[i] ? openFor[i] + dt : 0.0f;
      if (openFor[i] >= kYureiDraw && (longest < 0 || openFor[i] > openFor[longest])) longest = static_cast<int>(i);
    }
    if (!mayAct || cooldown > 0 || longest < 0) return -1;
    cooldown = kYureiDrawCooldown;
    openFor[longest] = 0.0f;
    return longest;
  }
  /** Another room's doors: their open times start again, but its minute between draws does not. */
  void reset() { openFor.clear(); }
};

/**
 * The door the next ability is aimed at (the Draw's door). The ability's entry first changes the ghost to
 * idle (whose own entry rolls Random.Range(0, 4) for its animation), then rolls Random.Range(0, doors) for
 * the door to slam: only that roll, after the idle change, is answered, and only once.
 */
struct Steer {
  int door = -1, doors = 0;
  bool inside = false, armed = false;
  void aim(int index, int count) {
    door = index;
    doors = count;
  }
  void begin() {
    inside = true;
    armed = false;
  }
  void idle_done() { armed = inside && door >= 0; }
  bool take(int min, int max, int& result) {
    if (!armed || min != 0 || max != doors) return false;
    result = door;
    armed = false;
    door = -1;
    return true;
  }
  void end() { *this = Steer{}; }
};

}  // namespace pg::tells
