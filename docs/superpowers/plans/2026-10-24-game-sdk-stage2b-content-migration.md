# Game SDK Stage 2b: Content Functions and Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every game feature the four mods use becomes a documented SDK function or map entry. Custom ghosts and custom evidence get one public content path. At the end, no mod, and no host or `core/` file outside the game API's runtime, holds an address, an offset, a layout, an obfuscated name or a direct IL2CPP call.

**Architecture:** The work runs in this order:
- **First, the safety net.** A ratchet scan counts raw sites per file. An expectation check proves every new entry resolves to the exact value it replaces.
- **Then the content layer.** Host services own the ghost and evidence plumbing (types, the Mimic, journal, difficulty, evidence, the ghost player), reached through host contract 2.2 and wrapped in `sdk/api/content.h`.
- **Then the move.** The mods move onto the content layer and the game API, smallest first, then `core/` and `host/`.
- **Last, enforcement.** The scan switches from ratchet to enforcing, and the docs are completed.

**Tech Stack:** C++20 / MSVC / CMake / Ninja, MinHook 1.3.4, Cpp2IL (external), Python 3 with UnityPy 1.25.2 (offline check only), PowerShell, REA for reverse engineering.

**Spec:**
- `docs/superpowers/specs/2026-10-24-sdk-content-design.md` (approved 2026-10-24). It wins where the two differ.
- `docs/superpowers/specs/2026-10-23-game-sdk-design.md`.

Stage 2a is merged. Its ledger, with the minors it deferred to 2b, is `.superpowers/sdd/stage2a/progress.md`.

## Global Constraints

- Everything in the foundation and stage 2a plans' Global Constraints still holds:
  - the resolver never picks among candidates;
  - facts fail closed;
  - nothing crashes or hangs the game at startup;
  - nothing touches or describes the anti-cheat;
  - no loader files go in the game folder;
  - only the user launches the game.
- **Nothing derived is committed.** No machine paths, personal details or derived game data are committed. The values an entry replaces go only in the gitignored `re/dump/expect/` files.
- **Host contract 2.2.** This plan raises the host contract to **2.2**:
  - `PgHostApi` and every published struct only grow at the end;
  - a mod built for 2.0 or 2.1 keeps loading;
  - every content struct begins with `uint32_t size` so it can grow.
- **Documentation.** Every new public declaration is documented: what it does, its thread (main thread unless stated) and its unresolved or inactive behaviour.
- **No private path.** The bundled mods use only public content functions. No content function special-cases a bundled mod id. `core/services.h`'s `custom-ghosts.v1`, `spectral-flame.v1` and `ghost-tells.v1` tables are removed by the end of this plan. `playable-ghost.v1` stays only for what is not content.
- **Small behaviour differences are allowed** (user, 2026-10-24). Each known difference is recorded in the task report and the ledger. Offline test coverage never drops.
- **Saves and other players.** A custom ghost type or custom evidence value never reaches a save file or a player without the content.
- **Vanilla UI only.** Content appears in the game's own screens and widgets, laid out as the game's own.
- **Reverse engineering.** Use REA (and the existing dump and Ghidra tooling) against copies outside the game folder. Record the facts in the map entries' notes and in `sdk/systems/*.md`, never only in a report.

## Review Focus

1. **Two content mods, each with a ghost, where one player lacks one mod.** All custom content is withdrawn for that lobby. The custom difficulty pick reverts to Random. Nothing custom is sent. Pinned in Task 3's `content_withdrawn_when_a_peer_lacks_it`.
2. **A Mimic copies a custom ghost.** That ghost's rules start, and the Mimic's previous copy's rules stop, in the same frame. Pinned in Task 4's `mimic_copy_switches_rules`.
3. **The game's language is not English.** A ghost with only English text shows English. A ghost with that language shows it, on its page and in its list entry. Pinned in Task 5's `missing_language_falls_back_to_english`.
4. **A replaced value moves in a future build.** The entry resolves to the new value and the expectation file is not consulted at runtime. A changed value during migration fails `gamecheck --expect`, naming the key. Pinned in Task 1's `expect_mismatch_names_the_key`.
5. **A content mod is switched off mid-lobby.** Its ghosts leave the journal, collection and difficulty option. Picks of them revert to Random. Other mods' ghosts keep their journal pages and the page sync still agrees. Pinned in Task 5's `withdrawing_one_mod_keeps_the_others_pages`.

---

### Task 1: The safety net: ratchet scan and value expectations

**Files:**
- Create: `tests/raw_scan_test.cpp` and `tests/raw_scan_baseline.txt`.
- Modify: `CMakeLists.txt`, `sdk/offline/gamecheck.cpp`, `tools/gamecheck.ps1`, `.gitignore` (`/re/dump/` already covers `re/dump/expect/`), and `sdk/README.md` (a "Moving code onto the SDK" section).
- Test: `tests/raw_scan_test.cpp`, `tests/gamecheck_test.cmake`.

**Interfaces:**
- **What the scan finds.** `raw_scan_test` reads every `.cpp`, `.h` and `.asm` under `mods/`, `core/` and `host/`. It counts raw sites:
  - a hex literal of 0x100 or more;
  - a non-ASCII identifier or string (obfuscated names);
  - a call into `il2cpp::` or a `p_il2cpp_` pointer;
  - `il2cpp::field<` and `static_get`;
  - `MH_CreateHook` outside `host/hooks.cpp`;
  - a `FindObjectOfType`, `Find(` or `NameToLayer` string literal.
- **Exemptions.** A line ending in `// pg:not-game-data: <why>` is exempt. Use it for colours, protocol bytes and masks. Exempt files: `core/il2cpp.*` and `core/mod_api.h` until Task 12, and the host's runtime files `host/game_map.*`, `host/hooks.cpp`, `host/code_memory.h`, `host/scene_engine.cpp` and `host/suite.cpp`'s service bodies.
- **The ratchet.** `tests/raw_scan_baseline.txt` holds a line `<path> <count>` per file. The test fails when a file's count rises above its baseline, or when a file appears that the baseline lacks. Lowering a count is done by editing the baseline in the same commit. A `--print` mode writes the current counts.
- **Expectations.** `gamecheck --expect <file>` reads lines `<key> <hex>`:
  - a method's value is its RVA;
  - a field's value is its offset;
  - an enum value's value is the value;
  - a native site's value is its RVA.

  Each listed key must resolve to that value. A mismatch prints `EXPECT <key>: map 0x.. != expected 0x..` and exits 1. An unknown key exits 2. `tools/gamecheck.ps1 -Expect` passes every `re/dump/expect/*.txt`.

- [ ] **Step 1: Write the failing tests:**
  - `scan_counts_each_kind`: on fixture files under `tests/fixtures/raw_scan/`, one of each kind, plus an exempt line.
  - `scan_fails_when_a_count_rises`.
  - `scan_fails_on_a_new_file`.
  - `expect_match_passes`: a fixture dump with `GhostAI.Awake` and its fixture RVA.
  - `expect_mismatch_names_the_key`: exit 1, and the output contains `EXPECT GhostAI.Awake`.
  - `expect_unknown_key_exits_2`.
- [ ] **Step 2: Run** `ctest --test-dir build -R "raw_scan|gamecheck" --output-on-failure`. Expected: they fail.
- [ ] **Step 3: Implement** the scan and `--expect`. Generate the baseline from the current tree with `--print`, and commit it. About 1,140 sites are expected (the 2a estimate). Record the total in the report.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 2: Map the ghost-type system and settle the Mimic by reverse engineering

**Files:**
- Create: `sdk/map/GhostInfo.h`, `sdk/map/GhostController.h`, `sdk/map/GhostCollection.h` (with `GhostData`), `sdk/map/GhostTypes.h` (the `GhostType` and `EvidenceType` enums), and `sdk/systems/ghost-types.md`.
- Modify: `sdk/map/all.h` and the map tests.

**Interfaces:**
- **Entries** (`Field`, `Method`, `Type`, `EnumValue` as the 2a kinds allow) that the content layer needs. Each is proved with `gamecheck --expect` against the value `mods/custom_ghosts`, `mods/playable_ghost` or `mods/ghost_tells` uses today. The keys:
  - `GhostInfo.identityType` (+0x28 today) and `GhostInfo.behaviourType` (+0x2C);
  - `GhostInfo.SyncValues` and `GhostInfo.SyncValuesNetworked`;
  - `GhostAI.Init`;
  - `GhostController.ghostType` and `GhostController.mimicCopies` (+0xC8, `List<GhostType>`);
  - the Mimic's copy method;
  - `GhostCollection.ghosts`;
  - `GhostData.type`, its evidence list and its name key;
  - `GhostType.None` (24), `GhostType.Mimic` (20) and every `EvidenceType` value.
- **The Mimic answers.** `sdk/systems/ghost-types.md` answers the content design's "Left to the builder" questions on the Mimic:
  - which field the game switches when the Mimic copies;
  - where its copy list comes from, and whether it is rebuilt;
  - how the Mimic's forced orbs are given;
  - which setup lets a custom ghost run the game's generic AI while the SDK dispatches its rules: identity = the custom type, behaviour = `None`, with an acting-as type kept by the host.

  If reverse engineering shows that setup cannot let the Mimic copy a custom type, the report says so with the evidence, and the controller stops for a ruling before Task 4.

- [ ] **Step 1: Reverse engineer** with REA on the copies. Write `sdk/systems/ghost-types.md`.
- [ ] **Step 2: Write the entries,** and an `re/dump/expect/ghost-types.txt` with the values the mods use today.
- [ ] **Step 3: Run** `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: every new entry resolves, and every expectation matches.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 3: The content registry, numbering and lobby rule (contract 2.2)

**Files:**
- Modify: `core/mod_api.h` (2.2) and `host/suite.cpp`.
- Create: `host/content.h`, `host/content.cpp`, `sdk/api/content.h` and `sdk/api/content.cpp`.
- Modify: `sdk/fake/fake_game.*`.
- Test: `tests/content_test.cpp` (new), `tests/mods_test.cpp`.

**Interfaces:**
- **Text** (C, in `mod_api.h`). The language codes are the game's own:

  ```c
  typedef struct PgText { const char* lang; const char* text; } PgText;
  typedef struct PgTextSet { const PgText* items; uint32_t count; } PgTextSet;  // "en" required
  ```

- **The ghost definition:**

  ```c
  typedef struct PgGhostDef {
    uint32_t size;
    const char* id;                 // "<mod id>.<name>", stable across versions
    PgTextSet name, description, strength, weakness;
    const PgTextSet* tells; uint32_t tellCount;
    const char* evidence[3];        // "emf" "spirit_box" "ultraviolet" "orbs" "writing" "freezing" "dots" or a custom evidence id
    PgTextSet abilityName, abilityDescription;          // count 0: no ability
    void* user;
    bool (*useAbility)(void* user);                       // host only; false if it could not
    int32_t (*hudState)(void* user);                       // -1 none
    bool (*heldBack)(int32_t kind, void* user);            // 0 interact, 1 event or ability, 2 hunt
    uint32_t (*playerView)(float* xyz, uint32_t max, void* user);  // points for the ghost player's view; null none
    const char* (*evidenceLook)(const char* evidenceId, void* user); // e.g. "green"; null: the evidence's own look
  } PgGhostDef;
  ```

- **The evidence definition:**

  ```c
  typedef struct PgEvidenceDef {
    uint32_t size; const char* id; PgTextSet name; PgTextSet journalButton;
  } PgEvidenceDef;
  ```

- **New `PgHostApi` members, appended in this order:**
  - `int32_t (*ghost_register)(const PgGhostDef*)`. Returns a handle greater than 0, or 0 if refused (a duplicate id, a missing "en", or an unknown evidence id); the log names why. Called during `pgmod_start`.
  - `int32_t (*evidence_register)(const PgEvidenceDef*)`.
  - `int32_t (*content_type)(const char* ghostId)`. Returns the ghost's game type number while content is active, else -1.
  - `const char* (*content_ghost_id)(int32_t type)`. Vanilla types answer `"vanilla.<enum name in lower case>"`, for example `"vanilla.mare"`.
  - `int32_t (*content_evidence)(const char* evidenceId)`. Returns the evidence's game value, or -1.
  - `bool (*content_active)(void)`. True when a contract or lobby has custom content in play.
  - `bool (*content_player_controlled)(void)`. True when a player controls the contract's ghost (read from `playable-ghost.v1`).
- **Numbering.** Ghost types are numbered from 31, and custom evidence from 8. Each is numbered in byte order of its id, over the content every peer in the room has.
- **The lobby rule.** A peer's content manifest is the sorted list of ids with a hash of each definition's text. It is sent on the host's own channel as each peer joins. Content is active only when every peer's manifest equals ours.
- **The C++ wrapper,** in `sdk/api/content.h`, namespace `pg::api::content`:
  - `GhostDef` and `EvidenceDef` builders that hold strings and produce the C structs;
  - `register_ghost`, `register_evidence`, `type_of`, `id_of`, `evidence_of`, `active` and `player_controlled`.

- [ ] **Step 1: Write the failing tests** (fake backend plus a fake peer channel):
  - `ghost_ids_number_in_byte_order`: ids `b.x`, `a.y` and `a.z` number 31, 32 and 33 as `a.y`, `a.z` and `b.x`.
  - `evidence_numbers_from_8`.
  - `duplicate_id_is_refused`.
  - `missing_english_is_refused`.
  - `unknown_evidence_is_refused`.
  - `content_withdrawn_when_a_peer_lacks_it`: two mods, a peer with one, `active() == false`, and `type_of` returns -1.
  - `manifests_equal_activates`.
  - `vanilla_ids_answer`: `id_of(6) == "vanilla.mare"`.
  - `mod_built_for_2_1_still_loads`, in `mods_test`.
- [ ] **Step 2: Run** `ctest --test-dir build -R "content|mods" --output-on-failure`. Expected: they fail to compile.
- [ ] **Step 3: Implement** the registry, numbering, manifest exchange and contract members. Registration only records definitions: nothing touches the game until Task 4.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 4: Ghost types in the game: collection, roll, names, identity and the acting-as dispatch

**Files:**
- Modify: `host/content.*` (game side, in `host/content_types.cpp`), `core/mod_api.h` and `sdk/api/content.*`.
- Test: `tests/content_test.cpp`.

**Interfaces:**
- **Rules.** New contract members, appended:

  ```c
  typedef struct PgGhostRules {
    uint32_t size; void* user;
    void (*start)(void* user); void (*tick)(void* user); void (*stop)(void* user);
    bool (*huntSpeed)(float* base, float* losScale, void* user);  // per hunt frame; false: the game's own
    bool (*heldBack)(int32_t kind, void* user);                   // for vanilla types (Ghost Tells)
  } PgGhostRules;
  ```

  - `int32_t (*ghost_rules)(const char* ghostId, const PgGhostRules*)`. Works for custom and `vanilla.*` ids. Several mods may attach rules to one id.
  - `int32_t (*content_acting_as)(void)`. Returns the contract ghost's acting-as type (for a Mimic, its copy), or -1.
  - `int32_t (*content_identity)(void)`.
- **What the host does while content is active.** Following `sdk/systems/ghost-types.md`, it:
  - adds each custom `GhostData` to the contract's collection;
  - answers `Journal_<type>Title` keys with the ghost's name;
  - sets identity and behaviour on every client, as `docs/custom-ghosts.md` describes;
  - adds the custom types to the Mimic's copy list;
  - runs `start`, `tick` and `stop` for the acting-as type;
  - applies `huntSpeed` by writing the hunt's base speed and its LOS hold (`docs/custom-ghosts.md` "Speeds work as the game's own do"), never the speed itself.

- [ ] **Step 1: Write the failing tests:**
  - `custom_type_joins_collection_in_alphabetical_place`.
  - `name_key_answers_custom_name`.
  - `identity_is_custom_behaviour_is_none`.
  - `mimic_copy_list_includes_custom_types`.
  - `mimic_copy_switches_rules`: the stop of the previous copy and the start of the new one happen in the same tick.
  - `vanilla_rules_follow_behaviour_type`: rules on `vanilla.mare` run for a Mimic copying a Mare.
  - `hunt_speed_writes_inputs_not_result`.
  - `withdrawal_removes_entries_and_stops_rules`.
- [ ] **Step 2: Run** the focused tests. Expected: they fail.
- [ ] **Step 3: Implement** it, on map entries only (Task 2's, plus any new ones, each with an `--expect` line).
- [ ] **Step 4: Run** `.\validate.ps1`, then `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 5: The journal: guess buttons, the Ghosts list and pages, and lines for vanilla ghosts

**Files:**
- Create: `host/content_journal.cpp`, and `sdk/map/Journal.h` (`JournalController`, the evidence spread's ghost page, the Ghosts tab's list and page objects, and the localiser), plus Scene entries for the paths `mods/custom_ghosts/pages.cpp` uses.
- Modify: `core/mod_api.h`, `sdk/api/content.*`.
- Test: `tests/content_journal_test.cpp`, reusing the fixtures of `tests/pages_test.cpp`.

**Interfaces:**
- `int32_t (*journal_lines)(const char* vanillaGhostId, const PgTextSet* lines, uint32_t count)`. Appended. It adds lines to a vanilla ghost's page, and several mods may add lines to one ghost.
- **What the host builds,** on every client's journals, from the game's own widgets (`docs/custom-ghosts.md` "The journal" gives the layout facts: 250 x 100 cells, 3 columns, the list grid shipped switched off):
  - per custom ghost, a guess button on the evidence spread's ghost page, with the grid fitted to the total number of ghosts;
  - a Ghosts tab list entry in its alphabetical place, leading to the ghost's page number;
  - the page itself: name, description, strength, weakness, tells, and the ability when there is one;
  - per custom evidence (Task 7), its button.

  The text follows the game's language switch, falling back to "en". Page numbers are identical on every client.

- [ ] **Step 1: Write the failing tests:**
  - `guess_button_crossed_out_by_filter`.
  - `list_entry_alphabetical_and_leads_to_page`.
  - `page_numbers_match_across_clients`: two fake clients with the same content.
  - `missing_language_falls_back_to_english`.
  - `language_switch_rewrites_pages`.
  - `vanilla_lines_from_two_mods_both_show`.
  - `withdrawing_one_mod_keeps_the_others_pages`.
- [ ] **Step 2: Run** them. Expected: they fail.
- [ ] **Step 3: Implement** it.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 6: Custom difficulty: the Ghost Type pick, and saves and sync

**Files:**
- Create: `host/content_difficulty.cpp`, and `sdk/map/Difficulty.h` (`DifficultyUIManager`, `Difficulty.ghostTypeOverride`, the option, `ES3` Save/Serialize/Load for `Difficulty`, `SyncDifficultyValues`, the summary, `FindObjectOfType` with includeInactive). The facts are in `mods/custom_ghosts/difficulty.cpp` and the `lobby-options-re-facts` notes.
- Test: `tests/content_difficulty_test.cpp`, porting `tests/difficulty_test.cpp`'s cases.

**Interfaces:** none new for mods. While content is active, the custom ghosts are offered in the Ghost Type option of Custom 1 to 3. Per preset, the picks are kept in `%LOCALAPPDATA%\PhasmoMods\custom_difficulty.txt`, which is keyed by ghost id, not by number. Saves and syncs get Random whenever anyone lacks the content. The summary names the custom ghost.

- [ ] **Step 1: Write the failing tests:**
  - Every case of `difficulty_test.cpp`, rewritten on the content layer.
  - `picks_keyed_by_id_survive_renumbering`: a new mod's ghost shifts the numbers, and the pick stays on the same ghost.
  - `save_never_holds_custom_type`.
- [ ] **Step 2: Run** them. Expected: they fail.
- [ ] **Step 3: Implement** it.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 7: Custom evidence in the game

**Files:**
- Create: `host/content_evidence.cpp`, and the entries for evidence selection (`GhostInfo.SyncValues`' evidence choice) and the journal's evidence filter. The facts are in `mods/spectral_flame` and the `flame-ghosts-re-facts` notes.
- Modify: `core/mod_api.h` and `sdk/api/content.*`, appending `const char* (*content_evidence_look)(const char* evidenceId)`. It answers the contract ghost's `evidenceLook`, or null.
- Test: `tests/content_test.cpp`.

- [ ] **Step 1: Write the failing tests:**
  - `custom_evidence_given_when_listed`.
  - `evidence_button_filters_ghosts`.
  - `evidence_look_answers_ghost_choice`: the Hydra case, "green".
  - `withheld_evidence_not_given`: on difficulties that hide evidence, as the game hides its own.
- [ ] **Step 2: Run** them. Expected: they fail.
- [ ] **Step 3: Implement** it.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 8: Move Spectral Flame onto the SDK

**Files:** `mods/spectral_flame/*`, the new `sdk/map` headers it needs (candles, lantern, candelabras, lights, sprites, Firelight), and `tests/spectral_flame_test.cpp`.

**How every migration task works** (Tasks 8 to 12 follow these steps):
1. List the file group's raw sites from the baseline.
2. Write an entry for each, with a behaviour note from the mod's comments and reverse engineering.
3. Add its old value to `re/dump/expect/<area>.txt`.
4. Run `gamecheck -Expect` until it matches.
5. Switch the code to the game API and the content layer.
6. Delete the constants, and set the files' baseline to 0.
7. Port the tests from production-offset fakes to the fake backend, keeping every case.

Record behaviour differences in the report.

- [ ] **Step 1: Do it for Spectral Flame:**
  - It registers `spectral_flame.flame` through `register_evidence`. The `spectral-flame.v1` service is removed.
  - The Hydra's colour comes from `content_evidence_look`.
- [ ] **Step 2: Run** `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass, with Spectral Flame's files at 0 in the baseline.
- [ ] **Step 3: Commit.**

### Task 9: Move Ghost Tells onto the SDK

**Files:** `mods/ghost_tells/*`, its map entries, and `tests/ghost_tells_test.cpp`.

- [ ] **Step 1:**
  - Move the six vanilla ghosts' tells to `ghost_rules("vanilla.<name>", ...)`.
  - Move the journal text to `journal_lines`.
  - Move the held-back checks to `PgGhostRules.heldBack`.
  - Remove `ghost-tells.v1`.
  - Port the tests.
- [ ] **Step 2: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass, with Ghost Tells' files at 0.
- [ ] **Step 3: Commit.**

### Task 10: Move Custom Ghosts onto the content layer

**Files:** `mods/custom_ghosts/*`, and `tests/custom_ghosts_test.cpp`, `registry_test.cpp`, `pages_test.cpp` and `difficulty_test.cpp` (their cases now live in Tasks 3 to 7's tests and are deleted here when covered).

- [ ] **Step 1:**
  - Delete `registry.*`, `pages.*` and `difficulty.*`.
  - Build the 15 ghosts from `ghosts.h` with `GhostDef`: ids `custom_ghosts.<name lower case, spaces as _>`, for example `custom_ghosts.black_shuck`. The text is English.
  - Register them in `pgmod_start`.
  - Attach each ghost's rules with `ghost_rules`.
  - Move the speed rules to `huntSpeed`, and the abilities, HUD states, held-back checks and the scent view to the ghost definition.
  - Remove `custom-ghosts.v1`.
  - Replace the type numbers 31 to 45 with `type_of(id)` wherever a number is needed.
- [ ] **Step 2: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass. The registration files are gone.
- [ ] **Step 3: Commit.**

### Task 11: Move the custom ghosts' behaviour rules onto the game API

**Files:**
- `mods/custom_ghosts/` behaviour files: `behaviour.cpp`, `apparition.cpp`, `asterion.cpp`, `bogeyman.cpp`, `brownie.cpp`, `echo.cpp`, `gremlin.cpp`, `hydra.cpp`, `kikimora.cpp`, `lethe.cpp`, `lithobolia.cpp`, `mesmer.cpp`, `restraint.cpp`, `sound.cpp` and `tempest.cpp`.
- Their map entries.

- [ ] **Step 1: Run the migration steps for these files.** The controller may split this into two dispatches by file if the first report shows more than about 150 sites remaining.
- [ ] **Step 2: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass, with Custom Ghosts' files at 0.
- [ ] **Step 3: Commit.**

### Task 12: Move Playable Ghost, `core/` and `host/` onto the SDK

**Files:**
- `mods/playable_ghost/*`, including `steer.cpp`, `steer_stub.asm` and `mimic.cpp`.
- `core/game.*`, `core/players.h`, `core/ui.*`, `core/notice.cpp`, `core/bindings.*` and `host/*` outside the runtime files.
- `sdk/api/game_api.h`: a `NativeSite<D>::patch(expected, stub, &page)` wrapper over `patch_site`.
- The tests of those areas.

- [ ] **Step 1: Run the migration steps, by file group.** The controller dispatches each group as its own round, in this order:
  - **(a) Playable Ghost's control, session and HUD.** It drives abilities, HUD state, held-back checks and the player view for any registered ghost through the content layer, not by type number.
  - **(b) Playable Ghost's movement and steering.** Steering moves to `UnityPlayer.SteerSite` through `NativeSite::patch`. The stub ends with a jump to `page + stubSize`, and its data sits behind that jump.
  - **(c) The rest of Playable Ghost.** The Mimic pick uses Task 2's entries.
  - **(d) `core/`.** `core/il2cpp.*` becomes private to the host, and `core/mod_api.h` keeps only the C contract.
  - **(e) `host/`.** Also take the 2a ledger items:
    - an index for the services' key search;
    - the deferred twins list, reported, not widened.
- [ ] **Step 2: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass, with every file's baseline at 0.
- [ ] **Step 3: Commit,** once per group.

### Task 13: Enforcement, documentation and the in-game smoke list

**Files:**
- `tests/raw_scan_test.cpp`, with the baseline deleted.
- `sdk/README.md`.
- `sdk/systems/ghost-types.md`, `sdk/systems/journal.md`, `sdk/systems/difficulty.md` and `sdk/systems/equipment.md`.
- The guides in `sdk/guides/`:
  - `adding-a-ghost.md`;
  - `adding-evidence.md`;
  - `tells-for-vanilla-ghosts.md`;
  - `mimic-and-your-ghost.md`;
  - `lobbies-saves-and-players.md`.
- `docs/custom-ghosts.md`, rewritten to point at the content layer.
- `docs/superpowers/plans/2026-10-24-stage2b-smoke-list.md`.

- [ ] **Step 1: Make the scan enforcing.** It fails on any raw site outside the runtime files, with no baseline. Then watch it fail once, on each of these deliberately added in a scratch commit that is then dropped:
  - a raw hex RVA in a mod;
  - a field offset in `core/`;
  - a Malayalam name in `host/`.

  Record the three failure outputs in the report.
- [ ] **Step 2: Write the guides.**
  - `adding-a-ghost.md` carries a complete small example ghost: one tell, three vanilla evidence and an ability. It is built as `sdk/examples/example_ghost/` by CMake, and loaded in a `content_test` case.
  - `equipment.md` describes the game's equipment system: truck slots, shop listing, price, description, loadout save, use and sync, and how equipment reveals evidence. It ends with the dowsing-rods worked example and what a registration function would need. No such function is added.
- [ ] **Step 3: Write the smoke list** the user runs in game. It covers:
  - every task's live checks;
  - 2a's untested parts: `scene_engine`, `patch_site` and the shutdown restore, the `static_access` throwing class constructor, and wrong-kind service calls;
  - the Mimic copying a custom ghost;
  - a non-English journal.
- [ ] **Step 4: Run** `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass, and the scan reports 0 raw sites.
- [ ] **Step 5: Commit.**

## The plans that follow

- **2c, safety switch-over:** what the stage 2a plan listed, less the source scan, which Task 13 enforces here.
- **Stage 3, the remaining main systems:** weather, players, the level and rooms, the lobby and menus in full, and the equipment classes. Each is mapped and documented until the coverage report has nothing left for them.
- **2d, outside authors**, as the stage 2a plan listed. The SDK zip ships only after stage 3 (user, 2026-10-24).
