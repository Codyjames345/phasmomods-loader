# Doors

What a mod author needs to find, move, open and lock the level's doors. The entries are in `sdk/map/Door.h` and
`sdk/map/UnityWorld.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam build
25811160, and the ones marked live were seen in the game.

## Finding a door

- Every swinging door is a `HingeDoor`, which is a `Door`. Find them with `Object.FindObjectsOfType` on
  `HingeDoor.type`. `LevelRoom.doors` holds a room's doors.
- `Door.kind` says what kind it is: `DoorKind.Main` is the front door, which the ghost's own door choice never
  picks.
- `Door.view` is the door's `PhotonView`. Its `PhotonView.get_ViewID` names the same door on every client, so
  send that number when you tell other clients about a door.
- A double door's leaves name each other in one of two fields, `Door.otherDoor` or `Door.pairedDoor`; the level
  uses either (read from the scene files, 2026-11-06). One leaf may name the other without the other naming it
  back, so check both fields both ways. A single door has neither set.

## A door's leaf and its angle

- `Door.body` is the leaf's `Rigidbody`. The leaf turns with its `Transform` (`Component.get_transform` on the
  body).
- The leaf turns about `HingeDoor.hingeAxis` (a Unity `Axis`). Read its angle from
  `Transform.get_localEulerAngles` on that axis: x for `Axis.X`, z for `Axis.Z`, else y.
- `Door.closedAngle` is its angle when shut and `HingeDoor.openAngle` its angle when fully open, both in degrees
  on that axis. `HingeDoor.Awake` sets the open angle from the hinge's limit, so read it once the door is awake.
- To turn a leaf yourself, make its body kinematic (`Rigidbody.set_isKinematic`), set the angle on the hinge's
  axis with `Transform.set_localEulerAngles`, and keep the other two angles as they were. This moves the leaf on
  your client only. Do it on every client, or tell them with your mod's own message.

## How the game moves doors

- **`Door.GhostForceDoor`** pushes a leaf open or shut with physics. It does nothing while `Door.forceRoutine` is
  set, and stores its push there until the push ends. The push switches the door off for players, waits until
  nobody holds it and this client owns it, pushes, waits, then switches the door back on. Anything you switch off
  while it runs is switched on again at its end. Live (2026-11-06): on a shut door the push often leaves the leaf
  where it is; three of four shut doors did not turn.
- **`Door.GhostOpenDoorEvent`** is a ghost event's push. When its first argument is true it pushes the double
  door's other leaf too, one time in three.
- **`HingeDoor.LevelControllerOpenDoor`** sets a leaf straight to a random amount open, on the calling client.
- **`Door.useForceInsteadOfAnimations`** says how the ghost's ordinary door moves (`GhostOpenDoorAnimated`) work
  on this door: with physics when set, else with an animation. `Door.GhostForceDoor` always uses physics.

## Switching a door off for players

- `Door.DisableOrEnableDoor` switches a door on or off for players on every client. Switched off, it drops out of
  any hand and can't be grabbed. It doesn't hold the leaf still.
- `Door.ToggleDoorLock` switches the door on or off for players (grabbing, colliders and the hunt collider) and
  unlocks a locked door. It never locks one.

## A player's grab

- A player's grab on a door runs `Door.GrabbedDoor` on their client. An unlocked door is grabbed on every client.
- A locked door isn't grabbed. The grab runs `Door.AttemptToUnlockDoor` instead: outside a hunt, a key the player
  holds of the door's kind (`Door.kind`) unlocks it; then an ordinary interior door (`DoorKind` None) is
  unlocked (`Door.UnlockDoor`, on every client), and any other kind rattles. So a locked interior door opens to
  the first grab. A `DoorKind.PermanentLock` door never does.

## Locking a door

- `Door.locked` is the door's locked flag on each client. While it is set, the crosshair shows the padlock on the
  door (`PCPropGrab.Update`).
- The game's lock is `Door.SyncLockState`, an RPC every client runs. It sets `Door.locked` and freezes the leaf's
  body where it stands (every position and rotation constraint), then calls `Door.CheckOcclusionPortal`.
- **Never send or call `Door.SyncLockState`** ([Photon](photon.md), "The game's checks on an RPC").
- **`HingeDoor.LockDoor`** is the game's own way to lock. It locks a shut door only: with `force` true, or when the
  leaf is nearly shut, it snaps the leaf shut first. An open door, unforced, stays unlocked. The game never locks
  an open door.
- To leave a door locked where it stands, open or shut, set on every client what `Door.SyncLockState` sets:
  `Door.locked` true, the body's constraints `Rigidbody.kFreezeAll` (`Rigidbody.set_constraints`), then
  `Door.CheckOcclusionPortal`. An interior door locked this way still opens to a player's first grab (above), so
  to keep it locked set its `Door.kind` to `DoorKind.PermanentLock` as well: a grab then rattles it. Custom Ghosts'
  Asterion does all of this for a door it breaks open, after swinging the leaf to `HingeDoor.openAngle`, with the
  host naming the door to each client by its ViewID.
- The door code tests a door's kind only for None and Main. A locked door of any other kind is still pushed by a
  hunt that passes through it, and still picked by ghost events (`GhostActivity.GetDoorToOpen` skips only the front
  door); not seen in the game yet.
- Let a leaf you moved rest before you lock it. A hinge door sets the volume of its creak (`Door.loopSource`)
  from how fast its leaf turns, and only while it is unlocked, so a leaf locked while still turning creaks on
  at that volume (seen in the game, 2026-11-07). Hold it still for half a second first; the game itself only
  locks a door that is still. Custom Ghosts' Asterion rests its broken doors this way.
- `Door.CheckOcclusionPortal` updates whether the camera sees through the doorway. Call it after you open, shut
  or lock a door yourself.
