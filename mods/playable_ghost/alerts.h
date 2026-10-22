#pragma once

#include "perceive.h"
#include "session.h"

// E3: things done to the ghost, caught where the game itself handles them (perceive.h lists them).
// Most are RPCs every client runs, some only the host runs (the ghost's reaction to its name), so
// every client hooks them: the ghost player's own client keeps what it sees, and the host relays
// what it sees to a remote ghost player, who drops the copy of anything already shown
// (perceive::Recent). Only while a player controls the ghost in a contract. Main thread.
namespace pg::alerts {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame. ghost: the ghost's GhostAI on this client (null when there is none).
void tick(session::Session& s, void* ghost, bool ghostControlled);
bool take(perceive::Alert& out);  // ghost player: an alert caught on this client
void teardown();

}  // namespace pg::alerts
