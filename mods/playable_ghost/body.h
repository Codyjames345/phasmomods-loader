#pragma once

#include "session.h"

// The ghost player is not an investigator, but their investigator (the networked Player the game
// creates for them in every contract) stays in the contract, parked where it stood in the truck, with
// its renderers, colliders, lights and sounds switched off. It is parked only once the ghost player's
// own client has stepped out of it (below) and said so (session HELLO): until then it is still their
// investigator, for everyone.
//
// While it is parked, and once this client's own load wait is over (GameController.allPlayersLoaded),
// the ghost player's spot is out of the game's player list on every client (ghost-player rounds need
// every client to run the mod, src/session.h), so no rule or screen that walks the list sees them:
// src/outlist.h. It goes back when the body comes back into play: the level gone, a leave or quit, the
// contract's end on the ghost player's client, the ghost player leaving the room, the mod stopping.
//
// On the ghost player's own client, once every player has loaded, the camera is detached to become
// the ghost's eyes and the local rig (controls, hands, HUD) is switched off. Network.localPlayer stays on
// the parked body: the game reads it with no check in many places, so a client without one throws, and
// an exception inside the ghost's state change recurses until the stack runs out. Their journal stays
// on the body (src/journal.h).
//
// The game's end of a contract needs the local investigator: src/contract_end.h. Main thread only.
namespace pm::body {

void tick(session::Session& s);
// Gives the ghost player their investigator back, and the body and its spot back to the game: on the
// mod's teardown, and as the ghost player leaves the contract or quits.
void teardown();
// This client has its investigator again: it does not play the ghost, and Network's local player
// (which the game's own Leave, Quit and ExitLevel.Exit read) is set. After teardown it is false only
// when Network or the Player was gone.
bool investigator_back();
bool stepped_out();  // the ghost player's investigator is out of the contract on this client

void* camera();  // the ghost player's camera while they play the ghost, else null

}  // namespace pm::body
