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

## 2026-11-11: third live try - the menu was hand-drawn, so the game's UI never saw it (fixed)

- Symptoms the user reported: the camera kept following the mouse behind the menu; rows could not be
  selected and had no hover highlight; no keyboard navigation; and closing the menu set the graphics
  pulsing bright and dark and then a black screen (game still running) - that last one unexplained yet,
  the next live try watches it (it is not reproducible offline; if it survives the fixes it goes to
  FINDINGS.md as a game issue with our steps).
- Cause (all ours): the canvas was painted, not built. The rows were RawImage bars with no Button
  component, the canvas had no GraphicRaycaster, and the click came from the mod polling the mouse. And
  the game never knew a menu was up: nothing held the player, so the look stayed live. Reading the game
  settled the rest (`re/out/evsys.c`): `JournalController.OpenCloseJournal` calls
  `EventSystem.currentInputModule.set_enabled(journalOpen)` - during a contract the EventSystem's one
  input module is switched OFF, so no canvas of any kind takes clicks and no menu takes arrow keys
  until something switches it on; the same toggle calls `PCCanvas.PausePlayerInPlace` (journal, its
  options page, and `VoiceRecognitionUI.ToggleUI` are that method's only callers).
- RE facts, recorded in the map notes and sdk/systems/menus.md ("Unity's UI input during a contract"):
  `FirstPersonController.EnableMovement(on)` writes its bool into the controller's two movement flags
  and with false also freezes the player's Rigidbody rotation (enabling never unfreezes);
  `PausePlayerInPlace(paused)` skips when the movement already matches the request, refuses the release
  when one component of the player holds an unidentified bool, and otherwise cuts walk/look, hides the
  crosshair, resets item sway, switches the HUD CanvasGroup interactable off/on and follows the game's
  cursor lock. New map entries: `PCCanvas.PausePlayerInPlace`, `FirstPersonController` and
  `FirstPersonController.EnableMovement`, `EventSystem.SetSelectedGameObject` (the EventSystem type,
  `get_current` and `currentInputModule` were already mapped). `EventSystemChecker.Awake` makes an
  EventSystem with its input module when a scene lacks one.
- The user chose the full vanilla route for the fix (their answer, 2026-11-11). Contract 2.4 adds
  `unity_action_new(target, listener)` at the end of PmHostApi: a UnityAction made the way the host's own
  menu panel makes them (`runtime_services::delegate_new` over the game's UnityAction ctor/Invoke; the
  listener takes the delegate's target, the instance-method calling convention leaves the hidden
  MethodInfo unread). `target` must be a managed object; key state off the target it is called with.
- New `sdk/api/buttons.h` (+ .cpp, pmmod): `attach` (the game's Button on the mod's own Graphic;
  `Selectable.Awake` takes the same object's Graphic as its target, decompiled, so the game's colour tints
  work on a component added at runtime), `catch_pointer` (a GraphicRaycaster on the canvas), `on_click`
  (the host's action added to the button's own onClick; a small table maps the target back to the mod's
  handler; re-wiring replaces, destroyed buttons are pruned and silent), `forget`, and the input-module
  trio plus `select` (EventSystem.current / SetSelectedGameObject, so arrow keys and the gamepad start
  from the menu).
- The mod: rows are bars carrying real Buttons (bar colour 9% white so the game's own hover/press tints
  show), the canvas gets a raycaster, open() borrows the input module (remembering its state), calls
  `actions::pause_player(true)` (the journal's own hold) and selects the section's first row; section
  clicks re-select; tick re-asserts the module and the hold (closing the journal mid-menu takes both
  away, and both calls skip a state that already matches); close/recover/destroy give both back; the
  mouse-polling block is gone. No new player-facing text; no i18n change.
- Tests: new `buttons` (offline, over the fake: attach idempotence, raycaster, click reaches the handler
  with its user, re-wire, forget, destroyed button silent and pruned, module read/switch, selection by
  component/GameObject/clear). The fake host gained `unity_action_new` (records asks, answers a token)
  with `fake::unity_actions()`. mods_test now pins contract 2.4. Note for test authors: a TU with static
  `Held`s ending on live handles can crash in static teardown behind the fake (destruction order) -
  empty the table (forget) before exit; buttons_test does.
- validate 94/94 (buttons new, mods version line updated), docs clean, package/stage/pinned/release pass.
  gamecheck on the same record e300eda993f24799-cd106e0f8a54c36e.txt: 1866 entries, 1754 resolved, 0
  missing, 0 ambiguous; the four new entries all resolve (PausePlayerInPlace, EnableMovement, the type,
  SetSelectedGameObject).

## Smoke list addenda for the next live try

13. Solo contract, menu open: the camera and crosshair stay still under the menu (the journal's hold),
    the mouse is free; rows show the game's hover and press look; arrows/gamepad move a selection, Enter
    or A clicks; closing gives movement, crosshair, cursor lock and the game's HUD back, and the input
    module goes back to the state open() found.
14. Open the menu, press J for the journal, close it, then use the menu: it keeps working (the mod
    re-asserts the input module and the hold next frame).
15. Watch the graphics on close: the bright/dark pulsing and black screen from the third try must not
    come back. If it does, the camera hold was not the cause - file it in FINDINGS.md as a game
    behaviour with our exact steps and retest at the next build.


## 2026-11-12: fourth live try - no buttons, an exception storm, and the picture (fixed)

The user's fourth report, with a screenshot of the main menu room bright and harsh: "The menu buttons
still don't work, and the graphics still freak out after closing the menu."

What was wrong, and the proof:

1. No row ever had a Button. `sdk/api/buttons.cpp`'s `attach` and `catch_pointer` accepted only GameObjects,
   while `pm::ui`'s creators hand back components (a Canvas, a RawImage), and their `GetComponent` was
   `Component`'s own method run with a GameObject as its object. Every row logged "could not be made a
   button". The helpers now take either, reach siblings through the GameObject's `Transform` (every GameObject
   has one) and add through the GameObject, and `attach` switches the row's `Graphic.raycastTarget` on,
   without which the raycaster never sees the row at all.
2. A NullReferenceException every frame at the main menu. `LevelValues.GetInvestigationBonusReward` throws
   when `LevelValues.mainObjective` is null, when the main objective has no `ObjectiveValues`, or when the
   side-objectives list (an unmapped field) is null or holds a null entry. `actions::bonus_reward` now asks
   only with a live LevelController and a main objective; a row whose read throws is logged once and shown
   empty; `refresh()` reads only the section on screen, not all six.
3. The host's bare "tick: managed exception caught at the frame boundary" line named no mod: the mod's own
   log sink carries the prefix but `log_exception` does not. `guarded_tick` now puts the mod's id in the
   message it hands the host.
4. The pause is off by default. The user chose "default off, keep rows" (2026-11-12), so
   `Switches::pauseWhenOpen` is false and opening the menu leaves `Time.timeScale` alone. The row and the
   tick-speed row stay for a developer who wants them. Test first: `dev_menu_test` gained
   `the_menu_does_not_pause_the_clock_by_default`, red against the old default and green after. `open()` and
   `close()` log the time scale they found and whether the player was held, so the next log says plainly what
   the menu did to the clock.
5. The hold is asked only for a PC rig that has a HUD and a journal controller
   (`LocalPCPlayer::type.is(rig)`), because the release reads that controller and throws when it is missing.
   `pause_player` answers whether it asked, and only a hold the menu took is re-asserted or released.

## 2026-11-12: verification pass over everything written (the user's request)

Every claim in this branch's code comments, map notes and system pages was re-read against the build (the C#
dump for shapes and offsets, `re/callers.mjs` for callers, headless Ghidra for bodies). Confirmed as written:
the journal toggle switches `EventSystem.currentInputModule` and calls `PCCanvas.PausePlayerInPlace` on
`LocalPCPlayer.canvas`; the hold's chain (EnableMovement false, a zero on the animation component, the HUD
state, the crosshair inactive, sway reset, CanvasGroup interactable off, cursor free) and its release (the
journal-controller check first, then the reverse, cursor locked again); `CursorLockMode` is None 0, Locked 1,
Confined 2, so our free-then-lock order around the hold is right; the offsets `LocalPlayer.player` +0x20,
`journalController` +0x28, `camera` +0x30, `LocalPCPlayer.canvas` +0x120, `FirstPersonController` +0x128,
`PCItemSway` +0x138, and that `LocalPCPlayer : LocalPlayer` with `PCCanvas` holding a `LocalPCPlayer`, so the
rig's base fields sit where the map says; `ObjectiveValues.moneyReward` +0x30; and the pause's callers, which
are the journal toggle, the options page, one more private journal method and the voice recognition UI.

What the pass corrected or strengthened:

- `Player.h`'s PausePlayerInPlace note claimed a sibling HUD-state method writes a different value to the
  same float. Decompiling `PCCanvas.SetState` by RVA does not show that, so the claim is gone and the note was
  rewritten from the decompile (it now says the journal check runs before the movement switch).
- `Difficulty.h` said the reward is the main objective's moneyReward "plus each side objective's". The loop
  adds only an objective whose own bool flag reads true, and that flag is obfuscated, so the note says that.
- The raycastTarget and GraphicRaycaster claims are proven, not inferred: `Graphic.set_raycastTarget` puts a
  graphic into or out of its canvas's raycast list (Unity's own graphic registry) and only while the component
  is active and enabled, and `GraphicRaycaster.Raycast` walks nothing but that list; no member of
  UnityEngine.UIModule's `Canvas` mentions a raycast. Both map notes moved from "Inferred: Unity's documented
  API" to the decompiled facts, and `sdk/systems/menus.md` gained "What a canvas needs before it answers the
  pointer" and "How a click reaches a Button".
- The Button paths are read from the build: `OnPointerClick` returns unless the event's button is the left
  one and then walks `onClick`'s invoke list after two guards on the button; `OnSubmit` calls `Press`, which
  walks the same list, then starts a short coroutine. `sdk/api/buttons.h` now documents that, including that a
  Button which is off, hidden or not interactable runs nothing.
- The clock claims hold two ways: 64 sites call `Time.set_timeScale` and all of them are middleware (a
  slow-motion demo's `Update`, an on-screen console, and the UI framework's time-scale graph node), and the C#
  dump mentions `timeScale` in only four non-game assemblies. The auto-exposure claim holds too: the exposure
  feature's compute pass puts `Time.get_deltaTime` into its per-frame constants beside the EV range and the
  adaptation speeds. `sdk/systems/media.md` gained "What a frozen clock does to the picture", which also tells
  authors to take the game's hold and leave the clock alone.

Still unproven: which of the menu's open-time changes blows the picture out. The pause is the only thing all
three broken rounds shared, and it is now off by default, so the live test decides. If the picture still goes
wrong with the pause off, file it in FINDINGS.md with the exact steps and stop blaming the clock.

Checks: build clean; `validate.ps1` 94/94; the API reference has no warnings; gamecheck 1867 entries, 1755
resolved, 0 missing, and `LocalPlayer.journalController` resolves (the record file's map hash moved with the
map, as it should).

## Smoke list addenda for the fifth live try (each line is the user's in-game check; the standing ban-risk warning still applies)

16. Main menu, single player: open the menu, move a selection with the arrows, press Enter. The row runs. With
    the mouse: hover shows the game's hover look, a left click runs the row, a right click does nothing.
17. `mods.log` gains no "tick: managed exception" line while the menu sits open at the main menu. If one row's
    read throws, exactly one line names that row and it shows empty from then on.
18. `mods.log` shows "dev-menu: open at timeScale 1.00, ..." and "dev-menu: closed at timeScale 1.00", and
    nothing changed the clock unless the pause row was switched on.
19. Graphics, pause off (the new default): open and close the menu at the main menu and in a contract; the
    picture stays as it was. Then switch "Pause when open" on, leave it open a minute and close: if the
    picture comes back bright and harsh, the pause is the cause. Say which happened.
20. With the pause on, the tick-speed row still cycles 0x, 0.5x, 1x, 2x, and closing the menu gives back the
    speed that ran before the pause.
21. The game's hold is felt behind the menu: no walking, no looking, no crosshair. Movement, crosshair, the
    cursor lock and the HUD come back on close, and the input module goes back to the state open() found.
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

## 2026-11-12: fifth live try - the rows the brief asked for, and a list that really scrolls

The user's fifth report: "it worked fine until I changed the game speed setting, then the same thing
happened (flashing colours then black screen). ... many of the options I asked for are missing,
including noclip and player movement speed ... the menu UI is placed too high and some text appears
over the 'Dev Menu' heading". They then pointed back at their original feature list (the untracked
`backup` file) and asked that everything in it be present, plus any extras worth adding.

What landed:

- The menu geometry lives in `mods/dev_menu/menu_layout.h` so the offline test can check it, which is
  the fix for the placement defect rather than a patch of the numbers. The old numbers let the hint
  line overlap the first row and put the section column partly off the 1920 canvas (`kNavX` -970 to
  -950). `the_rows_clear_the_heading_and_the_hint` and `a_long_section_reaches_its_last_row` hold every
  gap at 4px or more and pin the columns inside the canvas; both ran red against the old layout first,
  which is the user's defect written as a test.
- The list is the `ScrollRect` the user asked for. `core/ui.h` gained `create_scroll_view`,
  `set_scroll_height`, `scroll_reveal` and the pure `scroll_top_needed`: the viewport is stretched, the
  content is a child of it pinned to the viewport's top edge so rows never move as the list grows, a
  `RectMask2D` on the viewport stops off-view rows drawing over the heading, `movementType` is set to
  Clamped because a ScrollRect made at run time holds the free-running mode (the editor's Reset never
  runs), and a background graphic with `raycastTarget` on is what lets the wheel and a drag reach it at
  all. `follow_selection()` reads the event system's selection through `api::buttons::selected()` (new,
  over `EventSystem.get_currentSelectedGameObject`) and scrolls the selected row into view, so
  arrow-key navigation reaches a row below the frame. `sdk/systems/menus.md` gained "A list that
  scrolls" for authors.
- Player movement speed rows use the game's own path, not a controller write:
  `LocalPCPlayer.SetPlayerSpeedMultiplier` (the Monkey Paw speed wish's route) and
  `LocalPCPlayer.ResetPlayerSpeedMultiplier` (mapped this round; it copies back what the rig kept when
  its speed was first set and re-applies it). A quarter per press, snapped to quarters so the read-out
  stays readable; the 0.25..4 bounds are the menu's own, not the game's, and the note says so. The
  game's own walk and sprint arithmetic still produces the real speed, so nothing is capped.
- The ghost's line of sight is now a row, built on the sensor chain: `GhostAI.vision` is a `LOSSensor`
  that pulses and tests its own input sensor, and what that sensor detects is its `RangeSensor.layers`
  mask, so emptying the mask blinds the ghost's sight and nothing else. The mask is saved on the way in
  and put back on the toggle and in `stop()`: a mod must never leave a contract's ghost sightless.
- The debug HUD row (Game section) is the plan's section 6 opened: one canvas of its own at
  sortingOrder 29000 that survives the menu closing, showing the ghost's state, identity, hunt base
  speed, and the hunt target's room named through `Player::currentRoom` and the level's room list. The
  player, contract and sensor blocks and ESP are still follow-ups.

The clock, again. The user's answer to the fifth try was to keep the rows and find the cause, so
FINDINGS.md 33 carries what is proven (nothing in the game's own code writes the time scale; only
middleware does; the auto-exposure pass feeds its adaptation the scaled frame time) and the experiment
matrix the next live try should walk, one value at a time. FINDINGS.md 34 records why noclip did not
land: the controller's step writes the Rigidbody's position and hands a recomputed capsule to a pass
whose job is not yet known, no physics query with a layer mask appears in it, and the two layer-mask
fields and that pass carry names the game shuffles every update, so their entries have to be resolved by
shape. It also needs the user's design answer: through walls while still walking on floors, or free
flight.

Still missing against the brief, in the order the plan puts them: weather mid-contract (the Paw's
transition path), unlock-everything and the free shop, the forced interaction and the type's ability,
the difficulty's ghost speed multiplier, ESP, and the dummy networked player (the user's own ordering:
after the row gaps).

Text for the user: ten new labels are `PM_TEXT_TODO` placeholders and need the user's wording -
`dev-menu.ghost-blind`, `.player-speed`, `.speed-down`, `.speed-up`, `.speed-reset`, `.debug-hud`,
`.hud.state`, `.hud.type`, `.hud.speed`, `.hud.target`.

Checks: build clean; `validate.ps1` 100% of 94 tests; docs tests pass with no API reference warnings;
gamecheck exits 0 with 1881 entries, 1769 resolved and 0 missing, and `ScrollRect.movementType`
resolves as the nested enum (the entry needs no `.type`: the field is enum-typed, and asking for an int
was what made gamecheck report it missing).

## Smoke list addenda for the sixth live try (the standing ban-risk warning still applies)

22. The list scrolls: open Ghost (the longest section) and turn the wheel inside the rows. The rows
    move inside the frame, and nothing ever draws over the "Dev Menu" heading, the hint line or the
    section column. Drag a row with the left button and let go near an edge: the list stops at the edge
    instead of flying out of the menu (Clamped).
23. Arrow keys: hold down past the last row you can see. The list scrolls to keep the selected row in
    view and Enter still runs it.
24. Player speed: read the row's multiplier, press + twice, and walk (the stride is faster); press the
    reset row and the walk comes back to normal on its own, with no contract restart.
25. Ghost blind: switch it on and force a hunt that would normally see you from the same room; the hunt
    does not come for you. Switch it off, or stop the mod, and the ghost finds players normally again,
    including in the hunt that follows.
26. Debug HUD: switch it on, then close the menu. The read-out stays up and changes during a hunt: the
    state name, the type, the base speed and the target's room name. Switch it off and it goes.
27. The clock, one value at a time (the user's choice: keep the rows, find the cause): at the main menu
    set tick speed 0.5x, then 1x, then 2x, then 0x, then 1x, and at each step say whether the picture is
    wrong, whether writing 1 back brings it back, and whether the game is still running. Then repeat in
    a contract and end the contract to see whether a scene change clears it. FINDINGS.md 33 has the
    matrix and what is already proven.

## Round 7 (2026-11-12): the crash on opening the menu, found and fixed at the SDK layer

The user's report: "trying to open the dev menu crashed the game".

The chain was read, not guessed.

- Our own log ends at `actions::note_session()` ("dev session on") and never reaches the last line of
  `menu::open()`, so the fault is inside `menu::build()`.
- The game's crash folder held a `crash.dmp`. Reading it with `cdb -z` gives the exception record: an access
  violation (c0000005) on a read of `ffffffff0000000b`, from `mov rdi, qword ptr [rax+8]` with `rax` loaded from
  `[rbx+30h]`, inside UnityPlayer.
- DevMenu was relinked with the linker's map file, after comparing its `.text` byte for byte against the build that
  actually ran so the map belongs to that code. That puts our own names under the crash's frames: the guarded tick,
  the mod's tick lambda, `menu::open()+117`, `menu::build()+1123`, `pm::ui::create_scroll_view()+356`. The call at
  that address is `game::transform_of(view.viewport)`.

Root cause: a type confusion in our own helper. `ui::child` makes GameObjects, and `core/game.cpp`'s `transform_of`
asked every object through `Component.get_transform`. A GameObject is not a Component, so the engine read a layout
that object does not have and faulted. Every older use of that helper handed over a component, which is why only the
new scroll list fell over, and why five live tries passed before it.

Fix at the SDK layer rather than at the call site, so no mod can repeat it:

- `pm::game::transform_of` and `pm::game::game_object_of` ask the object's own class which accessor answers it
  (`GameObject` map entry, through `pm::api::Type::klass()` and `.is()`), and answer null when the class cannot be
  read. A GameObject's own GameObject is itself, as the engine's own `GameObject.gameObject` says.
- New `pm::game::rect_transform_of`: the object's transform, but only when its class says it is a RectTransform
  (`RectTransform` map entry). `core/ui.cpp`'s place, stretch, set_scroll_height, contains and normalized_point all
  use it, and log once instead of faulting when the object has no RectTransform. `ScrollRect.content` and
  `ScrollRect.viewport` are typed RectTransform in the map, so those two field sets use it too.
- `create_scroll_view` now puts the ScrollRect and the RectMask2D on the viewport before placing it, and gives the
  content a `LayoutElement` before stretching it. The order is what the game's own package declares: every UI class
  in this build's UnityEngine.UI carries `[RequireComponent(typeof(RectTransform))]` (the decompiled Graphic,
  RectMask2D, ScrollRect and LayoutElement), and the fifth live try laid out every row that was made as a bare
  GameObject plus a UI component. What converts the transform was not read, and nothing needs to know: the code asks
  the object instead of assuming, and refuses rather than faulting.

Tests first, as always. `tests/object_kind_test.cpp` (a new target) went red with 12 checks against the old
helpers; after that fix the four new checks for the RectTransform question went red; then green. `validate.ps1`:
95 of 95 tests pass, the docs tests pass, and the API reference builds with no warnings.
`tools/gamecheck.ps1 -IfConfigured -Expect` against the pinned build: 1882 entries, 1770 resolved, 0 missing,
0 ambiguous, with `LayoutElement` resolving by name.

Docs that carry the fact now: `sdk/systems/menus.md` ("A list that scrolls", the two kinds of object and what a
layout entry may act on), `sdk/internals/reverse-engineering.md` (the crash, how it was read, and what shape to
look for), the notes on `transform_of`, `object_transform`, `game_object_of` and `rect_transform_of` in
`core/game.h`, and the notes on `place` and `create_scroll_view` in `core/ui.h`.

Smoke list for the sixth live try, item 28: open the menu, and it must not fault. If the log carries "an object
asked to be placed has no RectTransform" or "a scroll view's content has no RectTransform", that is the engine
answering how a transform becomes a RectTransform, and the list will look wrong rather than crash the game. Report
the line.

Left open, and unproven: `mods/playable_ghost/journal.cpp` adds `RectTransform` itself as a component to a GameObject
it makes, which has never been proven from the game. It is guarded (`if (!rect) continue;`) and `place` now refuses
what is not a RectTransform, so it cannot fault; but if Playable Ghost's journal pages go missing, look there first.

## Round 8 (2026-11-12): the revive report, a safety audit, progress protection and the missing features

The user's report: the revive row crashed the game. The game's own log says more: `HACKER DETECTED: ... Revive`, a
self-report raised through the game's cloud code, then a fault. The decompile of Player's Revive coroutine shows why:
before its master check it needs a list held by LevelValues to contain 1 or 7 (very likely the Monkey Paw and the tarot
cards) and the player dead or dying, and otherwise reports the sender. Offline skips only the sender checks. The plan's
"no RPC can be refused offline" was wrong; corrected in the plan, photon.md, players.md and Player.RevivePlayer's note.
The only report in the retained logs is this one click.

Safety: the revive row is gone. menu_rule.h now refuses wherever the game's body would: Kill needs a contract (the death
RPC refuses on the menu's scene) and refuses friendly, dead or mid-hunt; Force hunt refuses on friendly; the Friendly
switch refuses mid-hunt (photon.md's "behind the game's back"); Appear refuses mid-hunt.

Progress protection was half built: only CheckMissions/CheckChallenges were hooked, so a dev session still paid XP and
money and could lose equipment. Now hooked, all on the dev session: RewardManager.StartResults (money save, challenges,
event score; Next does not depend on it, per its decompile), SingleplayerProfile.AddXPAndFunds, and
InventoryManager.ResetAllEquipment (runs the game's ReturnItemstoInventory instead); consumables are always kept in a
session, so the keep switch became a read-out. Stats, quests and apocalypse remain unmapped (plan's RE pass).

New: noclip (PhysicsCharacterController, new map entries; its FixedUpdate is the whole physical step and starts from the
transform, decompiled; noclip disables it and the Move hook turns the game's own walk along the camera's pitch),
teleport to the ghost / its room, bring the ghost to you (NavMeshAgent.Warp on the mesh), interact / ability / throw now
(unforced ChangeState), sanity lock, the given evidence read-out, ESP (ghost and room labels) and an 11-line HUD in the
new overlay.cpp (drafted by the local model, reviewed). All PM_TEXT_TODO replaced with PM_TEXT_AI text; the duplicated
speed-up/speed-down keys split into ghost-* and player-*. FINDINGS 34 (noclip) removed.

Not built, with reasons: mid-contract ghost re-type (needs CreateGhost/SyncValues/SyncEvidence re-run outside their
once-per-contract state, exactly the kind of RPC state check that reports; RE spike first), weather mid-contract
(TransitionWeatherProfile unmapped; lightning refuses outside a storm), unlock-all / free shop (writes saves, against
progress protection), the dummy player (spike). Open: whether PlayerSanity.insanity is sanity or insanity (its note says
100 is most insane; vanilla-ghosts.md's Thaye threshold reads like sanity) - the rows treat the field consistently.

Checks: validate.ps1 100% of 95 tests, docs and API reference clean; gamecheck 1884 entries, 1772 resolved, 0 missing,
0 ambiguous. raw_scan's canvas allowlist names overlay.cpp (plan's Game section; the author's ESP ask).

## Smoke list addenda for the seventh live try (the standing ban-risk warning still applies)

29. No revive row exists. Kill you: refused in the lobby, on a friendly difficulty and mid-hunt, with the reason shown.
30. Progress: note money, XP and level; play a dev contract, die with lose-items on, finish. The reward screen shows no
    totals and Next returns to the lobby; money, XP, level and the brought equipment are unchanged.
31. Noclip: on, look up and walk: you rise through the ceiling; sprint is faster; off: you fall back onto a floor.
32. Teleport to the ghost, to the ghost room, and bring the ghost to you; each lands on a floor.
33. Interact / ability / throw now: the ghost tries each (the game may still refuse, which shows as nothing happening).
34. Hold sanity at 50, then 0: the read-out stays there. Say whether "0" makes the ghost hunt more (insanity question).
35. Debug HUD and ESP on, menu closed: ghost label with its distance follows the ghost through walls; room names float
    in each room.
36. Search the game's Player.log for "HACKER" after the run; there must be none.

## Round 9 (2026-11-12): revive restored, kill in hunts, weather

The user: the self-report is not an issue; restore revive properly, not limited to contracts with the paw or the
cards; why no kill in a hunt; weather (to and from the Blood Moon), ghost type and dummy wanted; investigate unlock
and free shop without save writes.

- Crash cause read from the dump: the stack bottoms out in the game's own CloudEvents self-report continuation (the
  moderation call's completion), so the crash was the report's, not ours. The Revive body's list is
  `Difficulty.actualCursedItems` (real name, a CursedItemType[]: none 0, tarotCards 1, ouijaBoard 2, mirror 3,
  musicBox 4, summoningCircle 5, voodooDoll 6, monkeyPaw 7). New entries CursedItemType(+tarotCards, monkeyPaw) and
  Difficulty.actualCursedItems, resolved at +0xB0 with 1 and 7.
- Revive (the user's explicit allowance of the array swap, after the auto-mode classifier refused it once as a
  security weakening): a contract without either reviver gets a copy of its array with the paw added, the game's own
  Player.RevivePlayer runs, and the contract's own array goes back once the investigator is neither dead nor dying,
  or after 5 seconds (actions::tick, and on stop).
- Kill: hunts no longer refuse it. New entry Player.dying (the bool NotifyOfDeathStart stores; resolved +0x29, the
  byte the Revive body reads); the kill refuses only while a kill is already under way.
- Weather: RandomWeather.TransitionWeatherProfile (real name) mapped, decompiled: local only, writes
  LevelValues.weather, returns for a dead local player, else sound and sky tween; never writes actualWeather. The paw's
  handler calls it and costs 7 sanity. WeatherValueType (Difficulty's numbering, Random 0 .. BloodMoon 8) mapped and
  paired by name with WeatherType. The row writes Difficulty.actualWeather (read live by UpdateSpeed and GhostAbility)
  then runs the transition, so the Blood Moon is real both ways.

Smoke additions: 37. Die, then Revive in a contract with no cursed possession and one with the paw: you stand up, and
Player.log has no HACKER line. 38. Kill during a hunt works; a second kill click while dying says a kill is under way.
39. Weather to bloodMoon: red sky, and hunts run at Blood Moon speed (HUD live speed); back to clear: normal speed.
Heavy rain starts lightning; leaving it stops it.

Round 9, continued: the ghost type, changed now. The local model's research (verified for the risky step: the
SyncEvidence decompile tests its once-flag first and returns silently) gave the path now in ghost-types.md
("Changing the type mid-contract"): controller identity and behaviour, PickEvidence, clear GhostInfo.evidenceSynced
(new entry, GhostInfo's one bool, +0x7C; a Loads/Stores fact could not prove the register, so shape alone), then
GhostAI.Init on the controller's traits (the spawn's own path: sync RPCs as master, every per-type start step, and
the host's custom-ghost hooks). Then EvidenceController.ghostOrb (real name) is switched off when the new type gives
no orbs and EvidenceController.freezing (by its Stores fact in SpawnAllGhostTypeEvidence, +0x58) is set from the new
evidence. Refused mid-hunt. Not re-run: the Mimic/Thaye/Obambo timers; the model. gamecheck 1908 entries, 1796
resolved, 0 missing, 0 ambiguous.

Smoke 40: re-type a Spirit to a Mimic: orbs appear and its evidence line changes; to a type without orbs: the orb is
gone; to one with freezing: the ghost room freezes; to a Banshee: hunts aim at you; to a custom ghost: its tell runs.
Player.log has no HACKER line.

Shop and unlock research (local model, not yet built; scratchpad shop_findings.md, summary for the ledger): StoreItem's
obfuscated no-arg bool is the single unlock gate (level and an ES3 save bool), with a sibling level-only gate;
StoreEquipment.Buy and BuyUpgrade read and write ES3 money and the owned/upgrade keys directly; StoreItem's owned
amount setter writes no save. A save-free free shop means: hook the two gates to true, CanBuy/CanBuyUpgrade to true,
and replace Buy/BuyUpgrade with the owned-amount setter only. Open risk: ES3 may serialise whole StoreItems on quit
(diff the save in a live test). The gates are obfuscated with decoys: they need shape-and-fact entries before use.

Round 9, continued: unlock everything, the free shop and the save freeze (the user's ask: no change to the save).

- The lobby's equipment commit and the post-contract return both write the save on their own, so hooking the shop's
  writers one by one would leak free items into the save through the return. Instead: EasySave3's
  `ES3IO.CommitBackup` (real names, assembly EasySave3) is the one step of all 13 of its writers that replaces the real
  save (decompiled: moves the written .tmp over the file, or copies the temporary PlayerPrefs key). Skipped once a dev
  session starts, for the rest of the run (menu_rule.h freeze_save): nothing the run saves reaches disk. It stays on
  until the game closes, since in-memory free purchases would otherwise reach the save with the next ordinary write.
  The Equipment section shows it ("Save frozen until the game closes").
- Unlock everything: StoreItem.IsUnlocked (obfuscated; CalledBy InventoryEquipment.CanAddToLobby) and
  StoreItem.MeetsLevel (CalledBy StoreItem.IsUnlocked) answer true. Free shop: StoreEquipment.CanBuy (real) and
  StoreItem.CanAfford (obfuscated; CalledBy StoreEquipment.Buy) answer true. Each answers only while the sandbox gate
  holds (store_switch_applies), so an online lobby later in the run gets the game's own answers. Vanilla Buy still
  runs: its money and owned writes stop at the frozen commit; the owned amount rises in memory.
- Fixtures: EasySave3 is a new assembly for the map, so tests/fixtures/diffable/EasySave3 (placeholder), the three
  fixture lists in tests/gamecheck_test.cmake and game_map_test's fake images (23, 4600 classes) gained it.
- gamecheck: 1917 entries, 1805 resolved, 0 missing, 0 ambiguous; the three obfuscated store methods land on the
  research's RVAs. validate.ps1: 95/95.
- Research notes (raw RVAs) kept in the ignored re/dump/notes/2026-11-12-dev-menu-{shop,retype,dummy}.md.

Smoke 41: open the menu, then in the lobby: Unlock everything shows every item and tier unlocked; Free shop buys with
no money; close the game, start it again without the menu: money, owned amounts and unlocks are as before the run,
and SaveFile.txt's timestamp did not change during the dev run (a .tmp beside it may have).

Next: the dummy investigator, chosen by the user as "any ghost's target" (research in re/dump/notes; needs the
player prefab's Resources name, a spawn with Player.Start never run, and hooks on the hunt's three obfuscated
target pickers, which walk Network.playerSpots only).

Round 9, continued: the dummy investigator, as "any ghost's target" (the user's choice).

- Body: a copy of the local investigator's networked body (game::instantiate under a parent that is switched off),
  every MonoBehaviour but Player, PlayerSanity and PlayerLOSTarget destroyed before it is switched on, those three
  switched off. So no script of its own runs (Player.Start, which would claim Network.localPlayer for a view the master
  thinks is its own, never runs; the PhotonView is gone before its Awake). Player.Awake (decompiled: clears one runtime
  list) is the only code of it that runs. Cloning the live body, not Resources.Load("PCPlayer"), keeps a hard-coded
  prefab name out of the code. Never in Network.playerSpots.
- Hooks (dummy.cpp): StartPick and KeepNearest offer the dummy when it stands nearer the ghost's head than the player
  the game kept (new entry GhostHuntState.kept, the hunt's fourth Player field, +0x60, by its Stores facts in
  KeepNearest and StartPick); SightPick offers it when the game found nobody and the hunt's own Sees passes. A Banshee
  goes for it only through "Make the dummy the Banshee's target" (GhostAI.bansheeTarget, which StartPick keeps).
  The kill hook kills the dummy itself (Player.isDead on it): the game's kill sends its death animation to the view's
  Photon owner, which the dummy lacks. Recorded difference: a dummy kill does not join the hunt's killed list or
  lengthen the hunt.
- Its room (Player.currentRoom) is kept current every frame; the hunt's tests throw on a player with none.
- ESP shows it with the ghost's ESP (its body may sit on the layer the local camera leaves out).
- New type wrappers PlayerLOSTarget::type and PlayerSanity::type. gamecheck 1918 / 1806 resolved / 0 missing /
  0 ambiguous; validate 95/95.

Smoke 42 (watch Player.log for exceptions): add the dummy, walk away from it: a hunt chases whichever of you is
nearer (switch on ESP to see it). Hide in a closet with the dummy in the open: the hunt goes for the dummy. Banshee
contract: make the dummy the target: the hunt ignores you. Let it kill the dummy: the ESP label stays, "Dummy
investigator" reads off; revive it. Remove it. Leave the contract with it present: no errors.
Open: whether the copy's body is visible to the local camera; whether any kept script's absence throws in a reader
the research did not list (PlayerAudio is gone: the hunt's Listen never reaches the dummy, which is not in a spot).

## Round 10 (2026-11-12): the remaining plan features

Committed round 9 as 6209a59. Then, with the local model doing the mechanical parts:

- Difficulty: every custom option. The local model mapped the 24 Difficulty fields still unmapped (all real names,
  "Inferred: from its name" since no decompile was cited) and the two nested option enums, DifficultyValueType (the
  C# ValueType: None..VeryHigh) and FuseBoxValueType (Broken, Off, On); gamecheck 1952 entries, 1840 resolved,
  0 missing, 0 ambiguous. The Difficulty section gained one picker over 27 options (option: value, previous, next,
  lower, higher): a switch flips, a level or the fuse box's power wraps through the game's own enum values (the top
  read from the map), a number steps by the menu's step and stops at 0. The friendly option keeps its no-mid-hunt
  rule. When each option takes effect is the game's: some live, some at the next contract; not traced per option.
- Ghost: a type-dials read-out (Onryo flames, Thaye step and threshold, Gallu and Dayan states, interaction boost,
  Banshee target), one more blown-out flame / clear them, "run the type's change now" (typeChangeDue: the Mimic's
  copy, the Thaye's ageing, the Obambo's switch at the next state change), and "make the hunt go for you"
  (GhostHuntState.SetTarget).
- Player: teleport to the truck (the scene's ExitLevel).
- Game: tick-speed presets 0, 0.25, 0.5, 1, 2, 4; ESP for doors (open/shut, locked), hiding places, light switches
  and the fuse box (the local model's overlay work: scene searches cached for 2 s; the light uses
  LightSwitch.get_IsSwitchOn, the wrapper the map exposes); HUD lines for the weather, the type dials and the dummy.
- validate 95/95.

Not done, and why: the Loader page (every mod's state) needs a new host-contract function at the end (contract 2.5),
a versioning step left for the user's go. The objective board's complete/fail path, infinite durability and battery,
spawning items or cursed possessions, the sight-assist pin and a per-option map of when each difficulty option is
read all still need reverse engineering.

Smoke 43: Difficulty picker: step ghost speed up during a hunt and watch the HUD's live speed; step starting sanity and
the evidence count (some apply only at the next contract). 44: ESP doors/hiding/switches label the level; toggling a
light flips its label. 45: game speed cycles through 0.25 and 4. 46: type dials change with the rows; "run the
type's change now" on a Mimic copies another type at its next state change.

## Round 11 (2026-11-12): host contract 2.5 and the Loader page

Committed round 10 as 3238830. The user said go for both the contract change and the remaining RE.

- Contract 2.5 (PMMOD_API_MINOR 5): `bool mod_entry(int32_t index, PmModEntry* out)` at the end of PmHostApi, SMAPI's
  IModRegistry.GetAll as the precedent. One entry per call because an array of a growing struct has no stride the host
  could trust; PmModEntry begins with `size`, set by the mod, and the host fills only what fits (id, name, version,
  author, PmModEntryFlags STARTED / DEVELOPER_ONLY / RECORDS_VOICE, offReason). The flags became an enum named
  PM_MOD_ENTRY_* after PM_MOD_RECORDS_VOICE turned out to exist already in PmModFlags. host/mods.cpp entry() keeps each
  index's off reason alive for the pointer; host/suite.cpp wires it; sdk/fake answers an empty list.
  pm::mod::mod_entries() (core/mod_glue.h) copies the list out and returns nothing from a host older than 2.5.
- tests/mods_test.cpp: the fake mod's entry off with the Mods panel's reason, then started; null, -1 and a size too
  small are refused; the pinned version is 2.5 and older minors still load.
- sdk/CHANGELOG.md gained [2.5] and the missing [2.4]; sdk/README's contract section is "version 2.5" and now lists
  2.3's content_flags_of and mod_name, 2.4's unity_action_new and 2.5's mod_entry (it had stopped at show_message);
  the guides' anchors follow.
- Dev menu: a seventh section, Loader: a mod picker (name, id, version: on or off with the reason, [n/N]) and the
  contract's offered ghost types with how many are custom.
- validate 95/95, docs clean.

Round 11, continued: the RE batch (local model research in re/dump/notes/2026-11-12-dev-menu-batch2.md; its map
entries by the local model, reviewed; gamecheck 1966 entries, 1854 resolved, 0 missing, 0 ambiguous).

- Objectives (Contract): a picker over the contract's objectives (LevelValues.mainObjective, then
  LevelValues.sideObjectives), each shown by its kind's number (ObjectiveValues.type) and its state (Objective.state,
  the ObjectiveState enum: None, Complete, Failed; mapped by its values because a plain int sits beside it and `.type =
  "int"` would have taken the wrong field). Complete / fail call ObjectiveManager.CompleteObjective / FailObjective, the
  local methods the game's own objective RPCs tail-jump into (no sender check; refused by the manager itself until
  HasSetupObjectives). Challenge progress they touch is a save write, stopped by the session's save freeze.
- Item spawn (Equipment): a picker over LevelValues.equipmentLoadout (GameEquipmentLoadout.equipment, the C# `item`;
  StoreItem.itemName) and "spawn one at your feet" through ObjectPooler.SpawnFromEquipmentPool, the truck spawner's own
  local call (no RPC, no save); false when the pool has no copy left.
- Never run out (Equipment): candles burn back up to what each held when first seen (Candle.candleDuration), crucifixes
  reset to new (Crucifix.useCount, now Decompiled). Electronics' charge: the counter was not found (research notes);
  no row.
- Hunt sight assist (Ghost): game / held off (sightSpeedUp 1, the neutral multiplier) / held where it stood.
- Cursed possessions mid-contract: not built; they are pre-placed level objects enabled at Start by seven placement
  methods whose bodies were not read.
- validate 95/95. Still running: when each difficulty option is read (re/dump/notes/2026-11-12-difficulty-readers.md).

Smoke 47: complete a side objective: the truck's board ticks it; fail one. 48: spawn an item from the loadout at your
feet; spawn until the pool runs out (the refusal shows). 49: never run out: light a candle and watch it hold; use a
crucifix past its uses. 50: sight assist held off: a hunt that sees you never speeds up (HUD live speed).

## Round 12 (2026-11-12): the pause's broken picture, found

The user: "pause while menu is open still causes the graphics flashing and game to stop running (but no crash)".
mods.log: the last open was at timeScale 0.00 with no close after it. Player.log (no timestamps): 216 copies each of
"Infinity or NaN floating point numbers appear when calculating the transform matrix for a Collider" for every
collider of LocalPCPlayer(Clone) and PCPlayer(Clone): the local rig's position went to NaN.

Mechanism, from the decompile already read for noclip: PhysicsCharacterController.Move stores displacement /
Time.deltaTime; at timeScale 0 every frame of the pause stores 0/0 = NaN; when the clock moves again Unity runs the
controller's FixedUpdate before the next Update, so the step applies that NaN and the rig, and the camera on it, go to
NaN (flashing, then black; the investigator lost). It also explains FINDINGS 33's tick-speed reports: cycling past
the top preset wrapped to 0. The game never writes the time scale itself, so this is our own write.

Fix at the cause: the menu never writes 0. menu_rule.h kPausedScale = 0.0001 is "paused" for both the pause while
open and the tick-speed cycle's first preset. Test first: the_menu_never_stops_the_clock_dead (pause_open and two
laps of next_time_scale never give 0). FINDINGS 33 removed; sdk/systems/media.md "Never write a time scale of
exactly 0" records it for authors. validate 95/95.

Smoke 51: pause while open on, open and close the menu several times in a contract, walk after each close: no
flashing, no black, and Player.log has no "Infinity or NaN" line. Cycle the game speed through every preset twice.
If the picture still drifts bright after a long pause, that is the exposure adaptation media.md describes, not this.
