#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "Transform.h"
#include "quat.h"

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
 * house key's unlock outside a hunt (Door.AttemptToUnlockDoor) of an exit this client read locked reads as unlocked
 * again too, and is answered the same way. `now` is milliseconds on any steady clock. 0 on another client, before
 * the hold and once the map is gone. Every frame on the master (squatter.cpp's tick).
 */
int relock(uint64_t now);

// The quiet opener (sdk/systems/doors.md, "Opening a door quietly"): the game's own HingeDoor.SwingDoor, which turns the
// leaf to its open angle (HingeDoor.openAngle) and leaves no ghost's trace. HingeDoor.OpenDoor's physics push is not
// used: on a door whose open angle Awake reverses it pushes the leaf shut (user, 2026-11-09: a door that swings away
// stayed shut). The swing reaches the other clients only from the door's view's owner, through the door's
// PhotonTransformView, so the master takes the view first, as the ghost's own door step does, and opens the door once
// it owns it. Nothing is carried on the mod's channel.

// Milliseconds the master waits for a door's view to become its own before it gives the open up. A starting value, as
// carry.h's kOwnershipWaitMs, not a game value.
inline constexpr uint64_t kOpenWaitMs = 2000;

// How far a door's leaf must stand from shut for the Squatter to pass, in degrees: a narrower gap it opens rather than
// walks through (user, 2026-11-09: it never passes through doors). A starting value, judged with the user in game.
inline constexpr float kPassDegrees = 70.0f;
// The push a door the game moves by force is opened with (HingeDoor.OpenDoor): the strength that opened doors in the
// user's test of 2026-11-09, well over the game's own 0.05 times 1 to 5, so the leaf swings wide. A starting value.
inline constexpr float kPushStrength = 5.0f;
// How long the leaf takes to swing open, in seconds (HingeDoor.SwingDoor). A starting value, judged with the user in
// game against the opening-door clips.
inline constexpr float kSwingSeconds = 1.0f;

/**
 * How far a leaf at `angle` degrees on its hinge axis stands from its shut angle `shut` (Door.closedAngle), the shorter
 * way round, 0 to 180. Door.closed is not this: the game sets it when a door snaps shut and clears it only on a grab or
 * the contract-start opener's RPC, so a door its physics swung open still reads closed.
 */
inline float gap_degrees(float angle, float shut) {
  float d = std::fmod(angle > shut ? angle - shut : shut - angle, 360.0f);
  return d > 180.0f ? 360.0f - d : d;
}

/** Whether a door open `gap` degrees lets the Squatter through. */
inline bool fits(float gap) { return gap >= kPassDegrees; }

/**
 * The leaf's angle about its hinge from its local euler angles, `euler` (Transform.localEulerAngles), as the game's
 * opener reads the hinge's axis (HingeDoor.hingeAxis): x for `axisX` (Axis.X), z for `axisZ` (Axis.Z), else y.
 */
inline float leaf_angle(phasmo::Vector3 euler, int32_t axis, int32_t axisX, int32_t axisZ) {
  return axis == axisX ? euler.x : axis == axisZ ? euler.z : euler.y;
}

/** The shorter way round from `from` degrees to `to`, -180 to 180, as Mathf.DeltaAngle and HingeDoor.SwingDoor take it. */
inline float delta_angle(float from, float to) {
  float d = std::fmod(to - from, 360.0f);
  if (d < 0) d += 360.0f;
  return d > 180.0f ? d - 360.0f : d;
}

/** A door's leaf as this client sees it, for the side it swings to and where its handle is. */
struct Leaf {
  phasmo::Vector3 pivot;           // the leaf's Transform's position: SwingDoor turns it about this point
  phasmo::Quaternion rotation;     // its world rotation
  phasmo::Vector3 euler;           // its local Euler angles
  phasmo::Vector3 centre;          // the middle of its collider's box
  int32_t axis = 0, axisX = 0, axisZ = 0;  // HingeDoor.hingeAxis and the Axis values X and Z (leaf_angle)
  float openAngle = 0;             // HingeDoor.openAngle
};

/**
 * Whether opening `leaf` swings it toward `ghost`: its middle, turned a little about the hinge from where it stands
 * toward its open angle as HingeDoor.SwingDoor turns it, moves (on the flat) toward where `ghost` stands.
 */
inline bool swings_toward(const Leaf& leaf, phasmo::Vector3 ghost) {
  const float now = leaf_angle(leaf.euler, leaf.axis, leaf.axisX, leaf.axisZ);
  const float step = delta_angle(now, leaf.openAngle) >= 0 ? 10.0f : -10.0f;
  phasmo::Vector3 turned = leaf.euler;
  (leaf.axis == leaf.axisX ? turned.x : leaf.axis == leaf.axisZ ? turned.z : turned.y) += step;
  const phasmo::Quaternion parent = qmul(leaf.rotation, qinv(euler_quat(leaf.euler)));
  const phasmo::Vector3 r{leaf.centre.x - leaf.pivot.x, leaf.centre.y - leaf.pivot.y, leaf.centre.z - leaf.pivot.z};
  const phasmo::Vector3 offset = qrot(qinv(leaf.rotation), r);
  const phasmo::Vector3 moved = qrot(qmul(parent, euler_quat(turned)), offset);
  const float mx = moved.x - r.x, mz = moved.z - r.z;
  return mx * (ghost.x - leaf.centre.x) + mz * (ghost.z - leaf.centre.z) > 0;
}

/** What a quiet open came to. */
enum class Opening {
  Idle,     // no open waits (open_step only)
  Waiting,  // asked for the door's view; open_step opens it once it lands
  Opened,   // HingeDoor.OpenDoor ran on the master, which owns the door's view
  Refused,  // nothing opened: may_open said no, another door waits, or (open_step) the open was given up
};

/**
 * Whether the master may open `door` quietly now. True only on the master, in a contract (a live LevelController), for
 * a live HingeDoor (HingeDoor.OpenDoor is HingeDoor's own) that is not an exit of the map (exits, so
 * never one the Squatter holds), of kind DoorKind.None, unlocked (Door.locked), grabbable (Door.grabbable), reading shut
 * (Door.closed), and whose PhotonObjectInteract nothing is using (PhotonObjectInteract.inUse). The kind, shut and
 * interact tests are the game's own contract-start opener's; the lock, the grab switch and the use are left as the game
 * holds them. Main thread.
 */
bool may_open(void* door);

/**
 * Whether the Squatter can pass `door` without walking through its leaf: a hinged door whose gap fits (gap_degrees,
 * fits), any other door, and a door gone. Any client; it reads only this client's leaf. Main thread.
 */
bool passable(void* door);

/**
 * The leaf of the hinged `door` as this client sees it now: Door.body's Transform, which HingeDoor.SwingDoor turns, and
 * the middle of the first collider under it. Empty for a door gone, not hinged, or without a body or collider. Any
 * client. Main thread.
 */
std::optional<Leaf> leaf_of(void* door);

/**
 * On the master: swings the shut `door` open with the game's own HingeDoor.SwingDoor, all the way to its open angle
 * over kSwingSeconds, which makes no ghost sound and leaves no EMF reading, count or fingerprint. Opened at once when this client owns the door's view (always
 * offline). Otherwise it asks for the view (PhotonView.RequestOwnership, granted to a Takeover view when it comes back)
 * and answers Waiting; open_step opens the door once the view is this client's, since a leaf moved on a client that does
 * not own its view is pulled back to the owner's angle and seen by nobody. Refused, doing nothing, when may_open says
 * no, or while another door waits (the same door again answers Waiting and asks nothing more). The side is the
 * door's own open side (HingeDoor.openAngle). `now` is milliseconds on any steady clock. Main thread.
 */
Opening open_quietly(void* door, uint64_t now);

/**
 * Every frame on the master (squatter.cpp's tick): finishes the waiting open. Idle when none waits; Waiting while the
 * door's view is not yet this client's; Opened once it is and the door opened; Refused, opening nothing, when the
 * door is gone, may_open no longer holds (a player grabbed it, it was locked or switched off, this client is no longer
 * the master), or kOpenWaitMs passed since open_quietly. After Opened or Refused it is Idle again.
 */
Opening open_step(uint64_t now);

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
