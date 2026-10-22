#pragma once

#include <cstdint>

// The host's side of typestate.h: reads a ghost type's own state from the game (and from the Custom
// Ghosts mod for its types) into the two status bytes. Main thread, host only.
namespace pg::typestate {

bool resolve();
void read(void* ghost, int behaviourType, int identity, uint8_t trait[2]);
void forget();  // the ghost is no longer steered: drop what read() kept of it

}  // namespace pg::typestate
