# Dev menu: content plan

Date: 2026-11-10. Branch: `feature/dev-menu` (from `development` at 6ef3dad).
This is the content plan: what the menu holds and what each entry can rest on. It is not yet the
build plan. The user answered the plan's six questions on 2026-11-10 (recorded under Decisions at
the end) and added two standing requirements: protected progress, and testable multiplayer-only
features. What is left open is only the RE spikes the plan already names.

## What it is

One mod, `mods/dev-menu/`, id `phasmomods.dev-menu`, built on the SDK like the bundled mods, and
gated behind the developer flag below. It opens on the key left of the 1 (the `~` key, the usual
dev-menu key per the user; the binding sits on that physical position, so non-US layouts get the
same key) and only when the game runs offline, in its own single player mode
(`PhotonNetwork.get_OfflineMode`, already mapped) - the user's final call, a sandbox (third round,
below). While the menu is open the game runs at timeScale 0, so the contract freezes and the menu
stays responsive (Unity still runs `Update` at dt 0; the menu's own polling keeps working). Changed by
the user on 2026-11-12 after three live tries: the pause is off by default and is a row a developer
turns on, because the game's own code never writes the time scale and its picture adapts to the light
using the scaled frame time (`sdk/systems/media.md`). The clock is left alone until asked. The
menu puts nothing on a network there is not on: offline, every game path it calls is the game's own
single player path.

Its purpose is debugging the mods: every feature exists so a single contract can answer a question
about a ghost, a tell, a rule, or a piece of UI without restarting the contract or hand-modifying
saves.

Goals:

- all custom difficulty options, changeable mid-contract;
- direct control of the ghost, the player, equipment and the contract for testing;
- read-outs (debug HUD, ESP) that show what the game and the mods currently believe;
- a sandbox: offline only, pausable at will, no other players, and nothing the session does ever
  reaches a room or a save;
- a loader page that shows mod states and content, since that is where mod bugs first show.

Non-goals:

- any effect in any room, public or private (the menu only opens offline; the loader's rules
  already switch every mod off in a public room);
- sending or forging network values, bypassing an RPC's sender checks, or anything near the
  anti-cheat;
- changing which mods a lobby runs (the host owns that, unchanged);
- maps and contracts you cannot rebuild in place (a map change means a new lobby, not a menu item);
- any touch to player progress (see Progression protection below).

### The developer flag

The user's call (2026-11-10): the menu ships in public releases, but players never see it; a
developer opts in. Following SMAPI's `DevelopmentMode` config convention (and the loader's own
launcher.ini settings):

- A loader setting, `developer=1` in launcher.ini (the file that already holds the mod choices).
- Flag off (every player's default): the mod is not offered, listed, loaded or synced anywhere;
  it is invisible in the launcher, the Mods panel and every lobby list, exactly as if it were not
  installed. Friends of a developer never need it and never see it.
- Flag on (the developer's box): the mod appears, is chosen like any mod, and follows every existing
  lobby rule while chosen - including being required of everyone in that lobby (SDK: a loader-side
  setting and offer gate). One consequence worth seeing: the mod still rides the lobby's list
  rules, so a developer hosting friends with the flag on needs them to install it too (even
  though the menu never opens in a room) - or turns the flag off before inviting. A normal lobby
  never sees the menu at all.

### Sandbox rules (the user's final call, 2026-11-10, third round)

Dev mode is single player only and fully sandboxed. This replaces the second round's private-room
scope: the menu only opens while PUN runs offline, which is the game's own single player state.

- No room exists, so nothing is listed anywhere, nobody can join, and no RPC can be refused: the
  game's sender checks let everyone through outside a room or offline (systems/photon.md), so the
  refusal risk that row scoping existed for cannot arise. No special networking work is needed to
  "disable" it: single player already runs with PUN offline. The menu never connects, never creates
  a room, and simply will not open inside one (SDK: the same offline gate).
- Pause-while-open and the timeScale rows are fully live, since freezing the contract is freezing
  the whole sandbox.
- The offline game is still the master and first actor, so every row - ghost writes, forced hunts,
  events, re-type, god mode, dummy - uses the game's own single player path exactly as a vanilla
  contract does. The discipline holds regardless: call the game's own method, never an RPC body or
  an RPC by name.
- A mod loaded in a real lobby (the list rules still apply while the flag is on and the mod is
  chosen) shows nothing there: the menu never opens in a room. True multiplayer testing of the
  mods themselves stays what it is today: friends joining a development pre-release.

How each entry below is tagged:

| Tag | Meaning |
| --- | --- |
| map | the game members it needs are already mapped |
| hook | a mod hook on a mapped method, no new entry |
| RE | needs reverse engineering and a new map entry first |
| SDK | needs a loader/SDK addition (the host contract grows at the end only) |
| verify | plausible on mapped parts, but the in-game effect is not proven yet |

## The menu at a glance

```
~ Dev menu  (single player / offline only; game paused at timeScale 0 while open)
|
+- Difficulty   every custom difficulty option, live, mid-contract
+- Contract     weather, setup phase, objectives, payout, ending
+- Ghost        observe | hunts | room and movement | speed and senses
|              attempts (event, interact, ability, throw) | type and evidence | type flags
+- Player       vitals | movement | dummy investigator
+- Equipment    loadout and shop | durability and consumables | cursed possessions
+- Game         time | debug HUD | ESP | the ghost's sensors
+- Loader       mod states | content | journal tests | per-mod tests
```

Left column: the seven sections. Right pane: the section's groups. Rows are real Unity `Button`s
made at run time, sitting in a `ScrollRect` list (the user's call: no vanilla-UI dressing for this
menu), with their text built with the shared `ui::` helpers (the Playable Ghost HUD's precedent); the
list, its mask and its keyboard follow are the loader's own helpers in `core/ui.h`, and the geometry
lives in `mods/dev_menu/menu_layout.h` so the offline test can check it. Values shown are read from
the game every frame, never cached in the menu (the dynamic-over-hard-coded rule).

## 1. Difficulty

The live values of the running contract's difficulty (`LevelValues.difficulty`). The menu lists the
`Difficulty` object's own fields, read from the game's metadata at runtime (`Type::fields_of`), so a
game update that adds an option shows it without a menu change. The fields already mapped:

- Ghost type forced (`Difficulty.ghostTypeOverride`) - the forced type shows the whole offered
  collection, custom ghosts included, from `GhostController.allGhostsCollection` (map)
- Evidence given (`Difficulty.evidenceGiven`) - applied at the next evidence pick; a mid-contract
  change and its exact effect is RE (verify)
- Friendly ghost (`Difficulty.friendlyGhost`) - the vanilla guard already refuses hunt starts, hunt
  door closes and kills while set (map); the menu also says in-game, where a refusal came from (a
  friendly ghost refusing a forced hunt is the feature working)
- Lose equipment on death (`Difficulty.loseItemsOnDeath`) - read at the reward screen (map)
- Ghost speed multiplier - `GhostHuntState.UpdateSpeed` multiplies by "the difficulty's ghost
  speed" (noted in ghost-ai.md), but that field itself is not mapped yet: RE
- Weather choice - a number on the difficulty; only `actualWeather` is mapped: RE with the Contract
  section's weather work
- Every other field the scan finds (the vanilla menu's remaining toggles and sliders) - each needs
  a decompile pass to write its `///` note before its row appears: RE per field

- Apply-to-everything row: set the preset index (`Difficulty.difficulty`) - map, effect on reward
  multiplier only (verify)
- Reward read-outs: `GetRewardMultiplier`, `GetInvestigationBonusReward` (map)

Caveat kept from photon.md: changing `friendlyGhost` or the weather behind the game's back can make
an early guard read disagree with an RPC body's check. In offline single player the first-actor
checks pass, but each row still goes through the game's own paths, not around them.

## 2. Contract

- Weather, live: write `LevelValues.weather` (map) and play the game's own transition; the
  transition path is the Monkey Paw's, `RandomWeather.TransitionWeatherProfile`, unmapped: RE.
  Choice list from `WeatherType` (clear, foggy, rain, heavyRain, snow, wind, sunrise, bloodMoon;
  never `dead`) (map). Write `Difficulty.actualWeather` as the game's transition does, so hunt
  speed branches agree (map). Blood Moon and hunt speed interact; the HUD shows the speed split.
- Unlock the main door / end the setup phase: `SetupPhaseController.UnlockMainDoor` (map)
- Objective board: show main and side objectives (`LevelValues.mainObjective`, `Objective.values`
  - map); complete or fail one on the game's own path: RE (the completion path is unmapped)
- Objective payout: `ObjectiveValues.moneyReward` (map) - a dev-only number for testing the reward
  screen (verify where it is read)
- End the contract now: `ExitLevel.Exit` (map; offline this client is the first actor) - and the
  reward screen (`RewardManager`, map)
- Leave or quit as the pause menu does: `PauseMenuController.Leave` / `.Quit` (map)
- Quick restart (end and start the same contract again without touching the lobby board by hand):
  a scripted lobby path: RE, later; v1 uses End contract and the game's own lobby

## 3. Ghost

Observe (this is half the debug value, so it lives at the top of the section):

- Identity card: type (`GhostController.ghostType`, `behaviourType`), custom ghost id from the host,
  favourite room (`favouriteRoomIndex` named through `LevelController.rooms`) (map)
- Live flags: `state` (named), `isHunting`, `postHuntCooldown`, `huntsHeldOff`, `isAppeared`,
  `canWander`, `stayHome`, `speedFlag`, `smudge`, `flamesBlownOut`, `eventsBlocked` (all map)

Hunts:

- Force a hunt now: `GhostAI.ChangeState(hunting, ..., force)` on this client, which offline is the
  master (map). Refusals and their reason show on the row (friendly ghost, events blocked, a hunt
  already on)
- End the hunt: ChangeState back (map); set the hunt's length: RE (the hunt clock fields need
  notes)
- Clear the cooldown / un-hold hunts: `postHuntCooldown = 0`, `huntsHeldOff = false` (map)
- Kill test: force the hunt's target (`GhostHuntState.SetTarget`, map), send it to a closet
  (`SetCloset`, map), clear the smudge block (`GhostAI.smudge`, map), and a hook that lets or
  refuses `GhostHuntState.Kill` to test both sides (hook)

Room and movement:

- Change the ghost's room, manual or random: pick any `LevelController.rooms` row or roll it, then
  `GhostController.ChangeFavouriteRoomTo` plus `GhostController.WarpGhostToFavouriteRoom` (map);
  refused while hunting, and the row says so
- Teleport the ghost to a chosen player or point: a NavMeshAgent warp (no entry): SDK (one Unity
  entry) plus verify
- Ghost walk speed outside hunts and the hunt base speed: `GhostAI.baseSpeed` (map;
  `RefreshBase` rereads it every two seconds, so a change holds), shown as the live product
  `baseSpeed x sightSpeedUp` on the HUD

Speed and senses:

- Sight assist pin: hold `sightSpeedUp` at 1 or 1.65 through the host's own hook point just before
  `UpdateSpeed` (the hunt-speed rule path, map)
- Line of sight toggle: built 2026-11-12 on the sensor rather than on the hunt's answer. The sight
  sensor pulses and tests its own input sensor, and what that sensor detects is its layer mask, so
  the row empties that mask (map: `LOSSensor.get_InputSensor`, `RangeSensor.layers`) and puts the
  saved mask back on the toggle and on mod stop. Blinding the input sensor leaves the hunt's other
  senses alone, which is what a test wants.

Attempts (each is the game's own path; tarot's `forced` is the precedent):

- Fire an event of a chosen kind: `GhostAI.ChangeState` with each mapped `GhostState` (light ...
  ignite, summoningCircle, musicBox), and `GhostAI.EventPick` for a roll (map)
- Block or allow events: `LevelValues.eventsBlocked` (map)
- Force an interaction: `GhostActivity.TryInteract` / `NormalInteraction` / `GetDoorToOpen` /
  `TwinInteraction`, aimed by setting the interaction sensor's box (map)
- Use the type's ability: `GhostActivity.GhostAbility` (map). For a custom ghost the ability goes
  through its own rule entry points; v1 triggers vanilla abilities plus the Playable Ghost's
  player-side actions, and the Loader section's per-mod tests cover the rest
- Throw a prop now: `GhostActivity.GetPropToThrow` + `Prop.TryThrow` (map)
- Appear / unappear: `GhostAI.MakeGhostAppear`, `Appear`, `UnAppear`, flag `isAppeared` (map)
- Show or hide the model for filming tests: `GhostAI.model` renderers
  (`Renderer.set_enabled`) (map)

Type and evidence:

- Change the ghost type during the contract: the user's ruling (2026-11-10, second round): the
  Mimic does not mimic evidence in vanilla, so the proper route is a re-spawn with the new type.
  The row is therefore the game's own spawn path run again on the master: roll nothing, take the
  chosen type, re-hand its traits through `CreateGhost`/`GhostAI.Init` (map), re-pick evidence for
  it (`PickEvidence`, map), let the content rules re-arm against the new traits, and keep the
  current favourite room unless the row says otherwise - behind a confirmation on the row. The
  Mimic's native switches (`AttemptGhostTypeStateChange`, `mimicCopies`, map) stay as their own
  row for testing the Mimic itself. The 1:1 bar still rules: if the re-spawn cannot deliver the
  target type's evidence, tells and rules exactly as a contract that rolled it would, the row does
  not ship as a half-measure, and the RE spike (does re-handing traits re-init everything the
  content side armed at spawn?) runs before it is built.
- Evidence under test: `GhostController.givenEvidence` (map) - read now, and a dev override for
  journal and Spectral Flame tests that never syncs or saves (a contract is fully modded and
  closed, and offline there is nobody to sync to anyway)

Type flags (the AI's own dials, for reproducing a type's behaviour on demand):

- Banshee target: `GhostAI.SetNewBansheeTarget` (map)
- Thaye: `thayeStep`, `thayeThreshold` (map); Gallu: `galluState`; Dayan: `dayanState`,
  `DayanPlayerChecks` read-out (map)
- Onryo flame count: `GhostAI.flamesBlownOut` (map)
- Interaction boost: `GhostAI.interactionBoost` (map)

## 4. Player

Vitals:

- God mode: a hook that refuses `GhostHuntState.Kill` for the local player (hook; the kill's
  network body is never touched; offline the kill decision is this client's anyway)
- Sanity: set (`PlayerSanity.SetInsanity`, map), read average (map), freeze drain (hook over
  `PlayerSanity.Update`, hook)
- Stamina: set / refill (`Player.stamina`, map)
- Die on demand (to test death, revival and the slot shift): `Player.BeginDeathSequence` (map)
- Revive (self or any spot): `Player.RevivePlayer` (map; offline the owner checks pass)

Movement:

- Freeze the player: `Player.ToggleFreezePlayerMovement` (map)
- Walk / sprint speed: built 2026-11-12, and not by writing the movement fields. The game already has
  one multiplier on the rig (`LocalPlayer.speedMultiplier`), and the Monkey Paw's speed wish runs
  through `LocalPCPlayer.SetPlayerSpeedMultiplier`, with
  `LocalPCPlayer.ResetPlayerSpeedMultiplier` putting back the copy the rig kept when its speed was
  first set. The rows step that multiplier in quarters and reset it the same way a wish's end does,
  so the game's own walk and sprint arithmetic still decides the real speed (the no-caps rule).
- Noclip / fly: still open (FINDINGS.md 34). The controller's step writes its Rigidbody's position and
  applies gravity through its own multiplier, but which pass actually stops the player at a wall is
  unproven, and the pieces that look responsible carry names the game shuffles every update, so their
  entries have to be found by shape. It also needs the user's answer: through walls while still
  walking on floors, or free flight.
- Teleport (to a room, a player, the truck): `Player.body` transform write (`Transform.set_position`,
  map) plus how the controller re-seats: verify (a fake-game test first, then the smoke list)

Dummy investigator (the user's ask, 2026-11-10: test multiplayer-only features - Banshee and
Bogeyman targeting, networked animations and emotes - in single player):

- Honest limit first: a dummy cannot be a true networked player. The room's players come from the
  Photon server (`PhotonNetwork.get_PlayerList`, `Network.playerSpots`), and fabricating a client
  would mean forging network identities, which the plan will not do. What the menu can do is put a
  game-shaped investigator body in the level that the ghost's own senses find.
- The dummy: the game's own player prefab instantiated locally, moved by teleport or steer, posed,
  and carrying the sense signals the hunt reads: `PlayerLOSTarget`, `PlayerNoise` (map,
  GhostSenses.h), so `Sees`, `Hears` and the electronics test act on it, and it can be walked into
  closets to drive the Bogeyman's closet search (`GhostHuntState.SetCloset` plus real
  `ClosetZone`s, map). Playable Ghost's parked body (`mods/playable_ghost/body.cpp`) is the working
  precedent that a local player-shaped body lives safely in a contract.
- Banshee (Moroi) targeting: the game sets `GhostAI.bansheeTarget` from a real `Player` component;
  whether the local dummy can carry one safely is an RE spike (the parked body again is the
  precedent to test against). Until proven, the row reads the target instead of setting it.
- What the dummy never is: not in the player list, so player-slot UI (sanity bars, journal marks,
  Ready), the idle state's average-sanity roll over real players, and actor-keyed peer messages
  (the Emotes session keys by Photon actor) do not treat it as a player. Networked emote and
  animation *messages* need a real second client; that test stays what it is today: friends
  joining the development pre-release. The dummy does carry the animation half: a clip driven on
  its body for filming and timing tests.
- All of it behind one spike: if the dummy proves to disturb any game system, the group ships
  signals-only (no body) or not at all; it is the plan's riskiest feature next to the re-type.

## 5. Equipment

- Infinite durability, battery and charges: the decrement paths need mapping (Equipment.h has
  `isOn`, `placed`, `Use`; durability and battery are not entries): RE
- Never lose a consumable count: refuse `EquipmentInfo.RemoveAmountOwned` for the item type chosen
  (map, hook)
- Refill / reset the loadout: `InventoryEquipment.ResetItem`, `ReturnCommittedItem`,
  `InventoryManager.ReturnItemstoInventory` (map; lobby-side, the shop screen is the game's)
- Free shop, buy everything, spawn an item mid-contract: the store's listing, prices, the shop
  screen and the purchase are explicitly not mapped (guides/equipment.md): RE first; wallet money
  is a saved value, not a live field, so "add money" is also RE (and stays dev-only; the currency
  rule still says no mod inflates progression in a lobby this menu can't even run in)
- Cursed possessions: use paths exist (`MonkeyPaw.MakeWishNetworked`, `TarotCards.ChangeState` and
  the rest of CursedItems.h, map); spawning one at a point needs the store prefabs: RE with the
  shop work

## 6. Game

Time:

- Tick speed slider: 0x (paused), 0.5x, 1x, 2x, and fine values - `Time.get_timeScale` is mapped,
  the setter needs one entry (SDK small)
- "Pause while the menu is open": restores the previous timescale on close (same entry). Off by default
  since 2026-11-12, the user's call; the row and the tick speed row stay

Debug HUD (a `ui::` canvas, PG-HUD style; toggles per block). Started 2026-11-12 as one switch
showing the four ghost lines below (state, identity, hunt base speed, and the hunt target's room,
named through `Player.currentRoom` and the level's own room list) on its own canvas, which stays up
with the menu closed; the rest is still to come:

- Ghost: state name, `isHunting`, target and `searchTarget`, live hunt speed with its split
  (`baseSpeed`, `sightSpeedUp`, multipliers), position and current room, `huntsHeldOff` /
  `postHuntCooldown`, sanity the idle roll is using (`GhostIdleState.insanity`, map)
- Players: each one's room (`Player.currentRoom`), sanity (`PlayerSanity.insanity`), dead
  (`Player.isDead`), talking (`PlayerAudio.IsTalkingLoudly`, map)
- Contract: weather, FPS (`LevelStats.averageFPS`, map), timeScale, offline/master read-out
  (Photon.h, map)

ESP (drawn with mapped graphics: `Renderer`/`Material` colouring, `ui::` labels, the Camera for
world to screen; through-wall outlines are verify):

- Ghost outline or tint through walls (its renderers, map)
- Room names floating in world (names and rooms are mapped; placement verify)
- Doors and their state (Door.h, map); light switches (LightSwitch.h, map); hiding places
  (ClosetZone.h, map); props (Props.h, map)
- Team outlines through walls (bodies' renderers, map)
- PG's drawn map view (`mods/playable_ghost/mapview.cpp`) reused as a minimap toggle (hook)

The ghost's sensors (what the hunt currently sees, hears and senses):

- A live list from `GhostHuntState.Sees` / `Hears` / `SensesGear` results (hook), the signals in
  `GhostAI.vision`, `hearing` and `GhostActivity.nearElectronics` (map), and the sensor settings
  (Sensor.h, map) - this answers "why is the hunt chasing / not chasing" in one look

## 7. Loader

Mod states (the host already owns this answer, so the menu just reads it):

- Every mod: id, version, on/off and why, dependency chains, errors - a read-only query on the
  host contract (SDK, grows at the end)
- Content the lobby offers: the custom ghost types and their ids (`GhostController.allGhostsCollection`
  plus the host's offer), custom evidence (map + the host's own list)
- The consent state and the lobby's verdict (host, map of its own channel)

Journal (the lobby-journal rule makes this a test surface):

- Jump to each custom journal page, force the evidence rows and the vanilla-page lines to render,
  in-contract and in the lobby (JournalController is fully mapped; the host's page sync exists)

Per-mod tests (only where a game-side entry exists; v1):

- Emotes: fire each emote through the game entries the mod hooks, no mod-to-mod calls
- Spectral Flame: force a flame test on the nearest candles (`FireSource`/`Candle`, map)
- Playable Ghost: start as the ghost from the menu, use its player actions, and watch the AI parity
  read-outs side by side (its own UI already exists)
- A deeper per-mod trigger API is a question, not a decision (see Open questions)

Out of scope (loader rules, stated so nobody re-litigates them here): enabling or disabling mods
from inside a contract, changing the lobby's mod list, downloading mods, idle'ing anyone's mods.

## Progression protection (the user's rule, 2026-11-10)

While a dev session is on (a contract the developer menu is enabled for), nothing the contract
earns or changes may reach the player's progress: XP, money, kept equipment, stats, the apocalypse,
daily and weekly quests, and Steam achievements. This is the user explicitly raising achievements
for dev mode only; the default "leave achievements alone" rule stands for normal play.

- Mapped sites to suppress while a dev session is on: `ExitLevel.CheckMissions` and
  `ExitLevel.CheckChallenges` (map); the reward screen's saving of the money total and
  `SingleplayerProfile.AddXPAndFunds` (map, Rewards.h and GameController.h); the death-equipment
  branch (`InventoryManager.ResetAllEquipment` vs `ReturnItemstoInventory`, the save behind it).
- Unmapped sites to find by RE before the group ships: where stats save, where daily/weekly quests
  progress, and the apocalypse counter. The plan does not claim coverage of these until each is
  read and cited (never invent behaviour); the protection group is not enabled until the RE pass
  lists every write site.
- Steam achievements: the mapped Steam entries (systems/achievements.md) go no-op for the session's
  unlocks. (Normal play keeps the standing rule of leaving Steam entirely alone; this is dev-mode
  only, at the user's ask.)
- The sandbox makes this whole group local: a dev session runs only offline, involves no other
  player, and never reaches a room. The user's "all player progress must remain unaffected" is
  therefore this client's protection, and the no-inflated-currency condition for lobbies is safe by
  construction since no dev session is ever in a lobby.
- Cheats that touch economy are display-only: the payout row shows a number; it never writes the
  wallet (the wallet is a saved value anyway, and that path stays closed).
- Live smoke check (the user runs it): finish a dev session at a high tier with cheats on; money,
  XP, level, quests, apocalypse and achievements read identical before and after. Offline tests
  pin every suppression hook.

## Reverse engineering and SDK work the plan leans on

In rough order of how much of the menu waits on each:

1. The remaining `Difficulty` fields (notes for each; the field scan then feeds the rows) - RE
2. `Time.set_timeScale`, keyboard polling (the map has `Mouse` but no `Keyboard`; poll the physical
   key left of 1), Unity `Slider` and `ScrollRect` (the user's call: plain scroll views, no vanilla
   widget dressing for the menu) - SDK small, one-line entries. The `ScrollRect` entries and the
   loader's scroll helpers went in 2026-11-12, with `RectMask2D` and
   `EventSystem.get_currentSelectedGameObject`; `Slider` is still unmapped (the menu steps its
   numbers with +/- rows instead).
3. Mid-contract weather transition (`RandomWeather.TransitionWeatherProfile`) - RE
4. Movement speed and the character controller (player cheats) - the speed half went in 2026-11-12
   through the rig's own multiplier; what stops the player at a wall is still RE (FINDINGS.md 34)
5. Equipment durability/battery decrements and the whole shop flow (StoreItem, prices, purchase),
   saved money - RE
6. NavMeshAgent warp (ghost teleport) - SDK small
7. Objective complete/fail path; the hunt clock - RE
8. The re-type spike: does the Mimic's native switch deliver the target type's evidence and tells
   (the user's 1:1 bar); if not, the true re-type path - RE, gated
9. The dummy investigator spike: how much of a local player-shaped body the game's systems accept
   (PG's parked body as the precedent) - RE spike
10. Every progress write site a dev session must not reach: stats, daily/weekly quests, apocalypse
    - RE before the protection group ships
11. The developer flag: loader setting and offer gate - SDK
12. A mod-status read query on the host contract - SDK (grows at the end)

## Rules this design leans on

- Single player gate: `get_OfflineMode`; public lobbies already run every mod off (lobby rules),
  and a multiplayer lobby where this mod is loaded simply never shows the menu.
- Never send an RPC by name or invoke an RPC body; every action goes through the game's own method
  on the client the game would call it from; every refusal is surfaced, never worked around.
- Nothing the menu sets is saved or synced beyond what the game's own path does (weather through the
  Paw's path is the one transition the game itself makes mid-contract, which is why that path is
  used instead of a bare field write).
- Dynamic over hard-coded: field scans, the offered ghost collection, `WeatherType` names, room
  lists - all read at runtime.
- Text: every label the menu shows goes in the mod's `i18n/default.json` like any other text
  (the text-table rule). The user allowed AI-written placeholders (2026-11-10), each marked
  `PM_TEXT_AI` for later replacement, and `text_table_test` checks the table against the code.
- Progression protection is not a convenience: while a dev session is on, the suppression group
  must be on with it, or the session does not start (fail closed, like every loader decision).
- The developer flag keeps the menu out of every player's way: with it off the mod does not exist
  in the launcher, the Mods panel, any lobby list or the loader's loading order.
- No addresses in the menu's code, comments or UI: map keys everywhere.
- A game build the entries do not resolve on leaves the affected rows absent or greyed with the
  reason, never a guess.

## Decisions (the user, 2026-11-10)

1. Visibility: ships public, hidden by default behind a developer flag (`developer=1` in
   launcher.ini, following SMAPI's DevelopmentMode convention); flag off means the mod is
   invisible to players and to every lobby list.
2. Public releases include it, opt-in as above.
3. AI-written placeholder labels are allowed, marked `PM_TEXT_AI`.
4. Open key: the usual dev-menu key, the one left of the 1, bound at that physical position.
5. Type change: the Mimic does not mimic evidence in vanilla (the user), so the proper route is
   the game's own re-spawn with the chosen type; the 1:1 bar for evidence, tells and rules stands,
   and the Mimic's switches stay as their own row (see section 3).
6. ScrollRect entries; no vanilla-UI dressing for the menu itself.
7. New rule: no dev-mode action may touch progress - XP, money, equipment, stats, apocalypse,
   daily and weekly quests, Steam achievements (see Progression protection).
8. New feature to weigh: a controllable dummy investigator for multiplayer-only tests, scoped
   honestly as a local game-shaped body the ghost's senses accept, never a forged networked
   player (see section 4). The user: as close as a local-only version can get.

Second round (the user, same day, items 9-10 superseded by the third round):

9. (Superseded.) The single player restriction was briefly lifted for private-room testing; the
   row-scope design it produced is kept in git history only.
10. (Superseded.) Lobby-wide progression suppression went with it.

Third round (the user, same day, final):

11. Dev mode is single player only and fully sandboxed: the menu opens only while PUN runs offline
    (the game's own single player state). No room exists, so nothing lists, nobody joins, and no
    RPC refusal is possible; pause and timeScale are fully live; progression suppression is local;
    "disabling networking" needs no work because single player already runs with PUN offline - the
    menu just refuses to open in a room. True multiplayer testing of the mods themselves stays
    with friends on a development pre-release, outside dev mode. See Sandbox rules.

## Tests

- Offline (fake game): the menu's model (every row's set/get against the fake), the single player
  gate, pause/restore, the difficulty field scan, and each hook rule; no row ships without its
  offline test.
- `validate.ps1` and (for the value-moving rows) `tools/gamecheck.ps1 -IfConfigured -Expect` as
  usual.
- Live: the smoke list gains the menu's own run (open in single player, pause holds, each cheat
  class once, refusals show their reason), run by the user with the standing ban-risk note; and the
  room gate: in a private room with friends where the mod is loaded per the list rules, the menu
  never opens (it stays inert), and single player works normally again after leaving. The
  public-lobby gate is smoke-checked there too.
- Live, new: with the developer flag off, the mod is absent from the launcher, the Mods panel and
  the lobby list; after a high-tier dev session with cheats on, money, XP, level, kept equipment,
  stats, apocalypse, quests and achievements read identical before and after; the dummy is found
  by the hunt's senses and seen by ESP, while the player-list UI and networked messages ignore it.
