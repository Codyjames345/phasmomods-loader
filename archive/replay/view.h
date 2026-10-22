#pragma once

#include <string>

#include "moments.h"

// The replay in the lobby: a panel over the menu with the level's rooms seen from above (the floor
// the ghost was on), the ghost and the investigators moving through each top moment, a caption for
// each and who played the ghost. Plays once; a click closes it. Main thread.
namespace pg::replay::view {

bool resolve();
void show(const Replay& replay);  // shown once the lobby is up (no contract running)
void tick();
void hide();

}  // namespace pg::replay::view
