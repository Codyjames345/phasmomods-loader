#pragma once

#include "score.h"

// What an investigator's own client tells the ghost player for the deception score (outcome.h), read
// from the game through the map. Main thread.
namespace pm::guess {

// `player`'s guess: whether it is dead (Player.isDead) and the ghost type chosen in its journal
// (JournalController.GetGhostType), or score::kNoGuess when none is chosen (GhostType.None) or the type
// does not fit. The default Guess for a null or destroyed player, and no type when its journal is not one.
score::Guess of(void* player);

}  // namespace pm::guess
