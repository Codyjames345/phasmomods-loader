#pragma once

#include <functional>
#include <vector>

#include "LevelRoom.h"
#include "Transform.h"

// What the tells read of the contract, shared by every ghost's file, through the game API (sdk/map).
// Main thread.
namespace pm::tells::world {

using phasmo::Bounds;
using phasmo::Vector3;

bool create_hooks();  // before hooks::enable_all
void reset();

/** The frame's game time in seconds (pm::api::frame_time's deltaTime), at most 0.25 after a hitch; 0 unresolved. */
float frame_seconds();

void* ghost();     // the contract's GhostAI, alive, or null
bool is_master();  // this client runs the ghost's AI
int master_actor();
int local_actor();
/** Each investigator's Player that is still alive as a Unity object (dead investigators included). */
void each_player(const std::function<void(void*)>& visit);
bool living(void* player);  // a living investigator
void* player_of_actor(int actor);
void* local_player();

int state(void* ai);           // its GhostState number
bool hunting(void* ai);        // GhostAI.isHunting, set by the Hunting RPC on every client
bool appeared(void* ai);       // GhostAI.isAppeared: showing itself
void* favourite_room(void* ai);
void* hunt_state(void* ai);    // the hunt it runs on this client (the master), or null
void* hunt_target(void* ai);   // the player the hunt follows first, or null
void* hunt_search(void* ai);   // the player it falls back on, or null
void* ghost_room();            // the room the game says the ghost is in (synced)
void* room_at(Vector3 p);      // the room whose colliders hold p, or null
int room_index(void* room);    // its index in the level's rooms, or -1
void* room_by_index(int index);
bool in_room(void* room, Vector3 p);
std::vector<Bounds> room_bounds(void* room);
/** Distance from p to the nearest of a room's collider boxes (0 inside one). */
float distance_to_room(void* room, Vector3 p);
/** Whether nothing solid lies on the straight line between two points. */
bool clear_line(Vector3 from, Vector3 to);
float distance(Vector3 a, Vector3 b);
Vector3 feet(void* component);
Vector3 forward(void* component);
Vector3 chest(void* ai);    // the ghost's feet + 1.2 m
Vector3 eye(void* player);  // a player's feet + 1.6 m
/** Living players within `range` metres of p, on its floor (less than 2.5 m above or below). */
int living_within(Vector3 p, float range);
bool same_floor(Vector3 a, Vector3 b);

}  // namespace pm::tells::world
