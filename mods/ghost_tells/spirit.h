#pragma once

// The Spirit's tells (spirit_rule.h) in the game. The host stops it when incense reaches it in a hunt,
// silences it for a minute after incense outside one (a gate rule), and knocks when told its name was
// said near it; every client listens to its own player's recognised speech for the name. Main thread.
namespace pm::tells::spirit {

void resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on the host while the Spirit's rules run: the ghost's GhostAI.
void tick_host(void* ghost, float dt);
// Host: the player of Photon actor `actor` said the ghost's name.
void heard_name(int actor);
void reset();

}  // namespace pm::tells::spirit
