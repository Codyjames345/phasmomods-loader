# Photon

What a mod author needs about the game's Photon use: rooms and players, PhotonViews and their cleanup, and the
game's checks on who sent an RPC. **Read the last section before anything that sends.** The entries are in
`sdk/map/Photon.h`, `Player.h` and `GameController.h`; their `///` notes are the authority. Facts were read from
decompiled code of Steam build 25811160 (`re/decomp.mjs <key>`), and this page names map keys, never addresses.

## Rooms and players

- `PhotonNetwork.get_PlayerList` is the room's Photon players, each with `PhotonPlayer.get_ActorNumber`.
  `PhotonPlayer.IsLocal` says which is this client.
- `Network.playerSpots` holds one `PlayerSpot` for each, joined by `Network.GetSpotPlayerFromPhotonPlayer` and
  `Network.GetPlayerFromPhotonPlayer`.
- A leaver is told by `Network.OnPlayerLeftRoom`, which removes their spot before anything else reads it. A mod
  that needs to know who left compares the room's players over time: the spot is gone by the time it could look.
- When this client leaves a room, `Network.OnLeftRoom` empties `Network.playerSpots`.
- The room's master changes when the master leaves. `SetMasterClient` is not in this build, and nothing in it
  writes the room's master property.

## Which screens are rooms, and when offline mode is on

A mod that asks "am I in a room?" or "is this single player?" must read both states, because the game's own
screens do not line up with what the names suggest (read from the decompiled connect code, 2026-11-11):

- **The main menu is a connected Photon client.** The game connects for the lobby browser, so
  `PhotonNetwork.get_OfflineMode` is false at the main menu while `PhotonNetwork.get_InRoom` is false there.
- **Single player is offline mode plus a joined room.** The connect code calls `PhotonNetwork.set_OfflineMode`
  with true when no lobby or ready connection exists, and joins the room as usual afterwards. So a solo
  contract has `get_OfflineMode` **and** `get_InRoom` both true; no server is involved and RPCs stay local.
- **A multiplayer room,** public or private, is offline mode false with `get_InRoom` true while in it.

So "offline mode true" means single player, and "offline mode false with no room" means the menu screen; only
"offline mode false with a room" is a real room.

## A room holds four players

Kinetic's Photon servers cap a room at four players. Creating one for eight fails with the server's error that the
room's max players value is too big (limit 4), seen in a run. The cap is a setting on the servers, so no client can
raise it, and the project does not try. Bigger Lobbies was archived for this reason (`archive/bigger_lobbies`).

## Whether a room is public

`PhotonRoom.get_IsVisible` says whether the room browser lists a room.

- **The choice is made once.** `LobbyController.CreateServer` creates the room from the Create Lobby paper's options and
  keeps whether they list it. Nothing in the lobby's papers changes it afterwards, so a host cannot turn an open lobby
  public or private.
- **A contract unlists every room.** `ServerManager.LoadScene`, the RPC that starts a contract, has the master call
  `PhotonRoom.set_IsOpen` with false and `PhotonRoom.set_IsVisible` with false. So during a contract a public room reads
  as unlisted: `get_IsVisible` alone cannot tell a public lobby's contract from a private one.
- **The return from a contract lists it again.** As the main menu starts in the room, a coroutine runs
  `ServerManager.InitBackFromGame` and has the master call `set_IsOpen` with true and `set_IsVisible` with the kept
  value. This is the game's only other call to either setter, and it runs in the lobby, never during a contract. No room
  turns public during a contract.
- **A new master keeps its own value.** The value is kept on the client that called `CreateServer`, in a level value
  that outlives scenes. A player who later becomes the master restores the value from *its own* last `CreateServer`,
  not the room's. So if that player last created a public room, a private lobby can turn public on the new master's
  return from a contract, in the lobby. Judge a room in every lobby frame, and treat public as lasting until the room
  is left.

The loader's rule for mods follows from these (host/join_guard.h): a room read as public in the lobby stays public for
that client until it leaves the room.

## A contract's load, and the message queue

`ServerManager.LoadScene`, a contract's start, runs on every client as an RPC. Among the first things it does after the
level values is clear Photon's message queue, the flag `PhotonNetwork.get_IsMessageQueueRunning` reads.

- **The lobby looks untouched for a while.** The lobby's scene and its `ServerManager` stay up, and
  `LevelController.instance` stays dead, until the level activates. Only the queue's flag tells this window from a lobby:
  a hook on `LoadScene` is not needed to see it.
- **While it is clear, nothing arrives.** RPCs and events wait, in order, and arrive once it is set. So a client deciding
  anything during a load decides on what it had heard before `LoadScene`, and a message sent just after waits until this
  client's level is up.
- **Who sets it again:** `PhotonNetwork.NewSceneLoaded`, `Player.Start` in the level, and a method of `ConnectToServer`
  on the main menu, each on a condition of its own that was not followed. A lobby cannot work with it clear, since no RPC would reach it.
  `LoadingController`'s loading coroutine and `PhotonNetwork.LoadLevel` clear it too.

The loader's rule follows from this (host/room_actions.h, `lobby_open`): with the queue stopped it is no lobby frame, so
nobody is judged and no mod is switched on or off.

## How a contract starts

- **The press.** `ServerManager.startGameButton`'s one listener, added in `ServerManager.Start`, is an `async void` the
  compiler made, with `ServerManager.StartGame` inlined in it. So a hook on `StartGame` sees no click. `Button.Press` does,
  and it is where a click invokes the listener. Nothing else names `StartGame` or that listener, apart from an uncalled
  look-alike of `Start`.
- **What it does before sending.** It picks the voted contract into a `ServerManager` field and calls one virtual on each
  `PlayerSpot.player`, which was not identified, and it may wait on a task. Then it sends `ServerManager.LoadScene` by name with
  `PhotonView.RPC`, to everyone through the server, buffered. Nothing follows the send: the method marks itself finished and
  returns at once, so a send held back and made later leaves nothing of the method waiting on it.
- **The game's own gate.** Online the button lights only in `ServerManager.NetworkedReady`, on the master, when every
  `Network.playerSpots` entry has `PlayerSpot.playerReady`. `ServerManager.UpdateUI` with true clears them all and puts the
  button out; a join runs it at the end of `ServerManager.AssignNewPlayerSpot`'s work and a leave in `ServerManager.RemovePlayerCard`. So a
  join makes everyone ready again, the joiner too. But a joiner is in `PhotonNetwork.get_PlayerList` well before that, while a
  cloud check of their name runs, and until then the button stays lit. Offline the button needs no Ready.
- **The gate says nothing about mods.** A player who has not been verified can press Ready, and the game's gate has no
  input for anything else. Making the gate check mods would mean overriding the button every frame and working out its
  state again afterwards. Even then it could not cover a click in the frame a join arrives, because Photon's messages are
  dispatched in `FixedUpdate`, ahead of the `EventSystem`'s `Update`. So the loader leaves the gate alone. It refuses the
  press itself, which leaves the game's state unchanged, and it also refuses the `LoadScene` send, because the task can
  wait and a player can join meanwhile (host/lobby_gate.h).

## Nobody joins a running contract

- `ServerManager.LoadScene` on the master calls `PhotonRoom.set_IsOpen` with false, `PhotonRoom.set_IsVisible` with false and
  `PhotonRoom.set_MaxPlayers` with the players there, among its first steps. Photon refuses a join to a closed room.
- Those are room properties, on the server: a new master inherits them. The only other caller of `set_IsOpen` is the main
  menu's start coroutine, in the lobby after a contract (above). Nothing in the game calls `PhotonNetwork`'s rejoin methods directly.
- **The one window is the start itself.** The master's own `LoadScene` is the server's echo of what it sent, so in the game the
  room stays open for about one round trip after the send. A join the server takes in that window gets the buffered `LoadScene`
  and loads with everyone else.
- **The loader closes that window** (host/start_close.h, the user's ruling of 2026-11-03). On a modded host, in a
  private room:
  1. The start's `LoadScene` send is held.
  2. The master calls `PhotonRoom.set_IsOpen` with false itself.
  3. It raises a marker on the loader's own channel to `ReceiverGroup.All`. That is how the game's own via-server sends
     reach everyone and come back to their sender. Other clients ignore the marker.
  4. When the marker comes back, the master reads the room again. If everyone there is verified, the held send goes,
     exactly as the game made it. Otherwise the room opens again and the host hears who it waits for. With no answer in
     5 seconds the room opens again too, and the host is told to press Start again.

  That the server applies the close first, so that any join it took earlier reaches the master before the marker, rests
  on Photon's ordering and not on the game's code; the smoke list checks it. Once the send has gone, no press or send
  starts anything until the game's `LoadScene` leaves the lobby, and nothing opens the room from then on. A master who
  loses the seat and stays opens the room again, and so does stopping the mods. A master who leaves the room in that
  moment leaves it closed for the next master, who can still start or leave; it opens with the next contract's return.
  So nobody reaches a contract unverified. Outside a private room none of this runs, and the start goes as the game
  sent it.
- **What that rests on, unproven by the game's code:** that Photon's server handles one client's operations in order, so the
  marker returns only after the close was applied, and a join the server took before the close reaches the master ahead of the
  marker. That is Photon's documented behaviour, not the game's. That the marker comes back at all is what the game itself relies
  on for `ServerManager.LoadScene`. The smoke list checks both.
- **No kick reaches that player until the lobby.** `ServerManager.KickPlayer` needs the player's ID card and sends through the
  `ServerManager`'s view, and `ServerManager.KickPlayerNetworked` runs on that view on the player's client.
  - The joiner's client stops its message queue at `LoadScene`, and the server gives it `LoadScene` before anything the master
    sends after hearing of the join. The queue runs again in the level, and the contract's scene load replaces the menu's scene,
    `ServerManager` included, so the kick finds no view there.
  - The master stops its own queue at `LoadScene`, and `PhotonHandler.Dispatch` delivers nothing while it is stopped, so a join
    that arrives after that is not heard until the master's level is up, where it has no `ServerManager` to kick with.
  - A join heard just before that gets no card: `ServerManager.AssignNewPlayerSpot` stops when it finds the scene changing.
  - PUN's own way is no help. This build has no `PhotonNetwork` method to close a connection, and `PhotonNetwork`'s event
    handler has no case for that event.

## What must not reach a player who may not have the mods

A modded host's mods stay on while a player is being verified. So anything sent to the whole room in the lobby must not
carry a custom value until everyone present is verified. The host holds these back (host/lobby_gate.h,
host/content_difficulty.h):

| What | How it is sent | While a player is unverified |
| --- | --- | --- |
| The lobby's difficulty (`SyncDifficultyValues`, from `ApplyDifficulty`, or `ForceSyncDifficulty`) | `RpcTarget.All`, through `ES3UserType_Difficulty.WriteScriptableObject` | Writes Random; the host's next frame gives its own copies their pick back; the pick goes out once everyone is verified |
| The difficulty sent to a joiner | That player alone, through the same writer | Writes Random; the same later send carries the pick |
| A journal's custom page (the page sync, sent by `JournalController.GoToPage`, `NextPage` and `PreviousPage`, judged by its name and page) | `AllBuffered` for a game page | Runs on this client alone. Once everyone is verified it goes to everyone, but **never buffered**, at any time, so a later joiner replays no custom number |
| A journal's custom guess or evidence mark (the typed sync's `ToEveryone_NotifyOfButtonStateChange_Rpc`) | `RpcTarget.All` | Runs on this client alone; other players' marks catch up on the next press |
| A mod's channel messages | The host contract's `send` | Go only to players whose announced list equals the host's, and come only from them. A client whose own list does not match sends and hears nothing |

Not held, since they are the loader's own and hold no game value: presence and content manifests on the host channel, which
are how a player gets verified. A client without the loader ignores the event. A contract's own custom values
(`GhostInfo.SyncValues`, the acting-as messages) are sent only in a contract, which nobody unverified reaches, since the start
closes the room first (above). As a second line, a master that still sees an unverified player as a contract loads spawns no
custom ghost in it.

Buffered RPCs in the lobby that carry no custom value, and so reach a joiner as in the game: `ServerManager.NetworkedReady`,
`ServerManager.LoadScene`, `ServerManager.UpdatePlayerLevelInformation`.

## PhotonViews and the cleanup

- `PhotonNetwork.LeftRoomCleanup` runs in three places: as a client leaves a room, before its peer disconnects; from
  `PhotonNetwork.OnClientStateChanged`; and from `PhotonNetwork.set_OfflineMode`. It reads the GameObject of every view
  PUN holds registered from a network instantiation, and destroys those objects.
- **A registered view whose object Unity destroyed without its `OnDestroy` makes the cleanup throw.** On a disconnect
  the peer then never disconnects, and Photon's receive thread keeps the socket open. An object that was never
  activated has no `OnDestroy`. So a mod that keeps or copies a networked object, and destroys it itself, first removes
  its view with `PhotonNetwork.LocalCleanPhotonView`.

## Raising an event

- `PhotonNetwork.RaiseEvent` takes a code, a content, `RaiseEventOptions` and `SendOptions`. It refuses, sending nothing
  and answering false, a code above 199 or a client not joined to a room. In offline mode it sends nothing and raises the
  event on this client only when `RaiseEventOptions.Receivers` is not `ReceiverGroup.Others`.
- `RaiseEventOptions.TargetActors`, when set, names who it goes to, and the group is not read.
- Every event a client receives comes through `LoadBalancingClient.OnEvent` first, with `EventData.Code`,
  `EventData.get_Sender` and `EventData.get_CustomData`. The suite's own messages use it (a mod sends with the host
  contract's `send`, never by raising an event itself).

## The game's checks on an RPC

Most of the game's RPC bodies start by checking who sent them, and some also check the game's state.

| Check | Lets through | Examples |
| --- | --- | --- |
| The room's first actor | The lowest actor number in `PhotonNetwork.get_PlayerList`, or everyone outside a room or offline. A null list, entry or sender throws. | `ExitLevel.Exit`, `GameController.Exit`, `LiftButton.NetworkedUse`, `GhostAI.Hunting`, `Door.HuntingCloseDoorNetworked`, `Door.SyncLockState` when it locks ([doors](doors.md)), `PlayerStats.SyncStatsNetworked`, `ServerManager.KickPlayerNetworked` |
| The object's owner | The sender equals a given player, usually the view's owner. | `Player.CompleteDeathSequenceNetworked`, `TarotCards.ChangeState`, `MonkeyPaw.MakeWishNetworked` |
| The game's state | Depends on the RPC. | `Difficulty.friendlyGhost` refuses `GhostAI.Hunting`, `Door`'s hunt close and every kill. A kill of a player already dead is refused. `TarotCards.ChangeState` and `VoodooDoll`'s RPCs are refused unless a flag on the item is set. Lightning is refused unless the weather is a storm. |

**A refusal is never harmless, and it can harm an account.** Every refusal is to be avoided.

**Offline and outside a room, only the sender checks pass.** The game's state checks still run, and a refusal
still raises the report. The live log of 2026-11-12 shows it: in a single player contract, `Player.RevivePlayer`
on a dead investigator in a contract without the Monkey Paw or the tarot cards was refused by the `Revive` RPC's
body, the report was raised for `Revive`, and the game faulted after it ([players](players.md), "The `Revive`
RPC's own checks"). So a mod's single player sandbox is no excuse to reach an RPC in a state the game never
would.

### What a mod must not do

- **Send by calling the game's own method, on the client the game sends it from.** The method holds the guard the
  body refuses without. `GhostAI.ChangeState` turns a hunt into idle on a friendly ghost's difficulty before anything
  is sent, and `Door.HuntingCloseDoor` sends nothing there.
- **Never invoke an RPC's body, and never send an RPC by its name.** Those skip the guard.
- **Gate ghost and world RPCs on `PhotonNetwork.get_IsMasterClient`,** as the game does. The first-actor check and
  the master agree as long as the master is the lowest actor. Photon's documentation says a new master is the lowest
  active actor; that step is Photon's server and is not proven from the game's code.
- **A kick needs the first actor, not only the master.** `ServerManager.KickPlayer` itself asks only that this client
  is the master, while `ServerManager.KickPlayerNetworked` on the removed player's client asks that the sender is the
  first actor. Call it only when both hold.
- **A journal's page sync is its owner's.** Check `PhotonView.get_IsMine` before `JournalController.GoToPage` on a
  book you did not press.
- **Do not change a difficulty's `friendlyGhost` or the weather behind the game's back.** A guard read earlier can
  then disagree with the body's check.
- **A mod's random-number hook stays inside the asked range.** `GhostAI.Hunting` refuses an int argument above 3,
  and what the game passes there was not traced.

Nothing in the SDK maps, calls or alters the report itself (`tests/map_test.cpp`).
