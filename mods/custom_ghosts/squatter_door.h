#pragma once

#include <functional>
#include <vector>

// The Squatter's way out (sdk/systems/doors.md): the map's exits as the game holds them, the living players still
// within them, the exits' lock, and a player's try at a locked exit. Main thread.
//  - The exits are LevelController.huntingDoors and every map variation's SmallMapVariation.huntingDoorsToLock: on
//    every map they are its DoorKind.Main doors, a campsite's gate included. Nothing here names a map or a count.
//  - A player is within the exits while their room (Player.currentRoom) is not of kind RoomKind.outsideMap, the line
//    the game's own sanity drain and hunt draw.
//  - The lock is the game's hunt lock, Door.HuntingCloseDoor, called on the master only, never on a friendly ghost's
//    difficulty: its RPCs then pass every check on the receiving side (sdk/systems/photon.md). Nothing here unlocks:
//    the doors go with the map's scene when the contract ends.
namespace pm::custom::squatter::door {

/**
 * Every exit of the current map, each once: the live doors of LevelController.huntingDoors and of every map
 * variation's SmallMapVariation.huntingDoorsToLock. Empty outside a contract (no live LevelController) or when the
 * lists cannot be read. The Doors are managed references for this frame only.
 */
std::vector<void*> exits();

/**
 * How many living players (Player.isDead clear) are within the map's exits now: those in Network.playerSpots whose
 * Player.currentRoom is not of kind RoomKind.outsideMap. -1 whenever it cannot be read reliably: no Network or
 * spots, an empty list, a spot with no Player, a living player with no room yet, or an entry this build lacks.
 * Never 0 on a failed read, since 0 locks the exits (squatter_rule.h doors_lock). Any client; the master's count is
 * the one that locks.
 */
int living_inside();

/**
 * Locks every exit of the current map once, on every client that calls it. On the master it calls
 * Door.HuntingCloseDoor for each exit, which shuts, switches off and locks it on every client, as a hunt's start
 * does; on another client it calls nothing in the game. Either way this client then counts those exits as locked
 * (locked(), on_attempt). It locks nothing, and returns false, on a friendly ghost's difficulty, when the difficulty
 * cannot be read, or when the map has no exits it can read. True once the exits are locked, now or by an earlier
 * call for the same map; a later call does nothing more.
 */
bool lock_all();

/** Whether lock_all has locked the current map's exits on this client. False once the map's LevelController is gone. */
bool locked();

/** What on_attempt calls: the Door a player tried. */
using Attempt = std::function<void(void* door)>;

/**
 * Calls `callback` with the door each time the local player tries an exit lock_all has locked on this client:
 * through the interact input (Door.AttemptToUnlockDoor) or a grab (Door.GrabbedDoor), which run on the trying
 * player's own client. The game's own try runs after it, unchanged. Carrying the try to the master is the caller's.
 * Replaces any earlier callback; an empty one reports nothing.
 */
void on_attempt(Attempt callback);

/** Hooks Door.AttemptToUnlockDoor and Door.GrabbedDoor, before hooks::enable_all. False when either cannot be hooked. */
bool create_hooks();

/** Forgets the lock and the map it was for, as when the contract ends or the mods stop. Keeps the callback. */
void reset();

}  // namespace pm::custom::squatter::door
