#pragma once

#include <type_traits>

#include "Difficulty.h"
#include "runtime.h"

// The Squatter's one reader of the contract's difficulty. Main thread.
namespace pm::custom::squatter {

/**
 * Whether the contract's difficulty is a friendly ghost's (Difficulty.friendlyGhost). True when it cannot be read
 * (no live LevelValues, no difficulty, or the entry unresolved): the Squatter then mopes rather than chases and never
 * locks the exits, the side where nothing can be refused.
 */
inline bool friendly() {
  using Friendly = std::remove_cvref_t<decltype(phasmo::Difficulty::friendlyGhost)>;
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = values && api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  return !difficulty || !Friendly::use::resolved() || phasmo::Difficulty::friendlyGhost.get(difficulty);
}

}  // namespace pm::custom::squatter
