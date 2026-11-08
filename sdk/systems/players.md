# Players: death and revival

How the game marks a player dead, which of its rules pass over a dead player and which act on one, and how the
two revives and the dead count choose. The entries are in `sdk/map/Player.h`, `GameController.h` and
`CursedItems.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam build
25811160 (`re/decomp.mjs <key>`), and this page names map keys, never addresses.

## The mark

`Player.isDead` is one bool on each client's copy of a Player. The death's completion sets it, and
`Player.FinishPlayerRevive` clears it with the two flags beside it (dying, and one the death animation sets on the
owner's client). A kill sets the dying flag first: `Player.BeginDeathSequence` sends the death start to every
client through the server and the death animation to the view's owner, and the animation refuses a victim already
marked dead unless the High Priestess's flag is set (below).

## Who reads it

Most rules walk `Network.playerSpots` and read each spot's Player. Of the 2344 methods that reach `Network` (every
caller of `Network.get_Instance`, `Network.GetPlayerFromPhotonPlayer` and `Network.GetSpotPlayerFromPhotonPlayer`,
and `Network`'s own methods), these read the mark of a spot's Player or of one looked up from a Photon player.
Methods marked *decoy* have no caller.

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

Of `GameController.AmountOfDeadPlayers`'s ten call sites, the wish's coroutine and two of `FireSource` are live, both
for the Onryo only: a roll in `FireSource`'s update on the master (`Random.Range(0, 3 + dead) != 0`), and the flame's
random wait, set from that update and from `FireSource.Extinguish`. Which way the roll's result turns the flame was not
traced. The other seven sites are in decoys, and a second dead count on `GameController`, which starts at the room's
second spot, is reached only from decoys.

## The two revives and the dead count

- **The paw's revive wish.** The wish's coroutine (an RPC on every client) checks the sender, then the count: at 0
  the paw twitches and the wish is not used. Otherwise the use is spent, the paw animates and `MonkeyPaw.ReviveWish`
  runs on every client. It looks for a dead player it may revive; with none, or with nobody else in the room, the paw
  twitches and the use stays spent. Otherwise every client marks the paw's revive used, and the master revives the
  first dead player who is not the wisher (`Player.RevivePlayer` with false), then rolls 0 or 1, and on 1 kills the
  wisher (`Player.BeginDeathSequence`) if they are neither dead nor dying.
- **The High Priestess.** `TarotCards.HighPriestess` (an RPC) checks the sender, then on the master revives the first
  player dead and not dying, through a coroutine that waits for that player's mark and calls `Player.RevivePlayer`
  with false. With nobody dead it sends `SetHasDrawnHighPriestess` to every client, which sets a flag of the
  GameController; while it is set the death animation does not refuse a victim already marked dead.
- **`Player.RevivePlayer`** starts a coroutine that waits a frame at a time for the player's mark, then sends the
  `Revive` RPC to every client through the server and starts the revive animation. With its flag set it also sends
  `SendRemoveHighPriestess` to the master. Every caller in this build passes false.

So a Player marked dead by anything but a death is found by all three: it raises the count, and a revive that picks
it revives that Player on every client.

## A player with no spot

The game adds a spot with `Network.AddPlayerSpot` (which also gives the spot's equipment list its entries, so a
second add duplicates them), gives it a networked Player with `Network.SetSpotPlayer`, and drops it only in
`Network.OnPlayerLeftRoom`, when Photon reports the player gone. So in this build a Photon player still in the room
always has a spot, except for the moment between their departure and that callback.

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
count throw. A spot can be taken out of the list, which every rule above that walks it then passes over, dead or alive.
Playable Ghost does this on every client for the ghost player once their body is parked and the client's own load
wait is over (`mods/playable_ghost/outlist.h`). Because of the section above it also:

- hooks `Network.GetSpotPlayerFromPhotonPlayer`, so that player's Photon player still finds their spot, while every
  other Photon player gets the game's own lookup. `Network.GetPlayerFromPhotonPlayer` calls it directly, so it
  still answers their Player (the parked body). Every caller of either lookup therefore gets the answer it got with
  the spot in the list: the owner of every untaken scene object while that player is master, any RPC they send, and the
  game's own handling of a call it refuses
  ([photon](photon.md)). Only a player's position in the list differs ("A player's position in the list", [the maintainers' page](../internals/players.md)):
  the held spot has none, and every later player moves down one. Network's second, obfuscated
  Player lookup does not call the spot lookup, so neither hook reaches it; in this build nothing live calls it
  (`Network.GetPlayerFromPhotonPlayer`'s note).
- hooks `KrampusPresent.GetFurthestSpawnPositionFromPlayers`. While it runs, the spot lookup answers that player's
  Photon player with another player's spot from the same walk, so the search counts that position twice and never the
  parked body. A duplicate changes nothing in the search (the entry's note), so the present lands where it does in an AI
  ghost's contract. With nobody else in the room the lookup still answers the held spot, because a null would make the
  search throw.
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
otherwise, times the multiplier:

- the sprint input is held;
- sprinting is allowed: `Difficulty.sprinting` is not 0 (0 off, 1 on, 2 infinite), or no contract is running;
- the stamina may sprint (`PlayerStamina.canSprint`, of the rig's `LocalPlayer.stamina`).

So the speed a player would sprint at, whether sprinting is on or not, is `sprintSpeed` times `speedMultiplier` of
that player's rig. `LocalPCPlayer.SetPlayerSpeedMultiplier` changes the multiplier on that client only (a Monkey Paw
wish calls it), so another client knows the multiplier only through the difficulty, unless the player's own client
says.

The stamina drains in a contract only while `Difficulty.sprinting` is 1, so with 2 nobody tires. When it runs out on
PC, `PCStamina.OutOfBreath` clears `canSprint` and plays the out-of-breath sound: in a room by an RPC to every
client, which checks that the player's own client sent it, and alone through `NetworkedStamina.ForceOutOfBreatheLocal`.
With sprinting off it does nothing (unless a flag of the rig, not traced, is set). `canSprint` itself is never sent:
only the player's own client holds it. What every client does get is the player's `NetworkedStamina` (`Player.stamina`):
the player's client sends whether it walks and whether it sprints in the view's serialization, and `moving` follows
them, so another client sees a sprint stop but not why.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/players.md).
