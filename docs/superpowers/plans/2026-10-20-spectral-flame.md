# Spectral Flame Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An eighth evidence type, Spectral Flame, on the game's Firelight and house candelabras, as a new mod of the suite that Custom Ghosts requires, with the Draugr and Kikimora moved onto it.

**Architecture:** A new mod DLL (`mods/spectral_flame`) with pure, offline-tested rules (`flame_rules.h`), a game layer that reads every fire source each frame, tints flames and lights, adjusts burn time and sanity and rolls extra blow-outs (host decides blueness and sends it to every client on the mod's channel), and a journal layer that clones an evidence row. The suite gains `requires=` in `mod.ini`, enforced by the host, the launcher and the in-game Mods panel.

**Tech Stack:** C++20, MSVC, CMake, MinHook through the host, IL2CPP runtime API, offline tests with `tests/check.h`.

**Spec:** `docs/superpowers/specs/2026-10-20-spectral-flame-design.md`

## Global Constraints

- Game build 25811160; every offset and RVA below was read in REA decompiles on 2026-10-20 (listed per task).
- Evidence value 8 is Spectral Flame; vanilla ghost types' evidence never changes.
- Only in a private room where every client runs Spectral Flame; otherwise no flame, sanity, burn or journal change.
- No change at all while no ghost type in the contract's collection lists evidence 8.
- Held flames never change; picking a flame up resets its blueness to 0.
- Blueness rises only while the ghost is in the flame's room and the ghost's state is not hunting (2), appear (8) or randomEvent (14).
- Build times for a fully blue flame: Tier 1 60 s, Tier 2 50 s, Tier 3 40 s, candelabra 60 s; fade from full to none 120 s; false-positive cap 1/3.
- A placed flame more than half blue stops counting for the Firelight sanity effect; a blue flame burns down faster (up to 2x at full blue); the bluer above the cap, the likelier the host blows it out (about once per 75 s at full blue while the ghost is in the room).
- Nothing thrown may leave a mod (pg::mod guarded entry points); detours hold a `hooks::Guard`.
- Copy rules (no em dash, no emoji) for every string a player sees.

## Review Focus

- A contract where Spectral Flame runs but Custom Ghosts does not: no journal row, no tint, no blow-outs. Test: `inert_without_a_carrier` in Task 3 (`active_for(collection)` false).
- A flame carried between rooms: blueness resets on pick-up and starts from 0 where it is put down. Test: `held_flames_reset` in Task 3.
- A Nightmare contract that hid Spectral Flame: the ghost's given evidence list lacks 8, so flames stop at the cap. Test: `capped_without_the_evidence` in Task 3.
- Turning Spectral Flame off in the Mods panel or launcher while Custom Ghosts is on: Custom Ghosts turns off too, and turning Custom Ghosts on turns Spectral Flame on. Tests: `requirements_follow_a_toggle` in Task 1.
- A requirement that is installed but fails to start: the dependent mod does not start. Test: `start_order_puts_requirements_first` and `blocked_by_missing_requirement` in Task 1.

---

### Task 1: Mods that require other mods

**Files:**
- Modify: `core/mod_list.h` (Manifest gains `requires`; new helpers)
- Modify: `host/mods.h`, `host/mods.cpp` (Found and Mod carry `requires`; start in requirement order, skip unmet)
- Modify: `host/mods_panel.cpp` (a click switches the mod and its requirements or dependents; the status line says so)
- Modify: `tools/launcher.cpp` (a tick does the same to the checkboxes)
- Test: `tests/mod_list_test.cpp`

**Interfaces:**
- Produces: `struct Manifest { std::string id, name, dll, description, requires; }` (`requires=` line, one mod id or empty; an invalid id is a parse error "Its requires is not a mod id.").
- Produces: `std::vector<std::string> modlist::switched(std::vector<std::string> ids, const std::vector<Manifest>& all, std::string_view id, bool on)`: turning on adds `id` and, transitively, what it requires; turning off removes `id` and, transitively, everything that requires it.
- Produces: `std::vector<std::string> modlist::start_order(const std::vector<std::string>& ids, const std::vector<Manifest>& all)`: the ids with each requirement before its dependents, otherwise in the given order; an id whose requirement is not in `ids` is dropped.

- [ ] Step 1: tests `requires_is_parsed` (a manifest with `requires=spectral-flame` parses, `requires=Bad Id` fails), `requirements_follow_a_toggle` (on custom-ghosts adds spectral-flame; off spectral-flame removes custom-ghosts; unrelated ids untouched), `start_order_puts_requirements_first` ({custom-ghosts, spectral-flame} gives spectral-flame first), `blocked_by_missing_requirement` ({custom-ghosts} alone gives {}).
- [ ] Step 2: build and run `build\mod_list_test.exe`: fails to compile.
- [ ] Step 3: implement in `core/mod_list.h`.
- [ ] Step 4: `host/mods.cpp`: read `requires` from each mod's manifest (beside it, or `<copy>.ini`); `start()` walks `modlist::start_order` and skips a mod whose requirement is not `started`, logging "mods: <id> needs <req>, which is not running; it stays off".
- [ ] Step 5: `host/mods_panel.cpp` `toggle()` uses `modlist::switched`; when the result changes another mod, status reads "<Name> needs <Other>, so both were switched." or "<Name> is needed by <Other>, so both were switched off."
- [ ] Step 6: `tools/launcher.cpp`: on a checkbox click compute `switched` and set every box from the result before saving.
- [ ] Step 7: run the full test suite: passes.

### Task 2: Custom Ghosts requires Spectral Flame and moves two ghosts onto it

**Files:**
- Modify: `mods/custom_ghosts/ghosts.h` (`kSpectralFlame = 8`, `evidence_name(8) == "Spectral Flame"`, Draugr `{kEmf, kFreezing, kSpectralFlame}`, Kikimora `{kUltraviolet, kWriting, kSpectralFlame}`)
- Modify: `mods/custom_ghosts/registry.cpp` (`maybe_custom_key` accepts any two-digit type: "Journal_NNTitle")
- Modify: `mods/custom_ghosts/mod.ini` (`requires=spectral-flame`), `mods/custom_ghosts/mod.cpp` (enabled only while the Spectral Flame service reports active)
- Modify: `core/services.h` (Spectral Flame service)
- Modify: `docs/custom-ghosts.md`
- Test: `tests/custom_ghosts_test.cpp`

**Interfaces:**
- Produces in `core/services.h`: `#define PG_SERVICE_SPECTRAL_FLAME "spectral-flame.v1"`; `struct PgSpectralFlameService { uint32_t version; /* 1 */ bool (*ready)(void); }`: every client in this private room runs Spectral Flame, so evidence 8 can be given.

- [ ] Step 1: tests: `every_custom_ghost_is_identifiable` updated (combinations with 8 allowed; total unique), `the_flame_ghosts_carry_the_flame` (Draugr and Kikimora list 8; Echo, Dullahan, Black Shuck do not), `names_answer_two_digit_keys` (`type_of_title_key("Journal_45Title")` is 0 now, and the registry prefix check accepts "Journal_4" keys; the prefix helper moves into `ghosts.h` as `bool maybe_title_key(std::u16string_view)` so it is tested).
- [ ] Step 2: run `build\custom_ghosts_test.exe`: fails.
- [ ] Step 3: implement; `mod.cpp` `everyone_has_mod()` also requires `ready()` of the service (null service: not ready).
- [ ] Step 4: run tests: pass. Update `docs/custom-ghosts.md` (table, a line on the requirement).

### Task 3: Spectral Flame rules

**Files:**
- Create: `mods/spectral_flame/flame_rules.h`
- Test: `tests/spectral_flame_test.cpp` (+ `pg_test(spectral_flame)` in CMakeLists, include dir `mods/spectral_flame`)

**Interfaces (all in `namespace pg::flame`):**
- `constexpr int kEvidence = 8;`
- `enum class Kind { Tier1, Tier2, Tier3, Candelabra };`
- `struct Inputs { bool lit, placed, ghostInRoom, ghostCalm, hasEvidence; Kind kind; };` (`ghostCalm`: not hunting, appearing or in a random event)
- `float step(float blueness, const Inputs& in, float dt)`: returns the new blueness (0..1); 0 when unlit or not placed; rises by `dt / build_seconds(kind)` while `ghostInRoom && ghostCalm`, else falls by `dt / kFadeSeconds`; capped at `kFalseCap` without the evidence.
- `float build_seconds(Kind)`: 60, 50, 40, 60.
- `constexpr float kFadeSeconds = 120, kFalseCap = 1.0f / 3, kComfortLimit = 0.5f;`
- `bool gives_comfort(float blueness)`: `blueness <= kComfortLimit`.
- `float burn_multiplier(float blueness)`: `1 + blueness` (2x at full).
- `float blow_out_chance(float blueness, float dt)`: 0 at or below the cap, rising linearly to `dt / 75` at 1.
- `bool active_for(const int* evidence, int ghosts)`: true when any of `ghosts` triples (flattened) holds 8.
- `struct Rgb { float r, g, b; };` `Rgb tint(Rgb warm, float blueness)`: `warm` at 0; at 1 the same brightness with the hue of the game's own "Gas Flame Blue" (0, 0.55, 1.0) scaled to the warm colour's largest channel; linear between.
- Message format: `std::vector<uint8_t> encode(const std::vector<std::pair<int,float>>& flames)` and `bool decode(const uint8_t*, size_t, std::vector<std::pair<int,float>>&)`: byte 1 = kind 1, then u16 count, then per flame i32 view id and u8 blueness (0..255). At most 100 flames per message (`kMaxPerMessage`), larger lists split by the caller.

- [ ] Step 1: tests `rises_only_with_the_ghost_in_the_room`, `tier_build_times` (60 s of steps at 1/30 s reaches 1 for Tier 1 but 59 s does not), `fades_slowly`, `held_flames_reset`, `capped_without_the_evidence`, `nothing_rises_in_a_hunt`, `comfort_and_burn`, `blow_out_only_above_the_cap`, `inert_without_a_carrier`, `tint_ends`, `messages_round_trip` (including a truncated payload rejected).
- [ ] Step 2: run: fails to compile.
- [ ] Step 3: implement `flame_rules.h`.
- [ ] Step 4: run: passes.

### Task 4: Flames in the game

**Files:**
- Create: `mods/spectral_flame/flames.h`, `mods/spectral_flame/flames.cpp`

**Facts (REA, 2026-10-20):** LevelController +0x38 room the ghost is in, +0x40 GhostAI, +0x50 LevelRoom[] rooms, +0xC0 List<FireSource> (FireSource.Start adds itself). FireSource: +0x30 flame GameObject, +0x38 Light, +0x40 owner Equipment, +0xB0 lit; `BlowOutFire(bool byGhost)` RVA 0x652090 (master: interaction EMF, RPC to all). Candle: +0x3D0 CandleObject[] (CandleObject +0x18 FireSource), +0x400/+0x408/+0x410 truck/held/world model (PutDown 0/1/2), +0x464 remaining burn seconds; Equipment +0x180 StoreItem (+0x1C tier 0..2; null for house candles). Candlebra +0x20 Candle[]. GhostController static: +0x30 List<int> evidence the ghost gives (difficulty-reduced, host only), +0x90 GhostCollection (+0x18 List<GhostData>, GhostData +0x28 evidence x3). PlayerSanity.Update RVA 0xDA9430 reads PlayerSanity +0x60 List<Candle> (first lit one lowers drain). Flame materials: "Candle Flame" shader `_TintColour`; lantern "Shader Graphs/Lantern Flame" `_Color_2`.

**Interfaces:**
- `bool resolve(); bool create_hooks();` (PlayerSanity.Update detour)
- `void tick(bool enabled);` every frame; `enabled` = every client runs the mod and `active_for` the collection.
- `void receive(int sender, const uint8_t* data, size_t size);` (accepted only from the master actor)
- `void teardown();` restores every touched material colour and light colour.

- [ ] Step 1: implement enumeration (fire sources owned by a Candle that has a StoreItem, or is in a Candlebra), placed state (world model active, or candelabra), room lookup (cached, recomputed when the flame moves 0.25 m or every 2 s, via `game::in_room` over LevelController rooms), host step with `flame_rules.h`, sending at 4 Hz, client apply, visuals (Light colour and flame renderers' tint property, originals cached per object), burn (extra `dt * blueness` off +0x464), host blow-out roll, sanity detour (removes candles whose flames are over the comfort limit from the list for the original call, then restores them in order).
- [ ] Step 2: build: compiles with no warnings.

### Task 5: The journal row

**Files:**
- Create: `mods/spectral_flame/journal.h`, `mods/spectral_flame/journal.cpp`

**Facts:** evidence rows under `Content/GhostHuntingBookContent/Evidence.1/Layout Group` (7 rows; template "Spirit Box"; label child "Text (TMP)"). JournalController +0x1A8 EvidenceButton[], +0x1B0 JournalEvidenceSyncController (+0x30 EvidenceButton[]); EvidenceButton +0x78 evidence, NetworkThreeStateButton +0x58 user-changed Action; SetGhostTypes RVA 0x22C5090.

**Interfaces:** `bool resolve(); void tick(bool enabled); void teardown();` (same lifecycle as `mods/custom_ghosts/pages.cpp`).

- [ ] Step 1: implement: clone the template row into the layout group as the last row, rename "Spectral Flame", set +0x78 to 8, copy +0x58 from the template, set the label text (localiser disabled), append to both arrays, shrink the layout group's spacing so eight rows span what seven did, call SetGhostTypes; every journal on the client, once each.
- [ ] Step 2: build: compiles.

### Task 6: The mod, packaging and documents

**Files:**
- Create: `mods/spectral_flame/mod.cpp`, `mods/spectral_flame/mod.ini`, `docs/spectral-flame.md`
- Modify: `CMakeLists.txt` (SpectralFlame target), `tools/payload.psd1`, `tests/package_test.ps1` / `tests/stage_test.ps1` if they list mods, `FINDINGS.md` (never run in the game), `README.md` if it lists mods

**Interfaces:** mod id `spectral-flame`, name "Spectral Flame", channel 6 (unused in the suite: check every `PgModInfo`), service `PgSpectralFlameService{1, &ready}`.

- [ ] Step 1: implement `mod.cpp` (bind, resolve, hooks, handler, service; tick: `ready` = private room with every actor running the mod; flames and journal enabled when ready and the contract's collection carries evidence 8).
- [ ] Step 2: run `validate.ps1` through `harness-verify`: build, all tests, package and stage tests pass.
