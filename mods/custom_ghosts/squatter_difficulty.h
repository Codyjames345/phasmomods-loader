#pragma once

#include <optional>
#include <type_traits>

#include "Difficulty.h"
#include "runtime.h"

// The Squatter's one reader of the contract's difficulty. Main thread.
namespace pm::custom::squatter {

/**
 * Whether the contract's difficulty is a friendly ghost's (Difficulty.friendlyGhost), or none when it cannot be read
 * yet: no live LevelValues, no difficulty, or the entry unresolved.
 */
inline std::optional<bool> friendly_if_read() {
  using Friendly = std::remove_cvref_t<decltype(phasmo::Difficulty::friendlyGhost)>;
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = values && api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  if (!difficulty || !Friendly::use::resolved()) return std::nullopt;
  return phasmo::Difficulty::friendlyGhost.get(difficulty);
}

/**
 * Whether the contract's difficulty is a friendly ghost's, true when it cannot be read: the exits never lock on a
 * guess, the side where nothing can be refused.
 */
inline bool friendly() { return friendly_if_read().value_or(true); }

}  // namespace pm::custom::squatter
