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
calls `Door.AttemptToUnlockDoor` on that door, on the trying player's own client. A mod that hooks
`Door.AttemptToUnlockDoor` sees every try, on the client of the player who made it. `Door.GrabbedDoor` runs the same steps
for a locked door, but the PC player's grab never calls it for one (below).

On an unlocked door the try does nothing. On a locked one:

1. Outside a hunt (`GhostAI.isHunting` clear), a key in the local player's `Player.keys` whose `KeyInfo.keyType` is the
   door's kind unlocks it (`Door.UnlockDoor(true)`, with the unlock's click for everyone).
2. Otherwise a `DoorKind.None` door unlocks, unless `Door.LockDoorForTime` holds it.
3. Any other door gets the locked sound: `Door.NetworkedPlayLockSound` to every client in a room (the clip is played on
   the trying player's client alone offline). That RPC plays a clip only for a `DoorKind.Main` door, so the locked sound is
   the exits' alone.

So during a hunt a locked exit refuses even a key holder and rattles, and outside one the house key opens it.

## Grabbing a door open

Holding the interact input grabs. Each physics step `DragRigidbodyUse.FixedUpdate` casts the same ray and, for a door,
refuses it, holding nothing, when it is locked (`Door.locked`) or not grabbable (`Door.grabbable`), and refuses a locked
`DoorKind.Main` door during a hunt. Otherwise it calls `Door.GrabbedDoor` (which sends `Door.NetworkedGrabbedDoor` to every
client), asks for the door's PhotonView (`PhotonView.RequestOwnership`), frees the leaf's Rigidbody and drives it after the
player's aim. So a door can be swung open on a client only while both its lock and its grab switch allow it there.

`Door.grabbable` is set on every client by `Door.ForceDropDoorNetworked`, the RPC `Door.DisableOrEnableDoor` sends to
every client, and the hunt's lock clears it on every client as well (below). Unlocking leaves it alone: `Door.UnlockDoor`
and `Door.SyncLockState` touch only `Door.locked`, so a switched-off door stays shut when its lock clears. Two calls switch
it back on: `Door.ToggleDoorLock(true)`, and the end of a ghost's force (below).

**A VR player's grab** is gated by `VRGrabbable.IsSelectableBy`, which refuses an object while its
`PhotonObjectInteract.grabbable` is clear and never reads `Door.locked`. `Door.ForceDropDoorNetworked` sets that flag with
`Door.grabbable` on every client, so the same calls switch both off and on, and a locked exit holds for a VR player
through that flag alone.

**A ghost's force.** `Door.GhostForceDoor` (both overloads) starts one coroutine. It sends
`Door.ForceDropDoorNetworked(true, true)` to every client as it starts and `Door.ForceDropDoorNetworked(false, true)` as it
ends, which sets `Door.grabbable` and `PhotonObjectInteract.grabbable` again on every client whatever `Door.locked` or the
door's kind. Its callers: ghost states and the hunt state, directly or through Door's GhostOpenDoorAnimated (the door
state, `GhostState.door`, uses it), `ClosetZone` for its closet doors, and the training rooms. So a ghost that forces a
switched-off exit leaves it grabbable. For Custom Ghosts' Squatter these routes are closed: its restraint refuses every
change of state but idling, wandering and its favourite room, whose states call neither method, and refuses its
interactions (`mods/custom_ghosts/restraint.cpp`, `squatter_rule.h` `keeps_quiet`). For another mod's ghost they are open.

**Who owns an exit's PhotonView.** Every exit's view is set to `OwnershipOption.Takeover` in the map's scene
(`PhotonView.OwnershipTransfer`, scene read on all 13 maps), and the grab asks for the view each time a player grabs a
door in a room. So in practice an exit's owner is the last player who grabbed it, often not the master. Before anyone has,
it is the master's, as Photon gives a scene's views to the master.

**A ghost walking through.** No door leaf carries a NavMeshObstacle and the Ghost layer does not collide with the Door
layer, so the ghost's agent walks through a shut door and leaves the leaf where it is ([ghost AI](ghost-ai.md), "Moving
the ghost outside a hunt"). Only the grab above or a ghost's force moves a leaf.

## How a hunt locks the exits

1. As a hunt starts, the hunt state on the master sends `GhostAI.Hunting` and calls `LevelController.LockHuntingDoors`.
2. It calls `Door.HuntingCloseDoor` on every exit (and on a variation's, as above). On a friendly ghost's difficulty
   (`Difficulty.friendlyGhost`) that does nothing. Otherwise it switches the door off for players through
   `Door.DisableOrEnableDoor(false)`, whose RPC drops the door from the local hand holding it and clears `Door.grabbable`
   and `PhotonObjectInteract.grabbable`, and sends `Door.HuntingCloseDoorNetworked` to every client.
3. That RPC's body, on every client, clears `Door.grabbable` and `PhotonObjectInteract.grabbable`, drops the door
   from the local hand, switches the door's hunt collider on, and on the client that owns the door's PhotonView starts a
   coroutine. For a door that is open and unlocked, the coroutine switches the door's collider off and moves the leaf
   through the door's own `OpenDoor`; then it waits, sets the leaf to its closed angle once, switches the collider on,
   calls `Door.LockDoor(true)` and makes the leaf kinematic.
4. `Door.LockDoor(true)` runs on that owner. It sends `NetworkedSyncCloseDoor` to every client, which makes the leaf
   kinematic and marks it shut everywhere, and marks the door locked there. Only if the owner is the master does it send
   `SyncLockState(true)` and the lock's click, so only then is `Door.locked` set on every client.

**When a player owns the exit.** If a player other than the master owns the exit's view, the lock's click does not play,
and `Door.locked` is set on that player's client alone. The exit still cannot be swung open on any client:
`Door.grabbable` is clear everywhere, from both RPCs above, and the grab refuses it. Only the owner's tries meet a locked
door and so rattle (or, holding the key, unlock it); on every other client a try finds it unlocked and makes no sound.

As the hunt ends, `GameController.ResetHuntEffects` (whose two callers run it only when a hunt was on) calls
`LevelController.UnlockHuntingDoors`, which calls `Door.ToggleDoorLock(true, true)` on the same doors: they switch back
on and unlock. Otherwise only a ghost's force (above) and the Monkey Paw (below) switch them back on.

## The Monkey Paw

`Door.LockDoorForTime(seconds, takeOwnership)` is the Monkey Paw's timed lock. Its coroutine, with `takeOwnership`, first
requests the door's PhotonView and waits until this client owns it. It calls `Door.ToggleDoorLock(false)`, which clears
`Door.locked` at the start of the window and switches the door off; the close coroutine then shuts and re-locks it on its
owner. After `seconds` it calls `Door.ToggleDoorLock(true)`, which switches the door back on and unlocks it, with no sound,
whoever locked it. A `DoorKind.Main` door is skipped at the start, and left at the end, while the ghost hunts
(`GhostAI.isHunting`), so that hunt's end (`LevelController.UnlockHuntingDoors`) opens it.

Four wishes (`MonkeyPaw.MakeWishNetworked`) reach the exits, three of them through it:

- One, in its coroutine on the master, first calls `GhostAI.ChangeState` to `GhostState.summoningCircle` when the ghost is
  not hunting, then `LockDoorForTime(6, true)` on every exit (`LevelController.huntingDoors`).
- One, on the master, calls `LockDoorForTime(120, true)` on every exit, after `GhostAI.RemoveInteractionBlock(120)` and
  breaking the fuse box (`FuseBox.BreakFuseBox`).
- One times the doors of the ghost's favourite room, and of the room the wishing player is in, with `LockDoorForTime`,
  after sending the ghost to its favourite room, then on the master starts `GhostAI.TrapWish`. An exit among those rooms'
  doors (`LevelRoom.doors`) is timed with them.
- One, `MonkeyPaw.OpenExitsWish`, opens every exit for good. For each door of `LevelController.huntingDoors` it clears
  the timed-lock flag, calls `Door.UnlockDoor(true)` on the master (the unlock's click), `Door.ToggleDoorLock(true)` on the
  client that owns the door's PhotonView (the door back on, `Door.grabbable` set on every client), and
  `Door.ForceToggleHuntCollider(false)`. It checks no hunt and sets no timer.

Whatever locked the exits before, the three timed wishes reopen them when their windows end, and the fourth opens them at
once and for the rest of the contract.

With Custom Ghosts' Squatter haunting, its restraint refuses every change of state but idling, wandering and its
favourite room, whoever asks (`mods/custom_ghosts/restraint.cpp`, `squatter_rule.h` `keeps_quiet`), so the first wish's
`GhostState.summoningCircle` never begins and brings no hunt.

**Once the Squatter has locked the exits, no wish opens them** (the user's rule, 2026-11-08). Custom Ghosts refuses, on
every client that runs it, only the call that would undo the lock on an exit it holds (`squatter_door.h` `holds`):
`Door.LockDoorForTime` (the timed wishes and the rooms wish, so neither the lock's clearing at a window's start nor the
reopening at its end happens), `Door.ToggleDoorLock` either way, and, inside `MonkeyPaw.OpenExitsWish`, `Door.UnlockDoor`
and `Door.ForceToggleHuntCollider`. These are plain methods, not RPC bodies, and a refused call sends nothing, so every
client keeps the same door. The rest of each wish stays the game's: the summoning circle's ask, the interaction block, the
fuse box, the ghost sent to its room, the trap wish, and the timing of every door that is not a held exit. Before the lock
every wish is the game's own. The house key's unlock (`Door.AttemptToUnlockDoor`) is not a wish and stays the game's.

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
key who tries an exit unlocks it (step 1 above), with the unlock's click for everyone. `Door.locked` is clear from then on
and later tries make no sound, but the door stays shut: `Door.grabbable` stays clear on every client, and the grab refuses
it ("Grabbing a door open"). This was read from the code, not yet seen in a game. A Monkey Paw wish on the exits, or
another ghost's force on one, reopens them.

Custom Ghosts' Squatter locks every exit this way once no living player is left in the map, and answers each try with a
line of its own (`mods/custom_ghosts/squatter_door.h`).
