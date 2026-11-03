#pragma once

// The Onryo's tells (onryo_rule.h) in the game. The host snuffs flames lit near it, watches its count of
// blown-out flames and marks each flame it blows out; every client shows the count on every lit flame
// and refuses to light a marked flame. Main thread.
namespace pm::tells::onryo {

bool create_hooks();  // before hooks::enable_all
// Every frame on the host while the Onryo's rules run: the ghost's GhostAI.
void tick_host(void* ghost, float dt);
// Every frame on every client while the Onryo's rules run.
void tick_all(float dt);
void receive_count(int count);                // the host's word, on every other client
void receive_spite(int view, float seconds);  // likewise
void reset();  // every flame back to its own light and size

}  // namespace pm::tells::onryo
