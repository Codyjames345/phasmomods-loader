# Doors, exits and keys

Which doors are a map's way out, how the game locks and opens them, who may send each of their RPCs, how a player's try at
a locked door reaches code, and how to tell that a player is out of the map. The entries are in `sdk/map/Door.h`,
`LevelController.h`, `LevelRoom.h` and `Player.h`; their `///` notes are the authority. Code facts were read from
decompiled code of Steam build 25811160 (`re/decomp.mjs <key>`); scene facts from copies of that build's map scenes with
the pinned UnityPy. This page names map keys, never addresses.

## The exits

A map's exits are `LevelController.huntingDoors`, the doors a hunt locks. On every map of build 25811160 that list holds
exactly the map's `DoorKind.Main` doors, and every map variation's `SmallMapVariation.huntingDoorsToLock` is a part of it,
so reading `huntingDoors` together with the variations' lists, without repeats, gives every exit. Read them at run time:
the counts below are one build's.

| Map | Exits (`DoorKind.Main`) |
| --- | --- |
| Tanglewood, Edgefield, Ridgeview and Willow Street, Grafton Farmhouse, Sunny Meadows, Prison, Point Hope | 1: the front door |
| Bleasdale Farmhouse | 2 |
| Nell's Diner | 3: the two leaves of the front door and the fire exit |
| Brownstone High School | 6: the two leaves of the main door and four outside doors |
| Camp Woodwind, Maple Lodge Campsite | 2: the two leaves of the campsite's gate |

**Maps without a house.** The two campsites have no house: their way out is the campsite's gate, whose two leaves are
`DoorKind.Main` doors in `huntingDoors`, as a house's front door is. Maple Lodge's cabin door is a `DoorKind.Secondary`
door inside the map, not an exit.

**Map variations.** Brownstone High School, Sunny Meadows, Prison and Point Hope have variations
(`LevelController.mapVariations`). `LevelController.LockHuntingDoors` locks a variation's own doors only when the ghost's
room is one of its rooms (`SmallMapVariation.huntingRooms`) and a player is in one of them. On Brownstone a variation also
lists exits to switch off for its smaller map.

**Every exit starts the contract locked** in the scene (`Door.locked`), and so do the `DoorKind.PermanentLock` and
`DoorKind.Secondary` doors.

## Inside or out

`Player.currentRoom` is the last room whose trigger the player's camera collider entered (`LevelRoom.OnTriggerEnter`
sets it; the hunt reads it on the master for every player). Every map has exactly one room of kind `RoomKind.outsideMap`, `LevelController.outsideRoom`,
named Outside or Exterior: the outdoors beyond the exits. Every other room is within the exits, whatever its kind:
`RoomKind.insideRoom`, `RoomKind.exterior` (Sunny Meadows' courtyard, the campsites' fires and pitches) or
`RoomKind.exteriorRoom` (tents, Bleasdale's garden, most of Grafton Farmhouse).

The game itself draws the line there. `PlayerSanity.Update` counts a player's time in an `outsideMap` room apart and
drains no sanity for it, and the hunt never goes for a player in one (`GhostHuntState`, sdk/systems/ghost-ai.md). So a
living player (`Player.isDead` clear) is in the map exactly when their room is not of kind `RoomKind.outsideMap`. A
player with no room yet has not entered any trigger, and a count that meets one cannot be trusted.

## The lock and the house key

`Door.locked` is each client's copy of the lock. `Door.SyncLockState`, an RPC to every client, stores it: it freezes the
leaf's Rigidbody while locked and, for a `DoorKind.Main` door, calls `SetupPhaseController.UnlockMainDoor` (which does
nothing once the main door is open) and switches the door's hunt collider off.

- `Door.LockDoor(force)` locks a door: with `force`, or when the leaf is already nearly shut, it sets the leaf to its
  closed angle once and sends `NetworkedSyncCloseDoor`; it marks the door locked; on the master it also sends
  `SyncLockState(true)` and the lock's click (`Door.NetworkedPlayLockSound`). An open door it is not forced to shut stays
  unlocked.
- `Door.UnlockDoor(sound)` sends `SyncLockState(false)` to every client, and the unlock's click when `sound` is set.

Keys open locked doors. Every map has a `Key` object named as its main or house key, and some a car, garage, cabin
or cabinet key too. A player who
picks one up runs `Key.GrabbedKey` on their own client, which adds the key's `KeyInfo` to that client's local player's
`Player.keys`. `KeyInfo.keyType` is the `DoorKind` it opens.

## A player's try at a locked door

The interact input (`DragRigidbodyUse.OnInteract`) casts a ray from the player's camera and, for a collider tagged Door,
calls `Door.AttemptToUnlockDoor` on that door, on the trying player's own client. `Door.GrabbedDoor`, the door's grab event,
runs the same steps for a locked door. A mod that hooks `Door.AttemptToUnlockDoor` and `Door.GrabbedDoor` sees every try,
on the client of the player who made it.

On an unlocked door the try does nothing. On a locked one:

1. Outside a hunt (`GhostAI.isHunting` clear), a key in the local player's `Player.keys` whose `KeyInfo.keyType` is the
   door's kind unlocks it (`Door.UnlockDoor(true)`, with the unlock's click for everyone).
2. Otherwise a `DoorKind.None` door unlocks, unless `Door.LockDoorForTime` holds it.
3. Any other door gets the locked sound: `Door.NetworkedPlayLockSound` to every client in a room (the clip is played on
   the trying player's client alone offline). That RPC plays a clip only for a `DoorKind.Main` door, so the locked sound is
   the exits' alone.

So during a hunt a locked exit refuses even a key holder and rattles, and outside one the house key opens it.

## How a hunt locks the exits

1. As a hunt starts, the hunt state on the master sends `GhostAI.Hunting` and calls `LevelController.LockHuntingDoors`.
2. It calls `Door.HuntingCloseDoor` on every exit (and on a variation's, as above). On a friendly ghost's difficulty
   (`Difficulty.friendlyGhost`) that does nothing. Otherwise it switches the door off for players through
   `Door.DisableOrEnableDoor(false)`, whose RPC drops the door from the local hand holding it and clears a flag of its
   `PhotonObjectInteract` (`Door.interact`), and sends `Door.HuntingCloseDoorNetworked` to every client.
3. That RPC's body does the same switching off, switches the door's hunt collider on, and on the client that owns the
   door's PhotonView starts a coroutine. For a door that is open and unlocked, the coroutine switches the door's collider
   off and moves the leaf through the door's own `OpenDoor`; then it waits, sets the leaf to its closed angle once,
   switches the collider on, calls `Door.LockDoor(true)` and makes the leaf kinematic.

As the hunt ends, `GameController.ResetHuntEffects` (whose two callers run it only when a hunt was on) calls
`LevelController.UnlockHuntingDoors`, which calls `Door.ToggleDoorLock(true, true)` on the same doors: they switch back
on and unlock. Nothing else opens them, except the Monkey Paw's timed lock (`Door.LockDoorForTime`), whose end calls
`Door.ToggleDoorLock(true)` on its door whoever locked it.

**At the contract's end** the doors go with the map's scene; the next contract loads the scene afresh, with every exit
locked as above. Nothing carries over.

## Who may send what

| RPC | The check on the receiving side |
| --- | --- |
| `Door.HuntingCloseDoorNetworked` | refuses a sender that is not the room's first actor, and refuses outright on a friendly ghost's difficulty |
| `Door.SyncLockState` | refuses a lock (true) from a sender that is not the room's first actor; takes an unlock from anyone |
| `Door.NetworkedPlayLockSound`, `NetworkedSyncCloseDoor`, the unlock's click, `SyncHasBeenUnlocked`, and the RPCs of `Door.DisableOrEnableDoor` and of the door's collider switch | none: they take no sender |

A refusal can harm an account ([Photon](photon.md), "The game's checks on an RPC"), so lock exits only as the game does:
call `Door.HuntingCloseDoor` on the master, never on a friendly ghost's difficulty, and let it send what it sends.
`Door.HuntingCloseDoor` checks `Difficulty.friendlyGhost` before it sends anything, and `Door.LockDoor` sends
`SyncLockState` only from the master. Never call `Door.HuntingCloseDoorNetworked`, `Door.SyncLockState` or
`Door.NetworkedPlayLockSound` yourself: they are RPC bodies.

## Locking the exits outside a hunt

A mod that locks the exits with `Door.HuntingCloseDoor` while no hunt is on gets the hunt's lock: the doors shut, switch
off for players and lock. One thing differs from a hunt, because `GhostAI.isHunting` is clear: a player holding the house
key who tries an exit unlocks it (step 1 above), with the unlock's click for everyone. The door stays switched off and
its leaf kinematic, but `Door.locked` is clear from then on and later tries make no sound. That the switched-off door
cannot be grabbed open is inferred from the code (the cleared flag was not traced to the grab), not yet seen in a game.

Custom Ghosts' Squatter locks every exit this way once no living player is left in the map, and answers each try with a
line of its own (`mods/custom_ghosts/squatter_door.h`).
