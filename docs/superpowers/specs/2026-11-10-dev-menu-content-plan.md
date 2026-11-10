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
same key) and only in single player (`PhotonNetwork.get_OfflineMode`, already mapped). While the menu is open the game runs at
timeScale 0, so the contract freezes and the menu stays responsive (Unity still runs `Update` at
dt 0; the menu's own polling keeps working). Everything it changes is local to this client: it sends
nothing, syncs nothing, and never invokes an RPC body or sends an RPC by name.

Its purpose is debugging the mods: every feature exists so a single contract can answer a question
about a ghost, a tell, a rule, or a piece of UI without restarting the contract or hand-modifying
saves.

Goals:

- all custom difficulty options, changeable mid-contract;
- direct control of the ghost, the player, equipment and the contract for testing;
- read-outs (debug HUD, ESP) that show what the game and the mods currently believe;
- a loader page that shows mod states and content, since that is where mod bugs first show.

Non-goals:

- any effect in a multiplayer or public lobby (the menu refuses to open outside single player, and
  the loader's own rules already switch mods off in a public room);
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
  lobby rule while chosen - including being required of everyone in that lobby. A developer who
  wants a clean multiplayer lobby turns the flag off first. The menu itself still refuses to open
  outside single player (SDK: a loader-side setting and offer gate).

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
~ Dev menu  (single player only; game paused at timeScale 0 while open)
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

Left column: the seven sections. Right pane: the section's groups. Rows are cloned game widgets
(the Mods panel's `Toggle.1` rows, menus.md) plus TMP text built with the shared `ui::` helpers
(the Playable Ghost HUD's precedent). Values shown are read from the game every frame, never cached
in the menu (the dynamic-over-hard-coded rule).

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

- Force a hunt now: `GhostAI.ChangeState(hunting, ..., force)` on this client while it is the
  master (map; offline it is). Refusals and their reason show on the row (friendly ghost, events
  blocked, a hunt already on)
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
- Line of sight toggle: force `GhostHuntState.Sees` true or false for the test hunt (hook), with
  the sensor read-out below so you can see what the game itself decided

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

- Change the ghost type during the contract: the user's condition (2026-11-10): the Mimic's path
  is acceptable only if it gives a 1:1 test of every ghost - the target type's own evidence set and
  tells, not the Mimic's copies. The game changes a live ghost's type only for the Mimic
  (`AttemptGhostTypeStateChange`, map), and `CreateGhost` hands traits to `GhostAI.Init` at spawn.
  The first step is an RE spike: does the native switch re-roll `givenEvidence`, re-arm the type's
  rules and re-show the right tells (Ghost Tells gates on the live type, and Custom Ghosts' rules
  on its traits)? If yes, the row is the Mimic's own switches with a dev-set copy list
  (`BuildMimicCopies`, map). If no, the plan extends into a true re-type (re-hand traits through the
  spawn path, rules re-armed, evidence re-picked), behind a confirmation on the row, until the 1:1
  bar is met. A row that cannot meet the bar does not ship as a half-measure.
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

- God mode: a hook that refuses `GhostHuntState.Kill` for the local player (hook; local only, the
  kill's network body is never touched)
- Sanity: set (`PlayerSanity.SetInsanity`, map), read average (map), freeze drain (hook over
  `PlayerSanity.Update`, hook)
- Stamina: set / refill (`Player.stamina`, map)
- Die on demand (to test death, revival and the slot shift): `Player.BeginDeathSequence` (map)
- Revive (self or any spot): `Player.RevivePlayer` (map; offline the owner checks pass)

Movement:

- Freeze the player: `Player.ToggleFreezePlayerMovement` (map)
- Walk / sprint speed: the movement fields on `LocalPCPlayer.controls` (`PCControls`) are not
  mapped: RE. Until then this row is absent, not stubbed
- Noclip / fly: needs the character controller entries (not mapped): RE
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
- "Pause while the menu is open": the default; restores the previous timescale on close (same entry)

Debug HUD (a `ui::` canvas, PG-HUD style; toggles per block):

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
  unlocks.
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
   widget dressing for the menu) - SDK small, one-line entries
3. Mid-contract weather transition (`RandomWeather.TransitionWeatherProfile`) - RE
4. Movement speed and the character controller (player cheats) - RE
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
5. Mimic-path type change is fine only at the 1:1 bar: the target ghost's own evidence and tells;
   if the native switch cannot do that, the re-type is deepened until it can, or the row does not
   ship (see section 3).
6. ScrollRect entries; no vanilla-UI dressing for the menu itself.
7. New rule: no dev-mode action may touch progress - XP, money, equipment, stats, apocalypse,
   daily and weekly quests, Steam achievements (see Progression protection).
8. New feature to weigh: a controllable dummy investigator for multiplayer-only tests, scoped
   honestly as a local game-shaped body the ghost's senses accept, never a forged networked
   player (see section 4).

## Tests

- Offline (fake game): the menu's model (every row's set/get against the fake), the single player
  gate, pause/restore, the difficulty field scan, and each hook rule; no row ships without its
  offline test.
- `validate.ps1` and (for the value-moving rows) `tools/gamecheck.ps1 -IfConfigured -Expect` as
  usual.
- Live: the smoke list gains the menu's own run (open in SP, pause holds, each cheat class once,
  refusals show their reason), run by the user with the standing ban-risk note; nothing in the menu
  runs in a real room, and the gate itself is smoke-checked in a private lobby.
- Live, new: with the developer flag off, the mod is absent from the launcher, the Mods panel and
  the lobby list; after a high-tier dev session with cheats on, money, XP, level, kept equipment,
  stats, apocalypse, quests and achievements read identical before and after; the dummy is found
  by the hunt's senses and seen by ESP, while the player-list UI and networked messages ignore it.
