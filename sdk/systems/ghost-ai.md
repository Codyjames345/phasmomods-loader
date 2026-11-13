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

### Each type's hunt line

The hunt branch needs the check's insanity (step 1) to be at least 50, and draws 1 in 5 from 50 and 1 in 3 from 75.
`GhostIdleState.TypeHuntRules` adds the type's offset to it first, so the line in sanity is 50 plus that offset: a
ghost may start an idle hunt at that sanity or below. The offsets, read from its decompile (the map note lists the
exact conditions):

| Type | Hunt line (sanity) |
| --- | --- |
| Mare (6) | 40 while a light in its room is on, 60 otherwise |
| Shade (8) | 35 |
| Demon (9) | 70 |
| Yokai (12) | 80 while players talk in its room, 50 otherwise |
| Onryo (16) | 40 while a lit flame is near it on its floor, 60 otherwise |
| Raiju (18) | 65 while electronics are on near it, 50 otherwise |
| Deogen (22) | 40 |
| Thaye (23) | below `GhostController.thayeThreshold` (75 down 6 a step as it ages) |
| Gallu (25) | 60 enraged, 40 weakened, 50 otherwise |
| Dayan (26) | 65 in one state, 45 in the other, 50 before either |
| Obambo (27) | 65 aggressive, 10 calm |
| Kormos (28) | 70 while a condition on a player in its room holds, 50 otherwise |
| every other type | 50 |

These are the idle check's lines only. Hunts started another way skip them: the Onryo's three blown-out flames
(step 2), and whatever a type's ability, an event or a cursed possession starts. To read the line the game would use
now, whatever the ghost is doing, make an idle state of your own (`GhostIdleState.type.make()` and
`GhostIdleState.ctor` with the ghost), give it the three scene controllers the state's enter copies
(`GhostIdleState.levelController`, `.gameController`, `.soundController`, from each class's `instance`), run
`GhostIdleState.HuntInsanity` on it and compare `GhostIdleState.insanity` with the insanity it started from, as the
Dev Menu does. The ghost's running state is never touched.

The hunt's start then goes through `GhostAI.ChangeState`, which stores idle instead while a flag of
LevelValues is set (see "How it fits together"). `GhostAI.StopGhostFromHunting` holds hunts off for a
while: its callers are the incense's smudge and a Monkey Paw wish. While the hold is on
(`GhostAI.huntsHeldOff`) the idle state starts no hunt, the Onryo's included. The hold's length depends on the
behaviour type: one for the Spirit, another for the Demon, a third for the rest.

## Hunt speed

A hunt is a state object, `GhostHuntState` (`sdk/map/GhostHuntState.h`). On the master its tick sets
the speed in two steps:

- **Every frame,** `GhostHuntState.UpdateSpeed` moves `sightSpeedUp`: up 0.05 a second (0.075 for one type) while
  the hunt has a `target` (the player its sight last found), down 0.01 a second only once it has neither a `target`
  nor a `searchTarget`, and unchanged in between; it is kept between 1 and 1.65. So from 1 at a hunt's start it
  takes about 13 seconds of chase to reach its top. It then sets the NavMeshAgent's speed, with branches by type.
  The branch every type without one of its own takes ("no type", so a custom ghost, among them) is `baseSpeed`
  times `sightSpeedUp`, plus an offset only the Twins' hunt sets (0.2 or -0.2 at random as it begins; 0 for every
  other type). Every type but one then multiplies that by the multipliers they share: `Difficulty.ghostSpeed`,
  1.15 when the difficulty's actual weather is the Blood Moon or `GhostAI.speedFlag` is set, and the ghost's three
  factors `GhostAI.huntSpeedFactor1` to `3`, which `GhostAI.Awake` sets to 1 and nothing in the game changes.
  While the ghost is smudged (`GhostAI.smudge`) it uses 1 in place of `sightSpeedUp`. It reads the field afresh
  every frame, so a value a mod writes there counts from the next frame (decompiled 2026-11-12, the constants and
  factors 2026-11-13).
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

## Every route to a state change

Read from the direct callers of `GhostAI.ChangeState`, `GhostAI.Hunting`, `GhostActivity.TryInteract` and
`GhostActivity.NormalInteraction` in build 25811160, and from the decompiled named callers. Most callers of
`ChangeState` are `GhostActivity` and classes with obfuscated names, the ghost's states among them; the rest are these (`ChangeState` itself acts only while a flag of the ghost's PhotonView is set, above):

| Caller | Change |
| --- | --- |
| `HauntedMirror.BreakItem`, `OuijaBoard.BreakBoard` | a hunt, forced, while the ghost is not hunting |
| `MusicBox.BreakMusicBox`, `VoodooDoll.StartHunt` | a hunt, forced, after `DifficultySettings.AddHuntDuration(20)` (20 added to the hunt's duration, once a contract) |
| `TarotCards.ChangeState` | the card's state, forced: a hunt (after `DifficultySettings.AddHuntDuration(20)`), an event or the favourite room; only those three, and only while the ghost is not hunting |
| `MusicBox.EnterMusicBoxState` | `GhostState.musicBox`, with the box as the change's object, not forced |
| `SummoningCircle`, through methods with obfuscated names | `GhostState.summoningCircle`, not forced, and a forced hunt; look-alikes among them pass numbers no state has |
| `MonkeyPaw`, handlers with obfuscated names and a wish's coroutine (`MonkeyPaw.MakeWishNetworked`) | a forced hunt (one handler after `DifficultySettings.AddHuntDuration(20)`, while the ghost is not hunting), the favourite room, an interaction, or the summoning circle's pull; which handler is which wish was not traced |
| `Jackalope.TriggerHunt` (a seasonal item) | a hunt, not forced, after clearing `GhostAI.postHuntCooldown` and setting another bool of the GhostAI |
| `PhraseListenerController.NetworkedReaction` | the fuse box, not forced (a phrase a player says) |
| `SaltSpot.SyncSalt` | `GhostState.salt` (the ghost stepped in salt) |
| a coroutine of `GameController`; `GhostAI.EventPick` and another method of `GhostAI`; a `GhostAI` coroutine | the favourite room; idling; a hunt, forced as its caller says |

Every forced hunt above from a cursed possession (the mirror, the board, the box, the doll, the Death card and the paw's
hunt) sets the hunt's grace period, a field of `DifficultySettings`, to 1 just before the change and puts it back with
`DifficultySettings.ResetHuntGracePeriod` just after it; the box, the doll, the card and the paw also call
`DifficultySettings.AddHuntDuration(20)` first, which adds 20 to the hunt's duration once a contract, on every client by
RPC. Neither waits on the change, so both run whether it is refused or not.

`GhostAI.Hunting`, the RPC that starts a hunt on every client, has no direct caller: it is sent by name, so a hunt
reaches it only through a state the change made. A voodoo doll's pin (`VoodooDoll.NetworkedUse`, through
`VoodooDollPin`'s handlers) jumps straight into `GhostActivity.NormalInteraction` with its force flag, not through
`TryInteract`; `NormalInteraction`'s other callers are `TryInteract` and `GhostActivity`'s own helpers, which the idle
state calls. Each of the 173 direct callers of `GhostAI.Appear` lies in a class with an obfuscated name; which of them
are state objects was not traced.

So a ghost that must start no interaction, event or hunt, whatever asks, refuses at `GhostAI.ChangeState` before the
game's code runs, forced or not, and at `GhostActivity.TryInteract` and `GhostActivity.NormalInteraction`. A refusal
there returns to the caller, whose own code goes on: what the item does to a player (sanity, the card's own effect, the
box's song, the board's breaking) stays the game's, as none of that waits on the change. Refusing before the game's
change also means a forced hunt on a friendly ghost's difficulty never reaches the game's own turn of it into idling.
Custom Ghosts' Squatter does this (`mods/custom_ghosts/restraint.cpp`, `squatter_rule.h` `keeps_quiet`): it lets only
idling, wandering and the favourite room through, and only idling while the mod steers its agent ("Moving the ghost
outside a hunt" below).

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

## Moving the ghost outside a hunt

Read from decompiled code and the scene files of build 25811160. A mod that moves the ghost itself, a chase that must
not read as a hunt for one, has two routes: a hunt with its effects taken away, or a quiet state with the mod at the
wheel. The second is the clean one.

**What a hunt turns on.** The hunt state's start, on the master, sends `GhostAI.Hunting` to every client by name, then
calls `LevelController.LockHuntingDoors` ([doors](doors.md), "How a hunt locks the exits"),
`GhostInteraction.CreateAppearedEMF` at the ghost's position (an EMF reading where it appears) and
`GhostActivity.UpdateElectronics`. On each client `Hunting`'s start sets `GhostAI.isHunting`, plays the hunt's sound
through the ghost's GhostAudio noise, counts the hunt in LevelStats and invokes one of GameController's events; its end
stops the sound, hides the model and invokes another. While `isHunting` is set, `GhostAI.Update` makes the model blink
on a timer and the Kormos's hearing pulses. On a friendly ghost's difficulty `Hunting` refuses with a report on the
sender. A hunt without those effects would need a hook on each of them whose only job is to undo the mod's own hunt.
Which code flickers the lights and upsets the electronics in a hunt was not traced; every path to a hunt effect found
starts at the hunt's start or reads `isHunting`, and the quiet route reaches neither.

**The quiet route.** The ghost stays in `GhostState.idle` and the mod drives `GhostAI.agent` on the master:

- `GhostAI.Update` runs the state machine there every frame and once a second records where the agent has moved;
  nothing else in it touches the agent outside a hunt.
- The idle state holds no agent (its constructor takes only the GhostAI). Its entry sets the agent's speed to
  `GhostAI.baseSpeed`; no idle method sets a destination, stops, resets or warps the agent. Its roll asks
  `GhostAI.ChangeState` for wandering, the favourite room, the fuse box, events, abilities and hunts, and
  `GhostActivity.TryInteract` for interactions, so a mod refuses there whatever it does not want.
- Wandering (`GhostWanderState`) sets destinations and resets paths, and the favourite room's state sets the
  destination, so either would take the agent back: a mod that steers keeps the ghost out of both while it steers.
- Only the summoning circle's and the salt's state stop an agent (`isStopped`), besides the game's player-shaped
  event's own agent.
- Outside the states the ghost's agent is changed by `GhostAI.Awake` (speed 1.7), the incense's coroutine (one
  destination, where the ghost stands), `GhostController.WarpGhostToFavouriteRoom` (the truck's first use) and the
  ghost's own teleports. `GameController.ResetHuntEffects` sets an agent's speed at a hunt's end, and Monkey Paw wishes
  and the summoning circle warp one; whose agent those are was not traced.

The ghost's object carries a PhotonTransformView, so where the master's agent takes it shows on every client. Its agent
(the scene's settings) has acceleration 5 metres a second each second, angular speed 1000, stopping distance 0, radius
0.15 and height 1.9, with auto-braking and off-mesh link traversal off. A mod steers it with
`NavMeshAgent.SetDestination`, `set_speed` and `set_acceleration`, and reads `get_remainingDistance`, `CalculatePath`
and `get_velocity`; `NavMeshAgent::steer` (above) turns it for a frame instead.

**Doors on the way.** No door leaf carries a NavMeshObstacle (read on Grafton, 64 doors), and the Ghost layer does not
collide with the Door layer in the game's physics settings. So the agent's path runs through a doorway whatever the
door's state, the ghost passes through a shut leaf, and the leaf does not move. A player opens a door by grabbing it on
their own client (`DragRigidbodyUse.FixedUpdate`, [doors](doors.md), "Grabbing a door open"); a ghost moves one only by
its force (`Door.GhostForceDoor`, `Door.CheckDoorDuringHunt`), which is a ghost interaction with a ghost's traces: an
EMF reading, the door's ghost sound, a count in the results and, for a ghost with Ultraviolet, fingerprints
([doors](doors.md), "A ghost's force"). A mod-driven ghost can instead swing a shut door with its own physics,
`HingeDoor.OpenDoor(false, -1, false)`, which leaves no trace, once the master owns the
door's view ([doors](doors.md), "Opening a door quietly"). `LevelController.currentGhostRoom` names the room the ghost is in on every client, and
`LevelRoom.doors` that room's doors.

**Sight.** `GhostAI.vision` detects nobody outside hunts. Custom Ghosts' Lithobolia sees through copies of the vision's
two sensors that it pulses itself (`mods/custom_ghosts/sensor_copy.h`, "The ghost's sensors and throws" above): a player
is seen when the copy detects any of their `Player.losTargets`.

**Hearing.** The hunt hears by voice and by electronics (`GhostHuntState.Listen`, through `Hears` and `SensesGear`).
The voice rule (`GhostHuntState.Hears`): the player's body within 9 m of the ghost's head, 2.5 m for the Yokai, any
distance and floor for a player a Monkey Paw wish marks (`Player.wished`), always for the Deogen; on the floor of the
ghost's room otherwise; not outdoors; and `PlayerAudio.IsTalkingLoudly(0.1)` of `Player.audio`. A mod mirrors it with
those members. `IsTalkingLoudly` clears the loudness it answers true for, and its only other asker is the idle state's
check for talking in a room. Footsteps are heard only by the Kormos's hunt: `GhostHearing` pulses only while the ghost
hunts.

**Hiding places.** A closet's trigger keeps `ClosetZone.occupants` and each occupant's `Player.closet` on each client
its physics runs on, and the hunt, on the master, reads `Player.closet`. The hunt's closet search learns a closet only
from the player it goes for: `GhostHuntState.SetTarget` (a player its sight found) and `GhostHuntState.Listen` (one it
heard or sensed) give `SetCloset` that player's closet. It then goes to the closet (`ClosetZone.navSpot`) and runs
`ClosetZone.CheckCloset`, which moves the closet's doors as a hunting ghost does, and kills whoever hides there.
`ClosetZone.standOutsideEntrancePoint` is where a ghost stands outside the entrance without going in, and
`ClosetZone.leftClosetDoor` and `rightClosetDoor` are its doors. A `HidingSpot` (under a bed, behind a curtain) has no
part in the search: `SetCloset` and `Player.closet` take a ClosetZone only.

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
  own switches (`GhostController.AttemptGhostTypeStateChange`), where its AI copies at random. The player is never
  offered, and never re-picks, the type it is copying now, while the AI may keep copying it: its random pick is an
  element of the whole `GhostController.mimicCopies` list with nothing skipped (`GhostController.MimicCopy`). The
  user allowed the picking (Playable Ghost decision E2) and the leaving-out (2026-11-01).
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

The weights come from `GhostCollection.GetDefaultWeightedGhostList`, which gives every GhostData in the contract's collection a weight of 1 (custom ghosts the host put there included). The spawn then takes the Deogen out on one condition and two other types out in one game mode (neither traced further), and lays a dictionary of overrides from elsewhere over the weights of the types it names, before `PickType` draws. `PickType` adds up the positive weights only, so a type of weight 0 is never drawn while another type has weight. After the roll a Custom difficulty's ghost type replaces the rolled one unless it is "no type".

A mod that holds the setup back runs the spawn inside a `try`, and from its hook of `PickType` asks `phasmo::GhostController::abandon_spawn`: the host unwinds the spawn before it writes or sends anything, and `pm::api::dictionary_entries` reads the weights. `pm::api::inside<phasmo::GhostController::Spawn>` says a roll is the spawn's.

The favourite room moves by `GhostController.ChangeFavouriteRoomTo`. The game's own rules move it from `GhostWanderState.Exit` only (`pm::api::inside<phasmo::GhostWanderState::Exit>`). The other direct callers are the Monkey Paw's, by coroutine. The truck's first use (`LiftButton.NetworkedUse`, by RPC) calls `GhostController.WarpGhostToFavouriteRoom`, which until then leaves the ghost at the level's origin.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/ghost-ai.md).
