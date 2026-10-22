#pragma once

#include "session.h"

// The ghost in an investigator's form. The game's own "player" ghost event (GhostEventPlayer) is
// only a particle cloud that walks to a player (its prefab has no body), so the form is made here:
// every client but the ghost player's bakes the investigator's body as it is drawn on that client
// (their outfit, in the pose of the moment) into a still figure that stands where the ghost is and
// moves and turns with it until the event ends. Nothing networked is copied; the figure is local
// to each client and gone when the event, the ghost or the contract ends. Main thread.
namespace pg::form {

constexpr uint16_t kFormMs = 5000;  // how long the ghost keeps the form

bool resolve();
void tick(session::Session& s, void* ghost);  // every frame; ghost: the replicated GhostAI or null
void teardown();

}  // namespace pg::form
