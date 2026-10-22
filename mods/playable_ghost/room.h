#pragma once

#include "session.h"

// Glue between the Photon room, the session and the in-contract control. Main thread.
namespace pg::room {

void init();  // after net::resolve
void tick();  // once per frame
void teardown();
// A player controls the ghost in the contract now running.
bool ghost_controlled();
int ghost_actor();  // the ghost player's actor, 0 for none

}  // namespace pg::room
