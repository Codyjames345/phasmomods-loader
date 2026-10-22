#pragma once

// The Spirit's tells (spirit_rule.h) in the game. The host stops it when incense reaches it in a hunt,
// silences it for a minute after incense outside one (a gate rule), and knocks when told its name was
// said near it; every client listens to its own player's recognised speech for the name. Main thread.
namespace pg::tells::spirit {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on the host while the tells are on: the ghost's GhostAI and behaviour type.
void tick_host(void* ghost, int type, float dt);
// Host: the player of Photon actor `actor` said the ghost's name.
void heard_name(int actor);
void reset();

}  // namespace pg::tells::spirit
