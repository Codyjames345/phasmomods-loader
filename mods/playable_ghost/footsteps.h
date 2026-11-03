#pragma once

#include <vector>

#include "perceive.h"

// What the Kormos hears in a hunt, through the game API: each noise its hearing holds now (GhostHearing.heard,
// a List of HeardNoise structs), as a mark where the investigator who made it stands, by the kind of
// footstep (sprint, crouch, or a walk for any other kind). Dead and departed investigators make none.
// Host, main thread.
namespace pm::footsteps {

void heard(void* ghost, std::vector<perceive::Mark>& out);

}  // namespace pm::footsteps
