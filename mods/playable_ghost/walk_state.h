#pragma once

#include "tether.h"

// What the ghost's own state objects say about its walk, read through the map for the rules in tether.h.
// Main thread. A state object is valid for the current frame only.
namespace pm::walk_state {

// Whether `state` is the hunt, with no target and a player it searches for (GhostHuntState.target and
// searchTarget). False for a null state and any other state's object, and when the hunt's entries did not resolve.
bool hunt_searching(void* state);

// The reach of `state`, a wander or a D.O.T.S. walk, begun at (startX, startZ): its radius, or the point it chose
// when that is further (tether::reach_of). False, leaving `out` alone, for a null state, for any other state's
// object and when the state's type did not resolve.
bool wander_reach(void* state, float startX, float startZ, tether::Reach& out);

}  // namespace pm::walk_state
