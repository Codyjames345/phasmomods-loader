#pragma once

// Haunting (haunt_rules.h) in the game. The dead investigator's own camera is switched off and a
// camera of the mod's, set up like theirs, sits at the ghost's head and turns with the ghost's body.
// Their body stays where it was, frozen, so returning puts them back where they left; the game's
// own Interact, Primary Use and Secondary Use actions are switched off while they may not touch
// anything. Nothing about the ghost is shown (no type, room or state): only what it sees. Purely
// local: nothing is sent, and it works with the game's own ghost as with a player's. Main thread.
namespace pg::afterlife {

bool resolve();
void tick();
void teardown();

}  // namespace pg::afterlife
