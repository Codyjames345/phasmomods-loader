#pragma once

#include <cstddef>
#include <cstdint>

// The Asterion (asterion_rule.h) in the game. The host tracks each hinged door but the front door
// through its hunts. It never closes a door; a closed door in its way stops it while it batters it (the
// game's door slam noise, every client), then the door breaks: the host names the leaf, and a double
// door's other leaf, to every client, and each client swings them fully open (HingeDoor.openAngle) and
// locks them there as the game's lock does (sdk/systems/doors.md). Main thread.
namespace pm::custom::asterion {

bool resolve();
// Every frame on the host: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
// Every frame on every client: broken doors swing open and lock. type is the haunting custom ghost's.
void tick_doors(int type);
// The host's message naming the doors it broke (net_msg::kDoors).
void receive(int sender, const uint8_t* data, size_t size);
// Whether it stands battering a door now (behaviour.cpp holds it still).
bool battering();
void reset();

}  // namespace pm::custom::asterion
