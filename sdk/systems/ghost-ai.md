# Ghost AI

The ghost's brain is the `GhostAI` component, one per contract, a MonoBehaviourPun in Assembly-CSharp
(global namespace). It holds the ghost's state machine, its NavMeshAgent and the hunt flags. The
entries below are in `sdk/map/GhostAI.h`; their `///` notes are the authority, and this page ties them
together. Facts were read from decompiled code of Steam build 25811160 unless a note says otherwise.
This page lists no addresses or offsets. `tools/gamecheck.ps1` prints the value of every
entry for the build in hand.

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

## Hunt speed

The hunt is one of the state objects (`sdk/map/GhostHuntState.h`), a class whose name and members are
all obfuscated, found by its field types (a GhostAI, a NavMeshAgent, a ClosetZone, the ghost's type and
two lists of players). Each frame of a hunt on the master the hunt's tick calls `UpdateSpeed`, which
moves `sightSpeedUp` (0.05 a second up while the ghost sees its prey, down while it has none, kept
between 1 and 1.65) and then sets the NavMeshAgent's speed from `baseSpeed` times `sightSpeedUp` times
the multipliers every type shares (the difficulty's ghost speed, Blood Moon), with branches by type.
Every two seconds the hunt's tick calls `RefreshBase`, which writes `baseSpeed`: from the players' average
insanity for the Moroi, from a field of its GhostAI for the Deogen, and from GhostAI's base speed (1.7,
set in `Awake`) for every other type, "no type" among them.

| Key | Kind | Found by |
| --- | --- | --- |
| `GhostHuntState` | type | shape: `fieldTypes` |
| `GhostHuntState.ghost` | field, `@GhostAI` | shape |
| `GhostHuntState.RefreshBase` | method, `void()` | shape, `Calls GameController.GetAveragePlayerInsanity`, `Called` |
| `GhostHuntState.UpdateSpeed` | method, `void()` | shape, `Calls GhostAI.DayanPlayerChecks`, `Called` |
| `GhostHuntState.baseSpeed` | field, float | shape, `Stores RefreshBase` |
| `GhostHuntState.sightSpeedUp` | field, float | shape, `Stores UpdateSpeed` |
| `GameController.GetAveragePlayerInsanity`, `GhostAI.DayanPlayerChecks` | methods | name |

The obfuscator left uncalled decoys of both methods: near-copies that store other constants (the
refresh's decoy writes bases in the hundreds) and that nothing calls. On build 25811160 two `void()`
methods call `GetAveragePlayerInsanity` and four call `DayanPlayerChecks`; only one of each is called
by the class's other methods, so each method entry adds the `Called` fact (`sdk/README.md`). Without it
both are `AMBIGUOUS`; without the reader's epilogue rule (a register restored from the stack in an
epilogue still holds `this`) `sightSpeedUp` is `AMBIGUOUS`, since `UpdateSpeed` writes it through `rbx`
and restores `rbx` with a `mov` through `r11`.

Content rules set a ghost's hunt speed through these inputs, never the speed: the host runs a rule's
`huntSpeed` just before `UpdateSpeed`, with the game's own base for this hunt, and writes `baseSpeed`
and `sightSpeedUp` (`sdk/README.md`, "Hunt speed").

The hunt follows players through four Player fields. `GhostHuntState.target` is the class's one
internal one (the field's `access`); `UpdateSpeed` tests it alive first in its branches by type and
falls back on `GhostHuntState.searchTarget`, the one private Player field it reads (`Loads UpdateSpeed`,
`access` private). The hunt's tick clears both on one timer and a third on
another. `UpdateSpeed`'s other Player reads go through the GhostAI and the LevelController it
reaches through its own fields; the resolver does not count them, since neither type can be a hunt
state (`sdk/README.md`, facts). GhostAI keeps its states in a machine whose field is typed by the
states' interface, so a mod finds the running hunt as the object `UpdateSpeed` is called with (Ghost
Tells hooks it, `mods/ghost_tells/world.cpp`).

## The hunt's senses, target and kill

Custom Ghosts' behaviour rules (`mods/custom_ghosts/behaviour.cpp`, `hunt.cpp`) act on these members of
the hunt state, all in `GhostHuntState.h`. Each method has same-shape look-alikes in the class (the
obfuscator's copies), so each is found by what its own code compares or calls, and several are found
through another (the resolver orders them by their dependencies):

| Key | Kind | Found by |
| --- | --- | --- |
| `GhostHuntState.Sees` | `bool(Player, bool)`, the sight test | `Compares GhostType.Deogen` (its look-alike compares another number) |
| `GhostHuntState.Hears` | `bool(Player)`, the voice test | `Calls PlayerAudio.IsTalkingLoudly` |
| `GhostHuntState.SensesGear` | `bool(Player)`, the electronics test | `Compares GhostType.Deogen`, `Calls Object.op_Implicit` |
| `GhostHuntState.Listen` | `bool()`: sends the hunt to a player it hears or senses | `Calls Hears` (its only caller) |
| `GhostHuntState.SetCloset` | `void(ClosetZone)` | `CalledBy Listen` (six look-alikes differ only in the flags they store) |
| `GhostHuntState.SetTarget` | `void(Player)` | `Calls SetCloset` (its look-alikes call a look-alike of SetCloset) |
| `GhostHuntState.Started` | `bool()`, the gate Execute runs first | `Calls GhostAI.ChangeState`, `Calls GhostAI.Appear` |
| `GhostHuntState.Exit` | `void()`, the hunt's end | `Calls GameController.ResetHuntEffects`, `Calls SetCloset` |
| `GhostHuntState.ctor` | constructor | name |
| `lastPlayer` | private Player | `Stores Listen`, `Loads UpdateSpeed` negated |
| `seen`, `killed` | the two List<Player> | `seen`: `Loads Exit`; `killed`: `Loads Sees`, `Loads Exit` negated |
| `searchCloset` | the one ClosetZone | shape |
| `startDelay` | float | `Stores Started` |
| `killReach`, `slowKillReach` | float | `StoresConst ctor` 1.1 and 1.5 |

The SetTarget and SetCloset pair looked circular (each is singled out by the other): the voice test
breaks the circle, since its one caller, Listen, calls SetCloset and none of its look-alikes. Some
facts look elsewhere than the obvious method because the reader proves `this` only for a register that
holds it throughout a method: SetTarget reuses `rbx` for LevelStats after its stores, and the voice test
reads `LevelRoom.floor` through a Player, which a negated fact on it cannot rule out.

The may-go-for test (the player has a room, is alive, is not in `killed`,
and the room is not `RoomKind.outsideMap`, the truck) cannot be singled out: its look-alike `GhostHuntState.TargetAllows`
shares its one caller, compares with the same numbers and calls all it calls. `TargetAllows` also has a
second caller, `GhostHuntState.Listen`, whose own eligibility test it is; a negated call fact could use
that, but the resolver refuses those. Custom Ghosts rebuilds it from the
mapped fields, condition by condition (`mods/custom_ghosts/hunt.h`), and pins it in an offline test.

The idle state (`GhostIdleState.h`, found by its field types with a FrameSkipFilter) works out the
insanity its hunt check uses in `HuntInsanity` (`Compares GhostType.Thaye`, `Calls
GameController.GetAveragePlayerInsanity`; nineteen look-alikes) into `insanity` (`Stores HuntInsanity`).

## The ghost's own attempts

What the ghost does of its own goes through two entry points: `GhostActivity.TryInteract`, its
interactions, and `GhostAI.ChangeState`, its interactions, events, ability and hunts. The tarot cards
pass `forced` (the third argument) for the changes they make. The GhostState values a change may carry
are mapped by name (`GhostState.light` to `.ignite`, `.summoningCircle` and `.musicBox` for a cursed
possession's pull), so a mod that sorts the changes, as Ghost Tells' gate does (`gate_rule.h`), reads
their numbers from the running game. `GhostAI.StopGhostFromHunting` makes the coroutine
that holds the ghost's hunts off for a while; its only callers are the incense's smudge and
a Monkey Paw wish. The coroutine does nothing while a hold is on; otherwise it
sets the hold (`GhostAI.huntsHeldOff`), waits a time chosen by the behaviour type, and clears it. Only
that compiler-generated coroutine writes the hold, and such classes are never shape candidates, so the
entry is found through its readers: FireSource's count of the Onryo's blow-outs reads it beside
isHunting (a cross-type fact), and ChangeState reads isHunting but never the hold (a negated one).

## Every route to a state change

Read from the direct callers of `GhostAI.ChangeState`, `GhostAI.Hunting`, `GhostActivity.TryInteract` and
`GhostActivity.NormalInteraction` in build 25811160, and from the decompiled named callers. Most callers of
`ChangeState` are `GhostActivity` and classes with obfuscated names, the ghost's states among them; the rest are these (`ChangeState` itself acts only while a flag of the ghost's PhotonView is set, above):

| Caller | Change |
| --- | --- |
| `HauntedMirror`'s `BreakItem`, `OuijaBoard`'s `BreakBoard`, `MusicBox`'s `BreakMusicBox`, `VoodooDoll`'s `StartHunt` | a hunt, forced |
| `TarotCards.ChangeState` | the card's state, forced: a hunt (after adding 20 seconds to the hunt's length), an event or the favourite room; only those three, and only while the ghost is not hunting |
| `MusicBox`'s `EnterMusicBoxState` | `GhostState.musicBox`, with the box as the change's object, not forced |
| `SummoningCircle`, through methods with obfuscated names | `GhostState.summoningCircle`, not forced, and a forced hunt; look-alikes among them pass numbers no state has |
| `MonkeyPaw`, handlers with obfuscated names and a wish's coroutine (`MonkeyPaw.MakeWishNetworked`) | a forced hunt, the favourite room, an interaction, or the summoning circle's pull; which handler is which wish was not traced |
| `Jackalope`'s `TriggerHunt` (a seasonal item) | a hunt, not forced |
| `PhraseListenerController.NetworkedReaction` | the fuse box, not forced (a phrase a player says) |
| `SaltSpot.SyncSalt` | `GhostState.salt` (the ghost stepped in salt) |
| a coroutine of `GameController`; `GhostAI.EventPick` and another method of `GhostAI`; a `GhostAI` coroutine | the favourite room; idling; a hunt, forced as its caller says |

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
idling, wandering and the favourite room through.

## The ghost's sensors and throws

Two SensorToolkit sensors (`Sensor.h`) find what the ghost may act on. `GhostActivity.interactionSensor`
is a RangeSensor: TryInteract sets its box (`sensorBox`) and pulses it through the virtual PulseAll,
which for a RangeSensor is a thunk to `BasePulsableSensor.Pulse`. `GhostAI.vision` is a LOSSensor that
tests its input RangeSensor's signals (`LOSSensor.get_InputSensor`) for line of sight; no game code pulses
it directly, and outside hunts it detects nobody. A sensor's settings are the public value fields its
class declares (the LOSSensor's are a dozen obfuscated bools, ints, floats and curves that only its
TestSignal reads, so no code fact tells them apart): `Type::copy_settings` copies them from the game's
metadata, which is how Custom Ghosts' `sensor_copy` sets up its copies. Its signal processors are the
game's `GhostHuntingProcessor` (by the ghost's hunting) and `PlayerProcessor` (a sight part of a player
not marked dead and not outside the map); they hold only settings.

The hunt throws on its own timer (`GhostHuntState.throwTimer`, 4 seconds from the constructor): when it
runs out the hunt update takes `GhostActivity.GetPropToThrow` (the detected Props, shuffled, the first
`GhostActivity.throwable` passes), rolls RandomRangeInt(0, 2) unless its ghost type
(`GhostHuntState.ghostType`) is the Poltergeist, and calls `Prop.TryThrow(true, true)`.

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
steers can change without a mod rebuild. They are the map's most fragile (`sdk/README.md`, native sites). A game build
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
  still turns the aimed one (the Bogeyman's closet doors through `GhostActivity.GetDoorToOpen`, the Echo's repeats
  through `GhostActivity.NormalInteraction`), and one that loses a share of its AI's loses the same share of its
  player's (`PmGhostDef.interactionLost`), which stay paid for.
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

`GhostController.Spawn` sets the contract's ghost up, on the master, once every player has loaded (`GhostController.SpawnGate`, from `GhostController.Start` and the room callbacks). It rolls the type with `GhostController.PickType` over a `Dictionary<GhostType, int>` of weights (those with a positive weight, or every key when none has), the evidence with `GhostController.PickEvidence`, and the favourite room's index among `LevelController.rooms` into `GhostController.favouriteRoomIndex`, which it reads nowhere after. It ends with the `GhostController.CreateGhost` RPC, which runs at once on this client, inside the spawn, and hands a copy of the traits, the room's index among them, to `GhostAI.Init`. `PickType` has no other caller the game runs. The room roll itself goes through a generic helper, which has no code of its own a map entry could name.

The weights come from `GhostCollection.GetDefaultWeightedGhostList`, which gives every GhostData in the contract's collection a weight of 1 (custom ghosts the host put there included). The spawn then takes the Deogen out on one condition and two other types out in one game mode (neither traced further), and lays a dictionary of overrides from elsewhere over the weights of the types it names, before `PickType` draws. `PickType` adds up the positive weights only, so a type of weight 0 is never drawn while another type has weight. After the roll a Custom difficulty's ghost type replaces the rolled one unless it is "no type". The host gives a custom ghost whose definition says it may not be rolled a weight of 0 there (`PmGhostDef.rollable`, `host/content_types.h`).

A mod that holds the setup back runs the spawn inside a `try`, and from its hook of `PickType` asks `phasmo::GhostController::abandon_spawn`: the host unwinds the spawn before it writes or sends anything, and `pm::api::dictionary_entries` reads the weights. `pm::api::inside<phasmo::GhostController::Spawn>` says a roll is the spawn's.

The favourite room moves by `GhostController.ChangeFavouriteRoomTo`. The game's own rules move it from `GhostWanderState.Exit` only (`pm::api::inside<phasmo::GhostWanderState::Exit>`). The other direct callers are the Monkey Paw's, by coroutine. The truck's first use (`LiftButton.NetworkedUse`, by RPC) calls `GhostController.WarpGhostToFavouriteRoom`, which until then leaves the ghost at the level's origin.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/ghost-ai.md).
