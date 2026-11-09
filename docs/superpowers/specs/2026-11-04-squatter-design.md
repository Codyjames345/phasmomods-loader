# The Squatter: design

Custom Ghosts gains one more ghost type, the Squatter. Decided with the user on 2026-11-04. Items marked **(builder's call)**
are not the user's decisions and may be changed by whoever plans this. Everything else is the user's.

## What it is and why

The Squatter is not a ghost. It is a person living in the house, annoyed that investigators have turned up, wearing a white
bedsheet and pretending to be one. The joke is the contrast: where a vanilla ghost is eerie and supernatural, this is a human
running around a house in a sheet, ducking and rolling, shouting at people to get off the property, in the user's own voice.

A contract with it should make players laugh. If it reads as just another hunt with a different model, it has failed, however
well it works.

## What happens in a contract

1. **Finding it.** The Squatter keeps around the ghost room, moving about as any ghost does by its own idling,
   wandering and favourite room (user, 2026-11-09: holding it still left it wherever it spawned, a closed
   pantry among them), always visible, playing lines such as "ooOOOooo I'm a ghost", "this is
   private property" and "get out!". The team finds the room by listening.
2. **The chase starts** the first time it sees a player. Before then it only makes those sounds. From that moment the chase
   runs until every living player is out of the house.
3. **Chasing.** It runs at just under the players' sprint speed, so a sprinting player can barely outrun it and one whose sprint
   runs out gets caught. It is clumsy: it breaks up its run with ducks, weaves, rolls and parkour moves where the way ahead calls
   for them, never on a timer (user, 2026-11-09), and only once it faces the way it runs, since each carries it the way it
   faces (user, 2026-11-09: it rolled before it had turned to the player): it jumps down stairs and drops and over knee-high props, landing into a forward
   roll; it dive-rolls at the player it chases when they are a few metres straight ahead; it trips over ankle-high props (the
   builder's call); it weaves side-on through doorways and gaps and ducks past furniture beside it. It runs a little slower to
   keep under the sprint without them. It bumps into things it stalls against, playing recordings as it goes. When it goes through a door during the chase it plays an opening-door clip, inwards
   or outwards (user, 2026-11-07). These moves change its speed where relevant. It goes for the nearest player when
   several are inside. When players can't sprint, its speed stays the same and it taunts them with a unique line, like "awww too tired to
   run?".
4. **Losing track.** It does not always know where people are. When it loses them it searches, saying lines like "where are
   you?". It uses sight and hearing to find them again.
5. **Hiding.** When it knows someone is in a closet or locker, it stands there, demands they come out and bangs on the door
   repeatedly, by kicking it with the kick's clip until a banging clip turns up (user, 2026-11-07). Each kick sounds as that
   door being slammed: the door's own slam sound, played on every client as the foot lands (user, 2026-11-09). It cannot open
   it. Hiding is safe, but loud. After 10 seconds of kicking it loses interest and wanders off as it lurked, leaving whoever
   hides there alone until they step out, so they have a chance to get away (user, 2026-11-09). A hiding place whose door
   is not shut (by the game's own snap test) hides nobody: it never passes through a door, so it walks to the doors, opens
   the door that stands ajar with the opening-door clip, and walks in once the gap lets it through to kill them; a door
   already open that far it walks straight through (user, 2026-11-09). It never opens a shut door, and kicks when the only
   door ajar is one it may not open. When they
   step out it goes straight back to chasing them. It knows
   because it saw them go in, or found them while searching the way the game's own closet search does **(builder's call)**. Someone
   hiding where it never looks keeps the front door unlocked and the Squatter searching, for as long as they stay.
6. **The catch.** It kills the player with a kick to the face, replacing the stock choke, and says a funny line.
7. **The door.** It never follows anyone past the way out. The door does not lock until all living players are out; then it
   locks, and each later attempt to open it gets another line, like "hit the road dude". On a map with several exits, it stops at all
   of them and all of them lock. A map without a house still has a "door", which locks in the same way. Once they have
   locked, no Monkey Paw wish opens them: not the timed locks, not the rooms wish where an exit is among its doors, and not
   the wish that opens every exit; everything else each wish does stays vanilla, and before the lock every wish is
   vanilla (user, 2026-11-08).
8. **The end.** The team finishes in the truck as usual, naming the Squatter in the journal.

**There is no paranormal activity:** no events, interactions, evidence or hunts. Nothing about it may read as a hunt: no flickering
lights, no electronics interference and no hunt sounds.

**On a friendly ghost's difficulty** (`Difficulty.friendlyGhost`), where the game refuses every kill, it starts seated and depressed on the
floor of its favourite room (outside any hiding place the difficulty closed: there it walks to the closest valid place first, user 2026-11-09), saying random phrases like "what's the point anyway, I'm not going to chase you". On first seeing a player it
gets up, becomes excited, says something like "yay new friends", then follows them around inside saying random things. It never attempts a
kill. These are **(builder's call)**: no door locks, since nobody was chased out; it follows the nearest player, at a pace tuned in game; it
searches the same way if it loses them; and at a hiding place it waits and talks rather than banging.

**It is not playable in Playable Ghost** (user, 2026-11-07, replacing the 2026-11-04 decision that it would be): it is meant as a gag the AI
plays on the team. Playable Ghost never offers it and never gives it to a ghost player, so a contract with a ghost player never has the
Squatter. How a ghost is kept out of Playable Ghost is a per-ghost option any mod author can set, like the Mimic one **(builder's call)**.
When a Custom difficulty forces the Squatter and a ghost player was chosen, Playable Ghost stands aside for that contract: the AI plays
the Squatter, every volunteer joins as an investigator, and each is told why in the game's own popup (user, 2026-11-08).

Some consequences, all **(builder's call)**:

- The chase can start during the setup phase, since the rule is simply "the first time it sees a player".
- Smudge sticks and crucifixes do nothing to it, as the user confirmed. Cursed possessions that call on the ghost do nothing either, while
  anything they do to a player alone stays vanilla.
- It is never on the Mimic's copy list (user, 2026-11-06).
- "Can't sprint" covers both a custom difficulty that turns sprinting off and a chased player whose sprint has run out, so the taunt can play
  for either. Its speed comes from the game's sprint speed as if sprinting were on, which reverse engineering has to establish.
- Bumping into something is an animation and a pause. It moves no props, since that would read as a ghost interaction.
- At a shut door during the chase, the Squatter swings it open with the game's own `HingeDoor.SwingDoor`, all the way to its open angle (user,
  2026-11-09: the contract-start opener `HingeDoor.LevelControllerOpenDoor` did not hold mid-contract, and
  `HingeDoor.OpenDoor`'s physics push kept a door that swings away shut) and plays the opening-door clip (user, 2026-11-08). The ghost's own
  door push (`Door.GhostForceDoor`) was ruled out: it leaves an EMF reading, the ghost's door sounds and counts on the results screen.
  The quiet opener leaves none of those; the master takes the door's view first so every player sees the same open door. It never
  opens an exit. The leaf opens to its hinge's one side, all the way, and the clip pulls a leaf that swings toward the Squatter and pushes one that swings away. It must not send a game RPC by
  name or invoke an RPC body.

## Look and sound

- **Body:** a vanilla player model, not a ghost model.
- **Sheet:** a cloth-simulated white bedsheet draped over the model, with cutouts for the eyes. The mesh is a plain sheet generated
  in code. The user ruled on 2026-11-04 that this is not a creative asset, so `AI_POLICY.md` does not forbid it.
- **Animation:** Mixamo clips, in variety: walk and run, ducks, weaves, rolls, parkour moves, stumbles, a kick, sitting on the floor,
  and getting up excited. The movement should feel human and
  natural; GTA V's walk and run are the user's reference for the feel. Only the feel: none of GTA's assets are used. The user
  downloads the clips (Mixamo needs an Adobe login) and picks which ones to use. The user's picks (2026-11-07): run `running`; duck
  `crouched sneaking left` or `right`; weave `left cover sneak` or `right cover sneak`; roll `Sprinting Forward Roll` or `Stand To Roll`;
  parkour `jumping up` or `hard landing`; stumble `Tripping` and bump `Jogging Stumble` (the builder's reading of the user's description);
  sit `Sitting Idle`; get up `Getting Up`; kick and bang `Martelo 2`; doors `Open Door Outwards` and `Opening Door Inwards`; stand `Standing Idle` and walk `Crouched Walking` (user,
  2026-11-09): it stands while it lurks, waits at the exits or holds still, and walks to search, follow and find a
  place to wait. Where a move
  has two clips, each time it plays one is picked at random, the same on every client.
- **Sound:** every sound it makes is one of the user's recordings or a stock clip the user supplies. The user records all the
  lines and writes any more.

Each move changes its speed where relevant: a roll, a trip, a jump, a duck and a weave carry it as far as their clips travel. "Barely outrun" holds across the whole chase, so tune
the moves and base speed together in game **(builder's call)**. All players see the same move at the same time **(builder's call)**.
Its footsteps are the players' own step sounds, one per stride it moves on each player's game (user, 2026-11-09, replacing the
user's own recordings): the tag of the surface under it picks that surface's clips as a player's footsteps do, the running ones while
it runs. They play through the mod's own sound, never a player's noise, so no ghost hears them as a player. The game's ghost footsteps
were ruled out: each leaves a UV footprint that counts as photo evidence and adds to the ghost-distance stat, and the game sends them
only in a hunt.

## Rules this ghost breaks on purpose

The user approved each of these. Record them so a reviewer does not "fix" them.

- **Speed.** `CLAUDE.md` says custom ghosts set speed through the hunt's own base and that there are no speed caps. The Squatter
  ignores the hunt speed mechanics and has its own speed.
- **No vanilla kill animation,** and movement unlike any vanilla ghost's. `CLAUDE.md`'s "nothing strays from vanilla mechanics"
  does not apply to this ghost's chase.
- **Third-party animation data.** Everything else in the project is original.

These rules still hold:

- **Identifiable** on zero evidence, every map and every weather: it is visible and it talks. Evidence-giving is not a route to
  identifying it.
- **AI and player share one ruleset** does not arise: no player ever controls it.
- **The loader owns the lobby.** No per-mod checks. Everyone has the same mods, so everyone has the same clips and recordings.
- **Saves and other players.** The ghost type stays subject to the existing rule that custom types never reach a save or a player
  without the content.
- **Payouts.** Nothing here adds currency. Optional objectives that need paranormal activity cannot be done on this contract, which
  lowers its payout rather than raising it; that is acceptable **(builder's call)**.

## What the rig check found

Read from the game's `resources.assets` with the project's pinned UnityPy on 2026-11-04. These are static facts; nothing has been
played in game.

- The PC player's body, `PCPlayer/Rigged PC Model Base` in Resources, has a Unity humanoid avatar with 23 of 25 human bone slots mapped
  (the eyes are not), running the controller `PCPlayer (overhaul)`. `Player Body Offset`, on the VR player, shares that avatar and controller.
- The cosmetic bodies, `Male_1` to `6` and `Female_2` to `6`, each have humanoid avatars with 25 of 25 mapped. Those sampled use
  a Character Creator skeleton (`CC_Base_*`). They have no controller of their own.
- Older models, `Male1` to `4` and `Female1` to `4`, have humanoid avatars with 24 of 25 mapped, on the `PCPlayer` and
  `VRIK Animated Locomotion` controllers. The one sampled uses a literal Mixamo skeleton (`mixamorig:*`). They live only in `OldPCPlayer` and
  `OldVRPlayer`, and no route to them was found (`sharedassets*` were not searched for references), so they are not used.

So Mixamo clips should retarget through Unity's humanoid system. That is an expectation, not a result.

## Technical unknowns, in the order to settle them

Settle the first three in a throwaway test before building the rest. If one fails, stop and bring it back to the user.

1. **Loading clips.** New humanoid clips need an AssetBundle built in Unity 2022.3.40f1. The managed `AssetBundle` class is absent from
   the Cpp2IL dump, though the module's request classes are present and `UnityPlayer.dll` still has the engine's load calls
   (`AssetBundle::LoadFromFile_Internal`, `LoadAsset_Internal`). Whether they work with the class stripped is unproven.
   Fallback: convert the clips offline to the mod's own data and move the bones each frame.
2. **Cloth.** `UnityEngine.ClothModule`'s `Cloth` class and its engine calls exist, as do the `Mesh` and `SkinnedMeshRenderer` calls a
   runtime sheet needs. No code here has run them. The sheet's look under the game's lighting, and its behaviour through rolls,
   needs an in-game test. `Mesh`, `SkinnedMeshRenderer` and `MeshFilter` are partly mapped today (`sdk/map/UnityRendering.h`).
3. **A live copy of the player model.** `mods/playable_ghost/form.cpp` bakes an investigator's body into a still mesh, and
   `mods/custom_ghosts/apparition.cpp` copies a ghost model with its Animator. Neither is a running player model driven by new clips.
   Which model to copy, and from where when no other player is present, needs reverse engineering. Copies are local, never networked
   instantiation.
4. **The kill.** `Player.BeginDeathSequence` sends the death to every client and the animation to the view's owner. Its note says to reach
   it only as its callers do, because the animation's coroutine refuses, **with a report on the sender**, a kill of a dead player or on
   a friendly ghost's difficulty. `Difficulty.friendlyGhost` also refuses `GhostAI.Hunting` and `Door`'s hunt close. So the kick and the
   door must never trigger a refusal, and `friendlyGhost` must never be changed. How to replace the choke, and what the victim sees, needs
   reverse engineering. Read `sdk/systems/photon.md`, "The game's checks on an RPC", first. Do not send a game RPC by name or
   invoke an RPC body.
5. **Being a non-hunt.** Whether the chase runs on `GhostHuntState` with the hunt's effects removed, or on its own state, is **(builder's
   call)**, decided from reverse engineering. What matters is the behaviour above. `GhostActivity.TryInteract` and `GhostAI.ChangeState`
   are the two gates for interactions, events and hunts (`sdk/systems/ghost-ai.md`). `GhostAI.vision` detects nobody outside hunts, and
   `mods/custom_ghosts/sensor_copy.cpp` shows how Custom Ghosts sets up its own sensors.
6. **Zero evidence.** `Ghost::evidence` and the content layer's `GhostDef.evidence` hold three ids, and `GhostData` holds three values.
   Every existing ghost fills them. The content layer, the journal and `GhostController.PickEvidence` need to accept a ghost with none,
   using the game's own "no evidence" value. Establish how the journal's filter treats it.
7. **The doors.** `Door.ToggleDoorLock`, `Door.HuntingCloseDoor` and `DoorKind.Main` are mapped, and the first's meaning is only inferred from
   its name. How an attempt to open a locked door reaches code is unknown. So is how the game defines every map's exits, including
   maps without a house, and whether `DoorKind.Secondary` covers the others. How to tell a player is "out" also needs establishing, since
   `RoomKind.outsideMap` means the truck, not the yard. Read the exits from the game, per the dynamic-over-hard-coded rule. The
   ghost-leaves-the-building work (stage 2b Tasks 17 and 18) may share this.
8. **Hiding places.** `GhostHuntState.SetCloset`, `searchCloset` and `ClosetZone` are mapped. Banging on the door needs the door's own
   position and a way to find the occupant.

## SDK additions

Add whatever this needs to the map and the API, under the project's rules: readable names, no addresses or obfuscated names, a documented
interface on every public declaration, and useful facts in `sdk/systems/`. The user asked for this explicitly. Expect at least AssetBundle
loading, Cloth, runtime Mesh building, the player model, the locked door and being out of the house. Each is useful to community mods.
The host contract only grows at the end.

## Files and shipping

- **Recordings** live in the mod's folder. The format is **(builder's call)**, researched as `CLAUDE.md` requires. `AudioClip.Create` and
  `AudioClip.SetData` are mapped, and `mods/custom_ghosts/sound.h` plays a clip in the world.
- **Clips never enter the public repo as raw files.** Copies of Mixamo's FAQ reposted on Adobe's forum (2022) say use in games is royalty-free,
  credit is optional, and raw character and animation files may not be distributed. Adobe's live FAQ could not be read on 2026-11-04, and nothing
  official covers mods. Downloaded FBX files stay outside the repo.
- **Every release carries the Squatter.** The user decided on 2026-11-04 that public releases don't drop features, so the compiled clips ship
  inside the mod. A release build fails if they are absent. Credit Mixamo in `NOTICE` regardless, as a courtesy.
- **Failing.** If the Squatter's files or a required entry can't load, the Custom Ghosts mod is disabled and the error is written to the log, as
  for any mod with an error (user, 2026-11-06). No workaround keeps the other ghosts running.

## Player-facing text

The name "Squatter" is the user's. The journal text (description, strength, weakness) goes in `mods/custom_ghosts/i18n/default.json`, marked
`PM_TEXT_TODO`, and listed for the user. AI does not write it. Spoken lines are
audio, recorded by the user.

## Testing

Tests first, as `CLAUDE.md` says. The pure parts belong in a `squatter_rule.h` with offline tests, in the manner of `shuck_rule.h` and
`knell_rule.h`: the chase state (idle, chase, search, at a hiding place, door locked) and the friendly one (seated, excited, following), target
choice, the speed from a given sprint speed and move, when the door locks, and which line set plays. The sheet mesh generator is pure too. `.\validate.ps1` each task, and `.\tools\gamecheck.ps1 -IfConfigured -Expect` for moved values.

Every in-game check goes on the smoke list. Only the user launches the game, told the ban risk first. No loader file goes in the game folder.

## Left to the builder

Task order after the throwaway test, file layout, line timing, how long moves last and how often they play, what sets off a bump, exact speed
margin and each move's speed (tuned with the user in game), how the sheet attaches and how stiff it is, and how the master tells other clients which move and line to play (through the SDK's
one channel).
