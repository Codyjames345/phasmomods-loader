#pragma once

// Holding a hunting ghost still through the game's own hunt speed, so its footsteps and walk stop with
// it: the hunt state's base speed (+0xB0) is held at zero, and the base it had is written back when the
// hold ends. The hunt update builds the agent's speed, its footstep timing and its animation speed from
// that base (docs/custom-ghosts.md, the speed model). Host. Main thread.
namespace pg::tells::hold {

/** Every frame while `on`; false ends a hold (nothing when none is running). */
void hold_still(void* ai, bool on);
/** The line-of-sight speed-up (+0xB4) back to its floor of 1. */
void reset_sight(void* ai);
void reset();

}  // namespace pg::tells::hold
