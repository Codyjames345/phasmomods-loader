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
3. Main menu (the game is offline there too): the menu opens, and every contract, ghost and player row shows
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
9. Contract rows: unlock the main door; Leave; Quit. Payout read-out matches the in-game result.
10. Game rows: tick speed 0/0.5/1/2 cycle (also while frozen, and closing keeps the new speed), pause toggle,
    session and single-player read-outs.
11. Finish a dev-session contract (Leave and a full end): no XP, no money, no stats, no quest progress, no
    apocalypse movement; a contract started before the menu ever opened still earns normally.
12. The menu never opens in any room, public or private.
