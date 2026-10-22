#pragma once

// What the ghost player must not see: salt piles and crucifixes. A vanilla ghost walks into salt and
// tries to hunt beside a crucifix because it does not know they are there; a ghost player who could
// see them would simply avoid them. On the ghost player's client only, their renderers (and those of
// everything under them, effects included) stay switched off for the whole contract. Nothing changes
// for anyone else, and the objects themselves still work. Main thread.
namespace pg::hide {

bool resolve();
void tick(bool ghostPlayerInContract);  // every frame

}  // namespace pg::hide
