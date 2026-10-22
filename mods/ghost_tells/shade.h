#pragma once

// The Shade's tells (shade_rule.h) in the game, on the host: it vanishes when a player stares at it
// while it shows itself, does nothing with two or more players near (a gate rule), and hesitates in a
// hunt when its prey has another player beside them. Main thread.
namespace pg::tells::shade {

bool resolve();
// Every frame on the host while the tells are on: the ghost's GhostAI and behaviour type.
void tick_host(void* ghost, int type, float dt);
void reset();

}  // namespace pg::tells::shade
