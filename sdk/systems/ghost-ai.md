# Ghost AI

The ghost's brain is the `GhostAI` component, one per contract, a MonoBehaviourPun in Assembly-CSharp
(global namespace). It holds the ghost's state machine, its NavMeshAgent and the hunt flags. The
entries below are in `sdk/map/GhostAI.h`; their `///` notes are the authority, and this page ties them
together. Facts were read from decompiled code of Steam build 25811160 unless a note says otherwise.
This page lists no addresses or offsets. `tools/gamecheck.ps1` prints the value of every
entry for the build in hand. What each ghost type does differently is gathered in
[the vanilla ghosts, type by type](vanilla-ghosts.md).

## How it fits together

- **State.** `ChangeState(state, interact, force)` stores the state in `state` and starts that state's
  object, but only while a flag of the ghost's PhotonView is set; otherwise it logs an error. For
  `hunting` it stores idle instead while a flag of LevelValues is set, and starts the state object only
  when `isHunting` is clear or its third argument forces it. `wander` gives the state the ghost's
  NavMeshAgent. Writing `state` directly skips all of that, so a mod changes state through
  `ChangeState`.
- **Hunt flags.** `Hunting(start, number, info)` is the RPC that starts or ends a hunt on each client.
  It stores its first argument to `isHunting`, and when that is true sets `postHuntCooldown` to 1.
  `Awake` clears both. It also sets the agent's speed to 1.7, which the ghost keeps as its base hunt
  speed.
- **Appearing.** `UnAppear()` does something only when the ghost's shown flag is set (the flag `Hunting`
  clears as it hides the model). It then sends an RPC with two arguments to every client, so calling it
  on the host ends the appearance for everyone. Ghost Tells' Shade calls
  `phasmo::GhostAI::UnAppear(ghost)` when a player stares at it.
- **Facing and wandering.** `LookAtPlayer(player)` turns the ghost's transform to the player's
  position on this client only. `TemporarilyStopWander(seconds)` restarts the coroutine that times
  wandering; Tarot's Hermit card and a smudged Yurei use it.

## How a hunt starts

The idle state (`GhostIdleState`, `sdk/map/GhostIdleState.h`) decides when the ghost hunts, on the
master. Each frame `GhostIdleState.Execute` counts its timer down. When the timer runs out:

1. It works out the insanity its hunt check uses (`GhostIdleState.HuntInsanity`, into
   `GhostIdleState.insanity`): the players' average insanity, or the insanity of the player the ghost is
   set on (`GhostAI.bansheeTarget`), plus an offset, with branches for the Thaye and one more type.
2. **The Onryo's hunt.** If the Onryo has blown out three flames (`GhostAI.flamesBlownOut` above 2),
   and none of `LevelValues.eventsBlocked`, `GhostAI.postHuntCooldown` and `GhostAI.huntsHeldOff` is set,
   it clears the count and hunts (`GhostAI.ChangeState` with `GhostState.hunting`). This hunt draws no
   random number and skips the sanity check. The count grows only for the Onryo, in
   `FireSource.CountGhostBlowOut`, which also clears a count of 3 reached while the ghost hunts or its
   hunts are held off.
3. Otherwise a 0-to-2 coin picks the hunt branch or the activity roll. The hunt branch needs the same
   three flags clear, runs the type's, sanity and setup checks (the Thaye's against
   `GhostController.thayeThreshold`) and then draws a final 1-in-3, 1-in-4 or 1-in-5 chance.

The hunt's start then goes through `GhostAI.ChangeState`, which stores idle instead while a flag of
LevelValues is set (see "How it fits together"). `GhostAI.StopGhostFromHunting` holds hunts off for a
while: its callers are the incense's smudge and a Monkey Paw wish. While the hold is on
(`GhostAI.huntsHeldOff`) the idle state starts no hunt, the Onryo's included. The hold's length depends on the
behaviour type: one for the Spirit, another for the Demon, a third for the rest.

## Hunt speed

A hunt is a state object, `GhostHuntState` (`sdk/map/GhostHuntState.h`). On the master its tick sets
the speed in two steps:

- **Every frame,** `GhostHuntState.UpdateSpeed` moves `sightSpeedUp`: up 0.05 a second while the ghost
  sees its prey, down while it has none, kept between 1 and 1.65. It then sets the NavMeshAgent's speed
  to `baseSpeed` times `sightSpeedUp` times the multipliers every type shares (the difficulty's ghost
  speed, Blood Moon), with branches by type.
- **Every two seconds,** `GhostHuntState.RefreshBase` writes `baseSpeed`: from the players' average
  insanity for the Moroi, from a field of its GhostAI for the Deogen, and from GhostAI's base speed (1.7,
  set in `Awake`) for every other type, "no type" among them.

A content rule sets a ghost's hunt speed through these inputs, never the agent's speed. The host runs
the rule's `huntSpeed` just before `UpdateSpeed`, with the game's own base for this hunt, and writes
`baseSpeed` and `sightSpeedUp` (`sdk/README.md`, "Hunt speed").

The hunt chases `GhostHuntState.target`. `UpdateSpeed` tests that the target is alive first in its
branches by type, and falls back on `GhostHuntState.searchTarget`. The hunt's tick clears both on one
timer, and a third Player field on another. GhostAI keeps its states in a machine whose field is typed
by the states' interface, so a mod finds the running hunt as the object `UpdateSpeed` is called with
(Ghost Tells hooks it, `mods/ghost_tells/world.cpp`).

## The hunt's senses, target and kill

These members of the hunt state are in `GhostHuntState.h`, and their notes give the details. Custom
Ghosts' behaviour rules act on them (`mods/custom_ghosts/behaviour.cpp`, `hunt.cpp`).

| Key | What it does |
| --- | --- |
| `GhostHuntState.Sees` | the sight test, `bool(Player, bool)`: whether the ghost's vision sensor sees the player; the Deogen sees every player |
| `GhostHuntState.Hears` | the voice test, `bool(Player)`: whether a player within hearing talks loudly |
| `GhostHuntState.SensesGear` | the electronics test, `bool(Player)`: whether a player within range carries equipment that is on |
| `GhostHuntState.Listen` | `bool()`: sends the hunt to a player the voice or electronics test finds |
| `GhostHuntState.SetTarget` | `void(Player)`: makes the player the target, `searchTarget` and `lastPlayer` |
| `GhostHuntState.SetCloset` | `void(ClosetZone)`: sets the closet the hunt searches (`searchCloset`, null for none) |
| `GhostHuntState.Started` | `bool()`: the gate the hunt runs first each frame; it counts `startDelay` down and ends the hunt when its time is up |
| `GhostHuntState.Kill` | the kill: never while the ghost's smudge is alive; within `killReach` (1.1 m) at once, or `slowKillReach` (1.5 m) after a second; the Deogen also needs sight |
| `GhostHuntState.Exit` | `void()`: the hunt's end, which resets the hunt's effects and sends the ghost back to its room |
| `GhostHuntState.ctor` | the constructor, which sets the hunt's starting values |
| `lastPlayer` | the player the hunt last went for; the hunt's wander stays near where they were |
| `seen`, `killed` | the players the hunt has seen, and those it has killed |

The may-go-for test (the player has a room, is alive, is not in `killed`, and the room is not
`RoomKind.outsideMap`, the truck) has no entry, because it cannot be told apart from a look-alike.
Custom Ghosts rebuilds it from the mapped fields, condition by condition (`mods/custom_ghosts/hunt.h`),
and pins it in an offline test.

## The ghost's own attempts

What the ghost does of its own goes through two entry points: `GhostActivity.TryInteract`, its
interactions, and `GhostAI.ChangeState`, its interactions, events, ability and hunts. The tarot cards
pass `forced` (the third argument) for the changes they make. The GhostState values a change may carry
are mapped by name: `GhostState.light` to `.ignite`, and `.summoningCircle` and `.musicBox` for a cursed
possession's pull. So a mod that sorts the changes reads their numbers from the running game, as Ghost
Tells' gate does (`gate_rule.h`).

## The ghost's sensors and throws

Two SensorToolkit sensors (`Sensor.h`) find what the ghost may act on:

- `GhostActivity.interactionSensor` is a RangeSensor. TryInteract sets its box (`sensorBox`) and pulses
  it through the virtual PulseAll, which for a RangeSensor is a thunk to `BasePulsableSensor.Pulse`.
- `GhostAI.vision` is a LOSSensor. It tests its input RangeSensor's signals (`LOSSensor.get_InputSensor`)
  for line of sight. No game code pulses it directly, and outside hunts it detects nobody.

A sensor's settings are the public value fields its class declares. `Type::copy_settings` copies them
from the game's metadata, which is how Custom Ghosts' `sensor_copy` sets up its copies. The sensors'
signal processors are the game's `GhostHuntingProcessor` (by the ghost's hunting; the Raiju's signals
may always pass) and `PlayerProcessor` (a sight part of a player not marked dead and not outside the
map). They hold only settings.

The hunt throws on its own timer, `GhostHuntState.throwTimer`, 4 seconds from the constructor. When it
runs out, the hunt update:

1. takes `GhostActivity.GetPropToThrow`: the detected Props, shuffled, and the first that
   `GhostActivity.throwable` passes;
2. rolls RandomRangeInt(0, 2), unless its ghost type (`GhostHuntState.ghostType`) is the Poltergeist;
3. calls `Prop.TryThrow(true, true)`.

GhostAI holds two GhostModel arrays, the forms InitGhostModel picks between by its bool. Every GhostAI
method that reads one reads the other, so neither has an entry: `Type::fields_of` reads both in layout
order from the game's metadata.

## Steering an agent

A mod walks a NavMeshAgent with `phasmo::NavMeshAgent::steer(agent, dx, dz)`, and asks `steer.available()` in
`pmmod_start`. The direction stands for one frame on this client, and only the direction: the agent's destination,
speed, acceleration, braking and avoidance stay the game's. An object that is not a live NavMeshAgent changes nothing.

The host does the rest (`host/steering.cpp`, `host/steer_stub.asm`). It makes the effect ready the first time anything
asks, patching `UnityPlayer.SteerSite`, so a game with no mod that steers keeps its own code there. It checks that the
object is a live `NavMeshAgent`, then finds the agent's place in the crowd through `Object.m_CachedPtr` and
`UnityPlayer.NavMeshAgent.GetInternalAgent`. The patch is a site patch like any other: its bytes go back when the mod
that made it fails to start, when the mods restart, and at shutdown. With nobody steering, the patched code does what
the game's did and changes nothing. The host drops every ask before the mods tick, so a mod that stops asking, or
stops, leaves the game's own direction from the next crowd update on. A patch the host refuses is tried again only
when the mods start afresh.

These entries are the host's own (`pm::map::kSteeringEntries`), and no mod names or requires them, so how the host
steers can change without a mod rebuild. They are the map's most fragile ([mapping the game](../internals/mapping.md), "native site"). A game build
that changes the code they pin leaves them unresolved: the host logs which at startup, the offline check prints them
on its `HOST steering` line, steering reports unavailable, and a mod that needs it stays off.

## A player and its AI: the rules and the exceptions

A ghost's player-controlled and AI-controlled versions follow the same rules. Custom Ghosts' hooks take no input
for who steers (`mods/custom_ghosts/restraint.cpp`), and `tests/restraint_test.cpp` and `tests/hunt_rules_test.cpp`
run each case both ways, as `tests/brownie_test.cpp` does for the Brownie's footsteps (the host works out who looks
at the ghost it runs, whoever steers it). Where a player takes an attempt off the idle roll, a rule asks `content_roll_taken` rather than
assuming an order of hooks: the attempt comes later, when they make it.

The user allowed these differences, and no others (2026-10-31):

- **Hearing:** the Yokai player hears any volume within 2.5 m.
- **The timing of abilities:** the Echo's Speak, the Kikimora's Tidy away and the Gremlin's Short out are used on
  demand, where its AI uses them on timers or, for the Gremlin, inside `GhostActivity.TryInteract`.
- **Aiming and charges:** interactions are aimed and paid for, not rolled and picked. A rule that turns an interaction
  still turns the aimed one: the Bogeyman's closet doors through `GhostActivity.GetDoorToOpen`, and the Echo's repeats
  through `GhostActivity.NormalInteraction`. A rule that loses a share of its AI's interactions loses the same share of
  its player's (`PmGhostDef.interactionLost`), and those stay paid for.
- **Free steering in hunts:** no automatic walking, so the Bogeyman's route to the next hiding place and the Goryo's
  head for its room with no prey are the player's own. The Black Shuck's corridor stays.
- **The Mimic's copy:** the Mimic player picks what it copies next, and the pick takes effect at each of the game's
  own switches (`GhostController.AttemptGhostTypeStateChange`), where its AI copies at random. This is the user's
  original Playable Ghost decision E2.
- **Slots of the player list (provisional, pending the user's first in-game test, 2026-11-01):** with a ghost player out
  of `Network.playerSpots`, investigators after them move down one place, so their marks, votes, sanity bar and pause
  card photo read another slot's. With an AI ghost no position moves in a contract. See
  [players](../internals/players.md), "The shift while a spot is out".

## Winter's Jest and the ghost's speed

`WintersJestManager.RPC_MakeKrampusStronger` sets `GhostAI.speedFlag` on the client that runs it, after acting on
`Network.localPlayer`'s sanity, unless the weather (`Difficulty.actualWeather`) is the blood moon or the flag is already set.
A client with no `Network.localPlayer` throws at that step, before the flag. Playable Ghost's parked body keeps that reference
set, so its client acts on the parked body's sanity and sets the flag as any client does.

## The game's player-shaped event (its doppelganger)

The game has an event in which something walks to a player, `GhostEventPlayer`, held by `GhostController`. It has a
NavMeshAgent, a noise, particles, a photo evidence and a target player; spawning it warps the agent to a position and starts
it on every client by RPC, and an obfuscated state class calls it. Its prefab is only a particle system, an agent and the
evidence, with no model, so it is a particle cloud that walks to a player, not a figure in their form. Community notes say
the game never uses it, which was not checked in a run. None of it is mapped yet. Playable Ghost's disguise draws an
investigator's body itself instead (`mods/playable_ghost/form.h`).

## The ghost's setup and its favourite room

`GhostController.Spawn` sets the contract's ghost up, on the master, once every player has loaded
(`GhostController.SpawnGate`, from `GhostController.Start` and the room callbacks). It rolls three things:

1. the type, with `GhostController.PickType`, over a `Dictionary<GhostType, int>` of weights: those with a
   positive weight, or every key when none has;
2. the evidence, with `GhostController.PickEvidence`;
3. the favourite room's index among `LevelController.rooms`, into `GhostController.favouriteRoomIndex`, which it
   reads nowhere after. The room roll goes through a generic helper, which has no code of its own a map entry
   could name.

It ends with the `GhostController.CreateGhost` RPC. That runs at once on this client, inside the spawn, and hands
a copy of the traits, the room's index among them, to `GhostAI.Init`. `PickType` has no other caller the game
runs.

A mod that holds the setup back runs the spawn inside a `try`, and from its hook of `PickType` asks `phasmo::GhostController::abandon_spawn`: the host unwinds the spawn before it writes or sends anything, and `pm::api::dictionary_entries` reads the weights. `pm::api::inside<phasmo::GhostController::Spawn>` says a roll is the spawn's.

The favourite room moves by `GhostController.ChangeFavouriteRoomTo`. The game's own rules move it from `GhostWanderState.Exit` only (`pm::api::inside<phasmo::GhostWanderState::Exit>`). The other direct callers are the Monkey Paw's, by coroutine. The truck's first use (`LiftButton.NetworkedUse`, by RPC) calls `GhostController.WarpGhostToFavouriteRoom`, which until then leaves the ghost at the level's origin.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/ghost-ai.md).
