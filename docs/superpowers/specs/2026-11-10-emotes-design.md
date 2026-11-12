# Emotes: design

Decided with the user on 2026-11-10. Items marked **(builder's call)** are not the user's decisions and may be changed by
whoever plans this. Items marked **(Claude's call)** were chosen while writing this design and are listed for the user to
check. Everything else is the user's.

## Why

Players want to dance for each other, for the humour of it: an investigator breaking into a dance in a haunted house,
holding a crucifix, while the ghost listens. The user wants this as a bundled mod, and wants the SDK to give any mod
author enough to add emotes of their own (decided 2026-11-06), so the general parts belong in the SDK and only the
wheel, the eight emotes and their sounds belong in the mod.

## Goals

- **An emote wheel on a key.** Holding the key (B by default) opens the wheel; the mouse picks an emote; releasing the
  key plays it.
- **The player sees themselves dance.** The camera eases out to third person for the emote and back to first person
  after.
- **Everyone sees and hears it.** Every client plays the emote on the emoter's body and plays its music from where the
  emoter stands.
- **The ghost hears it as it hears a voice,** through the game's own talking-loudly check, so dancing near the ghost is
  as risky as shouting near it.
- **The SDK gains the general parts,** documented for community authors: a rebindable key in the game's keybinds menu,
  playing a clip on a player's own body and handing it back, a third-person view of the local player, and adding a
  sound to a player's talking loudness.

## Non-goals

- More than eight emotes, pages of emotes, or choosing which emotes fill the wheel in game.
- Any new rule for the ghost. The ghost hears the emote only through the check it already uses for voices.
- Changing what the emoter's voice chat does. Talking while dancing works as it always does.

## Anti-goals

- **The living seeing or hearing a dead player's emote.** A dead player's emote follows the game's rule for a dead
  player's voice.
- **A ghost that hears an emote by a rule of our own.** If the game's voice test would not hear a voice there, it does
  not hear the emote there.
- **Bodies that disagree.** Other players must see the emoter's body face the same way and stop at the same moment as
  the emoter does, within a little network delay.
- **A body or camera that snaps.** Starting, ending and switching emotes blend; the camera eases both ways.
- **Touching the game's networking.** No game RPC is sent, no networked object is created, and no check on who sent an
  RPC is weakened. Everything an emote shares goes over the mod's own channel.

## The assets (the user's, still to come)

AI makes none of these. The user provides:

1. **Eight clips,** chosen from `dance-animations.unitypackage` (kept in the private repo, `local/.mixamo/`). These are
   custom emotes made in Fortnite's style, not taken from that game (user, 2026-11-10). The user also says which
   segment each one sits in.
2. **One sound per emote.**
3. **One icon per emote:** a white silhouette, unique to its animation.
4. **Each emote's name,** as it appears under the wheel, and the **keybind's label** in the game's keybinds menu. Both
   are player-facing text and go in the mod's `i18n/default.json`. Until the user writes them they are `PM_TEXT_TODO`
   entries listed in the ledger.

The package holds humanoid clips (they carry `RootT` and muscle curves), so they retarget onto the investigator's
humanoid body as the Squatter's Mixamo clips do (`sdk/guides/custom-animation.md`). The package was made in Unity 2017;
the clips are imported into a Unity 2022.3.40f1 project in the private repo and built into the mod's bundle with the
sounds and icons, pinned and published as the Squatter's bundle is (`tools/payload.psd1`). Whether a clip loops is read
from the clip itself (its Loop Time), not written into the code.

The work can be planned and mostly built before the assets arrive, against a test bundle built from any eight clips in
the package. The mod is not released until the user's assets replace them.

## The wheel

The user's design. It is new UI that the user designed, so the vanilla-UI rule is satisfied by their decision; it needs
no `PM_UI_TODO`.

- A **grey disc with a centre circle,** split into **eight equal segments** with a **thin transparent gap** between
  each, so the game shows through the gaps.
- Each segment shows its emote's **icon.**
- Behind the whole wheel, the game gets a **slight blur and a vignette.**
- The **picked segment turns blue,** and the picked emote's **name appears in white text under the disc.**
- The exact grey, blue, gap width, disc size, blur and vignette strength are the user's to set. The builder uses
  placeholder values the user adjusts in game, kept in one place so they are easy to change **(Claude's call)**.

### Using it

1. **Holding the key opens the wheel.** The default key is **B**. It is remappable in the game's own keybinds menu
   (see "The SDK's part").
2. **The mouse picks by direction.** Moving the mouse up picks the top segment, and so on round the disc. The pick
   follows the mouse's movement since the wheel opened, not the cursor's position on screen **(Claude's call)**.
   Until the mouse has moved past a large dead zone in the middle, nothing is picked, so moving back toward the centre
   lets go of a pick easily (the user, 2026-11-10: it was small). The dead zone's size is a starting value
   judged with the user in game **(builder's call)**.
3. **While the wheel is open the mouse does not turn the camera** **(Claude's call)**.
4. **Releasing the key plays the picked emote.** Releasing it with nothing picked plays nothing and closes the wheel.
5. **The wheel can be opened during an emote.** The emote keeps playing; only releasing the key on a segment replaces
   it **(Claude's call)**.
6. The wheel does not open while the game's pause menu, journal, or any text field has the keyboard **(Claude's call)**.

### On a gamepad

The user's (2026-11-10):

1. **Holding D-pad down opens the wheel,** unless the game already binds D-pad down. Then the builder picks a gamepad
   button the game leaves unbound, from the game's own bindings, and reports it to the user **(builder's call)**. It is
   rebindable in the keybinds menu as the keyboard key is, if that menu rebinds gamepad buttons.
2. **The right stick picks by direction,** held toward a segment. While the wheel is open the right stick does not turn
   the camera **(Claude's call)**.
3. **Releasing D-pad down plays the emote the stick is held toward.** With the stick at rest, nothing plays and the
   wheel closes.

### In VR

The user's (2026-11-10):

1. **A button on the left controller that the game leaves unbound opens the wheel.** The builder finds which buttons
   are unbound, from the game's own VR bindings, picks one and reports it to the user **(builder's call)**.
2. **The wheel appears as a widget over the left hand,** the same disc, icons, highlight and name as on screen, without
   the screen-wide blur and vignette **(Claude's call: a blur over a VR view has no screen to sit on)**.
3. **The right controller picks by pointing** at a segment; the pointed-at segment turns blue and shows its name.
   **Pressing the right trigger plays it.**
4. Pressing the left button again, with nothing played, closes the wheel **(Claude's call)**.
5. **A VR player's view never changes** (user, 2026-11-10): no third person, and the view does not follow the
   dancing body's head, because emotes with a lot of movement, such as breakdancing, could cause nausea. The headset
   alone places the view, as always.
6. **A VR player's own body renders during the emote** (user, 2026-11-10), so they can see themselves dance by looking
   at it, including any part the game hides from its own VR view. It is hidden again when the emote ends. If the
   body's head would sit over the view and block it, the head stays hidden **(Claude's call)**.
7. The body-facing rule below uses the headset's turn in place of the camera's **(Claude's call)**. Moving more than
   0.5 m, in room scale or by stick, ends the emote as it does on screen.

## When a player may emote

**At any time** (user, 2026-11-10): in the lobby, in the truck, in the house, during a hunt, alive or dead, holding
anything or nothing.

- **Held items stay in the hand(s)** and move with the dance. Some will look janky; the user wants that kept, for the
  humour.
- **The dead.** A dead player may emote. Other dead players see and hear it; the living neither see nor hear it. This
  mirrors the game's own rule for a dead player's voice (`VoiceVolume.Update`, `VoiceVolume.deathMultiplier`, in
  `sdk/systems/voice.md`), which the builder confirms and reuses rather than restating.
- **The Playable Ghost's player** has no body in the contract, so the wheel does not open for them **(Claude's call)**.
- **Dying** ends an emote and no emote starts: from the kill's first frame the game's death animation has the body, so
  the dance leaves with nothing left blending and the wheel is taken away (the user, 2026-11-10, after being killed
  mid-dance with no death animation at all). The game's mark of the death, `Player.isDead`, only comes when the death
  completes, so the mod watches `Player.NotifyOfDeathStart` instead. A player whose death is over may emote again.

## Playing an emote

- The emoter's client starts it at once and tells every other client, over the mod's own channel, which emote started,
  where, and facing which way. Each client plays the clip on the emoter's body and the sound from the emoter's body.
- **Starting** blends from the body's current pose into the clip. The blend time is a starting value judged in game
  **(builder's call)**.
- **The sound** plays from the emoter's position and falls off with distance as a voice does **(Claude's call: the voice
  source's own volume, spatial blend and rolloff, `PlayerVoice.localTap` in `sdk/systems/voice.md`)**. It does not go
  over a walkie-talkie **(Claude's call)**. Its volume against voices is a starting value judged with the user.
- **A one-shot clip** plays once. If nothing has ended it, the body blends back to normal and the camera returns to
  first person when it finishes.
- **A looping clip and its sound loop** until something ends them.

### What ends an emote

All the user's (2026-11-10) unless marked:

- **Moving more than 0.5 m** from where the emote started. Measured on the emoter's client, across the floor
  **(Claude's call: horizontal distance, so a stair or a small drop is not a move)**.
- **Starting another emote.** The new one cuts in at once, blending from the old pose.
- **Using or placing any item.**
- **Picking up, dropping or throwing an item.**
- **Opening or closing a door, or flipping a light switch.**
- **Crouching.**
- **Not** switching which item is held.
- Dying (above).

Any other action leaves the emote playing **(Claude's call)**. Which game members mark each of these actions is
reverse engineered and cited by map key; none is assumed.

**Ending** blends the body back to its normal animation and eases the camera back to first person. The emoter's client
decides the end and tells the others, who blend out too.

## The camera

This section is for keyboard and mouse and gamepad. VR keeps its own view (see "In VR").

- **Starting** zooms the camera straight back from first person to a third-person view behind the body, with the
  character still facing away from the camera (user, 2026-11-10): the camera keeps looking the way the player looked,
  and the body does not turn to face it. The distance, height and
  ease time are starting values judged with the user in game **(builder's call)**. The camera does not pass through
  walls: it comes in closer where a wall is behind the player **(Claude's call)**.
- **The mouse turns the camera freely** during the emote.
- **The body's facing** (user, 2026-11-10): call the direction the body faced when the emote started its start heading.
  While the camera is within 90 degrees either side of the start heading, the body does not turn. Past 90 degrees the
  body turns to keep the camera exactly 90 degrees from it, on the side the camera was turned. So with the camera turned
  right round to 180 degrees, the body faces 90 degrees left or right, depending on which way the camera went to get
  there. **(Claude's call)** for what happens after the body has turned: the body keeps whatever facing it has reached,
  and turns again only when the camera goes more than 90 degrees from that facing. Turning the camera back toward the
  start does not turn the body back. This gives the user's two examples and lets a player keep turning the camera all
  the way round, with the body following 90 degrees behind.
- Other clients see the emoter's body turn the same way.
- **Ending** eases the camera back to first person, looking where the third-person camera was looking **(Claude's
  call)**.
- While in third person, the local player's whole body is shown to them, including any part the game hides from its own
  first-person view; it is hidden again on return.

## How the ghost hears it

Through `PlayerAudio.IsTalkingLoudly` (user, 2026-11-10). The game asks it in two places, both already mapped:

- the hunt's voice test, `GhostHuntState.Hears`, with its own range, floor and room rules (`sdk/systems/ghost-ai.md`);
- the idle state's check for talking in a room, through `LevelRoom.ArePlayersTalkingInRoom`.

The emote's sound adds to the emoter's talking loudness, so both checks treat it as the player talking, with every rule
they already apply. Where `PlayerAudio`'s loudness is written, and on which client the ghost's checks read it, is
reverse engineered first. How loud the sound counts as is read from the sound's own samples as it plays
**(builder's call)**.

## The SDK's part

Each of these is a documented SDK feature with map entries for every game member it uses, offline tests, and an
author-facing page, so a community mod can add its own emotes or keys with no reverse engineering:

1. **A rebindable key in the game's keybinds menu.** A mod declares a key with a default per device (for Emotes: B,
   D-pad down, and an unbound left-controller button in VR) and a label from its string table, and learns whether its
   gamepad or VR default is already taken by the game; the key appears in the game's own keybinds menu, the player can rebind it there, and the binding is
   saved and read back as the game saves its own. The game uses Unity's Input System (`InputActionAsset`,
   `InputActionMap`, `InputAction` in `sdk/map/Input.h`). How the keybinds menu builds its rows and how it saves a
   rebinding is reverse engineered first.
2. **Playing a clip on a player's own body and handing it back,** for the local player and for other players' bodies,
   with a blend in and out. The Squatter's helpers play clips on a script-free copy of the body (`sdk/api/models.h`);
   this is the live body, whose own Animator and scripts are driven by the game, so the hand-back must leave them exactly
   as they were.
3. **A third-person view of the local player,** with the eased transitions above.
4. **Adding a sound to a player's talking loudness,** as heard by `PlayerAudio.IsTalkingLoudly`.
5. **The emote's network message,** over the SDK's one mod channel (`send` and `set_receiver`).

Where the line between the SDK and the mod falls inside each is **(builder's call)**, keeping to the rule that the
loader does all interfacing with the game.

## What is reverse engineered before it is built

No behaviour is assumed. Each item ends as map entries or notes in `sdk/systems/` and `sdk/internals/`:

- How the investigator's body is animated on its own client and on others' (its Animator, controller, layers, and the
  scripts that set its parameters), and how a clip can play over it and give it back.
- Where held items attach on the body, in first person and in third person, so they stay in hand during an emote.
- What the local player's first-person view hides of its own body, and how to show it.
- How the player's camera is placed and turned, and how the game turns the body with the camera, on the player's own
  client and as other clients see it.
- Where `PlayerAudio`'s loudness comes from, and which client's copy the ghost reads.
- How the game marks each action in "What ends an emote".
- The keybinds menu's rows and its saving of bindings, for keyboard and gamepad.
- The game's gamepad and VR bindings: whether D-pad down is taken, which left-controller buttons are free, and how the
  right stick's look input can be paused.
- In VR: where the left hand's anchor is for the widget, and how the game's own VR pointing at UI works, so the right
  controller picks a segment the way it points at the game's menus.
- A dead player's voice rule, to reuse for the emote.

## Constraints

- The project's rules apply throughout (`CLAUDE.md`): map entries for every game member used, no invented game
  behaviour, offline tests first, `validate.ps1` and the game check pass, private lobbies only, every player in a lobby
  runs the same mods (so no "does every player have Emotes" check in the mod).
- The mod is a new bundled mod with the id `phasmomods.emotes` and no dependencies **(Claude's call)**. It does not
  record voice.
- The bundle carries no scripts: clips, a controller, sounds and icons only.
- Keyboard and mouse, gamepad, and VR are all supported (user, 2026-11-10), as "Using it" describes.

## Left to the builder

- Every starting value named above: blend times, the camera's distance, height and ease, the wheel's dead zone, the
  sound's volume, and the wheel's placeholder colours and sizes.
- The order of the work, and the split between SDK and mod.
- How the wheel is drawn, within the user's design.
- What the smoke list asks the user to check in game.

## Decisions after approval (2026-11-10)

- **The songs' loops** (user): a looping emote's song loops with exactly its animation's length, except where the
  author says otherwise. Floss's and Take The L's songs loop on their own: the songs mark no loop point, so they were
  measured from the tracks: Floss every 16.692 s from its start, Take The L every 12.631 s from its start. The song is
  timed from the clip's own clock, so the two never drift.
- **The assets** (user): the eight emotes are the folders the author supplied, matched to the package's clips by name;
  their names under the wheel are the author's song titles; they sit clockwise from the top in alphabetical order
  (Claude's call).
- **Every clip loops** (Claude's call): the package sets Loop Time on none of the eight, but each is a dance with a
  two-minute song that repeats until the player moves.
- **The keys' defaults** (Claude's call, by the user's "unless that key is taken" rule): the game binds B (the
  walkie-talkie) and D-pad down (the torch), so the defaults fall back to X and D-pad up; the left X button in VR is
  unbound. All rebindable in the game's keybinds menu.

## Decisions after the first playtests (2026-11-10)

The user played the mod in game and gave these.

- **The volume again** (user, twice on 2026-11-10): still too loud at the slider's default, so what the slider's 100%
  plays was halved, and halved again after the next run. Then (user, 2026-11-10): every song plays at 0.05 at the
  slider's 100%, the dancer's own and everyone else's alike, with no difference between them. The dancer hears their own
  song with no fall with distance, since it plays at their own voice's place; others hear it fall as a voice does.
- **The wheel's neutral zone** (user, twice on 2026-11-10): the dead zone in the middle is large, so a pick is easy to let
  go of, and the mouse's movement since the wheel opened is capped, so the way back to the middle is short after any
  flick. Both sizes are starting values, judged in game.
- **The slider's name** (user, 2026-11-10): "Music".
- **One line between the rows** (user): "the double lines are the problem. Each row should have one line between them",
  and "the keybind page looks perfect". The first fix copied the keybinds page's thin line between the mod's rows only;
  the user's next screenshot still showed doubled lines where the audio page's own dividers flank a row it switches off,
  and a missing line where the game groups two of its rows with no divider at all. So the SDK lays out the whole page:
  exactly one line between every pair of visible rows, the game's rows included, put away again when the mod's rows go.
- **The emote's name in the game's font** (user): the name text copies its font from a text under this client's own
  player rig (the journal, the HUD), not from whichever text the scene happens to hold.
- **Death ends the dance at the kill's start** (user): "I got killed while emoting and the death animation didn't play.
  Death should cancel emoting and return to first person", and "emoting should not be possible while the death animation
  is playing either". The mod hooks the game's notice of a kill beginning, `Player.NotifyOfDeathStart`, which every
  client gets; from there the body is the game's, the clip is off with nothing blending, the view goes back inside at
  once and the wheel is shut.

## Decisions for the second round (2026-11-12)

The user's, for pages, emote packs, the loop tool and walking emotes. The plan is
`docs/superpowers/plans/2026-11-12-emotes-pages-packs-walking.md`; it lists the builder's calls beside these. This lifts
the non-goal "more than eight emotes, pages of emotes".

- **Pages.** The wheel has more than one page, turned with the scroll wheel or the left and right bumpers. It opens on the
  page last viewed in this session; nothing is saved, so a restart opens on the first page.
- **Showing the page.** The page number sits at the top of the wheel with a glyph for each page control on either side.
  The game's own LB and RB glyphs are reused. The game has none for the scroll wheel; the user supplies that glyph
  (`PM_UI_TODO` until then). The page text's wording is the user's (`PM_TEXT_TODO`).
- **Other mods add emotes,** each shipping its own bundle and catalogue.
- **No props or particles.** The dance packages hold body-only humanoid clips.
- **Walking emotes.** Walking forward plays the emote's moving clip at the game's own walk speed, and stopping goes
  back. Forward is the player's input: the W key or up on the move stick, forward only, not a diagonal. Sideways or
  backward past 0.5 m, sprinting, crouching and the usual actions still end it. The camera stays in third person.
- **Intros.** An emote may play an intro clip and an intro song once before its looping clip and song.
- **Eight more emotes:** Build Up, Get Griddy, Lavish, Never Gonna, Running Man, Savor The W, Sprinkler and Stride, with
  the user's icons and songs; Never Gonna uses the second package's start and loop clips and its song. The user's rough
  song loops: Lavish 28 s, Running Man 15 s, Savor The W 15 s, Sprinkler 22.5 s, Stride 17 s.
