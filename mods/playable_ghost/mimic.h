#pragma once

#include "session.h"

// The Mimic's copies, chosen by its player. The game switches a Mimic's copied type on its own timer
// (GhostController.AttemptGhostTypeStateChange) to a random one of the types it may copy. On the host,
// the ghost player is offered those types and may pick the next copy; the pick is put in the game's
// own override when the game's timer next switches, so the copy changes exactly when it would have,
// to the player's choice instead of a random one. Main thread.
namespace pg::mimic {

bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s, void* ghost);  // host, every frame (ghost null: none)
uint8_t next();  // the pick waiting for the next switch, or session::kNoType

}  // namespace pg::mimic
