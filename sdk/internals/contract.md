# The contract: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [The contract](../systems/contract.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## What an ending leaves for the reward screen

The game keeps a death status and a mission status on a settings object that outlives the scene, the one
`GameSettings.gameplay` returns. The death status is `GameplaySettings.PlayerDied`; the mission status is `GameplaySettings.MissionStatus`.

| Status | Written by | Read by | Effect |
| --- | --- | --- | --- |
| Death | `ExitLevel.Exit` when the investigator is dead; `GameController.Exit` always | `RewardManager.Awake`, which copies it when `LevelValues` says the contract was played | The board shows the death row, halves its totals and adds the insurance row while the difficulty allows it. The result coroutine clears it. On a difficulty that loses items, the committed equipment is lost (below). |
| Mission | Both ends and both pause buttons | A `MainManager` method | Any of 1, 2 or 3 sends a signal, and it resets to 0. |

- The objective counts in `LevelStats` are not touched by the reward methods read so far. Past the reward screen
  they are unproven.
- `SingleplayerProfile.AddXPAndFunds` has three callers, all on the reward screen, among them `RewardManager.Skip`,
  so a player can reach it as soon as the screen is up. **Nothing proves how long after a contract's end that
  is.** It pays the experience only; the money is paid elsewhere (below).

## Where the reward screen's money and experience go

- **Money.** The screen keeps its own money total, which its reward methods add to as they show each row. The first
  run of `RewardManager.Results.MoveNext` (its step 3, in the next section) reads the player's saved money (the save key
  `PlayersMoney`), adds that total, caps the sum at 1,000,000,000 and saves it. That is the only place the
  screen's money reaches the save, and it runs once per screen, before any of the calls below.
- **Experience.** `SingleplayerProfile.AddXPAndFunds(xp, money)` stores `xp` twice on the profile card (the card's
  count-up) and `money` once, then adds `xp` to the player's level at once, levelling up while the level is under
  9999, and saves it (the save key `Experience`). The card's money field is written and never read: no method of
  `SingleplayerProfile` or its nested types reads it, and the field is private. So the `money` argument reaches
  nothing. Its callers, `RewardManager.Skip` and two other reward-screen methods, pass the screen's own experience and money
  totals; `Skip` then shows the saved money through `GhostOS.UpdateMoneyUI`.
- So a mod that changes the money argument of `AddXPAndFunds` changes nothing a player keeps, and a second call adds
  the experience twice. To change a player's money, the screen's own money total must change before
  `RewardManager.Results` saves it, or the saved money must change afterwards.
- **The totals have one setter.** `RewardManager.BuildRows` adds each row's money and experience to the totals, and
  calls `RewardManager.SetTotals` for its multipliers; that sets both and counts the shown total to the new value. The
  results' first step saves the money straight after `BuildRows` returns, so a `SetTotals` call there is both what the
  screen's total shows and what is saved. The rows above it still show their own amounts.
- **The results can wait until they start.** `RewardManager.Awake` listens with `RewardManager.StartResults` on an
  event of the screen's profile card. Until it runs nothing has been saved, and its first step cannot wait once it has
  begun (the state machine is on that method's stack until its first await). `RewardManager.Skip` and
  `RewardManager.Next` are the buttons' listeners, also added by `Awake`.
- **What Playable Ghost does (`mods/playable_ghost/reward.cpp`).** On the ghost player's client, `StartResults`, `Skip` and
  `Next` wait for the final deception score, which comes a few seconds after the contract's end. Then `BuildRows` is followed
  by `SetTotals` with the ghost's money and experience. The ghost player keeps that money and no investigator money.
- Mission and challenge progress has more than the two checks: the reward screen's results move challenge progress
  too, and can unlock Steam achievements. Skipping `CheckMissions` and `CheckChallenges` does not stop those (below).

## The reward screen's results

`RewardManager.Results` is the reward screen's async method. `RewardManager.Results.MoveNext` runs its steps; its
first run, in one call:

1. Runs the multiplier challenge's check. When `LevelValues.CompletedMultiplierChallenge` holds, the screen's copy of
   the death status is clear and a saved progress value is past each of three thresholds, it saves an unlock flag,
   **unlocks a Steam achievement** (through the game's achievement helper, which ends in
   `Steamworks.SteamUserStats.SetAchievement`) and progresses the matching challenge. It then progresses those three
   challenges again from their saved unlock flags, one more challenge if the local `PlayerSpot` lacks an unlock, and
   every challenge type `ChallengesController` collected during the contract, which it then clears.
2. Writes the screen's header, then runs `RewardManager.BuildRows`, which builds the rows and their totals and also
   progresses three more challenges from the contract.
3. Saves the screen's money into the player's saved money (below).
4. Builds the totals' animation. When a check on the player's level object holds, the screen's experience is set
   to 0 (by its use, the level cap; the check itself was not read).
5. Progresses the experience-earned challenge by the screen's experience and the money-earned challenge by its money,
   and copies the money into `LevelStats`.
6. Clears `GameplaySettings.PlayerDied`.
7. Starts `RewardManager.EventRewards` when the contract had a seasonal event, then waits a moment and refreshes
   the customisation screen.

`RewardManager.EventRow`, called straight after the money is saved, answers empty for a contract with no seasonal event.
Only a filled answer shows the event's row and runs the event part.

`RewardManager.EventRewards.MoveNext` saves the event's score with the game's server
(`SeasonalEventController.SaveEventProgress`), shows the themes and cosmetics that unlocks, refreshes the player's
badges and cosmetics, and only then progresses the event's challenge, so that part runs whenever the server answers.

Every challenge progress above goes through `ChallengesController.ChangeChallengeProgression`, which itself makes no
Steam call. The achievement in step 1 does not go through it. Playable Ghost drops that method's calls while either
`MoveNext` runs on the ghost player's client after their contract, and leaves the achievements to the game
(`mods/playable_ghost/reward.cpp`).

Playable Ghost also answers `RewardManager.EventRow` empty on the ghost player's client after their contract, so the ghost
player has no event row and sends no event score. `SeasonalEventController.SaveEventProgress` is the only send in the
event part. The rest of that part is the unlock popups, `CloudEvents`' refresh of the player's badges and cosmetics, the
event's challenge, and logs, so skipping it sends nothing else and the results then take the path of any contract with no
event. Investigators are unaffected.

Both state machines are compiler-generated types whose names the obfuscator changes, so their entries carry
`kCompilerGenerated` and are found by their fields.

## The death status and the saved equipment

Equipment a player puts in the truck moves out of their saved stock into a saved committed amount, per item:
`InventoryEquipment.AddToLobbyNetworked`, on the sender's own client, saves their new share as the committed amount
and takes it off the stock with `EquipmentInfo.RemoveAmountOwned`. A withdrawal is the same RPC with a negative
amount, so it puts the amount back.

The reward screen's handlers that turn its buttons back on also settle the committed amounts. With its copy of the death status set
(`RewardManager.Awake`) and the contract's `Difficulty.loseItemsOnDeath` set, it calls
`InventoryManager.ResetAllEquipment`: each item's `InventoryEquipment.ResetItem(true)` saves the committed amount as 0
without returning it, so it is lost. Otherwise it calls `InventoryManager.ReturnItemstoInventory`: each item's
`InventoryEquipment.ReturnCommittedItem` adds it back to the stock. The insurance row the screen shows on such a death
is a sum read from the local `PlayerSpot`; nothing in it returns an item.

So a client whose death status is written loses whatever is still committed when the screen ends, on a difficulty that
loses items. `GameController.Exit` writes the death status on every client, alive or not. Playable Ghost's ghost player
never died, so on their client the mod leaves the status as `GameController.Exit` found it
(`mods/playable_ghost/contract_end.cpp`), as `ExitLevel.Exit` does for a living investigator.

`InventoryManager.RemoveMyEquipmentFromLobby` withdraws through `InventoryEquipment.RemoveCommittedItemsFromLobby` on
each item, which sends the withdrawal only when `InventoryEquipment.CanRemoveFromLobby` allows it: in a room, outside
one kind of contract, with an amount in the player's `PlayerSpot` equipment, and then a store-wide state or the item's
own saved flag. An item it refuses stays committed. The withdrawal also only reaches the committed amount once the
server returns the RPC to the sender.

## Not mapped

The reward screen's own methods named in the results (the multiplier challenge's check and the achievement helper) are
obfuscated and have no entry: nothing calls or hooks them, and their behaviour is above.
