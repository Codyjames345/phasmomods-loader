# Ledger — dev menu (docs/superpowers/specs/2026-11-10-dev-menu-content-plan.md)

Branch feature/dev-menu, cloned from development. Work stays on this branch: it is not merged anywhere, and
must not be merged with any other branch (the user's standing instruction).

## 2026-11-11: the mod is implemented and passes the offline suite

- Mod `mods/dev_menu/` (id `phasmomods.dev-menu`), built as DevMenu.dll, channel 8, manifest
  `developerOnly: true`.
- Files: `menu_rule.h` (pure rules: sandbox gate, pause, hook predicates, per-row refuse table),
  `actions.h/.cpp` (the game side: four hooks, every row action, every read-out; all map keys, no addresses),
  `menu.h/.cpp` (the canvas: six sections down the left, plain text rows, no vanilla dressing by decision 6),
  `mod.cpp` (the toggle key on the key left of the 1, `<Keyboard>/backquote` with minus/equals fallbacks,
  rebindable through the game's keybinds menu), `i18n/default.json` (every label a `PM_TEXT_AI` placeholder),
  `manifest.json`.
- The developer flag: manifest field `developerOnly`, read by `modlist::parse_developer` from
  `%LOCALAPPDATA%\PhasmoMods\launcher.ini` (`developer=1|true`, last line wins); the launcher's discover_mods
  and the host's find() both gate at the single manifest choke point (read_folder/read_copy return
  Reading::None for a developerOnly mod when the flag is off, so it never reaches any list, session file or
  lobby list). Documented in sdk/README.md and sdk/manifest.schema.json.
- SDK additions made along the way (all map entries/wrappers, no raw numbers): `Time.set_timeScale`
  (UnityAudio.h, new entry), `PlayerSanity.SetInsanity` wrapper (entry already mapped),
  `SetupPhaseController.UnlockMainDoor` wrapper, `EquipmentInfo.RemoveAmountOwned` wrapper,
  `PauseMenuController.type` wrapper.
- Progression suppression is fail closed: the mod refuses to start unless the hooks on GhostHuntState.Kill,
  ExitLevel.CheckMissions, ExitLevel.CheckChallenges and EquipmentInfo.RemoveAmountOwned are all created.
  The dev session turns on with the menu's first open; coverage past the two ExitLevel checks waits for the
  RE pass the plan names (stats, quests, apocalypse): never overclaim it.
- Tests: `dev_menu` (9 offline rule tests), the developer-flag cases in mod_list and mod_manifest, the
  raw_scan entry for the author-designed menu canvas, the text table checked by text_table_test.
- `.\build.ps1` and `.\validate.ps1` pass (93/93 ctest, package/stage/pinned/release tests included).
  `tools/gamecheck.ps1 -IfConfigured -Expect -Mods dist/PhasmoMods/mods` against build E300EDA9...: 1862
  entries, 1750 resolved, 0 missing, 0 ambiguous; record e300eda993f24799-cd106e0f8a54c36e.txt.

## Gaps v1 leaves (all named by the plan, not oversights)

- No row calls ExitLevel.Exit directly (it would need a fabricated PhotonMessageInfo); Leave/Quit go through
  the game's own PauseMenuController found with FindObjectOfType.
- Not v1: weather transition, player speed/noclip/teleport, dummy investigator, the re-type route, ESP/HUD,
  a Loader section, the shop. Each waits on the RE/SDK gaps the plan lists.
- The forced-type row cycles the offered GhostCollection (option 0 is the game's Random), because only six
  GhostType values are mapped in the enum.
- ScrollRect paging (decision 6) is not in yet; the Ghost page's 21 rows fit the 1080-high canvas for now.
- Player-facing wording is `PM_TEXT_AI` placeholders for the user; the manifest description is `PM_TEXT_TODO`.

## Smoke list (every line is the user's in-game check; each run carries the standing ban-risk warning)

The user launches the game; no loader file ever enters the game folder.

1. Flag off: the Mods panel, the launcher list and a friend's lobby never show Dev Menu; the DLL is not loaded.
2. Flag on (`developer=1` in launcher.ini): the mod loads; the keybinds menu shows its row; rebinding sticks
   through Apply and a restart.
3. Main menu (Photon connected, no room joined): the menu opens, and every contract, ghost and player row shows
   its refusal ("no contract is running"); the switches and the game-speed row still work.
4. Single player contract: the key opens the menu over the game; the game freezes (timeScale 0) while open,
   with the cursor free; the toggle closes it and the game runs at its old speed with the cursor back.
5. Difficulty rows: friendly, lose-equipment, evidence +/- (the next investigation reflects it), forced type
   cycling the offered list, reward read-outs.
6. Ghost rows: state line, identity line, force/end hunt, cooldown clear, appear/vanish, room pick + apply +
   random (the favourite room moves and the ghost warps), base speed +/- (the next hunt's speed follows),
   block-events toggle, each event kind fires unforced.
7. Player rows: sanity 0/50/100, freeze, die, revive, god mode refuses a hunt's kill.
8. Equipment row: a used consumable's saved stock stays.
9. Contract rows: unlock the main door; end the contract (the truck leaves a few seconds after the
   click and the result screen follows, with the payout the contract earned); Leave; Quit. Payout
   read-out matches the in-game result. With no contract, the end row shows the no-truck refusal.
10. Game rows: tick speed 0/0.5/1/2 cycle (also while frozen, and closing keeps the new speed), pause toggle,
    session and single-player read-outs.
11. Finish a dev-session contract (Leave and a full end): no XP, no money, no stats, no quest progress, no
    apocalypse movement; a contract started before the menu ever opened still earns normally.
12. The menu never opens in any room, public or private.

## 2026-11-11: review of the first implementation (fixes committed with it)

- Close left the nav column and background on screen (only the row texts were hidden): close and open now
  gate the whole canvas with one set_active, and the section switches keep hiding single rows.
- A scene change while the menu was open could leave the game frozen or the cursor free: menu::tick
  detects a dead canvas and recovers (restore the saved timeScale, take the cursor back, rebuild next
  open), and pmmod_stop's destroy does the same before throwing the canvas away.
- Leave/Quit found the pause menu only among active objects; it now retries the search with inactive
  objects included (the note says the game's own buttons reach it the same way).
- The plan's identity card and live-flag line were incomplete: ghost_identity now shows ghostType plus
  behaviourType when they differ (the Mimic), and ghost_line adds speedFlag, smudge and flamesBlownOut.
  The speedFlag entry was mapped but had no api wrapper; it has one now (sdk/map/GhostAI.h).
- parse_developer tolerates spaces around the value; the launcher.ini test covers it.
- Checked and fine: the key path <Keyboard>/backquote is this repository's own binding name
  (core/bindings.cpp maps it to VK_OEM_3); no new entry sits on an obfuscated class (Time, Difficulty,
  LevelValues, SetupPhaseController, EquipmentInfo, PauseMenuController, PlayerSanity are the game's own
  names, and gamecheck resolves all 1862 with 0 missing); Kill's detour returning false matches the
  entry's documented semantics; the four hooks are all created before enable_all and the host rolls the
  group back if the mod's start fails.

## 2026-11-11: the end-the-contract row

- RE (`re/out/exit_attempt.c`, `re/out/exit_coroutines.c`): `ExitLevel.StartAttemptExitLevel` is an ordinary
  public method, not an RPC. It stops the object's coroutines, and when the object's ready bool (serialized
  in the scene; the editor writes it, no game code does) is set, starts a coroutine that sets
  `Network.sceneChanging`, RPCs the truck start-up sound through the object's own `PhotonView`, waits a fixed
  scaled `WaitForSeconds`, then rechecks the bool: set still, it sends the game's own `ExitLevel.Exit` RPC
  with the game's own sender; cleared meanwhile, it RPCs the stop sound and clears `sceneChanging`.
  `LiftButton.NetworkedUse` calls the method only with use true and `PhotonNetwork.get_IsMasterClient()`.
  In the decompile the yield reads as a MeshCombineStudio constructor: the linker folds both float ctors to
  one address and the name table lists both (pitfall now in sdk/internals/reverse-engineering.md); the yield
  type global and its use across unrelated coroutines settle it as the game's scaled wait, so a game held at
  timeScale 0 never finishes leaving.
- The row is therefore one call: find the scene's `ExitLevel` (cached, at most one search a second, the
  inactive-object retry like the pause menu's) and call `StartAttemptExitLevel`. Offline this client is the
  master, so this is exactly the call the game's own lift button makes; the mod never touches an RPC or a
  PhotonMessageInfo. With no `ExitLevel` in the scene the row refuses (`Why::NoTruck`, reported at the click,
  not from refuse()). The row closes the menu after starting, since the wait is scaled; the scene change the
  ending causes is then handled by last round's recover() path.
- SDK: `ExitLevel_StartAttemptExitLevel` was already a mapped entry; it gained api wrappers (`ExitLevel::type`
  and `StartAttemptExitLevel`) with full notes, `sdk/systems/contract.md` gained "What
  ExitLevel.StartAttemptExitLevel does", and the internals page gained the folded-ctor pitfall.
- i18n: `dev-menu.end-contract` and `dev-menu.why.no-truck` (both PM_TEXT_AI). Tests: the rule table now
  covers EndContract (allowed in a full offline contract; NoContract with none).
- validate: 93/93, docs clean; gamecheck unchanged (record e300eda993f24799-cd106e0f8a54c36e.txt).

## 2026-11-11: the user's first live try - the launcher ate the developer line

- Symptom: Dev Menu in the launcher's list with the flag on, but absent from the in-game Mods panel,
  no keybinds row, and the key did nothing; mods.log had no dev-menu lines at all (the gate's
  Reading::None is silent by design). launcher.ini on disk held only the enabled= line.
- Cause (our bug, not the game's): modlist::save_choice opens launcher.ini with trunc and rewrites it
  from scratch, so any save - ticking a mod, or chosen()'s id migration - deleted the hand-edited
  `developer=1` line between the launcher run that showed the mod and the game run that reads it.
- Fix: save_choice keeps every line it does not own (developer=, comments); it replaces only the
  enabled= line and its own header comment, writing one of each. The launcher's parse side already
  handled CRLF (each_line strips \r), so a Notepad-edited flag parses fine.
- Test (red first): the_saved_choice_keeps_the_lines_the_launcher_does_not_own - a CRLF ini with a
  note, an old header, an old enabled line and developer=1; after the save the flag parses on, the
  note survives, the old enabled line is gone (not shadowed), and the header appears once. Fixture
  note: write the fixture bytes with ios::binary - a text-mode ofstream turns a literal "\r\n" into
  "\r\r\n" on disk.
- validate 93/93; no map entries moved, so no gamecheck record change.
- For the user: put `developer=1` back in launcher.ini; ticking mods in the launcher now keeps it.

## 2026-11-11: second live try - the gate could never open (fixed)

- Symptom: mod loaded, keybinds row present and bound to <Keyboard>/backquote, pressing it did nothing.
- RE (`re/out/offline.c`, callers of `PhotonNetwork.set_OfflineMode` from `ConnectToServer` and the
  `Network` connection methods): the game's single player mode calls set_OfflineMode(true) only when
  neither `get_InLobby()` nor `get_IsConnectedAndReady()` holds, and then joins a room as usual - so in
  a solo contract `get_OfflineMode` and `get_InRoom` are both true. And the main menu is a connected
  Photon client (the lobby browser needs the connection; the user saw public lobbies listed), so
  OfflineMode is false there too. The gate `offline && !inRoom` was therefore false on every screen:
  the keypress fired and the rule honestly refused, silently.
- New rule (menu_rule.h may_open): sandbox = offline mode, or no room at all. Solo (offline+room) and
  the menu screen open; any online room never does. actions::can_open() fails closed when either
  `get_OfflineMode` or `get_InRoom` is unresolved (`available()`), since the new rule reads two probes.
  Can.offline renamed Can.sandbox (it is the gate, not the probe); snapshot fills it from can_open();
  the tick's defensive close now uses can_open() too. The raw "Single player" read-out row still shows
  the bare offline probe.
- Facts recorded for authors in sdk/systems/photon.md ("Which screens are rooms, and when offline mode
  is on"); the v1 smoke item 3 wording ("the game is offline there too") was a disproved assumption,
  reworded.
- Tests: the_menu_opens_only_in_the_sandbox covers all four states (the old defensive case
  may_open(true,true) is now the normal solo case, and may_open(false,false) is the menu screen).
  validate 93/93; gamecheck unchanged (record e300eda993f24799-cd106e0f8a54c36e.txt), the mod's
  requires list gained PhotonNetwork.get_InRoom.
- The user also asked about public lobbies still being listed at the main menu: expected. The loader
  rules are about joining - every mod is off in a public room - and never touch the vanilla browser.

## Plan rows the v1 cut left out although their map entries exist (for the user to schedule)

- Contract: objective board read-out (LevelValues.mainObjective, Objective.values) and the objective's
  moneyReward number. End-contract-now shipped 2026-11-11 through `ExitLevel.StartAttemptExitLevel`
  (the game's own lift-button starter, so no RPC is touched; see the map entry note and
  sdk/systems/contract.md).
- Difficulty: the preset-index row (Difficulty.difficulty, the plan itself marks it verify); the ghost
  speed multiplier field (RE).
- Ghost: kill-test rows (GhostHuntState.SetTarget, SetCloset, clear GhostAI.smudge), the live
  baseSpeed x sightSpeedUp product, GhostAI.EventPick roll, forced interaction (GhostActivity.TryInteract,
  NormalInteraction, GetDoorToOpen, TwinInteraction), the type ability row, the throw row, model show/hide
  via GhostAI.model renderers, givenEvidence read-out plus the never-synced dev override, and the type-flag
  dials (SetNewBansheeTarget, thayeStep/thayeThreshold, galluState, dayanState, interactionBoost).
- Player: stamina set/refill (Player.stamina).
- Equipment: the loadout reset rows (InventoryEquipment.ResetItem, ReturnCommittedItem,
  InventoryManager.ReturnItemstoInventory; lobby-side).
- Sections 6 Debug HUD and ESP, the sensors read-out, and section 7 Loader are whole-feature follow-ups,
  not oversights of v1.
