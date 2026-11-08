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
- **Dynamic over hard-coded** (user, 2026-10-24). Any count, range, list, order, size or name the game holds is read from the game through the map, never hard-coded; content fails closed only when it cannot be read.
- **Vanilla UI only.** Content appears in the game's own screens and widgets, laid out as the game's own.
- **Reverse engineering.** Use REA (and the existing dump and Ghidra tooling) against copies outside the game folder. Record the facts in the map entries' notes and in `sdk/systems/*.md`, never only in a report. A game member a task decompiled and relied on becomes a map entry when a mod author could reasonably use it, even when the mod reaches it by running the game's original, so an author can use it without repeating the work. The SDK holds every reasonable function, not literally everything (user, 2026-11-01): internals with no use to an author stay out. Reports are gitignored, so a fact that lives only there is lost to the SDK; each task's review checks this (user, 2026-11-01).

## Review Focus

1. **Two content mods, each with a ghost, where one player lacks one mod.** Under a host with both, that player changes nothing for anyone and is removed; under a host with one, this client's custom content is off. Nothing custom is sent to a player who isn't verified. Pinned in `content_follows_the_host_and_no_other_player`, rewritten by Task 13d from Task 3's `content_withdrawn_when_a_peer_lacks_it`.
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

### Task 9b: Show content in the lobby's journal

The user's rule (2026-10-28): every journal change from mods also shows in the lobby's journal, so players can read a custom ghost's page after a contract ends. Before this task, the content layer adds custom ghosts' entries only for a contract that holds them and removes them at its end (`host/content_types.h`), so the lobby journal shows none of them.

**Files:** `host/content_types.*`, `host/content_journal.*`, `host/content_evidence.*`, `sdk/systems/journal.md`, `sdk/README.md`, and `tests/content_journal_test.cpp` and `content_test.cpp`.

- [ ] **Step 1: Reverse engineer the lobby's journal.** Do this before writing any code; no behaviour without decompiled proof. Record in `sdk/systems/journal.md`, with RVAs:
  - which JournalController the lobby has, and when its Awake runs;
  - which GhostCollection (or other list) its ghost list, ghost pages and evidence-page buttons are built from;
  - whether its evidence rows come from the same source as in a contract;
  - whether lobby journals sync pages or guesses between players.
- [ ] **Step 2: Write the failing tests**, on the fake backend:
  - `lobby_journal_gets_custom_ghosts_while_content_active`: the list entry, page and evidence-page button, with the same page numbers on every client with the same content.
  - `lobby_journal_gets_custom_evidence_rows`.
  - `lobby_journal_shows_journal_lines_on_vanilla_pages`.
  - `lobby_journal_keeps_pages_after_contract_ends`: a contract with custom ghosts ends, the lobby loads, and the pages are there while content is still active.
  - `lobby_journal_withdraws_when_content_turns_off`: the room's mods stop agreeing in the lobby, or the mods stop, and the game's own journal is exactly as it was.
- [ ] **Step 3: Implement.** While content is active in the lobby (the room's mod agreement, which follows the room there), the lobby's journal gets everything a contract's journal gets, through the same code paths. Nothing is added per frame beyond what the contract path already does. Update the header comments that say "the lobby holds none of them", and `sdk/README.md`.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.** Add "lobby journal: custom ghost pages, evidence rows and lines, before and after a contract" to Task 14's smoke list.

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

### Task 11c: The Draugr's rework

Asked for by the user on 2026-10-30, to set the Draugr apart from the vanilla Goryo. Its "things" are the house's own props: whatever can be carried and lies in its room when its rules start (`survey_room`). Equipment is never its own, because players bring equipment into the contract. Everything goes through the game API. Every game mechanic used is proven from decompiled code in `sdk/systems/ghost-ai.md`, and nothing new is invented.

**Files:** `mods/custom_ghosts/behaviour.*`, `draugr_rule.h` (new, pure), `ghosts.h` (journal text), the `sdk/map` headers it needs, `sdk/systems/ghost-ai.md`, `tests/custom_ghosts_test.cpp`.

- **Keeping its room the Goryo way** (user's choice). The destination picker (0x21AA880) skips the long-roam roll for a Draugr, exactly as it does when `GhostInfo+0x2C == 14` (the Goryo). Copy only that skip, not the Goryo's other wander differences. A Monkey Paw wish can still move its room, as for the Goryo. Remove the `ChangeFavouriteRoomTo` hook.
- **It takes its things back** (outside hunts, while angered). It moves one of its props that has left the room back to the exact place it lay when surveyed. It uses the same move the Kikimora uses (`kikimora.cpp` `move_pending`: transform, then Rigidbody position), shared rather than copied. It acts at the Kikimora's own hide cadence, never on a prop a player is holding, and one prop at a time.
- **It goes for the thief.** For each prop, record the player who carried it out of the room: the player holding it when it left. Find how the game tracks the holder in decompiled code. When a hunt starts while the Draugr is angered, its first target is the most recent thief still in the house, if `may_go_for` allows. It uses the hunt's target setter mapped in 11a2. Otherwise the game's own pick stands.
- **Its room gets colder while it is angered.** It takes 3 °C extra off its room's temperature, through the game's own room-temperature system (find it in decompiled code). That system must still drive the readings: thermometer, breath. If the contract withholds Freezing, the extra drop never takes a reading below the game's freezing threshold, read from the game, so it never gives false evidence. When anger ends, the room returns to the game's own temperature.
- **Changes the user asked for after the first build** (2026-10-30):
  - **The cold is a tell, not evidence.** Like the Mimic's ghost orbs, which show even on zero evidence, the angered room's extra 3 °C may take readings below freezing even when the contract withholds Freezing. Remove the floor guard. On zero evidence, freezing temperatures point to a Draugr. Replace the tests `draugr_cold_never_fakes_freezing_when_withheld` with `draugr_cold_goes_below_freezing_when_withheld`.
  - **Freezing has evidence priority, as the Goryo's D.O.T.S. does.** When the difficulty withholds one or two evidence, a Draugr always keeps Freezing. Use the game's own rule (sdk/systems/evidence.md step 4): for the Hantu, Goryo, Obake, Moroi and Deogen the removal's `Random.Range(0, count)` ends one short, so the last of the three is never removed. With zero evidence all three still go.
    - The content layer gets this as a general feature. `PgGhostDef` grows at the end (contract 2.2, unreleased) with `uint32_t flags`, and `PG_GHOST_KEEPS_LAST_EVIDENCE = 1` means its third evidence follows that rule.
    - The host applies it to such a custom ghost by the narrowest proven way that makes the game's roll take that same "one short" range: find how in the decompiled roll (body 0x22AFCB0). Never re-implement the roll.
    - The Draugr's evidence becomes EMF, Spectral Flame, Freezing, with Freezing last, and it sets the flag.
    - Tests: `custom_ghost_keeps_last_evidence_when_one_withheld`, `custom_ghost_keeps_last_evidence_when_two_withheld`, `custom_ghost_keeps_none_on_zero_evidence`, `draugr_declares_freezing_priority`.
  - **A player-controlled Draugr, like the Banshee.**
    - `PgGhostDef` gains `void* (*playerTarget)(void* user)`, at the end and after `flags`: the one player the ghost may kill now, or null for no restriction. The Draugr answers with its thief while angered.
    - Task 12(a) consumes it the way Playable Ghost handles the Banshee's focus: the HUD shows that target as it shows a Banshee's (`session::kFocusSanity`, "Your target's sanity"), and the player cannot kill anyone else.
    - First reverse engineer how the game's Banshee restricts its hunt's kills and target, and how Playable Ghost applies it today. If the vanilla Banshee can in fact kill other players, stop and report before building the restriction.
    - Tests in 11c: `draugr_player_target_is_thief_while_angered`, `draugr_player_target_null_when_calm`. The kill restriction and the HUD are tested in 12(a).
    - **The Bogeyman shares it** (user, 2026-10-30). Its `playerTarget` answers the youngest living player, using the rule its AI already uses (`bogeyman_rule.h` `youngest`: lowest prestige, then lowest level). When that player dies, it moves to the next youngest. So a player-controlled Bogeyman sees the youngest player as its target and can kill no one else. Its AI behaviour is unchanged. Tests: `bogeyman_player_target_is_youngest` and `bogeyman_player_target_moves_on_when_youngest_dies`.
- **Journal text** (`ghosts.h`), a draft shown to the user on 2026-10-30, which their reply may change:
  - **Description:** "The restless dead of the Norse barrows, jealous guardians of their resting place and all that lies in it. It keeps to its room, and whatever is taken from there, it wants back."
  - **Strength:** "Carry anything out of a Draugr's room and it may hunt while your sanity is still high, and it comes for the thief first."
  - **Weakness:** "A Draugr takes back what was taken, and its room turns colder until it does. In a hunt it slows to a crawl the further it strays from its room."
- **Carried 11b re-review minors:**
  - in `ghost-ai.md`: the room RPC handler SetGhostRoom (0x22AF6E0) writes GhostInfo+0x70 and +0x60, and ChangeFavouriteRoomTo stores only the index; the exit's chance is 34, 67 or 100 in 100, with 33 for any other setting; the fourth Goryo test, in 0x21BABF0;
  - the sensor-copy test checks `share_filters`' result, and the fake models the all-or-none refusal.

- [ ] **Step 1: Reverse engineer** the long-roam roll's place in 0x21AA880, the prop holder, and the room temperature and its freezing threshold, each recorded with RVAs.
- [ ] **Step 2: Write the failing tests:**
  - `draugr_skips_long_roam_like_goryo`
  - `draugr_room_change_hook_gone`
  - `draugr_returns_prop_to_original_place`
  - `draugr_never_takes_a_held_prop`
  - `draugr_hunts_thief_first_when_allowed`
  - `draugr_falls_back_to_game_target`
  - `draugr_room_colder_while_angered`
  - `draugr_cold_never_fakes_freezing_when_withheld`
  - `draugr_cold_ends_with_anger`
- [ ] **Step 3: Implement**, with any game numbers as map entries.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.** Add to Task 14's smoke list: the Draugr stays near its room, takes props back to their spots, goes for the thief, and its room is colder while angered.

### Task 11d: The Brownie's footsteps

Asked for by the user on 2026-10-30. Runs after Task 12(a) and before 12(b).

Today every footstep of a hunting Brownie gets the game's Myling rule. A hook on `Footstep.Play` cuts the step's reach by 8 m (`brownie.cpp`, `brownie_rule.h`, 0x243E380). The new rule:
- **Unwatched in a hunt:** its footsteps are silent.
- **Watched in a hunt:** its footsteps are quiet, exactly the Myling's rule as today.
- **Outside hunts:** unchanged.

**Details:**
- **Watched** means the same "anyone looks at it" test that already sets its speed (`brownie::seen()`).
- **Every client plays footsteps** (an RPC spawns them), so every client must know the watched state. If it is decided only on the host today, the host sends it on the mod's own channel, accepted only from the master, as the Draugr's anger is. Changes are sent at once, and repeated while it hunts.
- **Silent through the game's own footstep system.** The step still happens: its timing, its animation, and anything else the game drives from a step stay as they are. Only what players hear is removed. Find from decompiled code what makes a step audible (the Noise's max distance, the AudioSource's rolloff and volume) and the narrowest proven way to make it inaudible. That might be the same `SetMaxDistance` call with a reach that leaves nothing audible, if the rolloff proves it. If a zero reach would still be heard up close, or if the Noise also drives something besides what players hear, stop and report with the evidence.
- **Journal text** (draft for the user):
  - **Strength:** "A Brownie is very fast in a hunt while nobody looks at it, and then its footsteps make no sound."
  - **Weakness:** "A Brownie slows to less than a walk while anyone looks at it, and then its footsteps can be heard, if only up close."
- **Playable Ghost parity.** A player-controlled Brownie gets the same rule, because the hook acts on every client and the watched test does not depend on who controls it. Check this and record it.

- [ ] **Step 1: Reverse engineer** step audibility. Record it in `sdk/systems/ghost-ai.md` with RVAs.
- [ ] **Step 2: Write the failing tests:**
  - `brownie_steps_silent_when_unwatched`
  - `brownie_steps_quiet_like_myling_when_watched`
  - `brownie_steps_unchanged_outside_hunts`
  - `brownie_watched_state_reaches_clients`
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.** Add to the smoke list: an unwatched hunting Brownie makes no step sound; a watched one is heard only up close.

### Task 12: Move Playable Ghost, `core/` and `host/` onto the SDK

**Files:**
- `mods/playable_ghost/*`, including `steer.cpp`, `steer_stub.asm` and `mimic.cpp`.
- `core/game.*`, `core/players.h`, `core/ui.*`, `core/notice.cpp`, `core/bindings.*` and `host/*` outside the runtime files.
- `sdk/api/game_api.h`: the effect "steer this ghost's agent this frame". The native patch behind it is the host's (spec, "Native UnityPlayer patches").
- The tests of those areas.

- [ ] **Step 1: Run the migration steps, by file group.** The controller dispatches each group as its own round, in this order:
  - **(a) Playable Ghost's control, session and HUD.** It drives abilities, HUD state, held-back checks and the player view for any registered ghost through the content layer, not by type number. It also drives `PgGhostDef.playerTarget` (Task 11c) as it handles the Banshee's focus: the HUD shows the target as a Banshee's ("Your target's sanity"). Kills follow the vanilla Banshee exactly (user, 2026-10-30; see `sdk/systems/ghost-ai.md`, "The Banshee's one target"): while the target is alive and in the house, the player can kill only the target; once the target is in the truck, dead, or killed this hunt, anyone. The target is carried to the controlling client, which may not be the host. Tests: `player_target_shown_like_banshee`, `player_kills_only_target_while_target_in_house`, `player_kills_anyone_once_target_out`.
  - **(b) Playable Ghost's movement and steering.** The stub, its register assumptions and the CrowdAgent and NavMeshAgent internals move out of the mod into the host, on `UnityPlayer.SteerSite`, and the mod asks the game API for the effect, as the spec requires (corrected 2026-11-01: this text first kept the stub in the mod behind a `NativeSite::patch` wrapper, so a game update could still have forced a mod rebuild). The stub ends with a jump to `page + stubSize`, and its data sits behind that jump.
  - **(c) The rest of Playable Ghost.** The Mimic pick uses Task 2's entries.
  - **(d) `core/`.** `core/il2cpp.*` becomes private to the host, and `core/mod_api.h` keeps only the C contract.
  - **(e) `host/`.** Also take the 2a ledger items:
    - an index for the services' key search;
    - the deferred twins list, reported, not widened.
  - **(f) The loader's menu controls use the game's own buttons** (user, 2026-10-29). Today every control the loader adds is a copy of a game button with its `UIButton`/`Button` switched off (`menu_panel::make_inert`). The loader then hit-tests the mouse and polls the left button itself (`menu_panel::press`, `row_clicked`), and plays only the hover states of the colour animator. That covers the main menu's Mods tape, the Mods panel's rows and buttons, and the consent popup's buttons. The game's pressed and selected states, the blue highlight, never play. This is invented click handling.
    - **Reverse engineer first,** recorded in a new `sdk/systems/menus.md` with RVAs:
      - how the game's own tapes are clicked: the Doozy `UIButton`, its click event and listeners, selection and the `EventSystem`;
      - where the News tape's own listeners come from (persistent or added at runtime), so a copy can be cleared of them;
      - how the colour animator's states 0-4 are driven by the button;
      - for the Multiplayer tape, the plain `UI.Button`.
    - **Implement:** each copied control keeps the game's button component enabled. Its own listeners are removed, and the loader's action is added as a listener on the game's click event. The game then drives hover, press, selection and the animator, and its own input handles clicks, keyboard and controller navigation included, wherever the game supports them. Remove the mouse polling and the manual animator calls. A managed delegate to a native callback goes through the runtime services, with a map entry for each game member used.
    - **Tests:**
      - `copied_tape_keeps_game_button_and_drops_its_listeners`;
      - `click_event_runs_loader_action_once`;
      - `no_manual_mouse_polling_remains` (the scan finds no `mouse_left_pressed` or `mouse_position` use in `host/` menu code).
    - **Smoke list:** the Mods tape highlights and clicks exactly like News; the panel's rows and the consent button do the same.
    - Tasks 13 and 13b build their new UI on this.
- [ ] **Step 2: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass, with every file's baseline at 0.
- [ ] **Step 3: Commit,** once per group.

### Task 12g: The mod manifest: identity, version, author and dependencies in one file

Asked for by the user on 2026-10-31. It runs after Task 12's groups and before Task 13, because Task 13's messages show mods' versions and Task 13b declares dependencies here. Community authors have nothing to build against until this exists, so it is settled before they arrive.

**Why, and the precedents** (checked 2026-10-31 against each project's source or docs):

| Today | Precedent | Here |
| --- | --- | --- |
| Flat ids (`ghost-tells`), so two authors can collide. | SMAPI's `UniqueID`, `Author.ModName`: `Pathoschild.LookupAnything`, and its own bundled `SMAPI.ConsoleCommands`. Thunderstore: `{team}-{package}`. BepInEx: reverse-domain GUIDs. | `<author>.<mod>`, chosen by the user as "some form of Author ModName". A dot, as SMAPI does, because this project's names contain hyphens. Thunderstore can use a hyphen only because its names forbid them. |
| No version and no author. A build is only a hash of the DLL. | SMAPI's manifest has `Author` and a semantic `Version`. Thunderstore's has a `version_number` in `Major.Minor.Patch`. BepInEx's `BepInPlugin` takes a version. | `author` and `version` are required. |
| Dependencies have no version limit (Task 13b as first written). | SMAPI: `UniqueID`, `MinimumVersion`, `IsRequired`. BepInEx: hard or soft, version ranges, `BepInIncompatibility`. Thunderstore: an exact version. | `id`, `minimumVersion` and `required`, as SMAPI does, plus `incompatibilities`, as BepInEx does. |
| The id is in both `mod.ini` and the DLL, and the dependency would have been in `requires=` and in an export. | SMAPI and Thunderstore keep one `manifest.json`. | One `manifest.json`, the only source of what it holds. |

**Files:**
- Create: `core/mod_manifest.h`, pure parsing and validation logic shared by the launcher and the host; `tests/mod_manifest_test.cpp`; `sdk/manifest.schema.json`, so editors can check a manifest; `third_party/nlohmann/` (the single header and its licence).
- Modify: `core/mod_list.h`, `host/mods.*`, `host/mods_panel.*`, `host/presence.h`, `tools/launcher.*`, `tools/payload.psd1`, `CMakeLists.txt`, `NOTICE`, and `sdk/README.md` ("The shape of a mod").
- Replace: each bundled mod's `mod.ini` with a `manifest.json`.

**Interfaces:**
- **The file.** `manifest.json` beside the DLL, standard JSON in UTF-8:

  ```json
  {
    "id": "phasmomods.ghost-tells",
    "name": "Ghost Tells",
    "author": "PhasmoMods",
    "version": "1.0.0",
    "description": "New tells for the Spirit, Mare, Shade, Yurei, Goryo and Onryo",
    "dll": "GhostTells.dll",
    "website": "",
    "dependencies": [{ "id": "phasmomods.spectral-flame", "minimumVersion": "1.0.0", "required": true }],
    "incompatibilities": []
  }
  ```

  `website`, `dependencies` and `incompatibilities` are optional. An unknown field is logged once and ignored, so a later loader can add fields.
- **Ids.** `<author>.<mod>`: exactly one dot, each part `a-z`, `0-9` and `-`, 64 characters at most in all. `vanilla` is reserved as an author, since content ids use it. There is no registry, as with SMAPI: a second mod with an id already loaded is refused, as today. The bundled mods use `phasmomods` as their author part, as SMAPI's own bundled mods use `SMAPI`.
- **Content ids** stay `<mod id>.<name>`, so `phasmomods.custom-ghosts.draugr`. A name holds no dot, so a content id splits unambiguously. Code that treats any dot as "a mod's id" (`host/content_difficulty.cpp` does) moves to one shared parser.
- **Versions.** Strictly `MAJOR.MINOR.PATCH`, as Thunderstore requires, compared part by part as numbers, so `1.0.10` is newer than `1.0.1`. Semantic Versioning's meaning applies. Pre-release tags are not accepted.
- **What stays in the DLL.** `apiVersion`, `channel` and `flags` stay in `PgModInfo`: they are compiled in and cannot drift from the code. `PgModInfo.id` must equal the manifest's id, or the mod is refused, as today. The manifest's `name` is what is shown. SMAPI's `MinimumApiVersion` is not copied, because the DLL's `apiVersion` already answers that.
- **Matching stays by build.** A lobby still compares the DLL's hash, because an author can rebuild without changing a version. The version and name are what players read.
- **Old mods.** A folder with only a `mod.ini` still loads, as the Global Constraints require, with its flat id, version shown as "unknown", and one log line. This path goes at the next major contract version.
- **Migration.** The ids in `launcher.ini` and the custom difficulty's saved pick move from the old bundled ids to the new ones once, through one table of the four old ids. An unknown saved pick still reverts to Random. The launcher's per-copy manifest beside each DLL becomes `<copy>.json`.
- **A bad manifest never hurts the game.** Parsing never throws past the parser. A malformed file, a file over 64 KiB, a bad id or a bad version skips that mod, with a log line and a Mods panel row saying why.
- **Parser.** nlohmann/json 3.12.0 (MIT, header-only, the usual C++ JSON library), vendored and pinned as MinHook is. Its licence joins the release payload and `NOTICE`.
- **Presence carries more, and every mod.** Today `take_announcement` stops adding mods at 511 bytes, with no log line (`host/presence.h`), so a player with more mods than fit announces a short list and is treated as lacking the rest. Longer ids and versions would make that happen sooner. Wire version 3 adds each mod's name and version, so a joiner can show a mod it does not have, and splits the list across messages with a part number and part count. A receiver replaces its list for that peer only when every part of an announcement has arrived.

- [ ] **Step 1: Write the failing tests:**
  - `manifest_reads_every_field`;
  - `manifest_rejects_bad_id` (no dot, two dots, upper case, too long, the `vanilla` author);
  - `manifest_rejects_bad_version`;
  - `version_compares_as_numbers`;
  - `manifest_malformed_skips_mod_with_reason`;
  - `manifest_unknown_field_is_ignored`;
  - `manifest_id_must_match_dll`;
  - `old_mod_ini_still_loads`;
  - `content_id_splits_at_the_last_dot`;
  - `old_bundled_ids_migrate_once`;
  - `presence_announces_every_mod_across_parts`, with 40 mods of the longest id;
  - `presence_keeps_last_list_until_all_parts_arrive`.
- [ ] **Step 2: Run** `ctest --test-dir build -R "mod_manifest|mods|presence" --output-on-failure`. Expected: the new tests fail.
- [ ] **Step 3: Implement it,** moving the bundled mods' ids, their content ids and their `PgModInfo` together.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass. Add to Task 14's smoke list: the Mods panel shows each mod's author and version, a saved custom difficulty pick survives the id change, and a lobby with a bundled mod in another build shows the versions.
- [ ] **Step 5: Commit.**

### Task 13: Lobby rules: every mod required, vanilla hosts, mismatched joiners

Added by the user on 2026-10-24 and widened on 2026-10-28. The three rules:
- **Every mod is required by every player.** `PG_MOD_VANILLA_PEERS` is removed. The flag value stays reserved in `PgModFlags`, so 2.0-2.2 mods that set it still load; the host ignores it, and the docs say so. A modded host always removes players without the same mods. The join guard's vanilla-peer branch goes.
- **A vanilla host disables mods for all players** in that lobby, as below. Every modded player there is told why.
- **A modded joiner who lacks a host mod** is removed. "Lacks" means the joiner does not run a mod the host runs, or runs it in another build. The joiner is shown the list of ALL the lobby's required mods: every mod the host runs, with its version, marking the ones the joiner lacks or has in another build. The joiner is then removed: it leaves by itself, and the host removes it if it does not. Build the list from the game's own widgets, shown once the joiner is out of the room, as the join guard's message is today. An unmodded joiner is removed by the host and sees the game's own kick.
- **A modded joiner with extra mods only** (user, 2026-10-30) stays. Here the host's mods are a subset of the joiner's, each in the same build. The joiner's extra mods are switched off for that lobby, with a notice naming them, and come back on when the joiner leaves the room. A vanilla host is the same rule with every mod extra.
- **No mod downloads from the host** (user, 2026-10-30: only if remote code execution is impossible). A mod is native code, so loading one a host sent is remote code execution by design. It is not built. The removal message tells the joiner which mods to install.

- **Mods run only in private lobbies** (user, 2026-11-01). In a public lobby (one the game lists in its browser; single player counts as private, as `game::private_room` reads it today) every mod is switched off for that room with a notice, for the host as well, through the same suspension as below, and comes back on leaving. Nobody is removed from a public lobby over mods, because none are running. Reverse engineer how the game makes a room public or private, and whether a host can change it with a room already open. A lobby that turns public suspends its mods at once. If it can turn public during a contract, stop and report, because suspension never happens in a contract.
- **Steam achievements are left alone** (user, 2026-11-02). Task 13 first blocked them while any mod ran; the user had that removed entirely the next day ("If someone wants to cheat this won't stop them"). The host hooks nothing of Steam's, and `sdk/systems/achievements.md` documents the game's path for authors.
- **The consent notice says so.** In `host/consent_text.h`, in the "Your account is at risk." paragraph, replace "Play with mods only in private lobbies, and only with friends who also agreed to this." with exactly "Mods only run in private lobbies and single player, and switch off in public lobbies. Play only with friends who also agreed to this." The notice says nothing about achievements.

Since every player must run the same mods, the host's per-room decision that the mod lists match (Task 9's gating) is the only agreement mods rely on.

**Files:**
- Modify: `host/join_guard.h` and the host suite's lobby tick (`host/suite.cpp`, `host/mods.*`).
- Modify: the notice. Build it from the game's own widgets, as `host/consent.cpp` and `core/notice.*` do.
- Test: `tests/join_guard_test.cpp` and `tests/mods_test.cpp`.

**Interfaces:**
- **The new verdict.** `joinguard::Verdict` gains `std::vector<std::string> suspend` (the ids of this client's mods to switch off for this room) and `bool resume`. Both rules apply only while this client is not the master, runs mods, and the lobby screen is up:
  - The host has not been heard from `kGraceMs` (10 s) after this client first saw the host: `suspend` holds every running mod.
  - The host's presence lists a subset of this client's running mods, each in the same build: `suspend` holds the rest.
  - A host mod this client lacks, or runs in another build, leads to the removal as above. That is never a suspension.
- **Suspending.** The mods restart for the lobby without the suspended ones, the way the Mods panel's restart works: hooks rolled back, content withdrawn, network messages stopped, then the rest started again. This client's presence then announces only the mods still running, so the room's mod agreement (Task 9) sees matching lists. Mods that require a suspended mod are suspended with it, a cascade Task 13b adds. Suspension never happens during a contract, because the guard judges only while the lobby screen is up.
- **The notice texts:**
  - Vanilla host: "This lobby's host isn't running PhasmoMods, so your mods are off until you leave it."
  - Extras: "This lobby's host doesn't run <names>, so those mods are off until you leave it."
  - Public lobby: "Mods only run in private lobbies, so your mods are off until you leave this one."
- **Resuming.** On leaving that room, `resume` is set and the mods start again, as after the Mods panel's restart.
- **A late reply from the host.** If the host's presence arrives late, the mods start again in the lobby and the existing matching rules then apply.

- [ ] **Step 1: Write the failing tests:**
  - `vanilla_host_suspends_after_grace`: `suspend` is false at 9,999 ms and true at 10,000 ms.
  - `vanilla_peers_flag_is_ignored`: a host whose every mod sets the old flag still removes an unmodded joiner after `kGraceMs`.
  - `mismatch_message_lists_every_required_mod`: the message names every host mod with its version and marks the missing and other-build ones.
  - `joiner_with_extra_mods_suspends_only_extras`: host {A, B}, joiner {A, B, C}, so `suspend` is {C} and nothing is removed.
  - `joiner_missing_host_mod_is_removed`: host {A, B}, joiner {A, C}.
  - `joiner_with_other_build_is_removed`.
  - `extras_notice_names_them`.
  - `extras_resume_on_leaving`.
  - `suspended_client_announces_only_running_mods`.
  - `leaving_resumes`.
  - `late_host_presence_resumes_then_matches`: matching hosts stay; a mismatching host leads to the existing leave.
  - `never_suspends_outside_the_lobby_screen`.
  - `host_never_suspends_itself`, except in a public lobby.
  - `public_lobby_suspends_every_mod_and_removes_nobody`, for the host and a joiner; `single_player_counts_as_private`; `leaving_a_public_lobby_resumes`; `lobby_turning_public_suspends`.
  - `notice_says_private_lobbies`, which also checks the fingerprint differs from the old notice's.
  - In `mods_test`, `suspend_and_resume_in_a_room`: hooks and content come out and go back.
- [ ] **Step 2: Run** `ctest --test-dir build -R "join_guard|mods" --output-on-failure`. Expected: the new tests fail.
- [ ] **Step 3: Implement it.** The current rule that mods restart "never in a room" applies to contracts. Suspending and resuming in the lobby screen is allowed, and the docs say so.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 13b: Mod dependencies, required and optional

Added by the user on 2026-10-29: a mod can name other mods it requires and other mods it can use when they are there. Examples: Custom Ghosts requires Spectral Flame; Playable Ghost optionally uses Custom Ghosts. The loader enforces this. No mod checks for another mod itself.

**The `phasmomods` author is reserved for the project's own mods** (user, 2026-11-02). Only mods shipped in the loader's install folder may use the id author `phasmomods` or the manifest author `PhasmoMods` (compared ignoring case and surrounding spaces). A mod anywhere else, such as `%LOCALAPPDATA%\PhasmoMods\mods`, that claims either is refused: it stays off, its row says why, and the real mod is unaffected whichever folder sorts first. This stops honest mistakes and casual impersonation. It isn't proof of origin, since there is no signing and someone with access to the install folder could still replace a file, and the docs say so. Tests: `third_party_mod_cannot_claim_the_reserved_id_author`, `third_party_mod_cannot_claim_the_reserved_author_name`, `real_mod_runs_when_an_impostor_sorts_first`, `bundled_mod_may_use_the_reserved_author`.

**Files:**
- Modify: `core/mod_api.h` (one `PgHostApi` member at the end, contract 2.2).
- Create: `host/mod_deps.h`/`.cpp`, pure logic: dependency resolution, start order, menu cascades.
- Modify: `host/mods.*` (load and start order, stop order), `host/mods_panel.*` (row reasons and cascades).
- Modify: the manifests of `mods/custom_ghosts` (requires `phasmomods.spectral-flame`) and `mods/playable_ghost` (optionally uses `phasmomods.custom-ghosts`).
- Docs: `sdk/README.md`, a "Dependencies" section in the mod author guide.
- Test: `tests/mod_deps_test.cpp` (new) and `tests/mods_test.cpp`.

**Interfaces:**
- **The declaration** (changed 2026-10-31, replacing a `pgmod_depends` export that was never built). Dependencies are in the manifest (Task 12g): `dependencies`, each with `id`, an optional `minimumVersion` and `required` (default true), and `incompatibilities`, a list of ids. So the launcher and the panel can read them without loading the DLL. A mod that declares none depends on nothing, so a 2.0-2.2 mod keeps loading.
- **The query.** `bool (*mod_started)(const char* modId);` is appended to `PgHostApi`. It answers whether that mod on this client has started. A mod asks it for an optional dependency, from its `pgmod_start` onwards. Move the static_asserts on the last member in `host/suite.cpp` and `sdk/fake/fake_game.cpp`.
- **The rules,** in `host/mod_deps`:
  - **Start order.** A mod starts after every dependency it names that is enabled, both required and optional. The Mods panel's saved load order breaks ties.
  - **A required dependency that is missing, disabled, or does not start.** A dependency does not start when its `pgmod_start` returns false, its `pgmod_requires` entries do not resolve, or its own dependencies fail. Then the dependent is not started, and that cascades. Each mod left out is logged with the chain of why.
  - **A dependency older than `minimumVersion`** counts as missing, for a required and an optional one alike. An old-format mod's version is unknown, so it never meets a `minimumVersion`. The reason names both versions.
  - **An incompatibility.** A mod naming an installed and enabled mod in `incompatibilities` is not started, with the reason on its row, as BepInEx does. The other mod is unaffected. Each mod in a mutual refusal stays off.
  - **A missing optional dependency** changes nothing except order.
  - **Cycles.** Mods joined by a cycle of required dependencies are not started, and the log names the cycle. An optional dependency that would close a cycle is ignored for ordering, logged.
  - **Unknown ids, self-dependency and duplicates.** A dependency on an id no installed mod has counts as missing. A mod naming itself, or naming one id twice, is refused, logged.
  - **Stopping and restarting.** Stop runs in reverse start order, and the panel's restart follows the same order.
  - **Start order fixes one known case.** Playable Ghost optionally depends on Custom Ghosts, so it starts after it. That keeps a player Lithobolia's aimed throw paid for, which today depends on the folder sort order (Task 12(a)'s parity round). Test: `playable_ghost_starts_after_custom_ghosts`.
  - **A lobby suspension (Task 13) cascades.** A mod that requires a suspended mod is suspended too, and named in the notice. Test: `suspension_cascades_to_dependents`.
- **The Mods panel** stays vanilla UI: rows and text drawn from the game's widgets, as now.
  - Turning a mod on also turns on the mods it requires.
  - Turning a mod off also turns off the mods that require it.
  - Optional dependencies never switch anything.
  - A row whose mod is not started for a dependency reason says so on the row, e.g. "Needs Spectral Flame", or "Needs Spectral Flame, which did not start".
- **Lobbies.** Task 13's rules are unchanged: the room's mod lists must match exactly. A dependency only decides what starts on one client.
- **Behaviour difference** (recorded in the report): before this task, Custom Ghosts without Spectral Flame kept the Echo, Dullahan and Black Shuck. Now Custom Ghosts does not start without Spectral Flame, and the panel says why.

- [ ] **Step 1: Write the failing tests** in `mod_deps_test`:
  - `required_dependency_starts_first`
  - `optional_dependency_starts_first_when_present`
  - `missing_required_dependency_leaves_mod_off_with_reason`
  - `failed_required_dependency_cascades`: A requires B, B requires C, and C's start fails, so A and B stay off.
  - `missing_optional_dependency_still_starts`
  - `required_cycle_refused_and_named`
  - `optional_edge_closing_cycle_ignored_for_order`
  - `self_and_duplicate_dependency_refused`
  - `stop_runs_in_reverse_start_order`
  - `panel_enabling_turns_on_required_dependencies`
  - `panel_disabling_turns_off_dependents`
  - `mod_without_dependencies_depends_on_nothing`
  - `dependency_below_minimum_version_counts_as_missing`
  - `incompatible_mod_stays_off_with_reason`
  
  In `mods_test`, add `mod_started_answers_for_optional_dependency`, and a test that the bundled Custom Ghosts and Playable Ghost declare the dependencies above.
- [ ] **Step 2: Run** `ctest --test-dir build -R "mod_deps|mods" --output-on-failure`. Expected: the new tests fail.
- [ ] **Step 3: Implement it.** Then remove any start-order assumption or "is that mod running" check the bundled mods still make in favour of these rules.
- [ ] **Step 4: Run** `.\validate.ps1` and the gamecheck with `-Expect`. Expected: pass.
- [ ] **Step 5: Commit.** Add "Mods panel: Custom Ghosts needs Spectral Flame (row text, cascades both ways)" to Task 14's smoke list.

### Task 13c: New game builds without a new release

Asked for by the user on 2026-11-01: when the game updates, compatibility is checked automatically, so a new release is needed only when something must be fixed. This replaces the spec's earlier rule that the host stays inactive on any build the offline check hasn't passed. It runs before Task 14, so a game update during the playtest doesn't switch everyone's mods off.

Most of it exists already. The host resolves every map entry against the running game at start, and a mod whose required entries don't resolve stays off and is named. Only `host/build_check.cpp`'s two compiled-in hashes stop an unseen build.

- **Known builds.** The compiled-in hashes become records under `sdk/builds/`, one small text file per build the offline check has passed, as the spec planned. The check writes the record and the release ships them. On a known build nothing changes.
- **An unseen build** (user's choice, 2026-11-01: ask once per new build). The host no longer stops. It logs both hashes and resolves the map as usual. The consent notice is asked again, with this paragraph after "Your account is at risk.":

  "<b>The game has updated.</b> These mods haven't been checked against this version of Phasmophobia. Mods that still find everything they need will run, and the rest stay off. The mods can't tell whether the update changed the anti-cheat, so playing now carries more risk than usual.\n\n"

  The paragraph appears only on an unseen build. The fingerprint covers the notice's text and the two build hashes, so each new build asks once. On "I agree", every mod whose required entries all resolve runs, and each one that doesn't stays off, with a Mods panel row and a log line naming its missing entries. "Play without mods" keeps the game vanilla, as today.
- **A build whose popup can't be copied** (user, 2026-11-02: the notice's own overlay is removed). No notice can be shown, so the answer is Declined and the game runs without mods, with a log line naming why. Name the cause to the player through the game's own UI if that UI still works on such a build, such as a Mods panel row. If it doesn't, the log and the player guide are all there is. A window of our own is not an option.
- **No mod's code runs before consent** (user, 2026-11-02). Today `host/suite.cpp` calls `mods::load`, which runs `LoadLibraryW` on every mod it finds, before the notice is asked, so a mod's `DllMain` runs before the player has agreed to anything. Split it:
  - **Before agreement** the host reads `manifest.json` files only. The notice and the Mods panel build from those, and no mod DLL is mapped into the game. This covers every mod, bundled ones too, through one path.
  - **On "I agree"** the host loads the DLLs of the mods that are ticked and may run. A mod that is off is not loaded until it is switched on. "Play without mods" and a Declined answer load none.
  - **What the host reads from a DLL before agreement today moves to the manifest,** including `PG_MOD_RECORDS_VOICE`, which the notice shows, as a new `recordsVoice` field in `manifest.json` and `sdk/manifest.schema.json`, defaulting to false. After loading, a DLL whose flag says it records voice when its manifest does not stays off, with a reason, since the player agreed on the manifest's word. A mod that is refused only once loaded, such as for its contract version, gets its row's reason then. A refused DLL can't be unloaded safely, so say so in the docs.
  - **Words.** The notice's "Choose which run" is then true. `sdk/README.md` tells authors nothing of theirs runs before agreement, and `DllMain` is not a place to do work.
- **Nothing else loosens.** Ambiguous entries still fail closed. Native sites still need their patterns. The scene facts are still checked as each scene loads. The map still never covers anti-cheat, SelfReport or moderation, and the loader never looks at them to judge an update.
- **Files.** `host/build_check.*` and the records; `host/consent_text.h` and `host/consent.*`; `tools/gamecheck.ps1` (it writes the record); `sdk/builds/`; the player guide (Task 14 Step 2b) and the spec's "Failure handling at startup".
- **Tests:**
  - `known_build_runs_without_the_update_paragraph`;
  - `unseen_build_asks_again_with_the_update_paragraph`;
  - `agreement_is_remembered_per_build`;
  - `unresolved_mod_stays_off_and_is_named`, while the others run;
  - `declined_on_an_unseen_build_stays_vanilla`;
  - `gamecheck_writes_the_build_record`;
  - `no_mod_dll_loads_before_agreement`;
  - `declined_loads_no_mod_dll`;
  - `unticked_mod_is_not_loaded_until_switched_on`;
  - `notice_shows_voice_from_the_manifest`;
  - `dll_recording_voice_against_its_manifest_stays_off`.
- **Smoke list:** the update paragraph fits the popup; a forced unknown hash shows it and asks once.

### Task 13d: The host decides the lobby's mods

Asked for by the user on 2026-11-03: "Every mod should treat loading the same, and not handle their own logic for modlist matching. The host has the definitive say on the mod list for that lobby. Any joining player must have those mods, or they are kicked. This included vanilla players. No player should be able to change which mods are part of that lobby."

**What this replaces.** Task 9's agreement (`sdk/runtime/mod_agreement.h`) asks each client whether every other player it can see runs the same mods. So one joiner, vanilla or mismatched or merely not heard yet, switches mods idle for everyone in the room until they're gone, which lets a player change what the lobby runs. Each client also decides for itself, so two clients can disagree during a contract, and the answer can flip mid-contract. All of that goes, including the 2026-11-02 ruling that accepted the flip.

**The rule.**
- **The lobby's list is the host's list.** It is set by what the master runs and announces. Nothing a joiner runs, says or fails to say changes it, or changes what the other players' mods do.
- **A client's mods are on when its running mods equal the host's list,** after Task 13's switching off of its extras. That is the only question each client asks, and it asks about the host alone. The host's mods are on from the moment the lobby opens, with no waiting on anyone. A vanilla host and a public lobby work as Task 13 set: mods off.
- **A joiner must match or is removed,** as Task 13 built: a modded joiner missing a host mod or running another build leaves with the required list, and a vanilla or never-heard player gets the game's kick after `kGraceMs`. A joiner's extras are switched off and it stays.
- **Nobody changes the list from inside a room.** The Mods panel's changes wait until out of the room, for the host too.
- **A new host** (the master leaves) takes over with its own list, which equals the old one, since it was verified. A client whose list differs from the new host's follows the same rule as a joiner.
- **Every mod treats loading the same.** No mod has any matching logic. It is registered, gated and started by the host in one way. Playable Ghost's use of `room_agrees` (`mods/playable_ghost/room.cpp`, `session.h`) comes out, or becomes the host's single answer with no logic of its own. `room_agrees` and `peer_has_mod` stay in the contract so 2.x mods load, answering from this rule. The docs say a mod has no need of them.

**A contract never holds an unverified player.** The grace is a lobby matter only. No client decides anything at load: everyone in a contract has already been verified.
- **Reverse engineer first:** `ServerManager.LoadScene` and whatever calls it. Does it close the room (`PhotonRoom.set_IsOpen`), so nobody can join a running contract? Record this in `sdk/systems/photon.md`. If a contract can be joined, stop and report.
  - **Found, and ruled by the user (2026-11-03):** the room closes only on the server's echo of `LoadScene`, so a join can land for about one round trip after the send. The user first ruled that player kicked, so the mods aren't affected, but the game's kick can't reach them (`task-13d-report.md`, "The join window"). **Final ruling: close the room first.** On Start the master closes the room, waits for the server to confirm, and re-reads who's there. If everyone is verified the contract starts; otherwise the room reopens and the waiting popup shows. Nobody gets in late, so no kick is needed and a contract is always fully modded. Tests: `the_room_closes_and_is_confirmed_before_the_start_goes`, `a_player_who_got_in_first_reopens_the_room_and_waits`, `no_confirmation_reopens_the_room_and_says_so`, `a_room_closed_for_a_start_opens_again_unless_its_master_left`, `pressing_again_while_closing_starts_nothing_twice`.
- **The start waits until everyone is verified** (user, 2026-11-03: so there aren't any unexpected kicks). While any player in the room is unverified, the host's start doesn't go ahead. Nobody is kicked because the host pressed start: the guard's normal timing still applies, so the wait ends when the player is verified, leaves, or is removed at the end of `kGraceMs` (10 s at most).
  - **Reverse engineer the start path first:** what the host presses, the game's own ready check (does the game already refuse a start until every player is ready?), and what calls `ServerManager.LoadScene`. Record it in `sdk/systems/lobby` notes or `sdk/systems/photon.md`.
  - **Use the game's own gate if one exists:** an unverified player counts as not ready, so the game's own button and text say so. If there is none, hook the start with a map entry and return before anything is sent, so the game's state doesn't change and the host can press again. Tell the host through the game's own popup, naming who is being waited for. No window of our own. Prefer what leaves the fewest new states, and report what you chose and why.
  - **No trap.** The wait must always end. If a kick is skipped and its retries run out, the host gets Task 13's popup naming the player, and the start stays refused while that player is here; the host can still leave the room. The game is never hung. If the start path's entries don't resolve, the lobby rules can't hold, so no mod starts, as for the host's other requirements.
  - **If nothing can hold the start safely, stop and report.**

**During the grace, what reaches an unverified player.** The host's mods stay on, so custom data must not reach someone who may not have it (the Global Constraint "Saves and other players"). Audit everything that goes to the whole room while a player is unverified: `SyncDifficultyValues` and the difficulty pick, the journal's page sync, the lobby's boards and any buffered RPC a late joiner replays. Each one holds back custom values until everyone present is verified, without changing what the host sees. List each in `sdk/systems/photon.md`, and report any that can't be held. A mod's channel messages go only to and from verified players.

**What goes away or changes:**
- `Agreement` becomes the host's-list check, so `everyoneHeard`, the held decision and its no-to-yes flip are removed.
- The content registry's withdrawal "when a peer lacks it" goes: content follows this client's match with the host. Review Focus 1 and `content_withdrawn_when_a_peer_lacks_it` are rewritten to the new rule.
- The notes on `room_agrees`, `attach_rules` and `active` and the README's "The room's mod agreement" say this rule, and no longer say rules can start partway through a contract.

**Tests:**
- `host_list_is_the_lobbys_list`;
- `joiner_cannot_switch_off_the_hosts_mods`: a vanilla joiner, a mismatched joiner and a never-heard joiner each leave the host's and other players' mods running;
- `joiner_with_host_mods_runs_them`, `joiner_extras_off_and_it_stays`;
- `vanilla_joiner_is_kicked_after_grace`, `mismatched_joiner_leaves_with_the_list`;
- `hostile_announcement_changes_nothing_for_others`;
- `start_waits_for_an_unverified_player`, `start_goes_once_everyone_is_verified`, `pressing_start_kicks_nobody`, `wait_ends_when_the_player_is_removed_or_leaves`, `no_decision_changes_during_a_contract`;
- `custom_values_held_back_while_a_player_is_unverified`, `custom_values_flow_once_everyone_is_verified`;
- `new_host_list_takes_over`;
- `panel_changes_wait_until_out_of_the_room`;
- `no_mod_has_matching_logic`: a scan fails on `room_agrees` or `peer_has_mod` in `mods/`.

**Smoke list:** a vanilla friend joins a modded lobby: the host's tells and custom ghosts stay on for everyone else, and the friend is kicked within 10 s. A friend with a missing mod sees the list and leaves. The host presses start just as someone joins: the start waits, nobody is kicked for it, and the contract starts fully modded once they're verified. With a vanilla friend joining, the start waits at most 10 s, then the friend is kicked and the host can start.

### Task 14: Enforcement, documentation and the in-game smoke list

**Files:**
- `tests/raw_scan_test.cpp`, with the baseline deleted.
- `sdk/README.md`.
- `sdk/systems/ghost-types.md`, `sdk/systems/journal.md`, `sdk/systems/difficulty.md` and `sdk/systems/equipment.md`.
- Every other `sdk/systems/*.md` page and the notes in `sdk/map/*.h` and `sdk/api/*.h` (Step 1d).
- The rename (Step 0): every file under `core/`, `host/`, `mods/`, `sdk/`, `tests/` and `tools/` with a `pg` name, `CMakeLists.txt`, and a fixture mod DLL with the old export names.
- Typed handles (Step 0b): `sdk/api/game_api.h`, `sdk/api/held.h`, `sdk/api/runtime.h`, `sdk/api/content.h`, every `sdk/map/*.h` wrapper struct, the bundled mods, `tests/compile_fail/`, and `tests/game_api_test.cpp`.
- The reference (Step 1e): every public header's comments, `sdk/Doxyfile`, a `docs` target in `CMakeLists.txt`, `validate.ps1`, `sdk/CHANGELOG.md`, and `sdk/internals/<area>.md`.
- The guides in `sdk/guides/`:
  - `adding-a-ghost.md`;
  - `adding-evidence.md`;
  - `tells-for-vanilla-ghosts.md`;
  - `mimic-and-your-ghost.md`;
  - `lobbies-saves-and-players.md`.
- `docs/custom-ghosts.md`, rewritten to point at the content layer.
- `docs/superpowers/plans/2026-10-24-stage2b-smoke-list.md`.

- [ ] **Step 0a: Audit the mod-drawn windows** (user, 2026-11-02). The vanilla-UI-only rule forbids invented windows and overlays, and any design that needs one goes to the user as a question first. The consent notice's fallback overlay broke that rule unnoticed and was removed in Task 12(f). The remaining places that make their own canvas with `ui::create_canvas` are `core/notice.cpp`, and Playable Ghost's `hud.cpp`, `panorama.cpp` (the cover), `perceive_view.cpp` and `picker.cpp`. Search again, in case more exist by then.
  - **For each, report to the user:** what it shows and when; whether the user ever approved it (with where that is recorded, or "no record"); what the game has that could replace it; and a recommendation to keep, replace or remove it.
  - **Wait for the user's ruling on each,** then carry it out before the test list is written. Anything replaced gets its own smoke item.
  - **Test:** `no_unapproved_canvas`. A scan fails on a `ui::create_canvas` call outside the list the user approved, so a new window has to be raised first.
- [ ] **Step 0: Rename the `pg` prefix to `pm`, for PhasmoMods** (user, 2026-10-30). `pg` was left over from Playable Ghost, the project's first mod. It now names the loader, the SDK and the host contract, which misleads outside authors and clashes with the Playable Ghost mod itself (`PgPlayableGhostService`). The rename happens before Steps 1d and 1e, so the reference is written once, with the new names. It runs at a quiet point, with no implementer working in parallel, since it touches about 370 files.
  - **What is renamed:**
    - namespaces `pg::` to `pm::` (`pm::api`, `pm::map`, `pm::sdk`, `pm::mod`, `pm::game`);
    - C types `Pg*` to `Pm*` (`PmHostApi`, `PmModInfo`, `PmGhostDef`);
    - macros `PG_` to `PM_` (`PM_LOG`, `PM_MOD_RECORDS_VOICE`, `PM_HOST`, `PM_TRACK`, `PM_MAX_MESSAGE`);
    - the exports `pgmod_*` to `pmmod_*`;
    - the CMake targets `pgmod`, `pgcore` and `pg_options`;
    - the scan's marker `// pg:not-game-data:` to `// pm:not-game-data:`.

    The `phasmo::` namespace for the game itself stays. Steps and tasks after this one read with the new names.
  - **Binary compatibility.** Only the exports' names are visible across the DLL boundary; struct layouts and values do not change. A mod built for 2.0 or 2.1 keeps loading, as the Global Constraints require. `host/mods.cpp` looks up each `pmmod_*` export first and falls back to its `pgmod_*` name, logging once per mod that it uses the old names. The fallback is removed at the next major contract version. The contract version itself is unchanged by the rename.
  - **Source compatibility.** None is kept. No outside author builds against the SDK before the 2d zip, so the old source names go entirely. A test fails on any `pg::`, `Pg[A-Z]`, `PG_` or `pgmod` left in `core/`, `host/`, `mods/`, `sdk/`, `tests/` or `tools/`. The test excludes the host's fallback lookup and `docs/superpowers` (maintainers' history).
  - **Tests:**
    - `loads_mod_with_new_exports`;
    - `loads_mod_with_old_exports_and_logs_once`, with a fixture DLL exporting only `pgmod_*`;
    - `no_old_prefix_left`.
  - Run `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass. The smoke list gains a check that every bundled mod starts. Commit the rename on its own, separate from the other steps.
- [ ] **Step 0b: Typed handles for game objects** (user, 2026-10-30; **moved to after the playtest by the user, 2026-11-03**, for its size: about 3,100 `void*` sites, and only MSVC can confirm them. Step 1e documents the `void*` signatures until it lands, and it updates them then). Today every game object in the game API is a `void*`, so nothing stops a mod passing a Player where `GhostAI` expects a ghost. As other SDKs do (Steamworks' `CSteamID`, Win32's `HWND`), each mapped type gets its own handle type, so the compiler catches the mistake. It runs after Step 0 and before Steps 1d and 1e, so the reference documents the typed signatures.
  - **The handle.** `pm::api::Ref<D>` is a non-owning reference to an object of the type entry `D`, a single pointer in size. Each type's wrapper struct names it: `phasmo::GhostAI::Ref ghost`.
    - It can be null and tested (`explicit operator bool`), and compared.
    - It converts implicitly to its base type's handle, following the parents the map records for the type. A `Ref` to a derived type passes where a base type is expected, and every Unity object handle passes to `pm::api::alive`.
    - Going the other way is checked: `phasmo::as<phasmo::GhostAI>(obj)` gives a null handle when `obj` is not of the type (`Type::is`).
    - Objects of unmapped types use `pm::api::Object`. `raw()` gives the `void*` for the runtime services and the host contract.
  - **Typed wrappers.**
    - A `Method` or `Icall`'s object and object parameters, a `Field` or `StaticField` of a reference type, and a return value of a mapped type all take and give handles.
    - Hook detours receive handles too.
    - `Held` becomes `Held<D>`, with `Held<>` holding any object.
    - `Type<D>::make()` and the `Array` and `List` views give typed handles where the element type is mapped.
    - A struct passed by address stays a pointer, typed where the struct is mapped.
  - **The game never sees a handle.** The game is always called, and hooked, with the raw types. Each wrapper converts at its own boundary, so the trampolines and `Bodies` entry functions do not change. The reason: on x64, MSVC returns a class with a constructor through a hidden pointer, not in RAX. A handle declared as a game function's return type would therefore break the call. Static asserts pin `sizeof(Ref) == sizeof(void*)` and that it is trivially copyable.
  - **What stays untyped.** The host contract in `core/mod_api.h` is C and stays `void*`. Handles exist only in the C++ game API.
  - **Migration.** Every `phasmo::` wrapper, `sdk/api/content.h` and the bundled mods move to handles in this step. A test fails on a `void*` object parameter or return in a `phasmo::` wrapper signature, except where its comment says why (an unmapped type with no `pm::api::Object` use).
  - **Tests:**
    - `compile_fail` cases: a Player handle passed to `GhostAI::isHunting.get`, a GhostAI handle passed where a Camera is expected, and a downcast without `as`;
    - `base_handle_accepts_derived`;
    - `as_gives_null_for_wrong_type`;
    - `hook_detour_receives_typed_handle`;
    - `typed_return_round_trips`, a game method returning a reference, called through the fake backend;
    - `held_typed_keeps_object`.
  - Run `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass, with no change in behaviour. The smoke list gains a check that each bundled mod still works as before. Commit on its own.
- [ ] **Step 1: Make the scan enforcing.** It fails on any raw site outside the runtime files, with no baseline. Then watch it fail once, on each of these deliberately added in a scratch commit that is then dropped:
  - a raw hex RVA in a mod;
  - a field offset in `core/`;
  - a Malayalam name in `host/`.

  Record the three failure outputs in the report.
- [ ] **Step 1b: Enforce that the loader owns the logistics** (user, 2026-10-29). No mod invents its own handling of anything the loader or SDK provides: networking, mod loading, dependencies, mod matching and agreement, lobby rules, content registration, saving or syncing content, or similar plumbing.
  - **The automated scan.** Add `tests/logistics_scan_test.cpp`, which fails on any of these in `mods/`:
    - loading or finding other modules: `LoadLibrary`, `GetProcAddress`, `GetModuleHandle`;
    - reading another mod's files or exports;
    - calls to `peer_has_mod`, `service` or `register_service`, except `playable-ghost.v1` for what is not content, each remaining one listed by name in the test;
    - map entries for Photon or the game's own RPC or raise-event paths used to send a mod's own data;
    - another mod's id or type numbers as literals, except in a dependency declaration;
    - a mod's own presence, handshake or version-exchange messages.
    
    Watch it fail once on each category in a scratch commit that is then dropped, and record the outputs.
  - **The audit, per bundled mod.** Write a table in the report: each place it touches networking, starting, other mods, the lobby, saves or content. For each, say which loader or SDK function it uses, or why the place is the mod's own game behaviour, not logistics. Anything that is logistics and not routed through the loader or SDK is fixed in this task, or, if the loader lacks the function, added to the loader with its own test.
  - **Messages between peers** (user, 2026-11-01, narrowed the same day: not a strict requirement, the aim is that no mod invents its own way of messaging peers). Every mod's messages go through the SDK's one channel (`pg::net::send`), which they already do; the scan keeps it that way. What a mod puts in its own messages is its own: Playable Ghost's session messages stay as they are, on that channel, and no new networking layer is built. Two narrow checks only:
    - any part of Playable Ghost's session hello that amounts to presence, a handshake or a version exchange moves to the loader's, as the scan above requires;
    - the kept journal's `PhotonView.RPC` hook and its sync switch-off (`journal.cpp`) are not peer messages. Ask whether they work around a side effect of our own code, a copied journal that should never carry live PhotonViews. If so, fix that cause and remove them.

    A small shared helper for encoding messages could go in Task 17 if the coverage spec shows a need.
  - **The documentation.** `sdk/README.md` gets a "What the loader does for you" section listing these jobs, and marks `peer_has_mod` and `service` as not for checking other players or mods.
  - **The final whole-branch review** repeats this audit independently over every mod, not only the diff.
- [ ] **Step 1c: Warn about community mods in the consent notice** (user, 2026-10-30). In `host/consent_text.h`, after the "Your account is at risk." paragraph, add exactly:

  "<b>Mods are programs.</b> A mod runs its own code on your computer, with your permissions, and can do anything a program can. Only install community mods from authors you trust, and only when their source code is publicly available.\n\n"

  The changed text changes the notice's fingerprint, so every player is asked again. That is intended.
  - Test: `notice_warns_about_community_mods`, which checks the paragraph is present whatever mods are installed, and that the fingerprint differs from the old notice's.
  - Smoke list: the whole notice still fits its popup, at the game's own text size, with the voice paragraph shown. If it does not fit, the layout follows the game's popup (scrolling or size), never a smaller font.
- [ ] **Step 1d: Take the addresses out of the SDK's documentation** (user, 2026-10-30). Authors read `sdk/README.md`, `sdk/systems/*.md`, `sdk/guides/*.md` and the `///` and `//` notes in `sdk/map/*.h` and `sdk/api/*.h`. None of them carries an RVA, a field offset (`+0x30`), a raw address or a native site's bytes.
  - **Why.** The numbers are right for one build only, while the resolver finds everything again on each update. They tempt authors to read raw instead of through the map. They are derived game data, which the Global Constraints keep out of commits.
  - **Cite by key.** A claim's proof names the map key it was read from, for example "`Difficulty.ApplyDifficulty` saves the lobby's difficulty, then sends it". `re/decomp.mjs` decompiles that key on any build.
  - **Unmapped methods get an entry.** A claim about a method with no entry (for example the wander state's destination picker) gets one, so it has a key to cite. The builder decides whether that entry is public or host-only.
  - **Where the numbers go.** Maintainers' raw numbers go in gitignored files: `re/dump/expect/*.txt` for the values entries replace, `re/dump/notes/<area>.md` for RE notes, and the build ledger. Build numbers ("read from build 25811160") may stay.
  - **The scan.** Extend `raw_scan_test` to `sdk/README.md`, `sdk/systems/`, `sdk/guides/`, `sdk/examples/`, and the comments in `sdk/map/` and `sdk/api/`. It fails on a hex literal of 0x100 or more and on an offset written `+0x..`. In Markdown a line may end in `<!-- pg:not-game-data: <why> -->`, as `// pg:not-game-data:` works in code. Watch it fail once on an RVA added to a `sdk/systems` page in a scratch commit that is then dropped, and record the output.
  - **Rewrite** every line the scan finds (about 160 lines in `sdk/systems` and 195 in the `sdk/map` notes when this step was added), with each claim kept and cited by key. No fact is lost: before it leaves the docs, each number is checked to be in an expectation file or a gitignored note.
  - **Review.** The task review checks a sample of rewritten claims against `re/decomp.mjs <key>` output, to confirm the cited key is the function the old address named.
- [ ] **Step 1e: The API reference: every interface documented without its implementation** (user, 2026-10-30; **generating it and filling its comments moved to after the playtest by the user, 2026-11-03**, as Doxygen isn't available on the build machine. Done before that: each `sdk/systems` page split, the Doxyfile, groups, script, target and validation step. `tools/docs.ps1 -IfConfigured` skips without Doxygen, so make it required when this lands). An author learns everything needed to use a declaration from its reference entry alone, as with other SDKs (the Unity Scripting API, Steamworks, the Windows SDK reference). The C and C++ standard is used: Doxygen comments, generated into a browsable reference.
  - **What is documented.** Every public declaration: the `phasmo::` wrappers, `pg::api::`, `sdk/api/content.h`, and the parts of `core/mod_api.h` a mod sees.
  - **What each comment gives**, where it applies:
    - `@brief`: one sentence, in game terms, on what it is or does.
    - `@param` for each parameter: its meaning, units (metres, seconds, degrees Celsius, sanity 0 to 100), valid range, whether null is allowed, and for a `void*` the game type expected.
    - `@return`: meaning and units, and the value given when the entry did not resolve or the object is null.
    - `@pre`: the thread (main thread), the game phase (menu, lobby or contract), and who may call it (the host only, or every client).
    - **Networking:** whether other players see a write or call, whether the game syncs it, and who has authority over it.
    - **Side effects:** what else the game does. For a field write that skips the game's behaviour, the comment names the method to call instead (for example `isHunting.set` does not start a hunt).
    - **Lifetime:** for a returned managed reference, that it is valid this frame only and is kept in a `pg::api::Held`.
    - **For a method mods hook:** when the game calls it, on which clients, how often, what its arguments hold at that moment, and what follows when the detour does not call `original`.
    - **Failure and availability:** what happens when the entry is unresolved, and whether it is required or optional (`available()`).
    - `@since` for the SDK version that added it, `@deprecated` with the replacement, and `@see` for related declarations and the guide.
    - `@code` with an example, for every type and every common call.
  - **No implementation in the reference.** How an entry is found (shape, facts, hints), what the decompiled code does inside, and obfuscated names stay out of it.
    - That material lives in the `pg::map` entry notes and in maintainers' pages, `sdk/internals/<area>.md`, which cite keys as Step 1d requires.
    - Each `sdk/systems/*.md` page splits in two. The author-facing concept page keeps what the system is, its lifecycle and which declarations to use. Its RE content moves to `sdk/internals/`.
  - **Generation.** Doxygen at a pinned version, used for the docs build only, run by a `docs` CMake target and by `validate.ps1`. It outputs HTML with search into the build directory, which is never committed.
    - It is grouped by game system (`@defgroup`: ghost, players, sanity, map and rooms, equipment, weather, journal, voice, lobby, content, runtime), with the guides and concept pages as related pages.
    - It excludes `pg::map`, every `detail` namespace and `PG_HOST`-only declarations. (`sdk/internals/` was excluded too, until the
      user included it as related pages on 2026-11-08, since the concept pages link to it.)
    - Warnings are errors (`WARN_IF_UNDOCUMENTED`, `WARN_NO_PARAMDOC`, `WARN_AS_ERROR`). An undocumented public declaration, a missing `@param` or `@return`, or a broken `@see` fails the build. Watch it fail once on an undocumented declaration added in a scratch commit that is then dropped, and record the output.
  - **Versioning.** `sdk/CHANGELOG.md`, in the Keep a Changelog form, records each SDK version's additions, deprecations and removals, under the library-style versioning already decided (additive within a major, deprecate before removal).
  - **Review.** The task review reads a sample of generated pages, one per group. For each, it confirms the page alone answers four questions: how to call it, what comes back, on which clients, and what happens when it fails. It also confirms the page names no implementation detail.
- [ ] **Step 2: Write the guides.**
  - `adding-a-ghost.md` carries a complete small example ghost: one tell, three vanilla evidence and an ability. It is built as `sdk/examples/example_ghost/` by CMake, and loaded in a `content_test` case.
  - `equipment.md` describes the game's equipment system: truck slots, shop listing, price, description, loadout save, use and sync, and how equipment reveals evidence. It ends with the dowsing-rods worked example and what a registration function would need. No such function is added.
- [ ] **Step 2b: Bring the player guide up to date** (`docs/installation.md`, the page players read). Today it describes the old lobby rule (players without a mod staying when every mod works without them) and the old build check. Rewrite those parts to what Tasks 9 to 13b built: every player needs the same mods, a vanilla host or a public lobby switches mods off, a joiner missing a mod is removed with the list, extra mods are switched off, dependencies. Say plainly what happens after a game update. No address or offset appears.

- [ ] **Step 3: Write the full test list** the user runs in game (user, 2026-11-01). The user does this testing, in place of the old roster audit, so nobody else runs it. Publish it as a page the user can tick off as they go. Bring in every smoke step from the gitignored task reports (12a to 12f and on), which are lost otherwise. Besides the items below, it has one section for **every custom ghost** and one for **every vanilla ghost Ghost Tells changes**, read from the code rather than a fixed list. Each section gives:
  - how to set the ghost up, including on zero evidence;
  - its evidence, including any priority or forced evidence;
  - every tell: what it does, under which conditions, and how a player sees it;
  - its strengths and weaknesses as built, and its journal text, to read for accuracy and tone;
  - whether it can be told apart in single player, at zero evidence, on every map, in every weather, with any tell that needs rain, a door, a closet, a second floor or a second player called out;
  - the player-controlled version: abilities, HUD, target and kill rules, and any allowed difference from the AI.

  It also covers:
  - every task's live checks;
  - 2a's untested parts: `scene_engine`, `patch_site` and the shutdown restore, the `static_access` throwing class constructor, and wrong-kind service calls;
  - the Mimic copying a custom ghost;
  - joining a vanilla host's lobby: mods off with the notice, back on after leaving (Task 13);
  - a non-English journal;
  - lobby journal: custom ghost pages, evidence rows and lines, before and after a contract (Task 9b); and a player with other mods joining the lobby puts every journal back as the game made it, the Ghosts tab's list entries where they were, until they leave.
  - the Draugr (Task 11c): it stays near its room, takes props back to their spots (never one a player carries), goes for the thief first, and its room is colder while angered on the host and on a client (thermometer and breath); with Freezing withheld its readings may still go below freezing (a tell), and the room's own temperature comes back when the anger ends.
  - the Brownie (Task 11d), on the host and on a client, AI and player-controlled: an unwatched hunting Brownie's steps make no sound, the sound sensor and parabolic microphone never detect them (no reading, no radar point, no playback, no sound-sensor challenge progress), while their UV prints still appear; a watched one is heard only up close and is detected as usual; outside hunts its steps are the game's own; a non-Brownie ghost's steps after a Brownie contract, and after switching the mod off mid-hunt, are detected again.
- [ ] **Step 4: Run** `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass, and the scan reports 0 raw sites.
- [ ] **Step 5: Commit.**

### Task 14b: The public release

The user decided on 2026-11-02 to release publicly straight after Task 14, because many of the tests need other players on the same build. Releases go out from `main` only (`tools/release.ps1`), so this branch merges first, and Tasks 15 to 18 follow the release. Task 15's multiplayer tests run on the released build, its fixes go out as patch releases, and Tasks 17 and 18 continue on a new branch.

The updater moves every existing install to a release without asking, so everyone on v0.1.0 gets this one when it is published, and deleting a bad release rolls them back.

- [ ] **Step 1: The final whole-branch review,** as the plan's closing review, over everything since the branch left `main`. Anything that could crash, trap a player or risk an account is fixed before Step 2.
- [ ] **Step 2: A short solo check by the user** (recommended by the controller; the user may skip it), on a local build that the updater won't replace (`PG_UPDATE_SOURCE`, with the steps in the test list). Read the log, then the consent notice, the Mods panel and a single-player contract. Raise the ban risk first; the user launches the game.
- [ ] **Step 3: Merge to `main`, with the user's go.** `NOTICE` is an add/add conflict: keep the nlohmann line. `tools/payload.psd1` needs `main`'s `LICENSE.txt` and `NOTICE.txt` lines. Run the stage and release tests on the merged result.
- [ ] **Step 4: Release v0.2.0, with the user's go at that moment.** Say in the release notes that the consent notice asks again, that mods run only in private lobbies, and that every player needs the same mods.

### Task 14c: Cache the CI build

The user decided on 2026-11-08, after v0.2.1, that CI gets a build cache **before the next release**. Do it as the next task, ahead of any other work that would end in a release.

**Why CI is slow.** On `windows-latest`, a run's setup (checkout, vpk, Doxygen) takes about 30 seconds. The pack step (`package.ps1`, which runs the whole of `validate.ps1` and then `vpk pack`) takes 10 to 19 minutes (builds #20 to #29). Every run starts on a fresh VM with no `build/` folder and no compiler cache, so all of Ninja's ~481 steps compile cold on 4 vCPUs. Locally `build/` is warm and Ninja rebuilds only what changed.

**What won't work.** Caching `build/` alone doesn't help. `actions/checkout` gives every source file the checkout time as its timestamp, so Ninja sees every input as newer than its restored object and rebuilds everything. The cache has to key on file contents. A compiler cache does that: ccache (4.6 and later) and sccache both support MSVC through `CMAKE_<LANG>_COMPILER_LAUNCHER`. The Release config adds no `/Zi` in `CMakeLists.txt` (checked 2026-11-08), so objects can be cached. If any target needs debug info, it uses `/Z7` (`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`), not a shared PDB.

**Files:** `.github/workflows/build.yml`, `CMakeLists.txt` (the launcher, only when the cache tool is present), `tests/dev_release_test.ps1` or a new `tests/ci_cache_test.ps1`, `docs/releasing.md`.

**Decide with the user before Step 1** (conventions rule: research first, then give the options):
- ccache or sccache. Compare MSVC support, how each stores its cache in `actions/cache`, and how the binary is fetched and checked.
- Whether the release job (the tag) uses the cache, or always builds cold. Content-keyed objects give the same result, but a cold release never depends on an earlier run. GitHub lets a run restore caches from its own ref and the default branch; check what a tag run can see.

**Constraints:**
- The cache only makes compiling faster. It never skips a step. `package.ps1` still runs all of `validate.ps1` in the publishing job, and a release is still packed only from a payload validated in that same job on that exact commit.
- A cache miss, or a cache that fails to restore, still builds correctly, only slower. No check is weakened to make the cache work.
- The key covers the compiler version (the MSVC toolset `vcvars64.bat` picks), the build config and the cache tool's version. Stale entries age out by GitHub's own eviction.
- A third-party action is pinned to a full commit SHA, as `actions/checkout` is. A downloaded binary is checked against its SHA-256 before it runs, as Doxygen is.
- A local build without the tool installed behaves exactly as now.

- [ ] **Step 1: Test first.** Extend the workflow test so it fails until every building job (Validate, Publish the development build, Publish the release, as decided) sets up the cache, pins any action by SHA and checks any downloaded binary by hash. Watch it fail.
- [ ] **Step 2: Add the cache** to the workflow and the launcher to `CMakeLists.txt`.
- [ ] **Step 3: Measure.** Push to development twice: the first run fills the cache, and the second should hit it. Record both pack-step times, and the cache hit rate the tool reports, in the ledger and in `docs/releasing.md`.
- [ ] **Step 4: Run** `.\validate.ps1`. Expected: pass, and a local build without the tool is unchanged.
- [ ] **Step 5: Commit,** and push development (no release needed to prove it).

### Task 15: The user's in-game test (replaces the roster audit)

The user decided on 2026-11-01 to test in game with Task 14's full test list, instead of the planned code-reading audit of every ghost. So no identifiability matrix, text review or parity report gets written, and the test list carries those checks. Nothing here runs without the user, and no agent launches the game. Wait for the user's results before Task 16. Playtesting with friends comes after Tasks 12 to 14, on the public release (Task 14b).

### Task 16: Changes from the user's test

Whatever the user asks for after testing, each one implemented and reviewed before the final whole-branch review.

**Already agreed for Task 16:**
- **The Mimic player cannot keep copying the same type** (user, 2026-11-01). At each native switch the player still picks what it copies (decision E2, an allowed difference), but never the type it is copying now. First reverse engineer how the AI Mimic's random pick treats its current copy and record it in `sdk/systems/ghost-types.md`; the player's choice list leaves out the current copy the same way. If the AI's pick can repeat, the player's rule still leaves it out, and the allowed-differences list in `sdk/systems/ghost-ai.md` says so. It uses the Mimic entries Task 2 mapped and Task 12(c) moved onto the map. Test: `mimic_player_cannot_pick_current_copy`.
- **Old Playable Ghost problems** found during Task 12(c) and left alone there (ledger, ruling of 2026-11-01): the user picks from the list.
- **The investigators' slot shift** (provisional, `sdk/systems/ghost-ai.md`): the user judges it in the test.

### Task 17: The community SDK: every main system, without reverse engineering

Asked for by the user on 2026-10-30. The finished SDK must let a community author write a mod without any further reverse engineering. This task runs after Task 16 and before the final whole-branch review, which covers it too. It takes in what this plan earlier left to stage 3, and the rest of Task 12(c)'s reverse-engineering catch-up: `Loadout.AddToLobby` and `.AddToEquippedLoadout`, `PhotonNetwork.RPC`, `PhotonNetwork.NetworkingClient`, `NetworkedStamina.NetworkSyncSprint`, and the 169 addresses cited in the task reports and reviews (about 100 unmapped), each mapped only if something relied on it and a mod author could reasonably use it.

**Why.** Answering "is the ghost hunting?" took a workaround. The map has `GhostAI.isHunting` but no way to reach the current ghost. `core/game.cpp` reads that ghost raw at `LevelController+0x40`, so an SDK-only mod had to hook `GhostAI.Awake`, and that misses a ghost that spawned before the mod started. A question an author would commonly ask, which needs RE or a workaround to answer, is a gap this task closes.

**Coverage.** Read access, plus write access wherever the game itself changes the value (through its own method, never by writing a field that skips the game's behaviour):
- **The ghost:** the current ghost; its AI and state; hunts; its type, identity and behaviour type; its favourite room and current room; ghost events and interactions; speed.
- **Players:** the local player and every player; alive or dead; position; room; held and inventory items; Photon actor number; in the truck or not.
- **Sanity:** each player's sanity, the average, and the game's drains and restores.
- **Map data:** the current map and its name and size; the truck; the fuse box; doors; lights and switches; hiding places; the cursed possession.
- **Rooms:** every room, with its name, bounds and temperature; which room a player or the ghost is in; the favourite room.
- **Equipment:** every equipment class, held or placed, on or off, each sensor's reading, and the evidence it reveals. Registering new equipment stays documented only (Task 14's `equipment.md`).
- **Weather:** the current weather and its effects (rain, lightning, wind, fog), from the game's own weather system.
- **The journal:** evidence marks, the ghost guess, pages, and photos with their rewards.
- **Voice chat:** who is speaking and how loud, from the taps the game already has, and the game's own speech recognition (Spirit Box, Ouija, the ghost hearing its name). Anything that keeps speech sets `PG_MOD_RECORDS_VOICE`.
- **Everything the bundled mods use.** Each entry Tasks 2 to 13b mapped for a bundled mod is public, documented and reachable, never held back for one mod.
- **Gaps found while doing this**, at least:
  - the contract's lifecycle (loaded, started, ended), hunts starting and ending, ghost events and a player's death, as documented hook points on the game's own methods;
  - difficulty and contract settings;
  - the game's UI widgets mods reuse (buttons, toggles, pages), under the vanilla-UI-only rule;
  - doors, lights, the fuse box and cursed possessions as objects a mod acts on;
  - **a ghost that leaves the building** (user, 2026-11-01: one ghost type will do this as its unique tell), in a new `sdk/systems/navigation.md`:
    - **What holds a ghost in.** Only the walkable mesh does. `GhostWanderState.PickDestination` samples around the ghost with `NavMesh.SamplePosition` (mask 1, area 0) and has no room test. The ghost's agent is on agent type 0, and each map's mesh for that type is baked. On all 13 maps it ends 3 to 22 m short of the truck (read from the scene files, build 25811160), so most of the outdoors has nothing to walk on. Outdoor maps such as Camp Woodwind and Maple Lodge have one connected mesh across the site.
    - **Map:** `NavMeshSurface` (constructor, `BuildNavMesh`, `AddData`, `RemoveData`, `navMeshData`, and the setters for `collectObjects`, `center`, `size`, `useGeometry`, `layerMask`, `agentTypeID` and `defaultArea`); `NavMeshLink` (constructor, `startPoint`, `endPoint`, `width`, `bidirectional`, `agentTypeID`, `area`, `UpdateLink`); `NavMeshAdder` (the game's own baked mesh loader, which `NavMesh.AddNavMeshData` is called from); `NavMesh.RemoveNavMeshData`, `Raycast`, `FindClosestEdge` and `CalculatePath`; `NavMeshAgent.get_pathStatus`, `ResetPath`, `set_isStopped` and `set_updatePosition`; `RoomKind.exterior`, `RoomKind.exteriorRoom` and `RoomKind.insideRoom`; `LevelRoom.OnTriggerEnter`; `LevelController.SyncCurrentGhostRoom`. `NavMeshAgent`'s `isOnNavMesh` and `areaMask` wrappers are not in the type dump, so look for them as icalls by name or do without them.
    - **Document:**
      - How to add ground: a `NavMeshSurface` built at runtime from physics colliders (players walk that ground, so colliders exist), for agent type 0, area 0, joined to the baked mesh by a `NavMeshLink` because separately built meshes do not connect by themselves. Nothing in the game calls `BuildNavMesh`, so mark this `Inferred` until a mod has run it in the game.
      - Add the ground only on a contract with that ghost type, because the wander picker has no room test and any ghost would otherwise roam onto it. Find out which clients run the ghost's agent, and add the ground on those.
      - What the game does with a ghost outdoors. `LevelRoom.OnTriggerEnter` moves the ghost's room only for a room whose kind is not `RoomKind.outsideMap`, so with no trigger the ghost's room stays the last one it entered, and that room's temperature behaviour stays with it. The hunt never goes for a player whose room is `RoomKind.outsideMap`, wherever the ghost stands.
      - The file format used for the measurements: a tile is a 72-byte header (poly count at +20, vert count at +24), then 12-byte verts, then 32-byte polys (six 16-bit vert indices, six 16-bit neighbours, flags at +24, vert count at +28, area at +29).
    - **Rules that carry over:** the player version uses the same ground as the AI (ai-player-same-rules), so Playable Ghost's steering and tether must be checked against it when that ghost is built.
    - **Hunts outdoors,** for Task 18: map and document every check that keeps a hunt from a player outdoors, what shuts and locks the front door in a hunt, and how the Deogen knows where players are. Establish which room a player standing outdoors has: the SDK's notes call `RoomKind.outsideMap` the truck, but `LevelController.outsideRoom` and `LevelController.IsPointInOutsideRoom` suggest it may cover more.
    - **Test:** every entry above resolves in gamecheck. The ghost itself is Task 18.
- **The loader's services every mod would otherwise invent** (user, 2026-10-31):
  - **Settings.** A mod declares typed settings with a default and a description, and reads them back, as BepInEx's `Config.Bind(section, key, default, description)` and SMAPI's `ReadConfig`/`WriteConfig` do. They are saved per mod in `%LOCALAPPDATA%\PhasmoMods\config\<mod id>.json`, as BepInEx saves `config\<GUID>.cfg`, and edited in the Mods panel with the game's own widgets. The spec answers one question those loaders do not face: how a setting that changes game rules agrees across a lobby. Show the user the answer with the spec.
  - **A data folder.** `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`, which updates never touch, as SMAPI gives each mod `DirectoryPath`.
  - **Frame time.** The game's own frame time and unscaled time, so no mod measures its own: Ghost Tells clamps `GetTickCount64` itself today. `pgmod_tick`'s signature does not change, since 2.x mods must keep loading. A new contract member answers it.
  - **Log levels.** Trace, debug, info, warn, error and alert, as SMAPI's `LogLevel` has, in a new contract member. `log(line)` stays and means info.

**Shape.** It is game-shaped: map entries under readable names, as the 2026-10-23 design says. Where a common question needs several entries joined (the current ghost, the local player, the room a player stands in), `sdk/api/` gets a small documented helper built only from map entries (for example `phasmo::current_ghost()`). Lifecycle hook points are thin wrappers over hooks of the game's own methods, each proven by reverse engineering. Every rule still holds: proof by RE for every entry and behaviour, counts and names read at runtime, the loader owns all logistics, and vanilla UI only. The new `sdk/systems` pages and notes cite map keys, never addresses or offsets (Task 14 Step 1d), and the extended scan covers them. Every new wrapper uses typed handles (Task 14 Step 0b), and a helper returns one (`phasmo::current_ghost()` gives a `phasmo::GhostAI::Ref`). Every new public declaration is documented to Task 14 Step 1e's reference standard: in its group, with `@since` and an example. The docs build, with warnings as errors, passes after each area group.

**Files:**
- `docs/superpowers/specs/<date>-sdk-community-coverage.md`.
- New and extended headers in `sdk/map/` and helpers in `sdk/api/`, with their tests.
- `sdk/systems/<area>.md` for each area.
- `sdk/guides/reading-the-game.md`.
- `sdk/examples/hunt_watch/`, built by CMake.
- `tests/coverage_questions_test.cpp`.
- `sdk/README.md`, with "What exists today" brought up to date.
- The smoke list.

- [ ] **Step 1: Write the coverage spec.** Per area, list:
  - what the map already has;
  - what is to be mapped, read or write;
  - the RE needed;
  - the helpers;
  - the questions authors will ask, each with the snippet that answers it.

  Add the gaps found. Show the user and wait for approval before Step 2.
- [ ] **Step 2: Map, prove and document each area.** The controller dispatches one area group per round, in the spec's order, starting with the basic lookups (the current ghost, the players, the map and rooms). Each entry carries its facts, a fake-backend test and an expectation line. Each area gets its `sdk/systems/` page.
- [ ] **Step 3: The coverage gate.** `tests/coverage_questions_test.cpp` compiles and runs every question snippet from the spec against the fake backend, using only `sdk/` headers. Adding a question means adding its snippet.
- [ ] **Step 4: The example and the guide.**
  - `sdk/examples/hunt_watch/` is a complete SDK-only mod. It logs hunts starting and ending, and the room and sanity of each player when a hunt starts.
  - `reading-the-game.md` walks through it.
- [ ] **Step 5: Add the smoke list's live checks** for each area. Run `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Expected: pass.
- [ ] **Step 6: Commit,** once per area group.

### Task 18: The ghost that leaves the building

**Held: do not start until the user says so (user, 2026-11-01).** It needs Task 17's navigation entries. The user decides then whether it lands before or after this branch merges.

**The design (user, 2026-11-01).** One ghost type can leave the house, as its unique tell, and that makes it extremely dangerous:
- **It walks outdoors,** on ground added as Task 17's `sdk/systems/navigation.md` describes, only on a contract with this ghost.
- **Its hunts can kill players outside the house.**
- **The front door still slams shut when a hunt starts, as usual, but does not lock.**
- **A player outside during a hunt draws it at once.** It becomes fast and goes straight to that player's location, as the Deogen always knows where players are.

**Decide with the user before Step 1:**
- Which ghost this is: a new custom ghost or a vanilla type changed through Ghost Tells.
- Whether players inside the truck can be killed too, or the truck stays safe. Recommended: the truck stays safe, otherwise players have nowhere to escape to.
- How fast "fast" is.
- What leaving the house means on maps that are mostly outdoors (Camp Woodwind, Maple Lodge, and any others Task 17's notes show), and how the tell stays unique there.
- What it does when players are both inside and outside. Recommended: it goes for the outdoor player, since the design says it goes immediately.

**Rules that apply:**
- **Proof first.** Every mechanic comes from the game's own code, proven by RE, and each useful member becomes a map entry. The design above is the user's; how it is carried out is not to be invented.
- **Same rules for AI and player.** A player controlling this ghost gets the same outdoor ground, kills, door behaviour and speed, and is shown the outdoor player's location. Any difference needs the user's explicit OK.
- **Identifiability.** The ghost stays uniquely identifiable in single player, with zero evidence, on every map and in every weather.
- **Network safety.** Door changes go out only from the client the game's own sender check accepts (`sdk/systems/photon.md`). A difficulty that keeps doors open in hunts (`Difficulty.huntDoorsStayOpen`) still does.
- **Journal.** Any journal page or row this ghost adds also shows in the lobby journal.

- [ ] **Step 1: Prove the mechanics.** From Task 17's notes and any further RE, establish:
  - each check a kill outdoors has to pass, including the hunt's target tests, its sight and hearing, and the kill itself;
  - how `Door.HuntingCloseDoor` shuts and locks, and the way to shut without locking or to unlock straight after, without a visible or audible glitch;
  - how the Deogen finds players and how its speed is set, including `GhostHuntState.RefreshBase` and `GhostHuntState.UpdateSpeed`;
  - which clients run the ghost's agent, and so which need the added ground.
- [ ] **Step 2: Add the rules to its mod, with offline tests:**
  - `outdoor_ground_only_on_this_ghosts_contract`
  - `hunt_goes_for_a_player_outdoors`
  - `player_outdoors_draws_it_at_once_and_fast`
  - `no_player_outdoors_leaves_the_hunt_as_usual`
  - `front_door_shuts_but_stays_unlocked`
  - `doors_stay_open_on_a_difficulty_that_keeps_them_open`
  - `other_ghosts_keep_their_own_door_and_target_rules`
- [ ] **Step 3: The player version,** in Playable Ghost: steering and tether on the added ground, and the outdoor player's location in the player's view.
- [ ] **Step 4: Update the docs and the test list.** The ghost's journal entry and tell, `sdk/systems/navigation.md` with what was proven, and a section in the in-game test list covering every map type, a player outdoors, a player in the truck, and the runtime-built ground itself.
- [ ] **Step 5: Run `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`.** Expected: pass. Commit.
- [ ] **Step 6: The user's in-game test,** which is the first proof that the added ground works. Raise the ban risk first, and the user launches the game.

## The plans that follow

- **2c, safety switch-over:** what the stage 2a plan listed, less the source scan, which Task 14 enforces here.
- **Stage 3, the remaining main systems,** is folded into Task 17 (user, 2026-10-30). The lobby and menus in full stay for stage 3 where Task 17's spec does not cover them.
- **2d, outside authors**, as the stage 2a plan listed. The SDK zip ships only after Task 17 (user, 2026-10-24, moved from stage 3 on 2026-10-30). The generated reference is built in Task 14 Step 1e, and 2d packs it into the zip. 2d also adds (user, 2026-10-31):
  - **Licensing for authors.** The project is Apache-2.0, as MelonLoader is. The SDK's headers compile into a mod's DLL, so the zip and the template mod carry `LICENSE` and `NOTICE`, and the guide tells authors to ship them with their mods. The template mod's own files are the author's to license as they choose.
  - **A changelog for players,** `CHANGELOG.md`, in the Keep a Changelog form, with each release's notes taken from it.
  - **Style files.** `.editorconfig` and `.clang-format`, written to match the code as it is, with a CI check. No reformatting of existing code beyond what they need.
  - **`CONTRIBUTING.md` and GitHub issue templates,** with one for a bug report that asks for the log and the loader's version.
