#pragma once

// Holding a hunting ghost still through the game's own hunt speed, so its footsteps and walk stop with
// it: while a hold runs, the told ghost's content rules (mod.cpp, PmGhostRules::huntSpeed) give the hunt
// a base speed of 0, and the game's own base comes back the frame they stop asking. The hunt update
// builds the agent's speed, its footstep timing and its animation speed from that base
// (sdk/systems/ghost-ai.md, "Hunt speed"). Host. Main thread.
namespace pm::tells::hold {

/** Every frame while `on`; false ends a hold. */
void hold_still(bool on);
/** The line-of-sight speed-up back to its floor of 1, at the hunt's next speed update. */
void reset_sight();
/** The rules' huntSpeed: writes the held inputs and returns true while a hold or a reset is due. */
bool hunt_speed(float* base, float* losScale);
void reset();

}  // namespace pm::tells::hold
