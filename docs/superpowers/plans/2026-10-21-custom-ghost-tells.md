# Custom Ghost Tells Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every custom ghost a zero-evidence tell built only from vanilla mechanics, with speeds and footsteps computed by the game itself.

**Architecture:** Speed rules stop scaling the agent's speed and write the hunt state's own inputs (base +0xB0, sight +0xB4), so the game's speed function, footsteps and animator see the real speed. Each ghost's new rule keeps its arithmetic in a `*_rule.h` header tested offline (`tests/custom_ghosts_test.cpp`, `tests/spectral_flame_test.cpp`) and its game calls in the ghost's `.cpp`, hooking the game's own functions by RVA through `hooks::create`.

**Tech Stack:** C++20, MSVC + CMake/Ninja, MinHook through `host/hooks`, IL2CPP field access by offset (`core/il2cpp.h`), offline tests with `tests/check.h`.

**Spec:** `docs/superpowers/specs/2026-10-21-custom-ghost-tells-design.md`

## Global Constraints

- Game build 25811160; every offset and RVA below is for that build and is commented where defined.
- Never invent behaviour that strays from vanilla mechanics; prefer the game's own entry points over new mechanisms.
- No speed caps on custom ghosts. A ghost's speed comes from its own rule only. Speeds are before the game's shared multipliers.
- Player-facing text says "players", not "investigators". No em dashes in copy.
- No custom ghost plays footstep sounds of its own.
- Rules for the Bogeyman's teleport, the Echo's repeat and the Lithobolia's aim act for the AI only (never when `steered`); speeds and footsteps apply to both.
- Any raw pointer kept across frames is an `il2cpp::Held` (`tests/held_scan_test.cpp` fails otherwise).
- There is no git repository: each task ends with the project's validation instead of a commit.

**Commands.** Build and run one suite (PowerShell, from `PlayableGhost`): `.\build.ps1; .\build\custom_ghosts_test.exe` (a failure prints `FAIL <file>:<line>: <expr>` and exits 1). Validate everything (bash, from `PlayableGhost`): `env -u PSModulePath harness-verify`, which must end `passed (exit 0)`. Decompile: `node re/decomp.mjs <out.c> <rva...>` with `GHIDRA_INSTALL_DIR="C:\Program Files\Ghidra\ghidra_12.1.4_PUBLIC"`. Call sites: `node re/callers.mjs <GameAssembly.dll> <rva...>`. Analyse a copy of GameAssembly in a scratchpad, never in the game folder.

## Review Focus

1. The game refreshes the hunt base every 2 s (0x214C2A0); without re-applying the rule afterwards the ghost runs one frame at 1.7 m/s every 2 s. Expected: no blips. (Task 1, refresh detour.)
2. Difficulty ghost speed 50% or 150% and Blood Moon must scale custom ghosts exactly as vanilla ones, never twice. (Task 1 test `own_speed_goes_through_the_games_multipliers`.)
3. A smudge mid-hunt: the game's smudge state wins over every custom steering rule, including the Black Shuck's trail. (Task 5 test `the_shuck_waits_out_a_smudge`.)
4. A followed or picked player who dies, leaves or is in the truck: the Black Shuck drops them, the Bogeyman never picks them. (Task 4 and Task 6 tests on empty and dead candidates.)
5. Footsteps are spawned on every client by RPC, so the Brownie's quiet-step rule must run on every client, not only the host. (Task 2, wired into `brownie::tick_all`.)

---

### Task 1: Speed rules write the game's own hunt speed inputs

Replaces `SpeedRule` and the `NavMeshAgent.set_speed`/`get_speed` detours. Every speed rule returns an `OwnSpeed`; behaviour.cpp writes it into the hunt state each frame and again right after the game's 2-second base refresh.

**Files:**
- Modify: `mods/custom_ghosts/speed_rule.h` (replace `SpeedRule` and `HuntSpeed` with `OwnSpeed`; keep `Holders`)
- Modify: `mods/custom_ghosts/behaviour.cpp`, `mods/custom_ghosts/behaviour.h`
- Modify: `mods/custom_ghosts/ghosts.h` (Dullahan, Draugr rules and journal text), `brownie_rule.h`, `knell_rule.h`, `bogeyman_rule.h`
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `docs/custom-ghosts.md` (speed paragraph and the Dullahan, Draugr, Knell, Brownie rows)

**Interfaces:**
- Produces (speed_rule.h):
  - `struct OwnSpeed { float base; bool keepSight; };` the hunt base to write (m/s, before multipliers) and whether the game's line-of-sight speed-up still multiplies it (false: it is held at 1).
  - `inline OwnSpeed flat_speed(float speed)` returns `{speed, false}`.
  - `inline OwnSpeed scaled_speed(float gameBase, float factor)` returns `{gameBase * factor, true}`.
- Produces (rules): `float dullahan_speed(float base, float angleDegrees)`, `float draugr_speed(float distance)`, `float knell_speed(float base, float seconds)`, `float brownie_speed(float base, bool seen)` (exists).
- Produces (behaviour.cpp, internal): `float game_base()` reads GhostAI +0xC8; `void apply(void* huntState, OwnSpeed s)`.

- [ ] **Step 1: Write the failing tests** in `tests/custom_ghosts_test.cpp`, replacing `a_speed_rule_applies_once_through_the_games_read_back`, `a_rules_own_speed_takes_the_place_of_the_games`, the Brownie "fastest" list, the Knell cap and step checks, the Dullahan cap check and `the_draugr_weakens_away_from_its_room`'s factor checks:

```cpp
// The game's default branch with its multipliers, as RVA 0x2153C20 computes it (test emulation).
float game_hunt_speed(OwnSpeed s, float sight, float bonus, float multiplier) {
  return (s.base * (s.keepSight ? sight : 1.0f) + bonus) * multiplier;
}

void own_speed_goes_through_the_games_multipliers() {
  CHECK(flat_speed(3.0f).base == 3.0f && !flat_speed(3.0f).keepSight);
  CHECK(std::fabs(scaled_speed(kGameBase, 0.8f).base - 1.36f) < 1e-5f && scaled_speed(kGameBase, 0.8f).keepSight);
  CHECK(std::fabs(game_hunt_speed(flat_speed(3.0f), kGameSightMax, 0, 1.5f) - 4.5f) < 1e-4f);  // 150%: once, no sight
  CHECK(std::fabs(game_hunt_speed(scaled_speed(kGameBase, 0), kGameSightMax, 0, 1.0f)) < 1e-6f);  // the Asterion stops
}

void the_dullahan_charges_straight_and_stalls_at_corners() {
  CHECK(kDullahanTurn == 45.0f);
  CHECK(std::fabs(dullahan_speed(kGameBase, 0) - kGameBase * 2.0f) < 0.001f);      // 3.4 m/s straight
  CHECK(std::fabs(dullahan_speed(kGameBase, 180) - kGameBase * 0.25f) < 0.001f);   // 0.43 facing away
  CHECK(dullahan_speed(kGameBase, 90) < kGameBase * 0.5f);
  CHECK(dullahan_speed(kGameBase, 30) > dullahan_speed(kGameBase, 60));
  CHECK(dullahan_speed(kGameBase, 0) < brownie_speed(kGameBase, false));          // the user's ceiling (spec)
}

void the_draugr_crawls_far_from_its_room() {
  CHECK(std::fabs(draugr_speed(0) - 2.8f) < 1e-5f);
  CHECK(std::fabs(draugr_speed(10) - 1.3f) < 1e-4f);      // 0.15 m/s per metre
  CHECK(std::fabs(draugr_speed(17) - 0.3f) < 1e-4f && draugr_speed(40) == 0.3f);
  CHECK(draugr_speed(40) < 0.4f);                          // below the Deogen's crawl
  CHECK(50 - kDraugrAngeredBonus == 35);                   // kept from the old test: hunts from 65% sanity
}

void the_knell_moves_on_a_strict_beat() {
  CHECK(knell_surging(0) && !knell_surging(1.01f) && knell_surging(2.01f));
  CHECK(std::fabs(knell_speed(kGameBase, 0.5f) - kGameBase * 1.75f) < 1e-5f);  // 2.98 m/s, no cap
  CHECK(std::fabs(knell_speed(kGameBase, 1.5f) - kGameBase * 0.35f) < 1e-5f);  // 0.6 m/s
}
```

Keep `the_brownie_works_unseen`'s view checks and its two `brownie_speed` value checks; delete the `fastest[]` list and its loop, and keep `brownie_speed(kGameBase, true) > 2 * 0.4f` with the comment "a slow walk, never the Deogen's crawl". Register the new test functions in `main`.

- [ ] **Step 2: Run them to see them fail**

Run: `.\build.ps1` (PowerShell). Expected: compile errors for `OwnSpeed`, `flat_speed`, `draugr_speed`, `knell_speed`.

- [ ] **Step 3: Implement the rules**
  - `speed_rule.h`: `OwnSpeed`, `flat_speed`, `scaled_speed` as in Interfaces; delete `SpeedRule` and `HuntSpeed`; the header comment explains that rules set the game's own speed inputs (spec, "The shared speed and footstep model").
  - `ghosts.h`: `kDullahanTurn = 45.0f`, `kDullahanStraight = 2.0f`, `kDullahanCornerFloor = 0.25f`, delete `kDullahanMaxSpeed`; `dullahan_speed(base, angle)` keeps the cos² blend and has no `min`. Draugr: replace `draugr_factor` and its four constants with `kDraugrRoomSpeed = 2.8f`, `kDraugrSlope = 0.15f`, `kDraugrFloor = 0.3f` and `draugr_speed(d) = clamp(2.8 - 0.15 * d, 0.3, 2.8)`.
  - `knell_rule.h`: `kKnellFast = 1.75f` (times the base), `kKnellSlow = 0.35f`, delete `kKnellMaxSpeed`, `kKnellStep`, `knell_factor`, `knell_steps`; add `knell_speed(base, seconds)`.
  - `brownie_rule.h`: the `kBrownieUnseen` comment loses "above every other ghost".

- [ ] **Step 4: Run the suite**

Run: `.\build.ps1; .\build\custom_ghosts_test.exe`. Expected: exit 0, no `FAIL`.

- [ ] **Step 5: Wire the game side** in `behaviour.cpp`:
  - Delete `detour_set_speed`, `detour_get_speed`, their hooks, `g.speed` and `kSetSpeedRva`.
  - Add `constexpr size_t kGhostBaseSpeed = 0xC8;` (GhostAI: the 1.7 m/s GhostAI.Awake gives every ghost) and `constexpr uintptr_t kRefreshBaseRva = 0x214C2A0;` (hunt state: copies the base from GhostAI +0xC8 every 2 s).
  - Each `tick` while hunting, compute the rule's `OwnSpeed` and `apply` it: write +0xB0 = `base`; when `!keepSight` write +0xB4 = 1.0f. Rules: Dullahan `flat_speed(dullahan_speed(game_base(), angle))`; Draugr `flat_speed(draugr_speed(distance))`; Knell `flat_speed(knell_speed(game_base(), seconds))`; Brownie `flat_speed(brownie_speed(game_base(), seen))` while `brownie::available()`; Bogeyman in an open room `scaled_speed(game_base(), kBogeymanOpenSpeed)`; Asterion while battering `scaled_speed(game_base(), 0)`; Black Shuck `flat_speed(game_base())`.
  - Detour `kRefreshBaseRva` (`void(void* huntState, const void* method)`): call the original, then if the custom ghost's current hunt state is `huntState` and a rule applies, `apply` the last computed `OwnSpeed`.
  - `sense(kDullahan)`: `100 * own base / game_base()` while hunting, else 0 (update the comment in `core/services.h` to "its speed in percent of the game's base").
  - `behaviour.h`: the header comment describes the new model and the new Dullahan, Draugr and Knell rules.

- [ ] **Step 6: Journal text and docs.** In `ghosts.h`: Brownie strength "A Brownie is very fast in a hunt while nobody looks at it, and its footsteps are hard to hear."; Draugr weakness "A Draugr never leaves its room for good, and in a hunt it slows to a crawl the further it strays from it."; Knell weakness keeps "crawls at a third of its speed". In `docs/custom-ghosts.md`: the speed paragraph describes base writes (no factor on the final speed, no caps), and the Dullahan, Draugr, Knell and Brownie rows carry the spec's numbers. The Brownie row separates its two rules and says "player": "In a hunt it slows to 1.02 m/s while any player looks at it" and "Outside hunts it never interacts or causes an event while a player or a running video camera can see it".

- [ ] **Step 7: FINDINGS 24** gains: no speed blip every 2 s in any custom hunt (the base refresh detour); at 50% and 150% ghost speed and in Blood Moon each custom ghost's speed scales once, as a vanilla ghost's does.

- [ ] **Step 8: Validate**

Run: `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 2: Footsteps through the game's own step system

**Files:**
- Modify: `mods/custom_ghosts/brownie.cpp`, `brownie.h`, `brownie_rule.h`
- Delete: `mods/custom_ghosts/knell.cpp`, `mods/custom_ghosts/knell.h`; remove them from `CMakeLists.txt` and their calls in `mods/custom_ghosts/mod.cpp` (`knell::resolve`, `knell::tick`, `knell::reset`)
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `docs/custom-ghosts.md` (Brownie and Knell rows), `FINDINGS.md` entry 24 (the Knell clip line goes; add the live checks below)

**Interfaces:**
- Produces (brownie_rule.h): `constexpr float kMylingStepQuiet = 8.0f;` and `inline float quiet_step_distance(float defaultDistance)` returning `defaultDistance - kMylingStepQuiet` (the game's Myling rule, 0x243E380). Delete `kBrownieQuiet`.

- [ ] **Step 1: Write the failing test** `the_brownie_steps_quietly_as_a_myling`: `CHECK(quiet_step_distance(20.0f) == 12.0f);`
- [ ] **Step 2: Run it to see it fail.** `.\build.ps1`. Expected: `quiet_step_distance` undefined.
- [ ] **Step 3: Implement** the rule; run `.\build.ps1; .\build\custom_ghosts_test.exe`, expected exit 0.
- [ ] **Step 4: Game side** in `brownie.cpp`: delete `Quieted`, `restore_footsteps`, `each_transform`, the AudioSource bindings and the per-hunt model scan. Detour the footstep play helper `Footstep.<play>` at RVA 0x243E380 (`void(void* footstep, float volume, void* name, const void* method)`): after the original, when the haunting ghost is the Brownie and hunting (GhostAI +0xF3), call `Noise.SetMaxDistance` (RVA 0x245AA40, `void(void* noise, float, const void*)`) on the footstep's Noise (Footstep +0x30) with `quiet_step_distance(field<float>(footstep, 0x48))` (+0x48: the default distance Footstep.Awake saved). The detour runs on every client; `tick_all` only records the haunting ghost and type.
- [ ] **Step 5: FINDINGS 24** gains: Brownie steps reach 12 m in hunts and 20 m outside, heard from a client that is not the host too; a Brownie, Knell and Dullahan's step rate and running sound follow their real speed (steps quicken in a Knell surge); no Knell step plays apart from the game's own.
- [ ] **Step 6: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 3: The ghost's own sight for the Lethe and the Lithobolia

**Files:**
- Modify: `core/game.h`, `core/game.cpp`
- Modify: `mods/custom_ghosts/lethe.cpp`, `lithobolia.cpp`, `lithobolia.h`, `lithobolia_rule.h`, `restraint.cpp`
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `docs/custom-ghosts.md` (Lithobolia row), `ghosts.h` Lithobolia text

**Interfaces:**
- Produces (game.h): `bool ghost_sees(void* ghostAi, void* player);` the ghost's player vision sensor (GhostAI +0xA0) `Sensor.IsDetected` (RVA 0x3D1C2E0, `bool(void* sensor, void* gameObject, const void*)`) on the player's GameObject; false for null or dead.
- Produces (lithobolia_rule.h): `inline int nearest_in_range(const std::vector<float>& distances, float range)` returning the index of the smallest distance not above `range`, or -1.
- Produces (lithobolia.h): `bool intercept_hunt_throw(void* ghost, void* prop);` true when it took the throw over or swallowed it.

- [ ] **Step 1: Failing test** `the_lithobolia_only_throws_at_people`:

```cpp
CHECK(nearest_in_range({}, kLithoboliaRange) == -1);                 // nobody in sight: no throw at all
CHECK(nearest_in_range({9.0f, 12.0f}, kLithoboliaRange) == -1);
CHECK(nearest_in_range({6.0f, 3.0f, 7.5f}, kLithoboliaRange) == 1);
```

- [ ] **Step 2: Run it to see it fail** (`.\build.ps1`: `nearest_in_range` undefined), implement, run the suite (exit 0).
- [ ] **Step 3: `game::ghost_sees`**, resolved in `game::resolve` from the RVA; document that it reads the game's own detection (the same call vanilla Footstep.OnEnable makes).
- [ ] **Step 4: Lethe.** `host_forget_prey` decides `sees` with `game::ghost_sees(ghost, prey)` and drops its raycast and `kEye`/`kChest`.
- [ ] **Step 5: Lithobolia outside hunts.** `visible_target` picks among living players `game::ghost_sees` reports, through `nearest_in_range`. `intercept` returns true (the throw is swallowed) when there is no target, so it never throws at nobody.
- [ ] **Step 6: Lithobolia in hunts.** RE: confirm the hunt update's throw (RVA 0x2147380, the `Prop.TryThrow` call after `GhostActivity.GetPropToThrow`) and whether the throwing state reaches `Prop.TryThrow` too (`node re/callers.mjs ... 2701C50`). Detour `Prop.TryThrow` (RVA 0x2701C50, `void(void* prop, bool, bool, const void*)`): while the Lithobolia hunts on the host, `intercept_hunt_throw` launches the prop at the hunt target (+0x48) with the existing `launch`, or swallows the throw when it has no target. Hook it in `restraint::create_hooks` beside the others.
- [ ] **Step 7: Text.** Lithobolia strength "A Lithobolia throws only at players it can see, and a hit costs more than a fright."; weakness "With nobody in sight a Lithobolia throws nothing at all."; docs row to match, including "in a hunt what it throws flies at its target".
- [ ] **Step 8: FINDINGS 24** gains: no Lithobolia throw with nobody in sight; hunt throws fly at the target; the Lethe drops its target one second after the game's sensor loses them.
- [ ] **Step 9: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 4: Scent rules for salt, water and rain

Pure arithmetic in `trail.h`; the game side comes in Task 5.

**Files:**
- Modify: `mods/custom_ghosts/trail.h`
- Test: `tests/custom_ghosts_test.cpp`

**Interfaces:**
- Produces (trail.h):
  - `struct Breaks { std::vector<Point> salt; std::vector<Point> water; };` salt piles, and running taps, sinks, baths and showers.
  - `constexpr float kWaterReach = 2.0f;` metres: running water washes out scent this close.
  - `void Trail::add(Point p, uint64_t now, const Breaks& breaks, bool rainedOut);` rainedOut (outdoors in rain) or `p` within `kWaterReach` of water: the trail breaks (steps cleared) and no step is laid; salt or water between the last step and `p`: the trail breaks, then `p` is laid.
  - `void Trail::wash(const std::vector<Point>& water);` water that started running: every step up to and including the freshest one within `kWaterReach` goes.
  - `bool scent_reaches(Point from, Point to, const Breaks& breaks);` false when the segment passes within `kSaltReach` of salt or `kWaterReach` of water.
  - `bool next_step(const std::vector<const Trail*>& trails, Point at, const Breaks& breaks, Point& out);` as today, but a step only counts if `scent_reaches(at, step)`.
  - `bool follow(const Trail& prey, Point at, const Breaks& breaks, Point& out);` `next_step` on the prey's trail alone; false means the hound has lost them.

- [ ] **Step 1: Failing tests** `water_and_rain_break_scent` and `scent_does_not_cross_a_break`:

```cpp
trail::Breaks none, tap{{}, {{5, 0, 0}}};
trail::Trail t;
for (int i = 0; i <= 3; ++i) t.add({float(i), 0, 0}, 1000u * i, none, false);
t.add({4, 0, 0}, 4000, tap, false);                       // within 2 m of running water: broken, not laid
CHECK(t.steps().empty());
t.add({8, 0, 0}, 8000, tap, false);
CHECK(t.steps().size() == 1);
t.add({9, 0, 0}, 9000, none, true);                       // outdoors in rain: broken, not laid
CHECK(t.steps().empty());
trail::Trail w;
for (int i = 0; i <= 6; ++i) w.add({float(i) * 2, 0, 0}, 1000u * i, none, false);
w.wash({{4.5f, 0, 0}});                                   // a tap turned on beside the trail
CHECK(w.steps().front().at.x == 8.0f);                    // only steps after the freshest washed one remain

trail::Breaks salt{{{2, 0, 0}}, {}};
CHECK(!trail::scent_reaches({0, 0, 0}, {4, 0, 0}, salt));
CHECK(trail::scent_reaches({0, 0, 1}, {0, 0, 4}, salt));
trail::Trail prey;
prey.add({4, 0, 0}, 1000, none, false);
prey.add({5, 0, 0}, 2000, none, false);
trail::Point next;
CHECK(!trail::follow(prey, {0, 0, 0}, salt, next));      // salt between hound and prey: lost
CHECK(trail::follow(prey, {0, 0, 0}, none, next) && next.x == 4.0f);
CHECK(!trail::follow(trail::Trail{}, {0, 0, 0}, none, next));  // a prey with no trail (dead, gone)
```

Update `the_shuck_follows_the_trail_and_salt_breaks_it` to the new `add`/`next_step` signatures with its existing assertions.

- [ ] **Step 2: Run them to see them fail** (`.\build.ps1`: signature errors), implement, run the suite (exit 0).
- [ ] **Step 3: Validate.** `env -u PSModulePath harness-verify` (behaviour.cpp's `record_trails` passes `Breaks{salt, {}}` and `false` until Task 5). Expected: `passed (exit 0)`.

---

### Task 5: The Black Shuck hunts blind and deaf, by scent

**Files:**
- Modify: `mods/custom_ghosts/behaviour.cpp` (`record_trails`, `tick_shuck`), `behaviour.h`
- Create: `mods/custom_ghosts/shuck_rule.h` (the steering decision, pure)
- Modify: `mods/custom_ghosts/bogeyman.h`, `bogeyman.cpp` (expose the closet search)
- Modify: `mods/playable_ghost/perceive_view.cpp` (the darkened hunt view)
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `ghosts.h` Black Shuck text, `docs/custom-ghosts.md` row, `FINDINGS.md` entry 12

**Interfaces:**
- Consumes: Task 4's `trail::Breaks`, `Trail::add`, `Trail::wash`, `trail::follow`, `trail::next_step`; Task 1's `flat_speed`.
- Produces (shuck_rule.h): `enum class ShuckMove { Native, Follow, Search, Sniff };` and `inline ShuckMove shuck_move(bool smudged, bool hasPrey, bool preyFollowable, bool preyHidden, bool otherTrail)`: smudged → Native (the game's smudge state wins); prey followable → Follow, or Search when the prey is in a hiding place and the trail has brought it there; prey lost or none and another trail in range → Sniff; else Native (it roams).
- Produces (bogeyman.h): `void search_closet(void* closetZone);` calls `ClosetZone.CheckCloset` (RVA 0x262DE80), as the Bogeyman already does.

- [ ] **Step 1: Failing test** `the_shuck_waits_out_a_smudge`:

```cpp
CHECK(shuck_move(true, true, true, false, true) == ShuckMove::Native);   // a smudge wins
CHECK(shuck_move(false, true, true, false, false) == ShuckMove::Follow);
CHECK(shuck_move(false, true, true, true, false) == ShuckMove::Search);  // its trail led to their closet
CHECK(shuck_move(false, true, false, false, true) == ShuckMove::Sniff);  // salt or water lost them
CHECK(shuck_move(false, false, false, false, false) == ShuckMove::Native);
```

- [ ] **Step 2: Run it to see it fail, implement `shuck_rule.h`, run the suite (exit 0).**
- [ ] **Step 3: RE the hunt's senses.** List the hunt state methods (class range 0x213B560-0x2156700, names in `re/names.tsv`) that store to +0x48 (the target) and decompile them; find which come from the vision sensor (GhostAI +0xA0), from hearing (the player noise sensor and GhostHearing) and from electronics, and how the hunt's kill finds its victim (whether it needs +0x48). Record the RVAs as named constants with one-line comments in `behaviour.cpp`.
- [ ] **Step 4: Blind and deaf.** Detour the target writers found in Step 3 so that, while the Black Shuck hunts on the host, the game never sets a target from sight, hearing or electronics. Its target is set only by `tick_shuck`: the player whose trail it follows (so the game's own chase and kill see them), cleared when `follow` fails.
- [ ] **Step 5: Steering.** `tick_shuck` (host, hunting, not steered): gather `Breaks` (salt as today; water: every `Sink` whose running flag is set, found by RE on the `Sink` class) and `rainedOut` (weather LevelValues +0x98 +0x20 is rain 2 or heavy rain 3, and the player stands outdoors, which the builder finds from the level's rooms); `record_trails` calls `Trail::add` with them and `Trail::wash` with water that started running. Then act on `shuck_move`: Follow and Sniff set the destination to `follow`/`next_step`'s point; Search walks to the prey's ClosetZone (Player +0x30) nav spot (ClosetZone +0x48) and calls `search_closet`; Native leaves the game alone. If the game's chase overwrites the destination every frame (check during Step 3), detour `NavMeshAgent.SetDestination` for the ghost's agent while it hunts instead of setting it from `tick`.
- [ ] **Step 6: The ghost player's view.** In `perceive_view.cpp`, `draw_marks(..., hunting && (st.type == kKormos || st.identity == kBlackShuck), now)` with `constexpr int kBlackShuck = 35;`; the scent marks it already shows stay.
- [ ] **Step 7: Text.** Black Shuck strength "In a hunt a Black Shuck follows the exact path its prey walked, round every corner and into hiding places, though it cannot see or hear."; weakness "Salt or running water breaks a Black Shuck's scent, and it loses anyone on the other side."; docs row to match (rain outdoors, taps and showers, smudges stun it, closets do not hide you).
- [ ] **Step 8: FINDINGS 12** gains: it walks past a visible, noisy player whose trail is cut by salt; it finds a player hiding in a closet by their trail; a smudge stops it; a running tap or rain outdoors breaks a trail; a ghost player sees the darkened view with scent marks.
- [ ] **Step 9: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 6: The Bogeyman of folklore

**Files:**
- Modify: `mods/custom_ghosts/bogeyman.cpp`, `bogeyman.h`, `bogeyman_rule.h`, `restraint.cpp`
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `ghosts.h` Bogeyman text, `docs/custom-ghosts.md` row, `FINDINGS.md` entry 24

**Interfaces:**
- Produces (bogeyman_rule.h):
  - `struct Candidate { int prestige; int level; float distance; bool living; };`
  - `inline int youngest(const std::vector<Candidate>& c)`: the living candidate with the lowest prestige, then lowest level, then nearest; -1 for none.
  - `constexpr float kBogeymanLurkReach = 10.0f;` metres from the chosen player a hiding place may be.
  - `inline int lurk_place(const std::vector<Place>& places, Place player)`: the nearest place within `kBogeymanLurkReach`, or -1.
  - `constexpr float kBogeymanClosetDoorShare = 0.75f;` (replaces `kBogeymanCreakChance`).

- [ ] **Step 1: Failing tests** `the_bogeyman_stalks_the_youngest` and `the_bogeyman_lurks_in_the_nearest_closet`:

```cpp
CHECK(youngest({}) == -1);
CHECK(youngest({{0, 50, 3, false}}) == -1);                              // the dead are never picked
CHECK(youngest({{1, 5, 2, true}, {0, 90, 9, true}}) == 1);               // prestige first
CHECK(youngest({{0, 20, 9, true}, {0, 20, 2, true}, {0, 30, 1, true}}) == 1);  // then level, then nearest
CHECK(lurk_place({{0, 0, 12}}, {0, 0, 0}) == -1);                        // none within 10 m: it starts where it is
CHECK(lurk_place({{0, 0, 8}, {0, 0, 3}}, {0, 0, 0}) == 1);
```

- [ ] **Step 2: Run them to see them fail, implement, run the suite (exit 0).**
- [ ] **Step 3: RE the player level.** Find where each player's level and prestige are held for every client (Photon custom properties or the networked Player; start from the `Prestige` and `PrestigeBadge` classes and what the lobby board reads) and add `int player_level(void* player)` and `int player_prestige(void* player)` to `bogeyman.cpp` (local helpers) with the offsets commented.
- [ ] **Step 4: Hunt start.** When the hunt begins (`restraint`'s `detour_change_state` with state 2 on the host, after the original, not steered), take `youngest` over every living player inside, then `lurk_place` near them among the scanned hiding places; if one exists, move the ghost there with `NavMeshAgent.Warp` (RVA 0x518C510, `bool(void* agent, Vector3* position, const void*)`, the call the Twins use) onto the place's nav spot, then open that closet's door with `Door.GhostForceDoor(door, true, kCreakStrength, false)`.
- [ ] **Step 5: Target choice in hunts.** When the hunt's target writers (Task 5, Step 3) set a target and more than one player is detected, replace it with `youngest` among the detected ones (detected = `game::ghost_sees`, Task 3).
- [ ] **Step 6: Closet doors.** `creak` fires with `kBogeymanClosetDoorShare` when a closet is within `kBogeymanCreakReach`.
- [ ] **Step 7: Text.** Bogeyman strength "A Bogeyman's hunts begin from the closet or locker nearest the youngest player, and it goes for them first."; weakness unchanged; the description gains "It always comes for the youngest."; docs row to match.
- [ ] **Step 8: FINDINGS 24** gains: hunts start with a closet door opening next to the lowest-level player; it goes for them when two are in sight; most of its door interactions are closets.
- [ ] **Step 9: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 7: The Echo repeats what players do

**Files:**
- Modify: `mods/custom_ghosts/echo.cpp`, `echo.h`, `restraint.cpp`
- Create: `mods/custom_ghosts/echo_rule.h`
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `ghosts.h` Echo text, `docs/custom-ghosts.md` row, `FINDINGS.md` entry 12

**Interfaces:**
- Produces (echo_rule.h): `constexpr uint64_t kEchoRepeatMs = 60000;` and `class Repeat { void used(void* object, bool inItsRoom, uint64_t now); void* take(uint64_t now); };` only the most recent use in its room counts, it lasts `kEchoRepeatMs`, and `take` hands it out once.
- Produces (echo.h): `bool repeat_pending();` and `bool repeat(void* ghost);` true when it used the switch or door instead of a normal interaction.

- [ ] **Step 1: Failing test** `the_echo_repeats_what_players_do` (objects are distinct dummy addresses):

```cpp
int a, b;
Repeat r;
r.used(&a, false, 0);
CHECK(r.take(1000) == nullptr);                 // outside its room: ignored
r.used(&a, true, 0);
r.used(&b, true, 5000);
CHECK(r.take(6000) == &b);                      // the most recent use
CHECK(r.take(7000) == nullptr);                 // once
r.used(&a, true, 10000);
CHECK(r.take(10000 + kEchoRepeatMs + 1) == nullptr);  // too late
```

- [ ] **Step 2: Run it to see it fail, implement, run the suite (exit 0).**
- [ ] **Step 3: RE the uses.** Find the functions a player's use of a `LightSwitch` and of a `Door` goes through on the host (the networked toggle and the grab or move RPC), and the ghost-side calls that use a given switch (the game's ghost light-switch interaction) and door (`Door.GhostForceDoor`, RVA 0x264C3C0). Record the RVAs as named constants.
- [ ] **Step 4: Record and repeat.** Detour the player uses found in Step 3 to call `Repeat::used` with whether the object lies in the Echo's favourite room (GhostInfo +0x70). In `restraint`'s `detour_try_interact`, when `echo::repeat(ghost)` uses the pending object, skip the original. A pending repeat is never lost to dormancy: `skip` returns false for the Echo while `echo::repeat_pending()`.
- [ ] **Step 5: Text.** Echo strength gains "and it repeats what players do in its room: the switch or door you just used is the next thing it touches."; docs row to match.
- [ ] **Step 6: FINDINGS 12** gains: a light switched or a door moved in its room is the next thing the Echo touches within a minute, with voice chat off.
- [ ] **Step 7: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 8: The Asterion as a battering ram

**Files:**
- Modify: `mods/custom_ghosts/asterion.cpp`, `asterion.h`, `asterion_rule.h`
- Test: `tests/custom_ghosts_test.cpp`
- Docs: `ghosts.h` Asterion text, `docs/custom-ghosts.md` row, `FINDINGS.md` entry 24

- [ ] **Step 1: Update the test** for the Asterion: delete the assertions on `kAsterionPassed` and any slam-behind rule; keep the in-the-way and battering-time assertions.
- [ ] **Step 2: Implement.** Delete `kAsterionPassed` and the slam-behind code in `asterion.cpp` (every `GhostForceDoor(door, false, ...)` that closes a door it passed); the battering and the jam stay. Run the suite (exit 0).
- [ ] **Step 3: Text.** Asterion strength "A closed door never stops an Asterion for good: it batters it down, and the door stays broken open."; weakness "Battering a door down takes an Asterion a few seconds, and it can be heard far away."; docs row to match; `asterion.h` comment drops the slamming.
- [ ] **Step 4: FINDINGS 24** gains: no door closes behind it; each closed door in its way costs it 3 s and stays jammed open.
- [ ] **Step 5: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 9: The Hydra's green Spectral Flame

**Files:**
- Modify: `core/services.h` (custom-ghosts service version 5)
- Modify: `mods/custom_ghosts/mod.cpp` (fills the new field), `ghosts.h` (Hydra description)
- Modify: `mods/spectral_flame/flame_rules.h`, `mods/spectral_flame/flames.cpp`
- Test: `tests/spectral_flame_test.cpp`
- Docs: `docs/spectral-flame.md`, `docs/custom-ghosts.md` Hydra row, `FINDINGS.md` entry 20

**Interfaces:**
- Produces (services.h): `bool (*greenFlame)(int type);` in `PgCustomGhostsService`, version 5: true for the Hydra (45).
- Produces (flame_rules.h): `struct FlameColours { Rgb pale; Rgb deep; };` and `inline FlameColours flame_colours(Rgb deep, bool green)`: blue as today; green swaps the deep colour for `kSpectralGreen` (from the game's assets if one fits, found by the builder; else an HDR green of the same brightness as the deep blue) and its pale for `flame_pale(kSpectralGreen)`.

- [ ] **Step 1: Failing test** `the_hydras_flame_burns_green` in `tests/spectral_flame_test.cpp`:

```cpp
bool same(Rgb a, Rgb b) { return near(a.r, b.r) && near(a.g, b.g) && near(a.b, b.b); }

void the_hydras_flame_burns_green() {
  const Rgb warm{10.7f, 3.6f, 1.5f};  // the candle's own HDR tint
  const FlameColours blue = flame_colours(kGasFlameBlue, false), green = flame_colours(kGasFlameBlue, true);
  CHECK(same(blue.deep, kGasFlameBlue) && same(blue.pale, flame_pale(kGasFlameBlue)));  // other ghosts unchanged
  CHECK(green.deep.g > green.deep.b && green.deep.g > green.deep.r);    // green, not blue
  CHECK(green.pale.g > green.pale.b);                                    // the pale stop is pale green too
  CHECK(near(brightness(green.deep), brightness(kGasFlameBlue), 0.05f));
  CHECK(same(tint(warm, green.pale, green.deep, kFalseCap), green.pale));  // withheld evidence: the cap, pale green
}
```

- [ ] **Step 2: Run it to see it fail, implement `flame_colours`, run `.\build.ps1; .\build\spectral_flame_test.exe` (exit 0).**
- [ ] **Step 3: Game side.** `flames.cpp` reads the ghost's identity type from GhostInfo +0x28 (every client) and asks the Custom Ghosts service's `greenFlame` (absent service or version below 5: blue); every `tint` call takes its pale and deep from `flame_colours`. Custom Ghosts publishes version 5.
- [ ] **Step 4: Docs.** `docs/spectral-flame.md` explains the Hydra's green, including the pale green stop on zero evidence; the Hydra row and journal description mention it (description gains "Its flame burns green.").
- [ ] **Step 5: FINDINGS 20** gains: a Hydra's flame turns pale green at the cap on zero evidence and full green with evidence given; other ghosts stay blue.
- [ ] **Step 6: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.

---

### Task 10: Personality, journal descriptions and difficulty

Text only, after every behaviour task, so each page describes what the ghost now does.

**Files:**
- Modify: `mods/custom_ghosts/ghosts.h` (each `description`; strengths and weaknesses only where a task above left them inconsistent with its behaviour)
- Modify: `docs/custom-ghosts.md` (a Difficulty column, values from the spec's "Personality and difficulty" table, and the row texts checked against the behaviour)
- Test: `tests/custom_ghosts_test.cpp`

- [ ] **Step 1: Failing test** `every_page_tells_its_story`: every `kGhosts` entry's description, strength and weakness are non-empty, contain neither "investigator" nor an em dash (U+2014), and stay within the journal page's length the existing page code allows (use the limit `pages.cpp` already applies; if it applies none, 260 characters for a description and 140 for a strength or weakness, measured on the current longest entries plus a margin).
- [ ] **Step 2: Run it to see it fail** on today's texts ("investigator" appears in several), then rewrite each description from the spec's Root and Personality columns in the existing voice: two sentences at most, the folklore first, then what that means in the house. Run the suite (exit 0).
- [ ] **Step 3: Docs.** The difficulty column and a short paragraph above the table: Easy shows its tell in almost every hunt or event, Medium needs a test or a setup, Hard needs patience or several players comparing notes.
- [ ] **Step 4: Validate.** `env -u PSModulePath harness-verify`. Expected: `passed (exit 0)`.
