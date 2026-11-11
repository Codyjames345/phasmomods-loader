# The contract

How a contract starts and ends on each client: the load wait, the two ends, every way out, and what each
leaves behind for the reward screen. The entries are in `sdk/map/GameController.h`, `Player.h`,
`LevelStats.h` and `Rewards.h`; their `///` notes are the authority. Facts were read from decompiled code of
Steam build 25811160 (`re/decomp.mjs <key>`), and this page names map keys, never addresses.

## The start

`GameController.allPlayersLoaded` is set once, by the controller's load coroutine, when each of
`Network.playerSpots` holds a live `Player`. It then raises the class's loaded event. `LiftButton.AttemptUse`
refuses the truck's button until it is set, so a Player taken out of a spot stalls every client still loading.
A mod that wants a player out of play leaves their spot and its Player alone until this client's own
`GameController.allPlayersLoaded` is set (see [players](players.md), "What a mod that keeps a player out of play
must do").

## The two ends

Both are RPCs every client runs, on different objects with their own views. Nothing shown stops both from
arriving in one contract.

| End | Sent when | Who sends |
| --- | --- | --- |
| `ExitLevel.Exit` | The truck leaves. | The master only: `LiftButton.NetworkedUse` starts `ExitLevel.StartAttemptExitLevel` only on the master with the doors closing. |
| `GameController.Exit` | Every player is dead or gone. | The master only: the sending coroutine tests it, about five seconds after the all-dead test. |

Each first checks its sender (see [photon](photon.md)); a refused call does nothing more.

### What `ExitLevel.StartAttemptExitLevel` does

The truck's lift button calls this on the master only, and nothing sends an RPC by name here: the method runs
`ExitLevel.StopAllCoroutines`, and when the object's ready bool (a value serialized in the scene, which no
game code writes) is set, starts a coroutine. That coroutine sets `Network.sceneChanging`, plays the truck's
start-up sound through the object's own `PhotonView`, waits a fixed scaled `WaitForSeconds`, then reads the
ready bool again: still set, it sends the game's own `ExitLevel.Exit` with the game's own sender; cleared in
the meantime, it plays the stop sound and clears `Network.sceneChanging` instead. A scene with no ready
ExitLevel starts nothing at all. Because the wait is scaled, a game held at `Time.timeScale` 0 never finishes
leaving.

### When every player is dead or gone

`GameController.PlayerDied` runs the all-dead test: every spot's live Player is dead. A **leaver** reaches the
same test. `Network.OnPlayerLeftRoom` removes their spot before anything reads it, then raises the event
GameController's handler runs the test from. So when A dies and B quits alive, the remaining spots are all dead
and the game ends the contract as if everyone had died. An ending does not tell a mod who actually died.

### What `ExitLevel.Exit` does, in order

1. The sender check.
2. `Network.sceneChanging` is set.
3. On the master, the local investigator's `Player.stats` runs `PlayerStats.SyncStats`.
4. `LevelValues.completed` is set.
5. While `LevelValues.eventsBlocked` is set, it clears `completed`, loads the main menu and disconnects, and
   none of the rest runs.
6. If the local investigator is dead, the death status is written ([the maintainers' page](../internals/contract.md)).
7. The mission status is written.
8. `ExitLevel.CheckMissions`, which completes the identify objective when the journal names the ghost.
9. `ExitLevel.CheckChallenges`.
10. `LevelStats.objectivesCompleted`, `objectivesFailed` (the remainder plus one) and `averageFPS`.
11. If the local investigator's journal is open, `OpenCloseJournal(false)`.
12. `GameController.LoadBackToMenu`.

Steps 3, 6, 8, 9 and 11 read `Network.localPlayer`, so a client with none throws there. A mod that clears that
reference puts it back before the game's body runs.

### What `GameController.Exit` does

The sender check, `LevelValues.completed`, the death status always, a mission status by the room's size (one
alone, another with others), both checks, the journal's close while `Network.localPlayer` is alive,
`Network.sceneChanging`, and the same coroutine as `LoadBackToMenu`. It has no stats sync, no objective counts and
no path for `eventsBlocked`.

## Leaving and quitting

- **`PauseMenuController.Leave`** silences the listener, writes the status, reads the local investigator, sets
  `Network.sceneChanging` and clears `LevelValues.completed`. Then it starts its coroutine, which runs
  `LoadingController.LoadScene` to the main menu, waits for `LoadingController.ready` and one frame more, and then
  calls `PhotonNetwork.Disconnect`.
- **`PauseMenuController.Quit`** writes the status, reads the local investigator and ends with `Application.Quit`.
  With `Network.localPlayer` null it throws there, and the game does not close.
- The coroutine needs the menu's object active. `PauseMenuController` shares the canvas with `JournalController`,
  and `JournalRigController.journalMeshObject` is a sibling of that canvas, so closing the journal leaves it active.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/contract.md).
