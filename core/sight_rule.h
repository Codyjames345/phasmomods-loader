#pragma once

#include <cstddef>

// Whether the ghost's vision sees a player, kept free of game calls so it can be tested offline. The
// ghost's player vision sensor (GhostAI.vision, a LOSSensor) detects the objects of a player's
// PlayerLOSTarget components (Player.losTargets), and the game's own hunt sight test (GhostHuntState.Sees) asks for
// those: the ghost sees a player when it detects any of them. The Player's own GameObject is not among
// what it detects.
namespace pm::game {

/** True when `detects` holds for any of the `count` parts (null slots skipped); false for no parts. */
template <class Detects>
bool sees_a_part(void* const* parts, size_t count, Detects detects) {
  for (size_t i = 0; parts && i < count; ++i)
    if (parts[i] && detects(parts[i])) return true;
  return false;
}

}  // namespace pm::game
