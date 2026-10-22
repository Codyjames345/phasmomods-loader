#pragma once

#include "session.h"

// "Play as ghost": a copy of the game's own Ready tape (ServerManager.readyToggle and its label),
// stuck to the bottom edge of this player's own ID card in the lobby. Ticking it volunteers this
// player; the host's client draws the ghost player among the volunteers automatically, and the tape
// then reads "You are the ghost" on that player's card. Nobody is drawn while anyone in the room lacks
// the mod, and a volunteer's tape then reads "Everyone needs the mod". Main thread only; the tape
// exists while the lobby screen and this player's card do.
namespace pg::lobby {

bool resolve();
void tick(session::Session& s);
void teardown();

}  // namespace pg::lobby
