#pragma once

#include <cstdint>
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
//  - Every client holds the exits exactly when the master locked them, from the master's word alone: the master as it
//    locks them, every other client in the same call that brings the master's lock (Door.HuntingCloseDoorNetworked,
//    which the game takes only from the room's first actor). No client's own count of the players inside decides.
//  - Once every client holds the exits, no Monkey Paw wish opens them (the user's rule, 2026-11-08): the hooks refuse,
//    on every client, Door.LockDoorForTime and Door.ToggleDoorLock on a held exit, and Door.UnlockDoor and
//    Door.ForceToggleHuntCollider on one inside MonkeyPaw.OpenExitsWish. Every other call, and the rest of each wish,
//    stays the game's.
//  - A wish another client began before the master's lock reached it is not held back there, and its unlock races the
//    lock: the timed lock's window and the open-exits wish send Door.SyncLockState(false), which the game takes from
//    anyone, and switch the exit back on for every client, and those can reach every client, the master included,
//    after the lock. The master answers (relock): a held exit that reads grabbable again, or unlocked again, gets the
//    game's own hunt lock once more, at most once each kRelockMs.
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
 * Locks every exit of the current map once, on the master: it calls Door.HuntingCloseDoor for each exit, which shuts,
 * switches off and locks it on every client, as a hunt's start does, and this client holds them from then on
 * (holds, on_attempt). True once this client holds the exits, now or earlier for the same map, and then it calls
 * nothing more: a client that became the master after holding them never locks them twice. False, locking nothing, on
 * another client (it holds the exits once the master's lock reaches it, see set_haunting), on a friendly ghost's
 * difficulty, when the difficulty cannot be read, or when the map has no exits it can read.
 */
bool lock_all();

/** Whether this client holds the current map's exits. False once the map's LevelController is gone. */
bool locked();

// Milliseconds within which relock calls the game's lock on one exit at most once. A starting value, not a game value.
inline constexpr uint64_t kRelockMs = 2000;

/**
 * On the master, while it holds the exits and not on a friendly ghost's difficulty (nor an unread one): calls
 * Door.HuntingCloseDoor again on each held exit a raced wish has reopened, and returns how many. An exit is reopened
 * when it reads grabbable here (Door.grabbable, which the lock clears on every client), or unlocked (Door.locked) after
 * this client read it locked since the hold or since its last re-lock; an exit another player's client owns keeps the
 * game's lock on that client alone and never reads locked here, so its plain unlocked state is the game's own and calls
 * nothing. One exit is locked again at most once each kRelockMs, and not again until it reads reopened afresh. The
 * house key's unlock outside a hunt (Door.AttemptToUnlockDoor) reads as unlocked again too, and is answered the same
 * way. `now` is milliseconds on any steady clock. 0 on another client, before the hold and once the map is gone.
 * Every frame on the master (squatter.cpp's tick).
 */
int relock(uint64_t now);

/**
 * Whether the Squatter holds `door` on this client: an exit of the map whose exits it holds. False for null, for a door
 * that is not an exit, before the hold and once the map's LevelController is gone.
 */
bool holds(void* door);

/** What on_attempt calls: the Door a player tried. */
using Attempt = std::function<void(void* door)>;

/**
 * Calls `callback` with the door each time the local player tries an exit this client holds (holds):
 * through the interact input (Door.AttemptToUnlockDoor), which runs on the trying player's own client, or any route
 * that reaches Door.GrabbedDoor for it (the PC grab refuses a locked or switched-off door before that). The game's
 * own try runs after it, unchanged. Carrying the try to the master is the caller's.
 * Replaces any earlier callback; an empty one reports nothing.
 */
void on_attempt(Attempt callback);

/**
 * Whether the Squatter haunts the contract on this client (squatter.cpp's tick says so every frame). While it does,
 * the master's hunt lock of an exit reaching this client (Door.HuntingCloseDoorNetworked, after the game's own body has
 * run, unchanged) makes it hold every exit of the map in that same call, when the game's own checks take the call:
 * from the room's first actor (anyone outside a room or offline), and not on a friendly ghost's difficulty or one that
 * cannot be read. While it does not, no lock holds anything, so another ghost's hunt leaves every wish the game's.
 */
void set_haunting(bool on);

/**
 * Hooks Door.AttemptToUnlockDoor and Door.GrabbedDoor (the tries), Door.LockDoorForTime, Door.ToggleDoorLock,
 * Door.UnlockDoor, Door.ForceToggleHuntCollider and MonkeyPaw.OpenExitsWish (the paw's way to a held exit), and
 * Door.HuntingCloseDoorNetworked (the master's lock arriving; the hook only watches), before hooks::enable_all. False
 * when any cannot be hooked.
 */
bool create_hooks();

/**
 * Forgets the lock and the map it was for, as when the mods stop. A new map's LevelController forgets it too, so a
 * contract's end needs no call. Keeps the callback.
 */
void reset();

}  // namespace pm::custom::squatter::door
