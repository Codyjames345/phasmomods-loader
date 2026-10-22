# Custom ghost tells: design

Status: approved by the user on 2026-10-21. Game build 25811160. Changes the Custom Ghosts mod
(`mods/custom_ghosts`) and, for the Hydra's flame colour, the Spectral Flame mod
(`mods/spectral_flame`). Builds on `2026-10-20-flame-ghosts-design.md` and
`docs/custom-ghosts.md`.

## Intent

Playing with zero evidence is a common way to play Phasmophobia. With no evidence to read, a ghost
is told apart only by how it behaves, so every custom ghost needs a tell of its own: sometimes
subtle, sometimes obvious, never shared with a vanilla ghost or another custom one. This pass
sharpens the tells that were weak or overlapped a vanilla ghost (Echo, Draugr, Black Shuck,
Lithobolia, Brownie, Bogeyman, Lethe, Knell, Asterion), gives the Hydra a green Spectral Flame and
the Black Shuck a weakness to water, and makes every custom ghost's movement and footsteps run
through the game's own systems.

Settled with the user on 2026-10-21. Where a number or detail is Claude's call rather than the
user's, it is marked so.

## Goals

- Every custom ghost has a zero-evidence tell no other ghost shares (the table at the end).
- Every rule is built from vanilla mechanics: the game's own speed pipeline, footstep system,
  vision sensor, doors, teleport, throws and hiding places. Nothing is invented that a vanilla ghost
  could not do in principle.
- A custom ghost's footsteps come from the game's own step system and follow its real speed:
  their rate, the walking or running sound, and its walk animation.
- Every rule works for the AI ghost and, where Playable Ghost lets a player control the ghost,
  binds the player as the existing rules do. Claude's calls for the new rules: the Bogeyman's
  hunt-start teleport, the Echo's repeat and the Lithobolia's aim act for the AI only, never moving
  or choosing for a ghost player; speeds and footsteps apply to both. A player controlling the Black
  Shuck gets the darkened hunt view Playable Ghost already gives the Kormos, showing only the scent it
  already shows them (no new UI).

## Non-goals

- Any change to a vanilla ghost. The user builds vanilla tells (Mare, Onryo, Goryo and others) in a
  separate mod of their own.
- New evidence, new ghost types, or changes to evidence combinations.
- Changes to the Kikimora, Gremlin, Mesmer or Tempest beyond the shared
  speed and footstep model.

## Anti-goals

- A behaviour that strays from vanilla mechanics: a sound the game would not play, a step timer of
  our own, a detection check of our own where the game has one, a speed the game does not compute.
- A tell that makes a vanilla reading lie, or a custom ghost that plays like a vanilla one. In
  particular the Brownie must not play like the Deogen, and the Black Shuck must not play like the
  Kormos.
- A counter that switches a ghost off completely. Weaknesses buy time or information, never
  immunity.

## Constraints

- No speed caps on custom ghosts. A ghost's speed comes from its own rule only.
- Speeds are stated before the game's shared multipliers (difficulty ghost speed, Blood Moon), as
  vanilla speeds are; the multipliers apply to custom ghosts exactly as to vanilla ones.
- Player-facing text calls people "players", not "investigators".
- Another session rebalanced Spectral Flame (false tell, hunts counting, blow-out rules; FINDINGS 20).
  The Hydra's green flame builds on that work. This is now the only session working on Spectral
  Flame (user, 2026-10-21).

## The shared speed and footstep model (decided)

Today each speed rule scales the agent's speed through detours on `NavMeshAgent.set_speed` and
`get_speed`, and `get_speed` answers with the game's own speed while a rule applies. Everything else
that reads the speed therefore sees the wrong one: the footstep timer, the walk-or-run step sound and
the walk animation. A Brownie running 3.83 m/s steps, sounds and animates like a 1.7 m/s ghost.

Instead, each rule sets the inputs of the game's own hunt speed, and the game computes the real speed:

- The hunt state's base speed (+0xB0) is where the game puts a type's own base (the Moroi's by sanity,
  the Deildegast's by things moved). A custom rule sets it to the ghost's own speed.
- For a ghost with no line-of-sight speed-up, the speed-up (+0xB4) is held at its floor of 1.
- The game then applies its multipliers, sets the agent, sets the animator speed, and every reader
  of the agent's speed (footsteps, the Hydra's false heads, Playable Ghost) sees the real value.
- The `set_speed` and `get_speed` detours and `SpeedRule` go once no rule needs them.

Footsteps: no custom ghost plays footstep sounds of its own. The Knell's own clip player goes. The
Brownie's quiet steps use the Myling's vanilla rule (its step sound carries 8 m less far in a hunt)
instead of the current code, which turns down sound sources the ghost models do not have.

## Per ghost (decided)

### Echo
Second tell, working without voice chat: it repeats what players do. Soon after a player uses a
light switch or a door in the Echo's room, the Echo's next interaction uses that same switch or
door. Its voice tell and its dormancy weakness stay. Claude's calls: "soon" is within 60 s, only the
most recent such use counts, "its room" is its favourite room, and a repeat is never lost to its
dormancy (without voice chat it is dormant all contract, and this tell exists for exactly those
teams).

### Draugr
Its hunt speed depends on its distance from its favourite room, gradually, like a Deogen in reverse
and with the Deogen's own slope: 2.8 m/s at its room, 0.15 m/s slower for every metre away, down to
0.3 m/s from about 17 m (slower than a Deogen's 0.4 m/s crawl). No line-of-sight speed-up, so the
floor holds. Claude's numbers; the user asked for dramatic, gradual, and a floor below a Deogen's.
Distance stays measured to the nearest of the room's colliders (0 inside it).

### Black Shuck
Blind and deaf: in a hunt it finds players by scent alone. This is the deliberate opposite of the
Kormos, which hunts by hearing footsteps in the moment: crouching or standing still does not hide
you from the Black Shuck, because it follows where you have been, and only salt or water breaking
your trail does.

- It ignores the game's sight and hearing in hunts. It picks up a player's trail within 8 m (steps
  from the last 30 s) and follows it step for step, round every corner, to them. Reaching them kills
  them by the game's normal hunt kill.
- Salt and water break scent. A trail that crosses a salt line or running water is cut there, and
  scent on the far side of a break cannot be picked up from this side. If a break lies between it and
  the player it follows, it loses that player and casts about for any other trail in range.
- With no trail in range it roams as a hunting ghost with no target does, still blind and deaf.
- Smudging works on it as on every ghost: the game's own smudge state (GhostAI +0xF8) wins over the
  trail, and it stops following for the smudge's time (user, 2026-10-21).
- Hiding does not: a trail that ends at a closet or locker leads it there, and it searches it with the
  game's own closet check (ClosetZone.CheckCloset) as if it had seen the player go in. Salt, water and
  smudging buy time against it; closets do not (user, 2026-10-21).
- Water: outdoors, while it rains, no trail is laid. A running tap, sink, bath or shower erases trail
  steps near it and lays none there (Claude's call: within 2 m).
- No line-of-sight speed-up (it never sees); otherwise the game's default hunt speed. Claude's call.

### Lithobolia
Set apart from the Poltergeist, which throws anywhere, often several things at once:
- It throws only at people. With no player in its sight within 8 m it throws nothing at all.
- In a hunt, the objects the game throws during its hunts fly at its target instead of at random.
- A hit still costs 5% sanity. "In its sight" is the game's own vision sensor.

### Brownie
Unchanged speeds (3.83 m/s while unseen, 1.02 m/s while watched, no line-of-sight speed-up), now
computed by the game. Its quiet hunt steps use the Myling's rule. The text separates its two rules:
in a hunt it slows while any player looks at it; outside hunts it never interacts or does an event
while a player or a running video camera can see it.

Being the fastest ghost is not a requirement (user, 2026-10-21): the comments, tests and text that
treat it as one go (the "fastest" test list, the comment on its unseen speed, "faster than any other
ghost" in its journal page, which becomes "very fast"), and no other ghost is limited on its account.

### Bogeyman
Folklore additions, on top of searching hiding places first in hunts and being slow in open rooms:
- Hunts start from hiding: as a hunt begins it picks the lowest-level living player anywhere in the
  house and steps out of the closet or locker nearest them, moved there by the game's own ghost
  teleport (as the Wraith and the Twins move). The user chose the whole house knowing it is a big
  power spike, like a Wraith teleport at every hunt (2026-10-21). Claude's call: only when such a
  hiding place lies within 10 m of that player; otherwise it starts where it is. The hiding place's
  door opens as it steps out.
- Most of its door interactions are closet and locker doors (Claude's call: 3 in 4 when one is in
  reach), not only some.
- It stalks the youngest: in a hunt, when it detects more than one player, it goes for the one with
  the lowest player level first (ties: the nearest).
- Claude's call: "lowest level" means least progressed, so the lowest prestige first, then the lowest
  level within it, as the lobby shows them.

### Lethe
Forgets its target 1 s after losing sight of it (done 2026-10-21), using the game's own vision sensor
(GhostAI +0xA0) rather than a raycast of its own.

### Knell
No line-of-sight speed-up and no cap: it surges to 1.75 times the base (2.98 m/s) for one second and
crawls at 0.35 times the base (0.6 m/s) for the next, on a beat that never changes. Its steps come
from the game's own footsteps, which quicken in every surge and slow in every crawl.

### Asterion
A battering ram. It never closes doors. A closed door in its way stops it for 3 s while it batters it
with the game's door slam noise, then the door stays jammed open, so closing a door buys time once.
Its route through the house is marked by broken doors. Journal text changes to match (strength: a
closed door never stops it for good; weakness: battering costs it time and can be heard far away).

### Hydra
Its Spectral Flame turns green instead of blue, and this is a zero-evidence tell (user, 2026-10-21):
- When Spectral Flame is one of the given evidence, the flame goes to full green where another flame
  ghost's goes to full deep blue.
- When it is withheld (zero evidence, or the difficulty removed it), the flame still stops at the
  pale "false" stop like every ghost's, but the Hydra's pale stop is pale green. A pale green flame
  therefore names the Hydra even when Flame is the hidden evidence.
- Spectral Flame so needs the ghost's type, not only its evidence list (a field in the Custom Ghosts
  service, or GhostInfo +0x28, left to the builder).
- Claude's calls: the same timings as every other ghost, only the colour differs; greens from the
  game's own assets if suitable ones exist.

### Dullahan
No line-of-sight speed-up and no cap (user, 2026-10-21): its speed depends only on how far it must
turn, never on what it sees. Its body turns at 45 degrees a second (was 70). Claude's call: 2.0 times
the base (3.4 m/s) charging straight, so it stays very fast on long runs without its old speed-up,
falling to 0.25 times the base (0.43 m/s) facing away from its path. Its top speed stays below the
Brownie's 3.83 m/s (user).

## Personality and difficulty (Claude's calls, by the user's brief of 2026-10-21)

The user asked for each ghost to feel unique, rooted in folklore or fiction, fair, and spread across
a range of zero-evidence difficulty. Each journal page's description tells its story so the tell
reads as the ghost's nature, not a rule; the docs table gains a difficulty column.

| Ghost | Root | Personality | Zero-evidence difficulty |
| --- | --- | --- | --- |
| Echo | the nymph cursed to repeat others' words (Greek myth) | a lonely mimic, quiet unless spoken to | Hard |
| Dullahan | the Irish headless rider | a charge that cannot turn | Easy |
| Kikimora | the Slavic household spirit behind the stove | a fussy tidier of other people's mess | Medium |
| Draugr | the Norse barrow dead | a jealous guardian of its own room | Medium |
| Black Shuck | the East Anglian hellhound | a blind, patient hound on your trail | Medium |
| Lithobolia | the stone-throwing devil of New Hampshire, 1682 | a spiteful thrower that wants a target | Easy |
| Gremlin | the airmen's saboteur of the Second World War | a meddler that cannot leave a machine alone | Easy |
| Brownie | the Scottish house spirit that works only unseen | shy, quick and quiet when unwatched | Medium |
| Bogeyman | the thing in the closet that comes for children | a lurker that picks on the youngest | Medium |
| Lethe | the underworld river of forgetting | forgetful, and makes the living forget | Hard |
| Mesmer | the showman whose audiences saw what he wished | an illusionist, a different face to everyone | Hard |
| Knell | the funeral bell | relentless, on a beat that never changes | Easy |
| Asterion | the Minotaur of the labyrinth | a battering brute; doors only slow it | Easy |
| Tempest | the storm-raisers of weather lore | rides the lightning (reworked 2026-10-22) | Easy |
| Hydra | the many-headed serpent of the swamp of Lerna | it multiplies when struck; green marsh fire | Medium |

Easy ghosts show their tell in almost every hunt or event; Medium ones need a test or a setup (salt,
a camera, a flame, a hiding place); Hard ones need patience or several players comparing notes.

## Zero-evidence tells after this pass

| Ghost | Tell without evidence |
| --- | --- |
| Echo | speaks in a player's voice; repeats the switch or door a player just used |
| Dullahan | very fast on straights, all but stops at corners |
| Kikimora | equipment left in dark rooms disappears into other dark rooms |
| Draugr | fast at its room, crawling (slower than a Deogen) far from it |
| Black Shuck | follows your exact path round corners at an even pace, even to your closet; salt or water makes it lose you |
| Lithobolia | objects fly straight at people, never at empty rooms |
| Gremlin | placed electronics switch themselves off |
| Brownie | very fast while unseen, slow walk while watched; quiet steps; nothing happens in view |
| Bogeyman | hunts start from a closet or locker next to the lowest-level player; goes for them first |
| Lethe | photos fade from the journal; forgets you within a second of losing sight |
| Mesmer | each player sees a different form in a different place |
| Knell | two-second surge-and-crawl beat you can hear in its steps |
| Asterion | batters closed doors open; they stay broken open |
| Tempest | invisible and slow in its hunts, visible and very fast in each lightning strike |
| Hydra | green Spectral Flame (pale green when the evidence is withheld); smudging splits it into false heads |

## Game facts found for this design (build 25811160)

- Hunt speed: hunt state update 0x2147380, speed function 0x2153C20. Default own speed = +0xB0 *
  +0xB4 + +0xB8; +0xB0 refreshed every 2 s by 0x214C2A0 from GhostAI +0xC8 (1.7 m/s, set in
  GhostAI.Awake 0x20211C0 from the constant at 0x558CC90). The function ends by setting the model's
  Animator speed to `get_speed()` (icall `UnityEngine.Animator::set_speed`). Line of sight +0xB4
  grows 0.05/s with a target (+0x48), holds while it has a search point (+0x50), decays 0.01/s
  otherwise, clamped 1 to 1.65, and counts as 1 while smudged (GhostAI +0xF8). After the own speed
  the game multiplies by the difficulty's ghost speed (LevelValues difficulty +0x78), 1.15 in Blood
  Moon (weather +0xAC == 8) or while GhostAI +0xF1 is set, and GhostAI +0xCC, +0xD0, +0xD4 (1 by
  default).
- Footsteps: GhostInteraction.Update (0x208AF00) times the next step as `random + 1 / get_speed()`
  while the ghost is visible or hunting, then sends SpawnFootstepNetworked (0x207FC00), which spawns
  a pooled Footstep and calls Footstep.Spawn (0x24331E0). The step sound plays from the Footstep's
  Noise (0x243E380): the running sound when `get_speed()` is above 2.0 m/s, and for the Myling
  (behaviour type 15) in a hunt the noise's max distance is its default minus 8 m. Ghost models carry
  no AudioSources.
- Vision: GhostAI +0xA0 is the ghost's player vision sensor; vanilla code asks it
  `Sensor.IsDetected(player GameObject)` (0x3D1C2E0). The ghost also carries a player noise sensor
  with GhostHearing (the Kormos's hearing).
- Hunt throws: the hunt update throws a prop (GhostActivity.GetPropToThrow, Prop.TryThrow) every
  0.5 s, half the time for every type but the Poltergeist.

## Left to the builder

- Where each rule hooks the game, found by reverse engineering, preferring the game's own entry
  points (the base refresh 0x214C2A0, the vision sensor, the hunt's target and detection code, the
  door and teleport functions) over new mechanisms.
- How the Black Shuck's blindness and deafness are made (the hunt's detection and target code), and
  how its kill reaches the game's own.
- Finding running taps, sinks, baths and showers, rain, and whether a point is outdoors.
- Reading a player's level, and the game's own teleport for the Bogeyman.
- Exact journal and docs wording within the meanings above, and offline tests for every rule.
- Each live check the offline tests cannot cover, as a FINDINGS entry.
