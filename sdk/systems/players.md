# Players: death and revival

How the game marks a player dead, which of its rules pass over a dead player and which act on one, and how the
two revives and the dead count choose. The entries are in `sdk/map/Player.h`, `GameController.h` and
`CursedItems.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam build
25811160 (`re/decomp.mjs <key>`), and this page names map keys, never addresses.

## The mark

`Player.isDead` is one bool on each client's copy of a Player. The death's completion sets it.
`Player.FinishPlayerRevive` clears it, with the two flags beside it: dying, and one the death animation sets on the
owner's client.

A kill sets the dying flag first. `Player.BeginDeathSequence` sends the death start to every client through the
server, and the death animation to the view's owner. The animation refuses a victim already marked dead, unless
the High Priestess's flag is set (below).

## Killing a player

A kill takes a kind of death, `DeathType`: `HangedMan` or `SnappedNeck`. The victim's body plays that kind's death
animation (the player's character keeps one override controller per kind) and the body left behind lies as that
kind. The game's own kills, all on the master:

| Caller | Kind | What it checks first |
| --- | --- | --- |
| A hunt's kill (the hunt state's kill attempt) | `SnappedNeck` | its target tests; it also adds the victim to its killed list and lengthens the hunt |
| `MonkeyPaw.ReviveWish` | `SnappedNeck` | the wisher is neither dead (`Player.isDead`) nor dying (the bool after it, unmapped) |
| `TarotCards.HangedMan` | `HangedMan` | the card's own tests |

A hunt's choke is the ghost's model during its kill; `Player.BeginDeathSequence` itself plays only the victim's
side. So a mod that kills outside a hunt calls `Player.BeginDeathSequence(player, DeathType.SnappedNeck)` on the
master, as these callers do, and shows its own killer. It must not call it on a friendly ghost's difficulty or for a
player marked dead: the animation refuses both, with a report on the sender (`sdk/systems/photon.md`, "The game's
checks on an RPC"). The dying flag is not mapped, since no fact tells it from the bools beside it, so a mod kills
each player at most once and reads `Player.isDead`. Custom Ghosts' Squatter kicks this way
(`mods/custom_ghosts/squatter.h`, `kick`).

## Who reads it

Most rules walk `Network.playerSpots` and read each spot's Player. 2344 methods reach `Network`: every caller of
`Network.get_Instance`, `Network.GetPlayerFromPhotonPlayer` and `Network.GetSpotPlayerFromPhotonPlayer`, and
`Network`'s own methods. Of those, the ones below read the mark of a spot's Player, or of a Player looked up from a
Photon player. Methods marked *decoy* have no caller.

### Rules that pass over a dead player

| Reader | What a dead player is to it |
| --- | --- |
| `GameController.PlayerDied`'s all-dead test, and its coroutine | counted as gone: the contract ends once every spot's Player is dead |
| `GameController.GetAveragePlayerInsanity` | left out of the average, which walks the spots alone: a Player whose spot is out of the list isn't in it either |
| `ExitLevel.ThereAreAlivePlayersOutsideTheTruck` | not outside |
| `GhostAI.GetNearestPlayer`, `GhostAI.SetNewBansheeTarget`, the hunt's target tests (`GhostHuntState`) | no target |
| `LookAtNearestPlayer`, `Mannequin` and `Prop`'s nearest-player tests, `GhostController`'s player checks | skipped |
| The Ouija board's answer coroutine | not a living player near the board |
| `Wristwatch.UpdateBarVisibility` | its sanity bar hidden |
| The truck's sanity monitor (`HeartRateData.UpdateUI`) | a row that reads "?" with an empty bar |
| `JournalController.PageSync` | the page's sound acts only while the owner is neither dead nor dying |
| `NetworkThreeStateButton.ValidatePlayerSelection` | read with the local player's mark; not traced further |
| `VivoxVoiceManager`'s participant handlers | a participant whose Player is dead has a `PlayerAudio` switch set |
| `SanityEffectsController`, `NetworkedPropGrab.GrabPropRPC`, `LocalPlayer.ReviveStart` | this client's own player, or the grabber; not traced further |

### Rules that act on a dead player

| Reader | What it does with one |
| --- | --- |
| `GameController.AmountOfDeadPlayers` | counts it; spots whose Photon player has left are not counted |
| The Monkey Paw's wish (`MonkeyPaw.MakeWishNetworked`'s coroutine) | accepts a revive wish only while the count is above 0; otherwise the paw twitches and the use is kept |
| `MonkeyPaw.ReviveWish` | revives the first dead player who is not the wisher |
| `TarotCards.HighPriestess` | revives the first player dead and not dying; with none, sets its flag |
| The Onryo's flames on `FireSource` | each dead player widens the master's roll in the flame's update by one, and lowers the bound of the flame's random wait by 20 |
| `Player.RevivePlayer`'s coroutine | waits for its own player's mark, then revives them |

`GameController.AmountOfDeadPlayers` has ten call sites, and three are live:

- the wish's coroutine;
- for the Onryo only, a roll in `FireSource`'s update on the master (`Random.Range(0, 3 + dead) != 0`). Which way
  the roll's result turns the flame was not traced;
- for the Onryo only, the flame's random wait, set from that update and from `FireSource.Extinguish`.

The other seven sites are in decoys. A second dead count on `GameController`, which starts at the room's second
spot, is reached only from decoys.

## The two revives and the dead count

- **The paw's revive wish.** The wish's coroutine (an RPC on every client) checks the sender, then the count:
  1. At 0 the paw twitches and the wish is not used.
  2. Otherwise the use is spent, the paw animates, and `MonkeyPaw.ReviveWish` runs on every client. It looks for a
     dead player it may revive. With none, or with nobody else in the room, the paw twitches and the use stays spent.
  3. Otherwise every client marks the paw's revive used. The master revives the first dead player who is not the
     wisher (`Player.RevivePlayer` with false). Then it rolls 0 or 1, and on 1 kills the wisher
     (`Player.BeginDeathSequence`) if they are neither dead nor dying.
- **The High Priestess.** `TarotCards.HighPriestess` (an RPC) checks the sender. Then, on the master, it revives the
  first player dead and not dying, through a coroutine that waits for that player's mark and calls
  `Player.RevivePlayer` with false. With nobody dead it sends `SetHasDrawnHighPriestess` to every client, which sets
  a flag of the GameController. While the flag is set, the death animation does not refuse a victim already marked
  dead.
- **`Player.RevivePlayer`** starts a coroutine that waits a frame at a time for the player's mark, then sends the
  `Revive` RPC to every client through the server and starts the revive animation. With its flag set it also sends
  `SendRemoveHighPriestess` to the master. Every caller in this build passes false.

So a Player marked dead by anything but a death is found by all three: it raises the count, and a revive that picks
it revives that Player on every client.

## A player with no spot

The game adds a spot with `Network.AddPlayerSpot`, which also gives the spot's equipment list its entries, so a
second add duplicates them. It gives the spot a networked Player with `Network.SetSpotPlayer`. It drops the spot
only in `Network.OnPlayerLeftRoom`, when Photon reports the player gone. So in this build a Photon player still in
the room always has a spot, except for the moment between their departure and that callback.

A Photon player with no spot is not invisible to the game:

- `Network.GetSpotPlayerFromPhotonPlayer` answers null for them, so `Network.GetPlayerFromPhotonPlayer`, which
  calls it, answers `Network.localPlayer`: each client's own investigator.
  `Network.GetIndexOfPlayer` answers 0, the first spot's place.
- Lookups by Photon player are common: about 225 methods call `Network.GetPlayerFromPhotonPlayer` (equipment,
  cursed items, the truck's lift button, the seasonal events), most with an RPC's sender or a view's owner
  (`PhotonView.owner`).
- A view with no owner number, such as a scene object's, is owned by the master client
  (`PhotonView.set_OwnerActorNr`, `PhotonRoom.GetPlayer`). So while the master has no spot, every object nobody has
  taken resolves, through such a lookup, to whichever investigator is local.
- `KrampusPresent.GetFurthestSpawnPositionFromPlayers` walks the room's Photon players, not the spots, and throws
  when the lookup answers null.
- `Network.OnPlayerLeftRoom` does nothing for a leaver with no spot: no event, so no all-dead test, and no removal of
  their RPCs on the master.

- `Network.SetSpotPlayer`'s coroutine waits until the list holds as many spots as the room has Photon players, so a
  Player set up while a spot is out waits for it to come back.
- The list is sorted by the spot's Photon actor number (int.MaxValue without one), as `Network.OnPlayerLeftRoom`
  sorts it.

## What a mod that keeps a player out of play must do

A spot cannot be emptied (see [the contract](contract.md), "The start"): a spot whose Player is null makes the dead
count throw. A spot can be taken out of the list instead, and every rule above that walks the list then passes over
it, dead or alive. Playable Ghost does this on every client for the ghost player, once their body is parked and the
client's own load wait is over (`mods/playable_ghost/outlist.h`). Because of the section above it also:

- hooks `Network.GetSpotPlayerFromPhotonPlayer`, so that player's Photon player still finds their spot, while every
  other Photon player gets the game's own lookup. `Network.GetPlayerFromPhotonPlayer` calls it directly, so it
  still answers their Player (the parked body).

  So every caller of either lookup gets the answer it got with the spot in the list: the owner of every untaken
  scene object while that player is master, any RPC they send, and the game's own handling of a call it refuses
  ([photon](photon.md)). Only a player's position in the list differs: the held spot has none, and every later
  player moves down one ("A player's position in the list", [the maintainers' page](../internals/players.md)).

  Network's second, obfuscated Player lookup does not call the spot lookup, so neither hook reaches it. In this
  build nothing live calls it (`Network.GetPlayerFromPhotonPlayer`'s note).
- hooks `KrampusPresent.GetFurthestSpawnPositionFromPlayers`. While it runs, the spot lookup answers that player's
  Photon player with another player's spot from the same walk. So the search counts that position twice and never
  the parked body. A duplicate changes nothing in the search (the entry's note), so the present lands where it does
  in an AI ghost's contract. With nobody else in the room the lookup still answers the held spot, because a null
  would make the search throw.
- hooks `Network.OnPlayerLeftRoom`, putting the spot back first when that player leaves, so the game's own departure
  runs for them.
- puts the spot back in the list's sort order while still in a room, and lets it go once out of one
  (`Network.OnLeftRoom` empties the list as the client leaves a room).

## Speed and stamina

Read from decompiled code of build 25811160. A player's speed is worked out on that player's own client, by its rig
(`LocalPlayer`, `Network.localRig`; `LocalPCPlayer` on PC). The rig holds a walking speed (`LocalPlayer.walkSpeed`) and a
sprinting speed (`LocalPlayer.sprintSpeed`), serialized floats its prefab sets, and a multiplier
(`LocalPlayer.speedMultiplier`), which `LocalPlayer.Start` copies from the contract's `Difficulty.playerSpeed`.
`LocalPCPlayer.UpdatePlayerSpeed` gives the controller the sprint speed while three things hold, and the walk speed
otherwise:

- the sprint input is held;
- sprinting is allowed: `Difficulty.sprinting` is not 0 (0 off, 1 on, 2 infinite), or no contract is running;
- the stamina may sprint (`PlayerStamina.canSprint`, of the rig's `LocalPlayer.stamina`).

It then multiplies that speed by the multiplier, except in a contract while the rig's player is dead
(`LocalPlayer.isDead`, which `LocalPlayer.CompleteLocalPlayerDeath` sets and the rig's revive clears). On VR the
stamina's own update (`VRStamina.Update`), on the branch a flag of `VRStamina` picks (not traced), gives the VR movement
the sprint or the walk speed and, in a contract, the same multiplier with the same exception; outside a contract it
applies no multiplier, where the PC does. The other branch, and where VR's movement reads the sprint gate, were not
traced.

So in a contract the speed a living PC player would sprint at, whether sprinting is on or not, is `sprintSpeed` times
`speedMultiplier` of that player's rig, and a dead one's is `sprintSpeed` alone; a VR player's is the same on the
traced branch above.
`LocalPCPlayer.SetPlayerSpeedMultiplier` changes the multiplier on that client only (a Monkey Paw wish calls it), so
another client knows the multiplier only through the difficulty, unless the player's own client says. Custom Ghosts'
Squatter has each client tell the master its own player's sprint this way (`mods/custom_ghosts/squatter_net.h`).

The stamina drains in a contract only while `Difficulty.sprinting` is 1, so with 2 nobody tires. When it runs out on
PC, `PCStamina.OutOfBreath` clears `canSprint` and plays the out-of-breath sound: in a room by an RPC to every
client, which checks that the player's own client sent it, and alone through `NetworkedStamina.ForceOutOfBreatheLocal`.
With sprinting off it does nothing unless `LocalPlayer.isDead` is set. On VR the stamina's out-of-breath step (a
`VRStamina` method that also plays the out-of-breath sound) clears `canSprint` the same way. `canSprint` itself is
never sent: only the player's own client holds it. What every client does get is the player's `NetworkedStamina`
(`Player.stamina`): the player's client sends whether it walks and whether it sprints in the view's serialization, and
`moving` follows them, so another client sees a sprint stop but not why.

## The body

What a mod needs to animate an investigator's own body, as the Emotes mod does (`sdk/api/player_anim.h`,
`sdk/api/third_person.h`). Read from the class dump and decompiled code of Steam build E300EDA9.

- **Its Animator** is `Player.animator`, on every client. The game drives it with its own controller, swaps override
  controllers for the held item (`Player.characterAnimations`, a `PlayerCharacterAnimations`), syncs its parameters
  with a PhotonAnimatorView and bends the hands and arms with Animation Rigging (a RigBuilder, whose graph runs at
  sorting order 1000). Unity's Playables are all in the build (`sdk/map/Playables.h`): a second graph with an animation
  output on the same Animator, at a higher sorting order and a weight from 0 to 1, plays a clip over all of that and
  gives the body back untouched when it is destroyed. Under the rig, the clip would be bent by its aims: `Head Rig`
  turns the head toward where the camera looks, and aiming rigs (equipment, torch, watch, lantern, the journal's
  hands) turn the arms. Over it, what the rig places rather than animates keeps the game's pose: the journal hangs from
  the rig's `JournalParent` (`JournalRigController.constraintParent`), which `Journal Parent Rig` puts between a socket
  on the waist bone and one in the right hand. The belt (`Belt`, with its equipment points) is a child of the first
  spine bone and follows the clip.
- **How the local player hides its own head.** `PlayerCharacter.headRenderersToTurnOff` lists the head's renderers
  (hair, eyes, teeth and the head's cosmetics). On the player's own client the game draws them as shadows only
  (`Renderer.set_shadowCastingMode`), and has methods of `PlayerCharacter` to switch them back and to move them to
  another layer. A view from outside draws them (`ShadowCastingMode.On`) and gives each its mode back after.
- **How the local player hides its own body.** `Player.renderers` lists the body's renderers. `Player.SetPlayerModelLayer`
  moves each one's GameObject between two named layers, and the local camera leaves the hidden one out. A camera of a
  mod's own that adds those renderers' layers to its culling mask shows the body without touching them.
- **Where the held item hangs.** On other clients it hangs from the body's hand anchors
  (`PlayerCharacterAnimations.parentingTargetRightHand` and `parentingTargetLeftHand`). On the player's own PC client it
  hangs in a slot in front of the camera: `PCPropGrab.GetItemInCameraSlot` gives it, and
  `PCPropGrab.ParentItemOnLocalPlayer` puts it back there at the item's own offset. `NetworkedPropGrab.GrabPropRPC`
  switches the item's sync component off and freezes its Rigidbody while it is held, so moving it on the holder's own
  client sends nothing.
- **How the body turns.** On PC the game's MouseLook turns the player's root by the mouse's sideways movement and the
  head camera (`LocalPlayer.camera`) by its up and down movement; `PCLook`, in LateUpdate, then bends the spine and head
  toward where the camera looks. The body's facing reaches other clients with the root's rotation.
- **VR.** A VR player's investigator is a `VRPlayer`. Its `VRPlayer.leftHandIK` and `VRPlayer.rightHandIK` are the IK
  targets that follow the controllers.

### What the player does, by input action

The game's input actions (`PCControls.actions`, the `InputActionAsset` of `LocalPCPlayer.controls`) are named, with their
default keyboard and gamepad bindings: Pickup (E, west button), Secondary Use (F, left trigger), Primary Use (right mouse
button, south button), HoldPrimaryUse (held Primary Use), Interact (left mouse button, right trigger), Drop (G, east
button), Crouch (C, right stick press), Run, Move, Look, InventorySwap (Q, north button), InventorySwapScroll (the
scroll wheel), InventorySlot1 to 4 (1 to 4), Torch and Hat Use (T, D-pad down), InspectWatch (H, D-pad left), Journal
(J, select), LocalPushToTalk (V, left shoulder), GlobalPushToTalk (B, right shoulder), Pause, and the menu actions UIUp,
UIDown, UILeft, UIRight (the arrow keys and the D-pad) and UISelect and UIShoulder ones. A mod reads one by its name
(`InputActionMap.m_Actions`, `InputAction.m_Name`) and asks `InputAction.WasPressedThisFrame`, so the player's own
rebinding counts. `PCPropGrab` holds the handlers behind the item actions.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/players.md).
