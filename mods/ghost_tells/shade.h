#pragma once

// The Shade's tells (shade_rule.h) in the game, on the host: it vanishes when a player stares at it
// while it shows itself, does nothing with two or more players near (a gate rule), and hesitates in a
// hunt when its prey has another player beside them. Main thread.
namespace pm::tells::shade {

void resolve();
// Every frame on the host while the Shade's rules run: the ghost's GhostAI.
void tick_host(void* ghost, float dt);
void reset();

}  // namespace pm::tells::shade
