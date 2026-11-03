#pragma once

#include <string_view>

// Which of the game's draws of Random.RandomRangeInt belong to what, from the scopes they run in rather
// than from where they are called. Read from the decompiled GhostIdleState.Execute, GhostActivity.doorChance,
// GhostActivity.lightChance and GhostActivity.InteractWithARandomProp. Pure logic, tested offline.
namespace pm::roll {

// The idle roll's own draws, in order: a coin (0 to 2), then on 1 either the hunt branch's final 1-in-3, 4
// or 5 chance (every native check having passed) or the activity roll. What Execute calls before the coin
// and between the two draws nothing, but for the state change it ends with when flames have been blown
// out, which is not its own (`inStateChange`).
struct Idle {
  int coin = -1;           // the coin, or -1 before it
  bool decided = false;    // the draw after a coin of 1 came
  bool huntRolled = false;  // that draw was the hunt branch's chance, and it passed

  void begin() { *this = {}; }
  void draw(int min, int max, int r, bool inStateChange) {
    if (inStateChange) return;
    if (coin < 0 && min == 0 && max == 2) {
      coin = r;
    } else if (coin == 1 && !decided) {
      decided = true;
      huntRolled = min == 0 && max >= 3 && max <= 5 && r == 1;
    }
  }
  // The roll reached its hunt check without hunting.
  bool passed_without_hunting() const { return coin == 1 && !huntRolled; }
};

// A door, light or prop chance draws its roll of 0 to 100 before anything else in it that could draw one,
// so the first such draw after it begins is its own (the rest are not: the door's state change may draw
// more). Inside an aimed interaction that roll is answered 0 (the category chance becomes the
// interaction's cost); every other draw is the game's.
struct Chance {
  bool due = false;

  int draw(int min, int max, int r, bool aimed) {
    if (due && min == 0 && max == 100) {
      due = false;
      if (aimed) return 0;
    }
    return r;
  }
};

// A chance's roll is due while it runs: begun with the chance, and put back as it was when the chance
// ends, whatever it ran or threw. A chance run inside another so leaves the outer one's state as it found it.
class ChanceScope {
 public:
  explicit ChanceScope(Chance& chance) : chance_(chance), outer_(chance.due) { chance.due = true; }
  ~ChanceScope() { chance_.due = outer_; }
  ChanceScope(const ChanceScope&) = delete;
  ChanceScope& operator=(const ChanceScope&) = delete;

 private:
  Chance& chance_;
  bool outer_;
};

// Counts the calls running (GhostAI.ChangeState's, whose draws are not the idle roll's own) while alive,
// whatever they run or throw; nested calls count each.
class Running {
 public:
  explicit Running(int& count) : count_(count) { ++count_; }
  ~Running() { --count_; }
  Running(const Running&) = delete;
  Running& operator=(const Running&) = delete;

 private:
  int& count_;
};

// What an aimed interaction is, by the state the game's interaction chain set for it.
enum class Aimed { Door, Light, Prop };

// What an aimed interaction costs of the interaction charge: how much rarer the type's own chance makes that
// kind than the usual chance does (the door and light chances roll against 25 in 100, the prop chance
// against 100), so a kind its type rolls rarely costs more. `type` is the vanilla id of the type whose rules
// the ghost's AI follows ("vanilla.deogen"), empty for none.
inline float interaction_cost(std::string_view type, Aimed kind) {
  const bool deildegast = type == "vanilla.deildegast";
  switch (kind) {
    case Aimed::Door:  // pm:not-game-data: literals in the door chance's code (GhostActivity.doorChance)
      return 25.0f / (type == "vanilla.deogen" || type == "vanilla.thaye" ? 30.0f : deildegast ? 10.0f : 25.0f);
    case Aimed::Light:  // pm:not-game-data: literals in the light chance's code (GhostActivity.lightChance)
      return 25.0f / (deildegast ? 10.0f : 25.0f);
    case Aimed::Prop:  // pm:not-game-data: literals in the prop chance's code (GhostActivity.InteractWithARandomProp)
      return 100.0f / (type == "vanilla.poltergeist" ? 20.0f : deildegast ? 85.0f : 100.0f);
  }
  return 1.0f;
}

}  // namespace pm::roll
