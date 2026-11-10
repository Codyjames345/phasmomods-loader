# Dev menu: content plan

Date: 2026-11-10. Branch: `feature/dev-menu` (from `development` at 6ef3dad).
This is the content plan: what the menu holds and what each entry can rest on. It is not yet the
build plan; that comes after the user approves this list and answers the questions at the end.

## What it is

One mod, `mods/dev-menu/`, id `phasmomods.dev-menu`, built on the SDK like the bundled mods. It
opens on the backtick key (`~` on the US layout) and only in single player
(`PhotonNetwork.get_OfflineMode`, already mapped). While the menu is open the game runs at
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
- maps and contracts you cannot rebuild in place (a map change means a new lobby, not a menu item).

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
+- Player       vitals | movement
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

- Change the ghost type during the contract: marked experimental. The game changes a live ghost's
  type only for the Mimic (`AttemptGhostTypeStateChange`, map), and `CreateGhost` hands traits to
  `GhostAI.Init` at spawn. v1 offers the Mimic's own switches (`mimicCopies`, `mimicNext`,
  `MimicCopy`, map) plus a forced type through the game's next native switch; a true re-type
  (resend traits, rules re-armed) is RE, behind a confirmation on the row
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

## Reverse engineering and SDK work the plan leans on

In rough order of how much of the menu waits on each:

1. The remaining `Difficulty` fields (notes for each; the field scan then feeds the rows) - RE
2. `Time.set_timeScale`, keyboard polling (the map has `Mouse` but no `Keyboard`), Unity `Slider`
   and `ScrollRect` - SDK small, one-line entries
3. Mid-contract weather transition (`RandomWeather.TransitionWeatherProfile`) - RE
4. Movement speed and the character controller (player cheats) - RE
5. Equipment durability/battery decrements and the whole shop flow (StoreItem, prices, purchase),
   saved money - RE
6. NavMeshAgent warp (ghost teleport) - SDK small
7. Objective complete/fail path; the hunt clock - RE
8. Mid-contract type change beyond the Mimic's switches - RE, and only if it proves safe
9. A mod-status read query on the host contract - SDK (grows at the end)

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
  (the text-table rule), with the wording question below.
- No addresses in the menu's code, comments or UI: map keys everywhere.
- A game build the entries do not resolve on leaves the affected rows absent or greyed with the
  reason, never a guess.

## Open questions for the user

1. Standing of the mod in a lobby. Recommendation: an ordinary mod on the host's list, inert
   outside single player; a friend joining a dev's multiplayer lobby installs the DLL like any mod.
   Alternative: a flag that keeps a dev-only mod off the lobby list (new rule, weakens "the host's
   list is the lobby's list", needs the user's explicit call). Alternative: dev builds only, never
   in a release.
2. Whether the dev menu ships in public releases at all, or only in the user's dev install / a
   separate debug download.
3. Menu wording. Labels are text the user reads in-game. Options: every string as a
   `PM_TEXT_TODO` row for the user to write (honest to the policy but heavy for ~70 labels), or the
   user ruling that a developer tool's technical labels (map keys, `timeScale`, ghost names from
   the game) are not player-facing and may be written as-is. A middle choice: keep bare technical
   terms verbatim, mark every sentence-like label `PM_TEXT_TODO`.
4. The `~` key on non-US layouts: fixed backtick, or configurable in the launcher/mod settings?
5. How far to take "change ghost type during a contract" if the re-type path proves unsafe: keep
   only the Mimic's own switches and a forced type at the next native switch, or allow an
   experimental respawn behind a confirmation.
6. Scrollable lists: add the Unity `Slider`/`ScrollRect` map entries, or page the lists with game
   buttons?

## Tests

- Offline (fake game): the menu's model (every row's set/get against the fake), the single player
  gate, pause/restore, the difficulty field scan, and each hook rule; no row ships without its
  offline test.
- `validate.ps1` and (for the value-moving rows) `tools/gamecheck.ps1 -IfConfigured -Expect` as
  usual.
- Live: the smoke list gains the menu's own run (open in SP, pause holds, each cheat class once,
  refusals show their reason), run by the user with the standing ban-risk note; nothing in the menu
  runs in a real room, and the gate itself is smoke-checked in a private lobby.
