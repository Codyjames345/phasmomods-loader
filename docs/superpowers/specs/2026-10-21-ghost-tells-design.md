# Ghost tells: design

Status: approved by the user on 2026-10-21 ("implement all three tells for each ghost"); implemented
2026-10-21, offline tests only (FINDINGS 27). Game build 25811160. A new mod of the suite, `mods/ghost_tells` (id `ghost-tells`, channel 6), with a small
change to Playable Ghost so a player controlling one of these ghosts is bound by the same rules.

Where a number or detail is Claude's call rather than the user's, it says so.

## Intent

Six vanilla ghosts are close to unidentifiable from how they behave: the Spirit, Mare, Shade, Yurei,
Goryo and Onryo. Players say the hardest
"leave you waiting around", that the Goryo "has no personality with no evidence", that other ghosts
fake the Mare's lights and the Yurei's doors, and that the Onryo's candle test "relies on candles
that melt away". Most of their current tells only rule a ghost out, or need a long timed test.

This mod gives each of the six three more tells, built from what the game already does and in
keeping with each ghost's journal description, so a careful team can name it from its behaviour.

## Goals

- Three new tells for each of the six ghosts (the table at the end), none shared with another
  vanilla ghost or with any custom ghost.
- Every tell is made of vanilla mechanics: the game's own light switch, flame blow-out, door force,
  door lock, door knock, appearance, interaction, ability, hunt speed and camera-only render layer.
- Each tell fits the ghost's journal description and folklore, and the journal says what it is.
- Balance: a tell gives information or buys time. None gives immunity, and none makes a ghost
  noticeably more or less deadly than vanilla except where its journal already says it should be.

## Non-goals

- Any change to evidence, to which evidence a ghost gives, or to any of the other 24 vanilla types.
- Any change to the custom ghosts (their behaviour type is GhostType.None, so no rule here matches
  them).
- New UI: the journal text is the game's own page with one line added (the vanilla-UI-only rule).

## Anti-goals

- A tell that makes a vanilla reading lie. In particular nothing here produces anything that looks
  like evidence: no D.O.T.S. silhouette, no ghost orb, no EMF 5, no blue or recoloured flame.
- A hard limit that gives players a safe zone. The Goryo's hunt rule in particular never stops it
  chasing anyone it detects, wherever they are.
- Mechanics of our own where the game has one: our own sounds, our own door locking, our own
  light switching, our own speed computation.

## When it is on

- A private room in which every client runs the mod (`peer_has_mod`), as every mod of the suite.
  Otherwise the contract is plain vanilla; the mod declares `PG_MOD_VANILLA_PEERS`.
- Decided when the contract loads and held until it unloads.
- The rules key on the ghost's behaviour type (`GhostInfo +0x2C`), as the game's own type rules do:
  Spirit 0, Mare 6, Shade 8, Yurei 10, Goryo 14, Onryo 16. A Mimic copying one of them therefore
  shows its tells, as it shows that ghost's vanilla traits.
- The host runs every rule that changes what the ghost does. Visual-only effects (the Goryo's
  camera figure, the Onryo's guttering flames, the Onryo's spiteful wick) run on every client from
  the host's messages on the mod's channel.

## The tells (decided)

### Spirit ("can be temporarily stopped by burning incense")

1. **Stopped by incense.** Incense that reaches it in a hunt (`GhostAI +0xF8` becomes set) stops it
   where it stands for 3 s, then it carries on blind as any smudged ghost does. Its hunt speed is
   held at zero through the hunt state's own base speed (`+0xB0`), so its footsteps and walk
   animation stop with it. Once per smudge.
2. **Quiet after incense.** Incense that reaches it outside a hunt (`GhostAI +0xF5` becomes set while
   not hunting) silences it for 60 s: no interactions, events or ability. Its vanilla 180 s hunt
   block is unchanged. This shows the incense landed, which the 3-minute test otherwise leaves to
   chance. (60 s is Claude's call.)
3. **Answers its name.** When a living player within 10 m of it on its floor says its name (the
   game's own speech recognition, `PhraseListenerController.OnPhraseRecognized`), it knocks: the
   game's door-knock state (`GhostAI.ChangeState(9)`) 1 to 3 s later. Not in a hunt, an event or
   its quiet minute; at most once every 20 s. Needs the game's voice recognition switched on.
   (Range, delay and cooldown are Claude's calls.) Vanilla processes heard phrases only during
   hunts, so no vanilla ghost answers its name like this.

### Mare ("the source of all nightmares, more powerful in the dark")

1. **Lights out first.** It never shows itself in a lit room. When it is about to appear outside a
   hunt (`GhostAI.Appear`) in a room whose lights are on, it first switches each lit switch in that
   room off with the call its vanilla switch-off makes (`LightSwitch.UseLightSwitch(false, true,
   true)`), then appears. When a lit switch cannot be used (broken), it does not appear at all.
   Other ghosts only flicker the lights in their events.
2. **Trail of darkness.** It notes each room it passes through in a hunt (every 0.5 s); when the hunt
   ends, each lit switch in those rooms is switched off with the same call. Light switches cannot be
   used during a hunt, so this happens as the hunt ends.
3. **Sharper switch-off.** Its vanilla tell (switching off a light a player has just switched on
   within 4 m, on its floor) happens 1 time in 2 instead of 1 in 7: the roll inside that coroutine
   (`LightSwitch` nested `MoveNext`, RVA 0x26FFD10, `Random.Range(0, 7) == 1`) is replaced there and
   nowhere else. (1 in 2 is Claude's call.)

### Shade ("very shy", "stops all paranormal activity if there are people nearby")

1. **Shy of your gaze.** When it shows itself outside a hunt and a living player looks straight at
   it (within 15 degrees of where they face, 20 m, a clear line) for 0.4 s, it vanishes at once: its
   event ends (`ChangeState(0)`, and `UnAppear` if it still shows). Other ghosts' appearances play
   out in full. (Cone, range and the 0.4 s are Claude's calls.)
2. **Shy of company.** With two or more living players within 5 m of it, through walls too, it does
   nothing at all: no interactions, events or hunts. Vanilla's own rule (nobody in its room) stays.
   This is the journal's "people nearby"; it matters only with two or more players.
3. **Falters at a group.** In a hunt, when its prey (`hunt +0x48`) has another living player within
   2.5 m who is also in clear view of it, it stops for 1.5 s and its line-of-sight speed-up falls
   back to 1. At most once every 15 s. (Claude's numbers.) The Brownie, by contrast, stays slow for
   as long as anyone watches it; the Shade only hesitates.

### Yurei ("smudging its place of death will trap it temporarily")

1. **Sealed in by incense.** When its incense trap starts (the game's own, `WhiteSage` handler at
   RVA 0x8256B0 calling `TemporarilyStopWander(90)` for a Yurei), it shuts every door of the room it
   is in (`Door.GhostForceDoor(false)`). For those 90 s, it shuts again any of those doors a player
   opens, 3 s after it was opened. No sanity is drained by these. (The 3 s is Claude's call.)
2. **Holds the door.** The door its ability closes (the one `Door.GhostForceDoor` is called on
   during `ChangeState(15)` for a Yurei) is locked shut for 3 s with the game's own hunt lock
   (`Door.HuntingCloseDoor`, then `Door.ToggleDoorLock(true, true)`, as `LevelController` locks and
   unlocks exit doors), so anyone trying it gets the hunt's locked-door response. When a hunt has
   started by then and the door is one the hunt locks, the hunt unlocks it instead. The difficulty
   setting that keeps doors unlocked in hunts (`HuntingCloseDoor` checks it) applies here too.
3. **Drawn to open doors.** A door left open for 20 s in the room it is in draws its ability
   (`ChangeState(15)`), aimed at that door (the ability's random door pick, `Random.Range(0, doors)`
   inside that call, is steered to it). Only while it is idle or wandering, not trapped, hunting or
   in an event, at most once every 60 s, and only when the game allows events. Closing the doors of
   its room keeps it from using its ability, as the community already advises. (20 s and 60 s are
   Claude's calls.)

### Goryo ("only show itself on camera if there are no people nearby", "rarely seen far from its place of death")

1. **Seen only on camera.** In its favourite room, with no living player within 10 m and a running
   placed video camera (CCTV) with it in view (Brownie's camera cone: 35 degrees, 12 m, clear line),
   every 30 to 60 s it stands visible for 4 s on the game's camera-only render layer (`LaserGhost`,
   the layer `GhostModel.SetDOTSMaterials` gives a Goryo). It is a copy of its own model with its
   own materials (the suite's `apparition` copy), so nobody sees it but a camera, and it never
   looks like a D.O.T.S. silhouette. Not while it appears, hunts or causes an event. (Claude's
   numbers.)
2. **Plays to the camera.** Under the same conditions it makes an extra vanilla interaction attempt
   (`GhostActivity.TryInteract`) every 20 s. The exact opposite of the Brownie, which never acts in
   view of a camera. (20 s is Claude's call.)
3. **Goes home to search.** In a hunt with no prey detected (`hunt +0x48` empty) while more than 12 m
   from its favourite room, it walks back towards that room instead of searching where it is, until
   it is within 3 m of the room; then the vanilla hunt carries on. Whenever it sees or hears anyone
   it chases them as any ghost does, however far from its room, and kills whoever it reaches on the
   way back. So nobody is ever safe far from its room: breaking its line of sight there only buys
   time. It never applies while a player steers the ghost. (Claude's design, after the user warned
   that limiting its hunt range might break it; 12 m and 3 m are Claude's numbers.) It differs from
   the Draugr's tell, a speed that falls with distance from its room: the Goryo's speed is vanilla.

### Onryo ("fears any form of fire", "extinguishing a flame can cause an Onryo to attack")

1. **Snuffs a new flame.** A flame lit within 4 m of it on its floor, outside a hunt, is blown out 1
   time in 2, 1 to 2 s later, by the vanilla ghost blow-out (`FireSource.BlowOutFire(true)` on the
   host: interaction EMF, sync, and the Onryo's own count of blown-out flames, `GhostAI +0x120`), so
   it counts toward its third-flame hunt. Testing it is dangerous in the Onryo's own way. (1 in 2 and
   the delay are Claude's calls; 4 m is its vanilla flame range.)
2. **The countdown.** Each time its count rises, every lit flame in the house gutters (its light dips
   to 30% and recovers over 0.6 s). While the count stands at two, every lit flame burns low (light
   at 60%, flame at 0.75 scale) until the count resets. Light intensity and flame size only, never
   colour, so it cannot be mistaken for Spectral Flame. (Claude's numbers.) Folklore: the hundred
   candles of Hyakumonogatari, one put out per story until a spirit comes.
3. **Spiteful wick.** A flame it blew out (any of its blow-outs, vanilla ones included) will not catch
   again for 15 s: `FireSource.IgniteFire` on that flame does nothing, on every client. (15 s is
   Claude's call.)

## Journal

While the mod is on, each of the six ghosts' journal text
(localisation key `Journal2_<Type>`, through `LocalisationSystem.GetLocalisedValue`) gains one line
naming its new tells, after its Strength and Weakness lines. English only (Claude's call; the
mod has no translations).

## Playable Ghost

A player controlling one of these ghosts is bound as the AI is. The rules that act through
`ChangeState`, `TryInteract`, `Appear` and the hunt state bind them automatically, because Playable
Ghost runs events, hunts and abilities through those. Its aimed interactions and its HUD ask
`custom_link::held_back`; that function also asks this mod's new service (`ghost-tells.v1`,
`heldBack(kind)`) so the Shade's company rule and the Spirit's quiet minute hold them back too.

## Tells after this mod

| Ghost | Vanilla tell | New tells |
| --- | --- | --- |
| Spirit | incense blocks hunts for 3 minutes | incense stops it dead for 3 s in a hunt; goes silent for a minute after incense; knocks when its name is said |
| Mare | never turns a light on; switches off a light just turned on (1 in 7) | darkens the room before every appearance; leaves the lights off where it hunted; the quick switch-off is 1 in 2 |
| Shade | does nothing with anyone in its room | vanishes when stared at; does nothing with two players near; hesitates at a group in a hunt |
| Yurei | shuts a door and drains sanity | seals itself in when smudged; holds its ability's door locked; uses its ability on a door left open |
| Goryo | stays near its room | stands visible on camera only; acts more on camera; goes home when it loses you far away |
| Onryo | flames block its hunts; third flame triggers one | snuffs a freshly lit flame; flames gutter as it counts; its snuffed flames won't relight for a while |

## Game facts found for this design (build 25811160)

- `GhostAI` (DiffableCs): `+0x38` GhostInfo, `+0x40` NavMeshAgent, `+0x58` GhostActivity, `+0x60`
  GhostModel, `+0xA0` LOSSensor, `+0xA8` appeared, `+0xB0` a Transform (the point its abilities
  measure from), `+0xF3` hunting (written by the `Hunting` RPC), `+0xF5` incense effect outside a
  hunt, `+0xF8` WhiteSage that smudged it in this hunt, `+0x108` may wander, `+0x120` the Onryo's
  count of blown-out flames.
- State enum: 0 idle, 1 wander, 2 hunting, 3 favouriteRoom, 4 light, 5 door, 6 throwing, 7 fusebox,
  8 appear, 9 doorKnock, 10 windowKnock, 11 carAlarm, 12 flicker, 13 cctv, 14 randomEvent, 15
  GhostAbility, 16 mannequin, 17 teleportObject, 18 interact, 19 summoningCircle, 20 musicBox, 21
  dots, 22 salt, 23 ignite.
- Ability dispatcher 0x2027080: the Yurei's (type 10) is state class `ഩജമരഝഥസനഥ`; it returns the
  ghost to idle, picks a random door of the ghost's current room (`LevelController +0x38` room,
  `+0x28` its doors, `Random.Range(0, count)`), forces it with `Door.GhostForceDoor`, then changes
  the sanity of players within range on its floor.
- `FireSource.BlowOutFire(true)` 0x652090 creates the interaction EMF and calls 0x656660, which adds
  one to `GhostAI +0x120` for an Onryo and resets it at three during a hunt or incense.
- Mare switch-off coroutine `MoveNext` 0x26FFD10: switch on, fuse fine, `Random.Range(0, 7) == 1`,
  a short random wait, same floor (`room +0x68`), within 4 m (constant 0x5582B28), then
  `UseLightSwitch(false, true, true)`. `UseLightSwitch` returns while hunting (`+0xF3`).
- `PhraseListenerController.OnPhraseRecognized` 0x2370E00 returns at once unless the ghost hunts.
- Yurei incense trap: `WhiteSage` handler 0x8256B0, `TemporarilyStopWander(90.0)` (constant
  0x55833B0).
- `Door.HuntingCloseDoor` 0x264CA70 (disables the door and sends its RPC unless difficulty `+0x6C`),
  `Door.ToggleDoorLock(bool unlock, bool)` 0x264E530; `LevelController.UnlockHuntingDoors`
  0x22FD220 calls `ToggleDoorLock(true, true)`.
- `GhostModel.SetDOTSMaterials` 0x208B580: layer "LaserGhost" for a Goryo, "Ghost" otherwise.
- Journal descriptions: localisation keys `Journal2_<Type>` in resources.assets.

## Left to the builder

- Each live check the offline tests cannot cover, as a FINDINGS entry (nothing here has run in a
  contract).
- Identifying the ghost-knock door and the appearance's event state end precisely enough to test.
- Exact journal wording within the meanings above, and offline tests for every rule's arithmetic.
