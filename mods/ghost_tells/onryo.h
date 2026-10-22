#pragma once

// The Onryo's tells (onryo_rule.h) in the game. The host snuffs flames lit near it, watches its count of
// blown-out flames and marks each flame it blows out; every client shows the count on every lit flame
// and refuses to light a marked flame. Main thread.
namespace pg::tells::onryo {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on the host while the tells are on: the ghost's GhostAI and behaviour type.
void tick_host(void* ghost, int type, float dt);
// Every frame on every client while the tells are on.
void tick_all(int type, float dt);
void receive_count(int count);                // the host's word, on every other client
void receive_spite(int view, float seconds);  // likewise
void reset();  // every flame back to its own light and size

}  // namespace pg::tells::onryo
