# Playable ghost: design

Status: in development. This records the agreed behaviour; the code is the authority on what is
implemented.

## Goal

Any player in a private lobby can play as the ghost, and the investigators can still identify its
type the normal way. Every type-specific behaviour (speed, acceleration, line-of-sight speed-up,
hunt thresholds, cooldowns, event restrictions, abilities, evidence) comes from the game's own
code. The mod changes who chooses *where* the ghost goes and *which* action it takes, never the
rules those choices are subject to. It should feel like part of the game: in-game UI built from the
game's own UI elements, the game's own key bindings, no mod menu.

## Distribution

The playable ghost is one mod of a suite (`docs/superpowers/plans/2026-10-20-mod-suite.md`):
`PlayableGhost.dll` is started by the suite's host, `ModHost.dll`, which a portable Windows x64
launcher (`PhasmoMods Loader.exe`) loads from outside the game folder after the game reaches its main
menu, together with whichever mods are ticked in it. Steam command wrapping provides automatic
loading; the launcher also offers attachment to a running game. See `installation.md` for setup
and verification limits, and `loading-investigation.md` for the evidence behind this choice.

The host does not depend on how it is loaded: it starts from `DllMain` on its own thread, waits
for the IL2CPP runtime, verifies the exact GameAssembly.dll and UnityPlayer.dll hashes and stays
inactive on any other build. Startup waits share a 60-second deadline. On the first main-thread
frame it shows the consent notice, as a copy of the main menu's own error popup (ban risk, voice
recording when a mod that records voices is on, no liability for lost progress, every installed mod
named); only "I agree" starts the mods, and it is remembered against the notice's exact text. Every
installed mod is loaded; launcher.ini says which start, and the main menu's Mods panel restarts
them with a new choice (each mod's hooks removed newest first, then the chosen mods started). The host owns the only MinHook instance (a second hook on the same function
chains), the frame tick, and Photon event 197, on which each mod has its own channel and every
host announces which mods, in which builds, it runs. A modded lobby admits only matching players
(host/join_guard.h): a joiner whose mods differ leaves by itself and says why, and the host kicks,
with the game's own kick, a mismatched player who stays and a player without the mod when a running
mod lacks PG_MOD_VANILLA_PEERS. Every feature is inactive outside single player and private
(unlisted) rooms. `PG_Shutdown` requests a stop; native modules and detour storage remain until
process exit. Restart the game to load another version. Every player in a modded lobby runs the
same mods in the same builds. Existing gameplay rewards, including ghost money and XP, are retained.

## Choosing the ghost player

Each player's own lobby ID card gets a "Play as ghost" toggle: a half-size copy of the game's Ready
tape, stuck across the card's bottom edge (the lobby board has no free space elsewhere). Nobody has
to do anything else, and the host has no lobby controls: the host's client draws a random volunteer
in the lobby, only among those who have played the ghost least often in this room, so the role
rotates. It draws again whenever another volunteer ticks in, so ticking first gives no advantage;
the draw is shown on the drawn player's tape ("You are the ghost") and made again after
every contract. Drawing in the lobby matters: the ghost player's client must know before the
contract loads, so it brings no investigator and no equipment. With no volunteers the game is
vanilla. The choice travels as Photon events on the
existing room connection (event code 197, see `mods/playable_ghost/session.h`). The host's client runs the real
ghost; a non-host ghost player sends its input to the host.

## Contract start: type and favourite room

* The host's native ghost spawn (`GhostController`, run once every player has loaded) is held while
  the ghost player chooses. The host plays on normally; nothing waits on the host.
* To know exactly which types the native weighted roll could produce (difficulty, seasonal and save
  filters plus the saved weights), the held spawn is run once as a probe: it is unwound with a
  managed exception from inside its own type roll, before it writes or sends anything.
* The ghost player chooses on their own journal (see "The ghost player's journal"), held open on its
  evidence spread. Behind it their camera stands in the chosen favourite room and turns slowly, lit
  by a light of its own, the way the haunted mirror shows the ghost's room (before a choice, the
  level's first candidate). The right page is the journal's own ghost page: they click the ghost
  they will be, which is ticked, and types the native roll could not produce are crossed out (a type
  forced by a custom difficulty is the only one not crossed out). The left page, in place of the
  evidence page, is the truck's map: rooms under the mouse are shaded, the chosen one is filled in
  journal red, and the arrows change floor. Ghosts under the mouse get the journal's own hover
  highlight, and the ghost page's heading reads "Ghost type" while choosing. "Begin the haunting"
  sends both. Once the ghost exists, the HUD names its type and favourite room. Escape switches to the
  journal's pause menu and back. If the journal or the map is unavailable, two plain lists driven
  by the movement keys and Interact are shown instead. Afterwards the ghost page shows the
  investigators' own choices again.
* As soon as both are confirmed, or after 30 seconds, the native spawn runs. Only two native rolls
  are answered by the choice: the weighted type roll (if the native roll could have produced that
  type) and the favourite-room roll. Anything not confirmed is left to the native rolls. Evidence,
  forced evidence, traits and everything else follow from the type natively.
* Until then the truck's door stays shut: the host refuses the truck's own door button
  (`LiftButton.AttemptUse`, which the host runs for whoever pressed it) and that investigator gets a
  popup saying the ghost is still being chosen, with the time left. Nobody is moved. The house's
  main door is not touched. The truck's first opening is also the game's own start of the
  investigation (it warps the ghost to its room and places the bone), so it needs the ghost to exist.

## The ghost player's journal

The journal hangs off the investigator's networked body. Just before that body leaves the contract
the ghost player's client takes the journal off it, so the ghost player keeps the game's own journal,
pause menu included; every other client destroys its copy with the body. Nothing of it reaches the
network again. Its network views stop synchronising and no longer count as this client's, so the
journal's own code turns pages and changes tabs locally instead of sending them. Any message still
sent from it is dropped.

* The Journal key opens it and Escape opens it on the pause menu, as for an investigator. It is held
  in front of the ghost player's camera, and the ghost holds still while it is open.
* Opening and closing are the game's own steps, run locally: the journal's own (its handler stops
  early for a client without an investigator) and the investigator rig's (switched off for the ghost
  player), including moving the pages onto the layer the game draws held items' UI on. The book is
  held where the game holds an investigator's, so it looks and draws as theirs.
* The investigators' evidence and ghost choices appear on its pages as they make them: the game
  shows them in every investigator's journal, and the mod adds the ghost player's. The evidence tab
  (evidence and ghost ticks) is view only; the rest of the journal is usable.
* The pause menu's Leave and Quit read the investigator, so for the ghost player they run the
  game's own leave (load the main menu, then disconnect) and quit without it.

## Favourite room changes

The native rules decide when the favourite room changes (the difficulty's setting, its odds, the
team's sanity), and the change happens then, exactly as in vanilla. For the next 20 seconds the
ghost player may choose another room instead: the HUD says so, and the Journal key opens the map
page of their journal, where any room can be chosen (without a journal, Crouch takes the room the
ghost is in). The claim goes through the same native change; the second change is taken back out of
the contract's statistics, so it still counts as one.
No change ever happens when vanilla would not make one.

## Winning and rewards

As in vanilla: each investigator makes their own journal guess and sees the normal results. The
ghost wins by killing every investigator; the contract then ends the game's own way.

The ghost player is paid only when every investigator died: the contract's own main objective
reward (what investigators earn for naming the ghost) scaled inversely to the difficulty's reward
multiplier (`Difficulty.GetRewardMultiplier`), so a harder contract, which is easier for the ghost,
pays the ghost less. A multiplier of 0 pays nothing, the scale is capped at 3x, and experience
equals the money (`mods/playable_ghost/payout.h`). It is paid through the game's own payout on the reward screen,
with a popup saying what it was for.

## The ghost player

* The ghost player is not an investigator. Their investigator (the networked player the game
  creates in the lobby and carries into each contract) is destroyed for the contract once every
  player has loaded, so every client's player slot for them is empty and the game's own rules
  (team sanity, targeting, deaths, the all-dead check, rewards) never see them. Their local rig is
  switched off and its camera detached to become the ghost's eyes. Back in the lobby the game's own
  `CreateNetworkedPlayer` gives them their investigator again.
* The end of a contract (`ExitLevel.Exit`, also used when everyone has died) needs the local
  investigator, so the ghost player's client settles the payout and returns with the game's own
  `GameController.LoadBackToMenu`.
* They bring no equipment: while they are the ghost player their loadout is withdrawn from the
  lobby (`InventoryManager.RemoveMyEquipmentFromLobby`) and adding to it is refused; afterwards the
  game re-equips their saved loadout (`LoadoutManager.CheckEquipped`).
* They see from the ghost's head with their own camera, turned with the mouse.
* Movement uses the game's movement bindings relative to where they look. Native speed, acceleration
  and avoidance are kept by replacing only the steering direction inside Unity's crowd simulation
  (`mods/playable_ghost/steer_stub.asm`); the destination stays the native state's own. The ghost moves only where a
  vanilla ghost moves (`mods/playable_ghost/tether.h`): in a wander (states 1 and 21, a hop of the state's radius,
  usually three metres, or as far as the point it chose, as a Banshee's towards its target) the
  player walks anywhere within that wander's reach until it ends natively; in a hunt the player
  steers freely. Between wanders, as a vanilla ghost does, it holds still. The native AI moves it
  while it is drawn home, blinded by smudging, in an event or using its ability (Fairness 2).
* Line of sight: the hunt's speed-up and its decay stay native (hunt state +0xB4 grows while the
  target is seen and decays once its search is over), and so does every type's own speed rule. A
  vanilla hunt that loses its target walks to the last sighting and searches there; a ghost player
  walking elsewhere has that search end where they are once a vanilla ghost, walking the path at its
  native speed, would have arrived.

## Actions

Pacing and the mix of actions are the native idle roll's (every 2-6 s, with its type and difficulty
odds). Each time it would act, the ghost player is given one action of the kind it chose, up to two
of each: an interaction (`GhostActivity.TryInteract`), or what `GhostActivity.GhostAbility` picked
(an event, the type's ability, or the fuse box, including its Shade, Hantu and difficulty rules).
Each action is then used through the native entry, and the roll's gates are checked again at that
moment: no interaction before the front door is unlocked, no event or ability while a Shade is
watched or the ghost is sent home (GhostAI +0x10A), no event while `LevelValues +0x23` is set.

* **Interact:** look at a door, light switch or prop and use the interact binding. The host pulses
  the native interaction sensor narrowed to that object, which picks the native door, light, throw
  or object state, so type rules inside those states apply. Only objects in the ghost's native
  interaction range qualify. A rarely touched category costs more (base odds / type odds); a throw
  with nothing throwable aimed at costs nothing. For the Twins (TryInteract: 0-4 of Random(0, 10) the
  ghost alone, 5-7 the twin alone, 8-9 both), the aimed interaction is the ghost's own and the twin
  follows it at the twin's vanilla rate, one time in two, through the native `TwinInteraction` a
  second or two later.
* Actions start only from idle or a wander, never in the middle of another action, a hunt or a
  native walk.
* **Event:** the native event entry (`GhostAI.ChangeState(randomEvent)`), which chooses the event
  type and target with its type restrictions (e.g. Oni never airballs, a Shade rarely shows).
* **Ability:** the native ghost ability entry (`ChangeState(GhostAbility)`), offered by name only
  for the seven types it starts a state for (Wraith, Phantom, Poltergeist, Jinn, Demon, Yurei,
  Obake), with a description in the journal (`mods/playable_ghost/ability.h`). The Jinn's needs the power on.
* **Fuse box:** the torch binding switches the power from anywhere through the native fuse box
  state, as a vanilla ghost does: EMF on the box, a Jinn never turns it off, a Hantu never on, and
  turning it on works one time in five.
* **Hunt:** the native hunt branch runs untouched, its final 1-in-3/4/5 chance included. When it
  would hunt, the hunt is offered to the player instead until the roll next reaches its hunt check,
  and only while the native checks (sanity threshold, cooldown, smudge) still pass. The HUD shows
  the investigators' average sanity (or, while the ghost is focused on one player, that player's,
  as the native check uses) with a mark at this ghost's hunt threshold (the native check's own type
  and difficulty modifiers, learned from its idle rolls), red once the sanity is low enough.
  Crucifixes are not checked beforehand: the attempt enters the native hunt, which burns a crucifix
  in range and starts the cooldown as normal.

## Fairness

1. **Restlessness:** once the player has not tried to act for 30 s (HUD meter), the native roll acts
   on its own again, hunts included, until they next try. Passive play cannot starve the
   investigators of activity. What the player has saved up survives a short gap in their input.
2. **Room tether (vanilla):** every 2-6 s that the native idle roll does not act, it either
   wanders a short hop or walks the ghost back to its favourite room. Both stay native: the player
   walks the hop within its reach, and the walk home is the native AI's own (its path and speed, no
   actions). A ghost that may not wander (`TemporarilyStopWander`: the Hermit card, smudging a
   Yurei) has every wander turned into the walk home by the native wander state itself.
   Smudging in a hunt blinds the ghost natively (the native AI moves it while the incense lasts);
   smudging outside a hunt, a hunt's cooldown or restored sanity take back an offered hunt. The
   Tower card's interaction boost lets the native roll act on its own; the Devil's event and the
   Death card's hunt run natively.
3. **Native evidence:** autonomous evidence (orbs, freezing temperatures, writing, spirit box,
   D.O.T.S. walks, prints on native touches) is never suppressed.
4. **Ghost senses:** no information the ghost would not have. The HUD shows hunt availability,
   restlessness, tether, the Banshee target, the sanity the hunt check measures and the type's own
   senses (below); never crucifixes, salt or smudge timers.
   The ghost player's microphone is muted through Vivox for the whole contract. While the ghost is
   chosen they hear nothing; afterwards they hear from the ghost's head (the listener is on their
   camera, so investigators' positional voices arrive with the game's own falloff), and
   walkie-talkie voice is muted for them. Dead investigators are silent for them, as they are for
   the living.
5. **Type ability prompts:** the HUD names the type's native ability, only for a type that has one,
   and dims it while the native conditions do not allow it.
6. **Telegraphing:** flicker, footsteps, ghost audio, speed changes and every counter stay native.
7. **Not an investigator:** with no investigator in the contract the ghost player never counts in
   the team's average sanity (which drives hunts and favourite-room changes) or the all-dead check.
8. **No equipment, harder pays less:** the ghost player brings nothing to the truck, and the ghost's
   payout falls as the difficulty rises.
9. **Protections stay hidden:** salt piles and crucifixes are not drawn on the ghost player's screen
   (`mods/playable_ghost/hide.cpp` switches their renderers off on that client only), so they cannot
   be steered around on purpose. They still work natively: a hunt burns a crucifix in range, and the
   ghost steps in salt where its path crosses it.
10. **Deception score:** the results screen shows the ghost a score, +1 for each investigator who died
    and +1 for each who named the wrong type (`mods/playable_ghost/score.h`). Each investigator's
    client sends its journal guess as it changes; the score is shown to everyone a few seconds after
    the contract ends. Money is unchanged and investigators still win the vanilla way, so misleading
    them (playing shy like a Shade, singling one out like a Banshee) pays while hard tells stay real.

## Senses and alerts

Each type perceives what its vanilla ghost reacts to, read by the host from the game's own state and
sent to the ghost player five times a second (`mods/playable_ghost/perceive.h`, `marks.cpp`,
`perceive_view.cpp`):

* **Kormos:** in a hunt (when its hearing, GhostHearing, pulses), the footsteps it detects, as
  ripples sized by walking, sprinting or creeping; its view darkens, so only what it hears shows.
* **Deogen:** every investigator during a hunt, as the vanilla Deogen always knows where they are.
* **Dayan:** the investigator its sight check watches (within 10 m), marked as moving (it is
  empowered) or holding still (it is weakened).
* **Raiju:** in a hunt, the electronics its own sensor detects.
* **Yokai:** in a hunt, investigators' voices farther than 2.5 m fall silent.
* **Black Shuck:** in a hunt, the scent within 8 m that it could pick up (through the Custom Ghosts mod);
  its view darkens as the Kormos's does, so only the scent shows.
* State lines under the type: Gallu rage, Dayan strength, Obambo mood and how long it has held,
  Thaye age, Deildegast speed, Raiju charge, and the custom ghosts' own (`typestate.h`).

Alerts tell the ghost what was done to it, with a direction and distance when it happened somewhere
(`alerts.cpp`; nothing used from anywhere, such as the Ouija board, voodoo doll or monkey paw, gives a
place; an action the game rejects raises nothing): its name spoken, the spirit box or Ouija board answering (with the Ouija's answer),
the music box, the summoning circle, the Tower, Devil, Death and Hermit cards, the voodoo doll, the
monkey paw, the haunted mirror, a crucifix burned, salt stepped in, and being smudged (no timer).
Every client hooks the game's handlers; the host relays what only it runs, and the ghost player's
client shows each once.

## The Mimic's copies

The game switches a Mimic's copy on its own timer to a random type it may copy. The ghost player is
offered those types and picks the next copy with Crouch (or leaves it to the game); the pick goes
into the game's own override, so the copy changes exactly when it would have
(`mods/playable_ghost/mimic.cpp`).

## Disguise

Any type may take an investigator's form as an event: Crouch held while tapping Primary Use. It costs
an event charge under the event's own checks, and the investigator is the one nearest where the ghost
looks. The game's own player-shaped event (`GhostEventPlayer`) is only a particle cloud, so every
client but the ghost player's bakes that investigator's body as drawn there into a still figure that
stands and turns with the ghost for five seconds (`mods/playable_ghost/form.cpp`). Copying voices
belongs to the Echo alone (Custom Ghosts).

## Companion mods

Features beyond the playable ghost are separate mods, each optional in the launcher. The README
lists them.

Bigger Lobbies (private rooms for eight) was archived to `archive/bigger_lobbies` on 2026-10-20.
The game's Photon servers cap a room at four players, so it cannot work. Its README there gives
the details.

Replay (the contract's top moments shown in the lobby) was archived to `archive/replay` on
2026-10-22, together with an approved design and plan to film those moments in the house and play
them on the lobby truck's monitor. Its README there says how to bring it back.

Afterlife (a dead investigator watching through the ghost's eyes) was archived to
`archive/afterlife` on 2026-10-23, before it had run inside the game. Its README there says how to
bring it back.
