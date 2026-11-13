# The vanilla ghosts, type by type

What each of the game's ghost types does differently, as far as the map covers it, with the entries that hold each rule.
Use this page to answer "which ghost does X?" or "what is special about the Y?". Then read the entry's `///` note in
`sdk/map/` for the details: the notes are the authority, and this page only gathers them. Facts were read from decompiled
code of Steam build 25811160 unless marked **inferred** (read from a name, not from code).

The number after each name is its `GhostType` value in that build. The game's code compares a ghost's **behaviour type**
with these numbers, so some notes and decompiles give the number alone. The running game reads the values from the enum
([Ghost types and the Mimic](ghost-types.md)), so a mod never writes a number into its code.

No type-specific rule is mapped yet for the Wraith (1), Phantom (2), Jinn (5), Revenant (7), Yokai (12) and Aswang (29).
A rule missing from this page is not mapped, which does not mean the type has none. A rule a mod needs that is missing
here is an SDK gap: reverse engineer it and add it, as `sdk/internals/reverse-engineering.md` describes.

## Rules many types share

- **Forced last evidence:** the Hantu, Goryo, Obake, Moroi and Deogen always keep their third evidence when a difficulty
  removes some (`GhostController.PickEvidence`, `GhostData.evidence3`; [Evidence](evidence.md)).
- **Hunt speed:** every type but the Moroi and the Deogen hunts from the base 1.7 m/s (`GhostAI.baseSpeed`,
  `GhostHuntState.RefreshBase`; [Ghost AI](ghost-ai.md), "Hunt speed").
- **The smudge's hold on hunts** lasts one time for the Spirit, another for the Demon and a third for the rest
  (`GhostAI.StopGhostFromHunting`).
- **The Mimic never copies** the Banshee or the Dayan when the ghost's traits flag says so (`GhostController.BuildMimicCopies`).

## Spirit (0)

- Its smudge hold on hunts has a length of its own (`GhostAI.StopGhostFromHunting`).

## Poltergeist (3)

- Its prop chance rolls against 20 in 100 where most types roll against 100, and it throws the prop where other types
  interact with it (`GhostActivity.InteractWithARandomProp`).
- In a hunt it throws every time the throw timer runs out. Other types skip half their throws on a coin
  (`GhostHuntState.throwTimer`).

## Banshee (4)

- **One target.** After each kill the master picks a new living target (`GhostAI.SetNewBansheeTarget`, `GhostAI.bansheeTarget`).
- In a hunt it goes only for that player: its sight finds nobody else (`GhostHuntState.SightPick`), the chase never
  switches to the nearest player (`GhostHuntState.Chase`, `GhostHuntState.KeepNearest`), and the may-go-for test passes
  only for the target (`GhostHuntState.TargetAllows`). [Ghost AI internals](../internals/ghost-ai.md), "The Banshee's one
  target", has the whole path.
- Its idle hunt check measures the target's insanity, not the team's (`GhostIdleState.HuntInsanity`).
- Its events perhaps force event 1 (`GhostState.randomEvent`). Its wander picker has a branch of its own
  (`GhostWanderState.PickDestination`).

## Mare (6)

- It never flickers lights: the light chance's second draw picks `GhostState.light` or `GhostState.flicker`, and never
  the flicker for a Mare (`GhostActivity.lightChance`).
- The light switch's switch-off coroutine rolls the Mare's switch-off (`Random.RandomRangeInt`).
- Its wander picker has a branch of its own that reads whether the room's lights are on (`GhostWanderState.PickDestination`;
  [Ghost AI internals](../internals/ghost-ai.md), "The wander and the favourite room").

## Shade (8) and Oni (11)

- When either appears, a coin may change the form asked for (`GhostAI.Appear`).
- Their events remap events 1, 3 and 4 (`GhostState.randomEvent`).

## Demon (9)

- Its smudge hold on hunts has a length of its own (`GhostAI.StopGhostFromHunting`).

## Yurei (10)

- The incense traps it: when the incense's sensor finds a Yurei, it stops the ghost wandering
  (`WhiteSage.YureiTrap`, `GhostAI.TemporarilyStopWander`).

## Hantu (13)

- Forced last evidence (see above). No other rule is mapped.

## Goryo (14)

- It never roams far: the wander picker skips the long-roam roll, so its wander never moves its favourite room
  (`GhostWanderState.PickDestination`, `GhostWanderState.Exit`).
- More branches in the wander: no path test for its sampled point, back to idle on arriving in its favourite room, and
  its D.O.T.S. walk (`GhostWanderState`; [Ghost AI internals](../internals/ghost-ai.md), "The wander and the favourite
  room").
- Forced last evidence.

## Myling (15)

- Its footsteps reach 8 m less while it hunts (`Footstep.Play`).

## Onryo (16)

- **Three flames, then a hunt.** Each flame the Onryo blows out adds one to `GhostAI.flamesBlownOut`
  (`FireSource.CountGhostBlowOut`). When the idle timer next runs out with the count above 2, the idle state clears the
  count and hunts, with no sanity check and no random draw (`GhostIdleState.Execute`). That hunt needs
  `LevelValues.eventsBlocked`, `GhostAI.postHuntCooldown` and `GhostAI.huntsHeldOff` clear. A count that reaches 3
  while the ghost hunts or its hunts are held off goes back to 0. [Ghost AI](ghost-ai.md), "How a hunt starts".
- Dead players change its flames: each widens a roll in the flame's update on the master and shortens the flame's random
  wait (`GameController.AmountOfDeadPlayers`; [Players](players.md), "Rules that act on a dead player"). Which way that
  roll turns the flame was not traced.

## The Twins (17)

- Its interaction roll starts a second interaction a second or two later, around the twin's own box, on 5 or more of
  `Random.RandomRangeInt(0, 10)` (`GhostActivity.TwinInteraction`, `GhostActivity.TryInteract`).

## Raiju (18)

- An electronics sensor finds running electronics near the ghost, which its hunt speed reads
  (`GhostActivity.electronicsSensor`, `GhostActivity.nearElectronics`; **inferred**).
- A hunting processor passes its signals whether or not it hunts, while the processor's `allowRaijuSkip` is set
  (`GhostHuntingProcessor`).

## Obake (19)

- Forced last evidence.
- The journal has media for its fingerprint and its shapeshift (`JournalController.h`; **inferred**).

## Mimic (20)

- It copies other types: at each of the game's switches, `GhostController.AttemptGhostTypeStateChange` runs
  `GhostController.MimicCopy`, which writes another type to the ghost's behaviour type on that client
  ([Ghost types and the Mimic](ghost-types.md), and the guide [The Mimic and your ghost](../guides/mimic-and-your-ghost.md)).
- The game's own pick is a plain random element of the whole `GhostController.mimicCopies` list: nothing skips or
  re-rolls, so it can copy the type it is copying now (`GhostController.MimicCopy`).

## Moroi (21)

- Its hunt base speed comes from the players' average insanity (`GhostHuntState.RefreshBase`).
- Forced last evidence.

## Deogen (22)

- In a hunt it sees every player (`GhostHuntState.Sees`), and its kill needs it to see them (`GhostHuntState.Kill`).
- A player must stay within its slow kill reach for twice as long, two seconds instead of one (`GhostHuntState.slowKillReach`).
- Its hunt base speed comes from a field of its GhostAI (`GhostHuntState.RefreshBase`).
- The hunt's voice and electronics tests compare with it too (`GhostHuntState.Hears`, `GhostHuntState.SensesGear`).
- Forced last evidence.

## Thaye (23)

- It ages: its age step runs 0 to 10, and the sanity below which it may hunt falls from 75 by 6 a step
  (`GhostController.thaye`, `GhostController.thayeStep`, `GhostController.thayeThreshold`).
- The idle hunt check scales its chance by a further factor and tests the threshold (`GhostIdleState.HuntInsanity`,
  `GhostIdleState.Execute`).
- Its timer asks for the ageing through the type-change path (`GhostController.AttemptGhostTypeStateChange`), and its
  events read the age step (`GhostState.randomEvent`).

## Gallu (25)

- Its rage is Normal, Enraged or Exhausted (`GhostController.galluState`). What moves it is not mapped.

## Dayan (26)

- Its strength is Normal, Empowered or Weakened (`GhostController.dayanState`).
- It checks the players near it every frame of a hunt, and in the idle state (`GhostAI.DayanPlayerChecks`,
  `GhostHuntState.UpdateSpeed`, `GhostIdleState.Execute`). A player who moves empowers it (`NetworkedStamina.moving`).

## Obambo (27)

- Its aggressive mood flips on or off at each of its switches (`GhostAI.obamboAggressive`,
  `GhostController.AttemptGhostTypeStateChange`).

## Kormos (28)

- Its events come from a fixed array (`GhostState.randomEvent`).
- In a hunt the chase never switches to the nearest player (`GhostHuntState.Chase`, `GhostHuntState.KeepNearest`).
- Its sensors pass a player's signal only within a fixed distance of the sensor, and not while a flag reached through
  the player is set (`PlayerProcessor`).

## Deildegast (30)

- Its light chance rolls against 10 in 100 where other types roll against 25 (`GhostActivity.lightChance`), and its prop
  chance against 85 where most roll against 100 (`GhostActivity.InteractWithARandomProp`).
- Its speed falls with the number of props it has moved (`GhostAI.movedProps`; **inferred**).
