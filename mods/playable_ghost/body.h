#pragma once

#include "session.h"

// The ghost player is not an investigator, but their investigator (the networked Player the game
// creates for them in every contract) stays in the contract, parked where it stood in the truck.
// Every client's player slots therefore stay full, as the game expects: GameController's load wait
// holds back the all-loaded event (which shows the house key, among other things) until every slot
// has a live Player, so a body taken out of the contract stalled every client still loading.
//
// The parked body is kept out of play on every client (ghost-player rounds need every client to run
// the mod, src/session.h): marked dead, which keeps it out of the game's rules exactly as an empty
// slot did (the averaged sanity, hunts and targets, the everyone-dead ending, the truck's
// players-outside check), with its renderers, colliders, lights and sounds switched off. A revive
// skips it. It is parked only once the ghost player's own client has stepped out of it (below) and
// said so (session HELLO): until then it is still their investigator, for everyone.
//
// On the ghost player's own client, once every player has loaded, the camera is detached to become
// the ghost's eyes and the local rig (controls, hands, HUD) is switched off; the game's local-player
// reference is cleared, so no local-player code treats them as an investigator. Their journal stays
// on the body (src/journal.h).
//
// The game's end of a contract (ExitLevel.Exit, also used when everyone has died) needs the local
// investigator, so for the ghost player it is replaced by the payout and the game's own
// GameController.LoadBackToMenu. Main thread only.
namespace pg::body {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s);
// Gives the ghost player their investigator back, and the body back to the game: on the mod's
// teardown, and as the ghost player leaves the contract or quits.
void teardown();

void* camera();  // the ghost player's camera while they play the ghost, else null

}  // namespace pg::body
