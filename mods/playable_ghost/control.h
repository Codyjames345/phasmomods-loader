#pragma once

#include "session.h"

// Playable ghost in a contract. Two sides, possibly on the same client:
//  - ghost player: the client whose actor is the assigned ghost. It has no investigator in the
//    contract (body.h); it sees from the ghost with its own camera and sends movement and triggers.
//  - authority: the master client, which runs the native GhostAI. It applies the ghost player's
//    movement through native steering and runs their chosen actions (actions.h).
// Everything here runs on Unity's main thread.
namespace pg::control {

void tick(session::Session& s);
void teardown();  // release everything owned; safe to call repeatedly

}  // namespace pg::control
