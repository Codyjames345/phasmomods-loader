#pragma once

#include <cstdint>

// The host's side of typestate.h: reads a game ghost type's own state from the game into the two status
// bytes, or a registered ghost's from its definition (sdk/api/content.h hud_state). Main thread, host only.
namespace pm::typestate {

bool resolve();
// The state of the ghost `ghost`, whose AI follows the rules of `behaviourType`, into `trait`. True when
// trait[0] is the definition's state of the type it acts as (a registered ghost's), which that
// definition's hudLine reads on the ghost player's client (session::kCustomState).
bool read(void* ghost, int behaviourType, uint8_t trait[2]);
void forget();  // the ghost is no longer steered: drop what read() kept of it

}  // namespace pm::typestate
