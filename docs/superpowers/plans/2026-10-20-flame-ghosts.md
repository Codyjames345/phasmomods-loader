# Flame Ghosts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ten new custom ghost types (GhostType 36 to 45) that carry Spectral Flame, each with its own strength, weakness and tell, in the Custom Ghosts mod.

**Architecture:** Each ghost's rules are pure, offline-tested arithmetic in a header under `mods/custom_ghosts/` (one per ghost where it has any), and a game file that applies them on the host every frame, following `behaviour.cpp` (speed through the existing `SpeedRule` on `NavMeshAgent.set_speed`), `restraint.cpp` (holding back `TryInteract` and `ChangeState`) and `kikimora.cpp` (moving props). What every client must see (decoys, per-player apparitions, lights) goes over Custom Ghosts' channel from the host. Game facts are read in REA decompiles of slim PE slices before each task's code is written, and recorded beside the constant that uses them.

**Tech Stack:** C++20, MSVC, CMake, MinHook through the host, IL2CPP runtime API, offline tests with `tests/check.h`.

**Spec:** `docs/superpowers/specs/2026-10-20-flame-ghosts-design.md` (and `2026-10-20-spectral-flame-design.md` for the evidence).

## Global Constraints

- GhostType values 36 to 45, in this order: Lithobolia 36, Gremlin 37, Brownie 38, Bogeyman 39, Lethe 40, Mesmer 41, Knell 42, Asterion 43, Tempest 44, Hydra 45.
- Evidence exactly as the spec's evidence-spread table; every combination unique among all 45 types (tested).
- Behaviour type GhostType.None for all, as the existing custom ghosts (`registry.cpp`).
- Vanilla ghosts unchanged; nothing runs unless the haunting ghost is one of these types, on the host.
- Numbers live in `ghosts.h` (or the ghost's rules header) as named constants, tuned later.
- Every rule degrades safely: a binding that fails to resolve leaves that ghost's rule off and logs once; the ghost still plays with its evidence.
- Copy rules (no em dash, no emoji) in every journal string.

## Review Focus

- A ghost type picked by Playable Ghost: the rules that need the AI's choices (Bogeyman's closet search, Hydra's decoys, Knell's cycle) still apply to the speed and to what clients see; nothing assumes the AI chose the destination.
- A hunt that ends early (crucifix, smudge, death of the target): every rule resets its hunt state (speed factor, decoys, door battering, invisibility).
- A contract that ends mid-effect: decoys destroyed, lights and weather restored, no GameObject left behind.
- A client that joins late or misses a message: what it shows comes from the host's latest message (state, not events), so it recovers.
- Two effects on one object (Gremlin switching off a camera Brownie is checking; Asterion jamming a door the Bogeyman opens): the later wins and nothing throws.

---

### Task 1: The ten types as data

**Files:** Modify `mods/custom_ghosts/ghosts.h`, `tests/custom_ghosts_test.cpp`, `mods/custom_ghosts/mod.ini` (description), `docs/custom-ghosts.md` (table, one section per ghost).

- [ ] Tests: `every_custom_ghost_is_identifiable` (45 unique), `the_flame_ghosts_carry_the_flame` (twelve carry 8; each other evidence appears 3 or 4 times beside it), `names_answer_two_digit_keys` (`type_of_title_key("Journal_45Title") == kHydra`).
- [ ] Add the ten `Ghost` entries (name, evidence, description, strength, weakness from the spec; no player ability unless a task below adds one).
- [ ] Run tests; build CustomGhosts.

### Task 2: Knell (42), the speed cycle

**Rules (`knell_rule.h`):** `float knell_factor(float secondsIntoHunt)`: a 2 s cycle, the first second at `kKnellFast` (1.5), the second at `kKnellSlow` (0.35), never above `kKnellMaxSpeed` m/s. Tests: phase, bounds.
**Game:** in `behaviour.cpp` beside the Dullahan: `SpeedRule` on while hunting with `knell_factor`. The tell (an even, metronome footstep) is a toll sound every cycle at the ghost while hunting, played through the ghost's own audio (`GhostAudio` / footstep source found by REA).

### Task 3: Hydra (45), decoys on a smudge

**Facts to read:** the smudge-in-hunt flag (GhostAI +0xF8, WhiteSage) and `GhostModel` visibility in a hunt (`SetVisibility`, renderers +0x30).
**Rules (`hydra_rule.h`):** decoy count per hunt (+1 per smudge, at most 3), decoy paths (a random NavMesh point within 8 m, re-picked on arrival), decoys gone when the hunt ends. Tests.
**Game (`hydra.cpp`):** host spawns decoys as copies of the ghost model's renderers (no colliders, no audio) on a NavMeshAgent of their own; their positions go to every client at 10 Hz (message kind on Custom Ghosts' channel); every client mirrors the real model's flicker onto them. Outside hunts, on an appear event (state 8) a single fading second apparition for 2 s.

### Task 4: Asterion (43), doors

**Facts to read:** `Door` (open/close/lock API used by the ghost and the exit door, its angle, its rigidbody), how the hunting ghost passes a door.
**Rules (`asterion_rule.h`):** per door: open, shut-behind, battering (3 s), jammed open; a door is shut behind the ghost when it is 1.5 m past it in a hunt. Tests for the state machine.
**Game (`asterion.cpp`):** host: doors the ghost passes in a hunt close; a closed door within 1 m on its path stops it (speed factor 0) and plays a knock every 0.6 s for 3 s, then opens it with force and holds it open (jammed) for the rest of the contract.

### Task 5: Lithobolia (36), aimed throws

**Facts to read:** the ghost's throw (GhostInteraction / prop throw force), `PlayerSanity.ChangeSanity`.
**Rules (`lithobolia_rule.h`):** aim velocity for a target point (flat throw with gravity, capped speed), hit test (a prop within 0.7 m of a player's chest while moving faster than 2 m/s). Tests.
**Game (`lithobolia.cpp`):** host hooks the throw: when a player is in line of sight within 8 m, the thrown prop's velocity is replaced by the aim; a hit sends that player's client a message and it loses `kLithoboliaHit` (5) sanity. No line of sight: the vanilla throw.

### Task 6: Gremlin (37), placed electronics

**Facts to read:** `Equipment.Use` networking (who may switch off placed equipment), which classes count (video camera, motion sensor, sound sensor, D.O.T.S., EMF reader, parabolic, thermometer when placed).
**Rules (`gremlin_rule.h`):** eligible (placed, on, no player within 2 m, not held), the chance per interaction roll. Tests.
**Game (`gremlin.cpp`):** on the host's interaction roll (TryInteract), with the ghost in the room of an eligible device, that device is switched off instead of the vanilla interaction (an interaction EMF at it). Player ability "Short out".

### Task 7: Bogeyman (39), closets

**Facts to read:** `ClosetZone` (doors, nav spot), `HidingSpot`, the hunt target field (+0x48).
**Game (`bogeyman.cpp`):** in a hunt with no target, destinations are hiding places nearest first (each visited once per hunt); speed factor 0.8 in rooms whose floor area exceeds `kOpenRoomArea`; outside hunts, a closet or locker door near the ghost creaks open on some interaction rolls.

### Task 8: Brownie (38), unseen

**Rules (`brownie_rule.h`):** seen-by test (in a 50 degree half-angle cone of a player's camera within 15 m, line of sight checked by the caller). Tests.
**Game (`brownie.cpp`):** host: in a hunt, speed factor 0 while seen, 1.6 while not; quiet footsteps (the ghost's footstep source's max distance cut as the Myling's, found by REA); interactions skipped while it or the object is in view of a player or a placed video camera.

### Task 9: Lethe (40), forgetting

**Facts to read:** the journal's media list (photos, videos, recordings: `PhotosManager`, `JournalMedia`), how a media item is viewed, the appear event's position and form, the hunt state's lost-target timer.
**Rules (`lethe_rule.h`):** media fade schedule (3 min unless viewed), replay schedule (each event replayed once, 2 to 4 min later). Tests.
**Game (`lethe.cpp`):** host replays each ghost event at the same place; every client removes faded media of its ghost; hunts give up a lost target after 3 s.

### Task 10: Tempest (44), the storm

**Facts to read:** `WeatherControl`, `RandomWeather`, `LightningController` (how a storm and a flash are made, and synced).
**Rules (`tempest_rule.h`):** storm build from the ghost's activity, hunt allowed only at full storm, visible only within 0.2 s of a flash. Tests.
**Game (`tempest.cpp`):** host raises the weather step by step; thunder at each ghost event; in a storm hunt the model is hidden on every client except during a flash.

### Task 11: Mesmer (41), what each player sees

**Facts to read:** the appear event (model choice, position), the truck sanity monitor (`SanityMonitor` or `HeartRateData` text), room lights (`LightSwitch`).
**Rules (`mesmer_rule.h`):** per-client offset and model index from a seed and the actor number, the monitor's shown sanity (up to 20% higher, never above 100), dim rate. Tests.
**Game (`mesmer.cpp`):** every client shows its own form and offset for the ghost's appear events (cameras and photos see the true one), the truck monitor shows the raised value, and lights in its room dim slowly.

### Task 12: Validation and documents

- [ ] `docs/custom-ghosts.md` describes all fifteen; `FINDINGS.md` records what has not run in the game.
- [ ] Full validation (`validate.ps1` through `harness-verify`) and an independent review.
