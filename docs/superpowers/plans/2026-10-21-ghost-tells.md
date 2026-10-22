# Ghost Tells Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new suite mod, Ghost Tells, giving the Spirit, Mare, Shade, Yurei, Goryo and Onryo three new tells each, built from the game's own mechanics.

**Architecture:** One DLL (`mods/ghost_tells`) like the suite's other mods: a `mod.cpp` that decides per contract whether the tells are on, one game-side file per ghost, a pure `*_rule.h` per ghost holding every timer, roll and threshold (tested offline), and a shared gate (`ChangeState`/`TryInteract`) and roll (`Random.RandomRangeInt`) detour. The host runs every rule that changes what the ghost does; clients show the visual-only effects from the host's messages on channel 6.

**Tech Stack:** C++20, MSVC, CMake + Ninja (`.\build.ps1`), IL2CPP API + the host's MinHook (`core/hooks.h`), offline tests with `tests/check.h`.

**Spec:** `docs/superpowers/specs/2026-10-21-ghost-tells-design.md`

## Global Constraints

- Game build 25811160 only; every RVA and offset comes from the spec or is read again before use.
- Mod id `ghost-tells`, name `Ghost Tells`, channel 6, flags `PG_MOD_VANILLA_PEERS`.
- On only in a private room where every client runs the mod; decided per contract, held.
- Rules key on the behaviour type `GhostInfo +0x2C`: Spirit 0, Mare 6, Shade 8, Yurei 10, Goryo 14, Onryo 16.
- The host (`game::is_master()`) makes every change to the ghost, doors, lights and flames.
- Every detour body holds `hooks::Guard`; nothing thrown leaves the mod (`pg::mod::guarded_*`).
- Managed objects kept across frames are `il2cpp::Held` (tests/held_scan_test.cpp checks the dir).
- Player-facing copy says "players", has no em dash, and is English.
- Nothing is placed in the game folder and the game is never launched by the plan; every live check
  becomes a FINDINGS entry.
- No git repository: the plan's commit steps are replaced by the verification step.

## Review Focus

- A door, switch, flame, model copy or room kept after the contract unloads: every module's `reset()`
  drops them, and pointers are checked with `game::alive` before use.
- Leaving a contract mid-rule: a door the Yurei locked is unlocked, flames get their own light and
  scale back, the Goryo's figure is destroyed (`teardown` paths, Yurei `Hold::pending`, Onryo
  `Flicker::reset`, Goryo `CameraShow::reset` tests).
- A hunt or event starting while a rule runs: the Spirit's and Shade's holds end when the hunt ends,
  the Goryo's figure stops when it appears or hunts, the Yurei's draw never fires in a hunt
  (`IncenseStop`, `Falter`, `CameraShow`, `Draw` tests below).
- A non-host client running a rule twice: only the host acts; clients send what they saw
  (`tells_net.h` round-trip tests).
- A Mimic changing type mid-contract: modules reset when the behaviour type changes (each module's
  `tick_host` compares the type it last saw).

---

### Task 1: The mod, its switch, its messages and its journal lines

**Files:**
- Create: `mods/ghost_tells/mod.ini`, `mods/ghost_tells/mod.cpp`, `mods/ghost_tells/tells.h`,
  `mods/ghost_tells/tells_net.h`, `mods/ghost_tells/journal_text.h`, `mods/ghost_tells/journal.h`,
  `mods/ghost_tells/journal.cpp`, `mods/ghost_tells/world.h`, `mods/ghost_tells/world.cpp`,
  `tests/ghost_tells_test.cpp`, `docs/ghost-tells.md`
- Modify: `CMakeLists.txt` (new `GhostTells` target with `mods/custom_ghosts/apparition.cpp`, test
  include dir, `pg_test(ghost_tells)`), `tools/payload.psd1` (two entries), `tests/held_scan_test.cpp`
  (add `"mods/ghost_tells"`), `core/services.h` (new service), `mods/playable_ghost/custom_link.cpp`
  (`held_back` also asks it), `docs/installation.md` (a row in the mod table)

**Interfaces:**
- Produces: `namespace pg::tells` constants `kSpirit 0, kMare 6, kShade 8, kYurei 10, kGoryo 14,
  kOnryo 16`, `bool told(int type)`, and the shared timing
  helpers `struct Edge { bool rose(bool now); }` and `struct Timer { void start(float s); bool
  step(float dt); bool running() const; void stop(); }` (tells.h).
- Produces: `tells_net.h` message kinds `kName 1` (client to host), `kTrap 2` (client to host),
  `kGoryoShow 3` (host to all, u8 on), `kOnryoCount 4` (host to all, u8 count), `kOnryoSpite 5`
  (host to all, i32 view, u16 tenths left); `std::vector<uint8_t> encode_*(...)`,
  `int kind_of(const uint8_t*, size_t)` (0 for anything not ours, prefix `'G' 'T'`),
  `bool read_*(const uint8_t*, size_t, ...)`.
- Produces: `const char* tell_line(int type)` and `int journal_type(std::u16string_view key)`
  (-1 unless `Journal2_Spirit`, `_Mare`, `_Shade`, `_Yurei`, `_Goryo`, `_Onryo`) in journal_text.h.
- Produces: world.h `int behaviour_type(void* ai)` (-1 for none), `bool hunting(void* ai)` (+0xF3),
  `void* favourite_room(void* ai)`, `void* room_at(il2cpp::Vector3)`, `float distance_to_room(void*
  room, il2cpp::Vector3)`, `bool clear_line(il2cpp::Vector3, il2cpp::Vector3)`,
  `il2cpp::Vector3 chest(void* ai)` (feet + 1.2 m), `il2cpp::Vector3 eye(void* player)` (+1.6 m).
- Produces: services.h `PG_SERVICE_GHOST_TELLS "ghost-tells.v1"`, `struct PgGhostTellsService {
  uint32_t version; bool (*heldBack)(int kind); }` (kind 0 interact, 1 event), version 1.
- Produces: mod.cpp `bool on()` exported to the modules through `tells.h` as
  `bool tells_on()` (held decision for this contract).

- [ ] **Step 1: Write the failing tests** in `tests/ghost_tells_test.cpp`: `told(0)`, `told(16)`,
  `!told(24)`, `!told(1)`; every `encode_*` round-trips
  through its `read_*` and `kind_of` (and `kind_of` of `{'C','G','F',1}` is 0, of a 1-byte buffer 0);
  `journal_type(u"Journal2_Goryo") == kGoryo`, `journal_type(u"Journal2_Jinn") == -1`,
  `journal_type(u"Journal_GoryoTitle") == -1`; `tell_line` is non-null for the six and null for
  `1`, and no line contains `"\xE2\x80\x94"` (an em dash) or `"investigator"`.
- [ ] **Step 2: Add the CMake test target, build, run** `build\ghost_tells_test.exe`; expected: it
  fails to compile (headers missing).
- [ ] **Step 3: Write the headers and the mod.** `mod.cpp` follows `mods/spectral_flame/mod.cpp`:
  `ready()` (private room, every actor `peer_has(kId)`), per-`LevelController` held decision
  `g_on`, `tick()` calling each module's `tick_host` (host) and `tick_all`, `receive()` dispatching
  by `kind_of`, `start()` resolving and creating hooks for every module and registering the service,
  `pgmod_stop` resetting every module. `journal.cpp` chains `LocalisationSystem.GetLocalisedValue`
  (RVA 0x8D1EF0, signature as `registry.cpp`'s `Localised_t`): for a `journal_type` key while
  `tells_on()` and the contract's ghost is any type, return the game's text + `"\n Tells: "` +
  the line. The service's `heldBack` asks the gate (Task 2) and is false until it exists.
  `custom_link::held_back` returns true when either service says so. Journal lines:
  - Spirit: "incense stops it dead in a hunt and silences it for a minute; it knocks when you say its name."
  - Mare: "it puts the lights out before it shows itself, leaves the rooms it hunted through dark, and often switches off a light you have just turned on."
  - Shade: "it vanishes the moment you stare at it, does nothing with two of you nearby, and hesitates when it sees you together."
  - Yurei: "smudged, it shuts itself in; the door its ability slams stays locked for a moment; a door left open in its room draws its ability."
  - Goryo: "a video camera can catch it standing in its room while nobody is near, where it also acts more; it heads home when it loses you far from there."
  - Onryo: "it snuffs a flame lit near it, every flame gutters as its count of snuffed flames rises, and a flame it snuffed will not relight for a while."
- [ ] **Step 4: Build and run** `.\build.ps1` then `build\ghost_tells_test.exe`; expected: no FAIL
  lines, exit 0; `build\GhostTells.dll` exists; `build\held_scan_test.exe` passes.

### Task 2: The gate and the Spirit

**Files:**
- Create: `mods/ghost_tells/gate.h`, `gate.cpp`, `hold.h`, `hold.cpp`, `spirit_rule.h`, `spirit.h`, `spirit.cpp`
- Modify: `mod.cpp` (wire the modules), `tests/ghost_tells_test.cpp`

**Interfaces:**
- Consumes: Task 1's `tells_on()`, world.h, tells_net.h `kName`.
- Produces: gate.h `bool create_hooks()`, `void tick(void* ghost, int type)`, `bool held_back(int
  kind)`, and `using Rule = bool (*)(int kind)` with `void add_rule(Rule)` (each ghost module adds
  its own "held back now" rule; kind 0 interaction, 1 event, 2 hunt); `void around_state(void (*)(void* ai, int state, bool before))` listener list so the Yurei can see `ChangeState(15)`.
- Produces: hold.h `void hold_still(void* ai, bool on)` (hunt state `+0xB0` held at 0 while on, the
  saved base written back when off) and `void reset_sight(void* ai)` (hunt state `+0xB4` = 1).
- Produces: spirit_rule.h `struct IncenseStop { bool step(bool smudged,
  bool hunting, float dt); }`, `struct Quiet { bool step(bool incense, bool hunting, float dt); bool
  quiet() const; }`, `bool says_name(std::string_view phrase, std::string_view name)`,
  `struct Answer { bool hear(bool allowed, float delayRoll01); bool step(bool allowed, float dt); }`;
  constants `kSpiritStopSeconds 3`, `kSpiritQuietSeconds 60`, `kSpiritHearRange 10`,
  `kSpiritAnswerCooldown 20`, `kSpiritAnswerMinDelay 1`, `kSpiritAnswerMaxDelay 3`.
- Produces: spirit.h `bool resolve()`, `bool create_hooks()`, `void tick_host(void* ghost, int type)`,
  `void heard_name(int actor)` (host), `void reset()`.

- [ ] **Step 1: Write the failing tests:** `IncenseStop` holds 3 s from a smudge in a hunt and not at
  3.05 s; a smudge already set at the start of a hunt does not hold again; a smudge outside a hunt
  never holds; `Quiet` lasts 60 s from incense outside a hunt, does not start from incense in a hunt,
  and a still-set incense flag does not restart it; `says_name("hey thomas are you here", "Thomas
  Miller")`, `says_name("THOMAS MILLER", "Thomas Miller")`, `!says_name("thomasina", "Thomas
  Miller")`, `!says_name("", "Thomas Miller")`, `!says_name("thomas", "")`; `Answer` knocks once
  `1 + 2 * roll` s after a hear, refuses a second hear within 20 s, accepts one after, and drops a
  pending knock when `allowed` turns false.
- [ ] **Step 2: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: compile failure.
- [ ] **Step 3: Implement.** gate.cpp chains `game::change_state()` and `game::try_interact()` as
  `custom_ghosts/restraint.cpp` does: on the host, for the told ghost, `TryInteract` is skipped when a
  rule holds kind 0; `ChangeState` is skipped for 14 (kind 1), 15 (kind 1) and 2 (kind 2) when a rule
  holds; listeners see every `ChangeState` before and after the original. spirit.cpp: smudge flags
  `GhostAI +0xF8` (non-null) and `+0xF5` (bool); `hold_still` while `IncenseStop` holds; the quiet
  rule holds kinds 0 and 1; hooks `PhraseListenerController.OnPhraseRecognized` (RVA 0x2370E00,
  `void(void* self, il2cpp::String* phrase, const void*)`) on every client: when `says_name(phrase,
  GhostInfo +0x48 name)` and the local player is living, within 10 m and on the ghost's floor
  (|dy| < 2.5 m), the host calls `heard_name(local actor)` and a client sends `kName`; the host's
  `Answer` knocks with `ChangeState(9, null, false)` while not hunting, appeared or quiet.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0, no FAIL.

### Task 3: The Mare

**Files:**
- Create: `mare_rule.h`, `mare.h`, `mare.cpp`, `rolls.h`, `rolls.cpp`
- Modify: `mod.cpp`, `tests/ghost_tells_test.cpp`

**Interfaces:**
- Produces: mare_rule.h `constexpr int kMareSwitchOffOneIn = 2`, `int mare_roll(int fresh)` (the value
  the coroutine sees for a fresh roll in `[0, kMareSwitchOffOneIn)`: 1 for 0, else 0), `enum class
  Appearance { Appear, DarkenFirst, Withhold }`, `Appearance mare_appearance(bool roomLit, int
  litSwitches, int usableLitSwitches)`, `struct RoomTrail { void pass(int room); const
  std::vector<int>& rooms() const; void clear(); }`, `constexpr float kMareTrailStep = 0.5f`.
- Produces: rolls.h `bool create_hooks()` and `using Roller = bool (*)(void* caller, int min, int
  max, int& result)` with `void add_roller(Roller)`: the first roller returning true replaces the
  result of `Random.RandomRangeInt`.
- Produces: mare.h `resolve`, `create_hooks`, `tick_host(void* ghost, int type)`, `reset`.

- [ ] **Step 1: Write the failing tests:** `mare_roll(0) == 1`, `mare_roll(1) == 0`;
  `mare_appearance(false, 0, 0) == Appear`, `(true, 2, 2) == DarkenFirst`, `(true, 2, 1) ==
  Withhold`, `(true, 0, 0) == Withhold`; `RoomTrail` keeps each room once in order and `clear()`
  empties it.
- [ ] **Step 2: Run** the tests; expected: compile failure.
- [ ] **Step 3: Implement.** rolls.cpp chains `game::random_range_int()` with `_ReturnAddress()`.
  mare.cpp adds a roller: caller in `{0x26FFD10, 0x2CC}`, `min == 0 && max == 7`, the Mare is told
  and on: `result = mare_roll(fresh roll in [0, 2))`. Chains `GhostAI.Appear` (RVA 0x2020FB0,
  `void(void* ai, int form, const void*)`) on the host: not hunting, the Mare, `room_at(ghost)`;
  lit = `LevelRoom.AreRoomLightsOn` (RVA 0x8A9CD0, as kikimora.cpp); switches `LevelRoom +0x20`
  `List<LightSwitch>`, on = `LightSwitch.get_IsSwitchOn`, usable = not `+0x68` (broken); Darken: each
  lit switch `UseLightSwitch(false, true, true)` (RVA 0x26E5510) then the original; Withhold: return
  without it. Trail: every 0.5 s in a hunt `pass(room index)`; on the hunt's end each lit usable
  switch of those rooms is switched off the same way, then `clear()`.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0.

### Task 4: The Shade

**Files:**
- Create: `shade_rule.h`, `shade.h`, `shade.cpp`
- Modify: `mod.cpp`, `tests/ghost_tells_test.cpp`

**Interfaces:**
- Consumes: gate `add_rule`, hold `hold_still`/`reset_sight`, tells.h `Timer`.
- Produces: shade_rule.h constants `kShadeGazeCone 15`, `kShadeGazeRange 20`, `kShadeGazeSeconds
  0.4f`, `kShadeCompanyRange 5`, `kShadeGroupRange 2.5f`, `kShadeFalterSeconds 1.5f`,
  `kShadeFalterCooldown 15`; `struct Gaze { bool step(bool appeared, bool lookedAt, float dt); }`
  (true on the frame it vanishes), `bool crowded(int livingWithinRange)`, `struct Falter { bool
  step(bool hunting, bool groupInSight, float dt); bool holding() const; }` (true on the frame it
  falters). Facing uses `pg::custom::in_view` from `mods/custom_ghosts/brownie_rule.h`.
- Produces: shade.h `resolve`, `tick_host(void* ghost, int type)`, `reset`.

- [ ] **Step 1: Write the failing tests:** `Gaze` does not vanish after 0.39 s of looking, vanishes
  once at 0.4 s, restarts after a look away and when not appeared; `crowded(1)` false, `crowded(2)`
  true; `Falter` falters on a group in a hunt, holds 1.5 s, does not falter again 10 s later, does 15 s
  later, and stops holding when the hunt ends.
- [ ] **Step 2: Run** the tests; expected: compile failure.
- [ ] **Step 3: Implement.** Gaze: while `GhostAI +0xA8` and not hunting, any living player with the
  ghost's chest within `in_view(..., 15, 20)` of their facing and `clear_line(eye, chest)`; on
  vanish, the host calls `ChangeState(0, null, false)` and, if `+0xA8` is still set,
  `GhostAI.UnAppear` (RVA 0x202E140). Company: a gate rule holding kinds 0, 1 and 2 while two or more
  living players are within 5 m of the ghost. Falter: in a hunt, the hunt target (hunt state
  `+0x48`) and another living player within 2.5 m of it, both on a `clear_line` from the ghost's
  eye; on the falter frame `reset_sight`, then `hold_still` while holding.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0.

### Task 5: The Yurei

**Files:**
- Create: `yurei_rule.h`, `yurei.h`, `yurei.cpp`
- Modify: `mod.cpp`, `tests/ghost_tells_test.cpp`

**Interfaces:**
- Consumes: gate `around_state`, rolls `add_roller`, tells_net `kTrap`, Timer.
- Produces: yurei_rule.h constants `kYureiTrapSeconds 90`, `kYureiReseal 3`, `kYureiHold 3`,
  `kYureiDraw 20`, `kYureiDrawCooldown 60`; `struct Seal { void start(); bool active() const;
  std::vector<int> step(const std::vector<bool>& open, float dt); }` (indices to shut now: every open
  door on the first step after `start`, then each door open for 3 s while active); `struct Hold {
  void lock(); bool pending() const; bool step(float dt); }` (true on the frame to unlock);
  `struct Draw { int step(const std::vector<bool>& open, bool mayAct, float dt); void reset(); }`
  (a door index open for 20 s while it may act, at most once per 60 s; else -1).
- Produces: yurei.h `resolve`, `create_hooks`, `tick_host(void* ghost, int type)`, `trapped()` (host),
  `reset()`.

- [ ] **Step 1: Write the failing tests:** `Seal` returns every open door on the first step, then a
  door re-opened at 1 s is shut at 4 s and not before, nothing after 90 s; `Hold` unlocks once 3 s
  after `lock`, `pending()` true until then; `Draw` returns -1 before 20 s, the door at 20 s, -1 when
  `mayAct` is false, -1 again until 60 s have passed, and `reset()` clears its counts.
- [ ] **Step 2: Run** the tests; expected: compile failure.
- [ ] **Step 3: Implement.** Seal: chain the `WhiteSage` handler (RVA 0x8256B0, `void(void* self,
  void* gameObject, void* sensor, const void*)`) on every client; after the original, for a told Yurei
  the host calls `trapped()`, a client sends `kTrap`. Doors: `room_at(ghost)` `+0x28` doors,
  open = not `asterion_rule.h door_closed(leaf angle, Door +0xB0)` (leaf angle as asterion.cpp); shut
  with `Door.GhostForceDoor(false, -1, true)` (RVA 0x264C3C0). Hold: a gate listener marks the
  Yurei's `ChangeState(15)`; a `GhostForceDoor` detour records the first door forced inside it; after
  it, `Door.HuntingCloseDoor` (RVA 0x264CA70) and `Hold::lock`; on unlock
  `Door.ToggleDoorLock(true, true)` (RVA 0x264E530) unless hunting and the door is in
  `LevelController +0xA8`; teardown unlocks a pending door. Draw: may act = idle, wander or favourite
  room state, not trapped, not hunting, `LevelValues +0x23` false; on a draw the host sets the steer
  door and calls `ChangeState(15)`; a roller during that call answers `min == 0 && max == doors`
  with the steer door's index.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0.

### Task 6: The Goryo

**Files:**
- Create: `goryo_rule.h`, `goryo.h`, `goryo.cpp`
- Modify: `mod.cpp`, `tests/ghost_tells_test.cpp`

**Interfaces:**
- Consumes: `pg::custom::apparition` (make, place, set_layer, show, destroy, model_of),
  brownie_rule.h `kBrownieCameraCone`/`kBrownieCameraRange`/`in_view`, tells_net `kGoryoShow`.
- Produces: goryo_rule.h constants `kGoryoAlone 10`, `kGoryoShowMin 30`, `kGoryoShowMax 60`,
  `kGoryoShowSeconds 4`, `kGoryoExtraEvery 20`, `kGoryoFar 12`, `kGoryoHome 3`; `struct CameraShow
  { int step(bool conditions, float dt, float roll01); bool showing() const; void reset(); }` (+1 on
  start, -1 on stop, else 0; waits `30 + 30 * roll01` s of conditions between shows),
  `struct Extra { bool step(bool conditions, float dt); }` (true every 20 s of conditions),
  `struct HomeSearch { bool step(bool hunting, bool hasPrey, bool steered, float distanceToRoom); }`.
- Produces: goryo.h `resolve`, `tick_host(void* ghost, int type)`, `tick_all(void* ghost, int type)`,
  `receive_show(bool on)`, `reset()`.

- [ ] **Step 1: Write the failing tests:** `CameraShow` starts after the rolled wait, stops 4 s later,
  stops at once (-1) when conditions end mid-show, and `reset()` while showing reports not showing;
  `Extra` fires at 20 s and 40 s of conditions, not while conditions are false; `HomeSearch` goes
  home only past 12 m with no prey, keeps going down to 3 m, stops at 3 m, never goes with prey (at
  any distance), stops the moment prey is found, never when steered or not hunting.
- [ ] **Step 2: Run** the tests; expected: compile failure.
- [ ] **Step 3: Implement.** Conditions: told Goryo in `favourite_room(ghost)` (in_room), no living
  player within 10 m, not hunting or appeared (`+0xA8`), not in state 14, and a CCTV (`Equipment
  +0x281` on, found by `FindObjectsOfType(CCTV)` every 2 s) with the ghost's chest in its cone and a
  clear line. Show: the host sends `kGoryoShow`; every client (`tick_all`) makes an apparition copy of
  `model_of(ghost)`, puts it on layer `LayerMask.NameToLayer("LaserGhost")`, places it at the ghost's
  transform each frame and destroys it on stop, on contract end or when the ghost appears or hunts.
  Extra: `game::try_interact()` original through the gate's chain with the ghost's activity.
  Home: the hunt state's target `+0x48`; going home sets `game::set_destination(agent, point)` every
  1 s, point = the favourite room's largest bounds centre snapped by `NavMesh.SamplePosition` (RVA
  0x518F100, as mesmer.cpp); `steered` from the Playable Ghost service.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0.

### Task 7: The Onryo

**Files:**
- Create: `onryo_rule.h`, `onryo.h`, `onryo.cpp`
- Modify: `mod.cpp`, `tests/ghost_tells_test.cpp`

**Interfaces:**
- Consumes: tells_net `kOnryoCount`, `kOnryoSpite`.
- Produces: onryo_rule.h constants `kOnryoReach 4`, `kOnryoSnuffMin 1`, `kOnryoSnuffMax 2`,
  `kOnryoSpite 15`, `kGutterLow 0.3f`, `kGutterSeconds 0.6f`, `kBurnLowLight 0.6f`,
  `kBurnLowScale 0.75f`; `float snuff_delay(float chanceRoll, float delayRoll)` (-1 when
  `chanceRoll >= 0.5`, else `1 + delayRoll`); `struct Flicker { void count(int n); void step(float dt);
  float light() const; float scale() const; void reset(); }`; `struct Spite { void mark(int view,
  float seconds); bool spited(int view) const; void step(float dt); void clear(); }`.
- Produces: onryo.h `resolve`, `create_hooks`, `tick_host`, `tick_all`, `receive_count(int)`,
  `receive_spite(int view, float seconds)`, `reset()`.

- [ ] **Step 1: Write the failing tests:** `snuff_delay(0.49f, 0) == 1`, `snuff_delay(0.2f, 1) ==
  2`, `snuff_delay(0.5f, 0) < 0`; `Flicker` light dips to 0.3 on a rise and is 1 again at 0.6 s, a
  count of 2 burns low (light 0.6, scale 0.75) until a count of 0, a fall to 0 does not dip,
  `reset()` gives 1 and 1; `Spite` holds a view for 15 s, not at 15.1 s, and `clear()` drops all.
- [ ] **Step 2: Run** the tests; expected: compile failure.
- [ ] **Step 3: Implement.** Snuff: chain `FireSource.IgniteFireNetworked` (RVA 0x6526A0, `void(void*
  fire, bool byGhost, void* info, const void*)`) on the host: after the original, the fire lit
  (`+0xB0`), the told Onryo not hunting, within 4 m and |dy| < 2.5 m: schedule
  `BlowOutFire(fire, true)` (RVA 0x652090) after `snuff_delay`. Count: the host watches `GhostAI
  +0x120` and sends `kOnryoCount` on change; every client applies `Flicker` to every lit
  `LevelController +0xC0` fire's `Light` (`+0x38`, intensity relative to the value it had before) and
  flame object (`+0x30`, local scale relative to its own), restoring both on reset. Spite: chain
  `BlowOutFire` on the host: `byGhost` and the Onryo: mark the fire's PhotonView id and send
  `kOnryoSpite`; chain `FireSource.IgniteFire` (RVA 0x6528C0, `void(void* fire, bool, const void*)`)
  on every client: a spited fire returns without lighting.
- [ ] **Step 4: Run** `.\build.ps1; build\ghost_tells_test.exe`; expected: exit 0.

### Task 8: Live checks, documentation and validation

**Files:**
- Modify: `FINDINGS.md` (entry 27: what has never run in a contract, with a pasteable prompt),
  `docs/ghost-tells.md` (final), the spec's status line.

- [ ] **Step 1:** Write FINDINGS 27 listing every live check: each hook's signature and RVA, the
  journal text fitting its page, the hunt-speed hold stopping footsteps, the LaserGhost copy showing
  only on camera feeds, the hunt lock rattling, the phrase hook firing outside hunts, and Playable
  Ghost's held-back prompts.
- [ ] **Step 2: Run** `env -u PSModulePath harness-verify`; expected: the build, every ctest and the
  packaging tests pass.
- [ ] **Step 3:** Read the whole diff line by line; then `/cmh-harness:review`.
