#pragma once

#include <algorithm>
#include <vector>

// The Mare's tells, kept free of game calls so they can be tested offline. "The source of all
// nightmares, more powerful in the dark": it never shows itself in a lit room (it puts the lights out
// first), it leaves the rooms it hunted through dark, and its vanilla switch-off of a light a player
// has just turned on happens 1 time in 2 instead of 1 in 7.
namespace pg::tells {

constexpr int kMareSwitchOffOneIn = 2;  // the vanilla coroutine's Random.Range(0, 7) == 1 becomes 1 in this
constexpr float kMareTrailStep = 0.5f;  // seconds between looks at the room it hunts through

constexpr int kMareVanillaRollMax = 7;  // the coroutine's Random.Range(0, 7)

/** Whether a roll is the switch-off coroutine's own: made while it runs, with its arguments. */
inline bool mare_roll_applies(bool inCoroutine, int min, int max) { return inCoroutine && min == 0 && max == kMareVanillaRollMax; }

/** The value the switch-off coroutine's roll should give it for a fresh roll in [0, kMareSwitchOffOneIn). */
inline int mare_roll(int fresh) { return fresh == 0 ? 1 : 0; }

enum class Appearance { Appear, DarkenFirst, Withhold };

/**
 * What it does as it is about to show itself outside a hunt, in a room whose lights are `roomLit`, where
 * `litSwitches` switches are on and `usableLitSwitches` of those can be used (not broken).
 */
inline Appearance mare_appearance(bool roomLit, int litSwitches, int usableLitSwitches) {
  if (!roomLit) return Appearance::Appear;
  return litSwitches > 0 && usableLitSwitches == litSwitches ? Appearance::DarkenFirst : Appearance::Withhold;
}

/** The rooms a hunt passed through, each once, in the order it reached them. */
struct RoomTrail {
  std::vector<int> passed;
  void pass(int room) {
    if (room >= 0 && std::find(passed.begin(), passed.end(), room) == passed.end()) passed.push_back(room);
  }
  const std::vector<int>& rooms() const { return passed; }
  void clear() { passed.clear(); }
};

}  // namespace pg::tells
