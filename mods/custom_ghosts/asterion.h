#pragma once

// The Asterion (asterion_rule.h) in the game, on the host: each hinged door but the front door is
// tracked through its hunts. Doors it passes slam shut behind it (Door.GhostForceDoor); a closed door
// in its way stops it while it batters it (the game's door slam noise, every client), then it forces
// the door open and jams it so nobody can close it again. Main thread.
namespace pg::custom::asterion {

bool resolve();
// Every frame on the host: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
// Whether it stands battering a door now (behaviour.cpp holds it still).
bool battering();
void reset();

}  // namespace pg::custom::asterion
