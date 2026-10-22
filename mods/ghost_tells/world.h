#pragma once

#include "il2cpp.h"

// What the tells read of the contract, shared by every ghost's file (build 25811160 offsets). Main
// thread.
namespace pg::tells::world {

int behaviour_type(void* ai);   // GhostInfo +0x2C, the type whose rules its AI follows; -1 for none
bool hunting(void* ai);         // GhostAI +0xF3, set by the Hunting RPC on every client
bool appeared(void* ai);        // GhostAI +0xA8, showing itself
void* favourite_room(void* ai); // GhostInfo +0x70
void* hunt_state(void* ai);     // the current state while it hunts, else null
void* hunt_target(void* ai);    // the hunt state's detected player (+0x48), or null
void* ghost_room();                // LevelController +0x38: the room the game says the ghost is in
void* hunt_search(void* ai);       // the hunt state's search player (+0x50), or null
void* room_at(il2cpp::Vector3 p);  // the LevelRoom whose colliders hold p, or null
int room_index(void* room);        // its index in LevelController +0x50, or -1
void* room_by_index(int index);
/** Distance from p to the nearest of a room's collider boxes (0 inside one). */
float distance_to_room(void* room, il2cpp::Vector3 p);
/** Whether nothing solid lies on the straight line between two points. */
bool clear_line(il2cpp::Vector3 from, il2cpp::Vector3 to);
il2cpp::Vector3 feet(void* component);
il2cpp::Vector3 chest(void* ai);    // the ghost's feet + 1.2 m
il2cpp::Vector3 eye(void* player);  // a player's feet + 1.6 m
/** Living players within `range` metres of p, on its floor (less than 2.5 m above or below). */
int living_within(il2cpp::Vector3 p, float range);
bool same_floor(il2cpp::Vector3 a, il2cpp::Vector3 b);

}  // namespace pg::tells::world
