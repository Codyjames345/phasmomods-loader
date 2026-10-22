#pragma once

// The Yurei's tells (yurei_rule.h) in the game, on the host: when its incense trap starts it shuts the
// doors of its room and keeps them shut for the trap; the door its ability slams is held with the
// game's hunt lock for a moment; a door left open in its room draws its ability to that door. Every
// client watches its own incense set off the trap and tells the host. Main thread.
namespace pg::tells::yurei {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on the host while the tells are on: the ghost's GhostAI and behaviour type.
void tick_host(void* ghost, int type, float dt);
void trapped();  // host: the Yurei's incense trap has just started
void reset();    // unlocks a door it still holds

}  // namespace pg::tells::yurei
