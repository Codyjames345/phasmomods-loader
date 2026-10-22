#pragma once

#include <cstdint>

// The Echo's repeat and its dormancy, kept free of game calls so they can be tested offline. It repeats what
// players do: soon after a player uses a light switch or a door in its favourite room, its next interaction
// once it is in reach of that switch or door uses it. With nobody heard near it for a minute it is dormant
// and loses most of its interactions and events, but never a repeat.
namespace pg::custom {

constexpr uint64_t kEchoRepeatMs = 60000;  // how long after a player's use the Echo still repeats it
constexpr int kEchoDormantLoss = 75;       // percent of a dormant Echo's AI interactions and events lost

/** Whether a dormant Echo's AI loses an interaction or event, for a roll in 0..99. */
inline bool echo_dormant_loses(int roll) { return roll < kEchoDormantLoss; }

enum class EchoRoll { Repeat, Lost, Game };  // what becomes of the Echo AI's interaction roll

/**
 * What becomes of the Echo AI's interaction roll: its repeat when that fired (the repeat is the interaction,
 * so no dormancy roll is drawn); otherwise, while `dormant`, lost when `roll()` (0..99) falls in the share it
 * loses; else the game's own interaction. `roll` is drawn only for a dormant Echo whose repeat did not fire.
 */
template <class Roll>
EchoRoll echo_roll(bool repeated, bool dormant, Roll roll) {
  if (repeated) return EchoRoll::Repeat;
  return dormant && echo_dormant_loses(roll()) ? EchoRoll::Lost : EchoRoll::Game;
}

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
    void* object = pending(now);
    object_ = nullptr;
    return object;
  }

  /** The object waiting to be repeated at `now`, left for take; null when none. */
  void* pending(uint64_t now) const {
    void* object = static_cast<void*>(object_);
    return object && now - at_ <= kEchoRepeatMs ? object : nullptr;
  }

 private:
  Keep object_{};
  uint64_t at_ = 0;
};

}  // namespace pg::custom
