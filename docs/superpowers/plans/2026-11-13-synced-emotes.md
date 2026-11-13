# Synced Emotes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two players dance one emote together: a player near a dancer joins through a prompt over them, glides into
the partner's spot and dances the second part in step; either stopping ends both. Sway is the first.

**Architecture:** The catalogue gains an optional `partner` part per emote. The pure session code (`emote_session.h`)
learns roles and pairing, and the message format learns Join and Accept. The first dancer's client decides who joins,
and every client times both parts from the first dancer's clock. Pure helpers (`sync_rule.h`, `keycap_glyph.h`) hold
the prompt rule, the spot and the glide, so the game glue in `emote.cpp` only reads the game and applies their answers.
The SDK gains what the glue needs, from reverse engineering: moving the local rig, an animation start time, and the
game's glyph for an action's button.

**Tech Stack:** C++20 (MSVC, CMake via `build.ps1`), the repo's offline test harness (`tests/check.h`, fake game in
`sdk/fake`), C# 9 for the Unity tool, Ghidra via `re/decomp.mjs` for reverse engineering.

**Spec:** `docs/superpowers/specs/2026-11-13-synced-emotes-design.md` (approved by the user, 2026-11-13).

**Execution:** inline (superpowers:executing-plans) in the `emotes-2` worktree. The user's standing instruction:
**local-qwen gets every pure-logic task, test file and draft it can do**, with a brief written to the workspace; Opus
writes the briefs, reverse engineers, writes the game glue and gates qwen's work (RED seen, diff read, tests green).
The ledger is `.superpowers/sdd/2026-11-13-synced-emotes/progress.md`.

## Global Constraints

- Every rule in `CLAUDE.md`. Above all: never invent game behaviour (cite map keys); never send a game RPC or weaken
  the game's sender checks; mods message only on their SDK channel; player-facing text only in string tables.
- "Join" is the user's word: string key `emotes.join`, value `Join`.
- The join button is the game's own `Interact` action on every device, found by name in the game's input asset.
- Sway: first part `Sway (Player 1)`, second part `Sway (Player 2)`, both 15.9 s; one song, 63.6 s.
- Partner spot for Sway: side by side, facing the same way, about 1.3 m apart (a starting value). The side is found by
  posing both clips in Unity at the pose in `local/reference/sway.png` (private repo).
- Exactly two dancers per synced emote. A synced emote is never a walking emote (catalogue rule).
- The first dancer's client accepts the first Join while the part is free; the joiner's glide starts on its Accept.
- While paired, neither body turns with its camera. No joining while the first dancer is in an intro. The joiner's
  half-metre rule starts at the spot, after the glide. Each client draws only its own prompt.
- Either dancer stopping, for any reason the mod already ends an emote for, ends both.
- Only the first dancer's song plays for the pair; a `partner.song` (the synced song) replaces it at the same point on
  joining, when the catalogue names one.
- Tests first; `.\validate.ps1` green after each task; `tools\gamecheck.ps1 -IfConfigured -Expect` after map changes.
- Commit messages: an imperative subject, prose body, `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- No push to `development` before the user's in-game test.

## Review Focus

1. **A Join that races an End** (the first dancer stops just as someone presses Join): no one is left gliding or
   dancing alone, on any client. Test in Task 3 (`a_join_after_the_end_is_refused`, `an_end_during_the_glide_ends_both`).
2. **Two players pressing Join at once:** exactly one joins, on every client. Test in Task 3
   (`the_first_join_wins_and_the_second_is_refused`).
3. **A client that joins the room mid-dance, or lacks the emote:** the pair is shown in step or not at all, never one
   dancer out of step. Test in Task 3 (`an_accept_for_an_unknown_dance_is_ignored`).
4. **A blocked or unsafe spot** (a wall, a prop, another player, a drop): no prompt. Test in Task 4
   (`a_blocked_spot_shows_no_prompt`) and the smoke list.
5. **The partner's part out of step after a long dance** (float drift over minutes): both parts read one clock. Test in
   Task 3 (`the_partner_time_follows_the_first_dancers_clock`).

---

### Task 1: Reverse engineer what joining needs

**Owner: Opus.** local-qwen digests long decompiles (`mcp__local-delegate__ask_files` on the decompile output).

**Files:** `sdk/map/*.h` and `sdk/map/all.h` (new entries), `sdk/systems/players.md` and `sdk/systems/menus.md`
(notes), `sdk/builds/*.txt` (gamecheck's record).

Find, cite and map (each a documented map entry, `gamecheck -Expect` OK):

- [ ] **Step 1: Moving the local rig.** How the game itself puts the local PC and VR player at a position and yaw (a
  teleport, a respawn, the truck's or a door's placement). Map the method(s), note what a mid-frame move must also do
  (the CharacterController, the camera, any networked position). If the game has no such path, stop and ask the user.
- [ ] **Step 2: Interact on a player.** What the game does when `Interact` is pressed while the crosshair is on another
  player (the PC rig's interact raycast and its layers). If it does anything, stop and ask the user.
- [ ] **Step 3: The glyph for Interact.** Where the game's sprite for the gamepad button bound to an arbitrary action
  comes from (a `GamepadButton` widget for `Interact`, or `GamepadIcons`' sprite-by-control-name method on a live
  widget's set). Map what is needed; note it in `menus.md`, "Input glyphs".
- [ ] **Step 4: A clear spot.** Which of `Physics.CheckBox`/`OverlapSphere`/`Raycast` and which layers the game uses for
  "a player fits here" (a spawn check, the ghost's or a player's placement); map any missing entry.
- [ ] **Step 5: Where a player's head is**, for the prompt's height (a head bone or `PlayerCharacter` member).
- [ ] **Step 6:** `.\tools\gamecheck.ps1 -IfConfigured -Expect` (all new entries OK), `.\validate.ps1`, commit
  "Map what joining a synced emote needs".

### Task 2: The catalogue's partner part

**Owner: local-qwen** (test first), Opus gates.

**Files:** Modify `mods/emotes/catalogue.h`; Test `tests/emotes_catalogue_test.cpp`.

**Interfaces:**
- Produces: `struct PartnerDef { std::string clip, introClip, song, loudness; float offsetRight = 0, offsetForward = 0; }`
  and `std::optional<PartnerDef> partner` on `EmoteDef`; `EmoteDef::synced()` returns `partner.has_value()`.
- Catalogue JSON: `"partner": {"clip": "Assets/....anim", "introClip"?: "...", "offset": {"right": 1.3, "forward": 0},
  "song"?: "Assets/....wav", "loudness"?: "loudness/<name>.partner.txt"}`. `offset` is the partner's spot in the
  first dancer's frame, metres.

- [ ] **Step 1: Failing tests:** `a_partner_part_is_read` (Sway-like entry: clip, offset right 1.3 forward 0,
  `synced()` true); `a_partner_needs_a_clip_and_an_offset`; `a_partner_song_needs_its_loudness` (both or neither);
  `a_synced_emote_cannot_walk` (partner with `moveClip` refused); `a_partner_intro_needs_the_first_dancers_intro`
  (`partner.introClip` without `introClip` refused); `partner_paths_obey_the_path_rules` (`..`, drive, wrong extension
  refused, as `clip`); `the_tool_fields_still_pass` (unknown fields still warn only).
- [ ] **Step 2:** Run `ctest --test-dir build -R emotes_catalogue` after `build.ps1`: FAIL (compile).
- [ ] **Step 3:** Implement in `catalogue.h` (`kKnown` gains `partner`; messages in the file's style).
- [ ] **Step 4:** Tests pass. **Step 5:** Commit "Read a synced emote's partner part from the catalogue".

### Task 3: Roles, pairing and the Join messages

**Owner: local-qwen** (tests first, then the code), Opus gates.

**Files:** Modify `mods/emotes/emote_net.h`, `mods/emotes/emote_session.h`; Test `tests/emotes_play_test.cpp`,
`tests/emotes_test.cpp` (pack/unpack).

**Interfaces:**
- Consumes: `EmoteInfo` gains `bool synced; float partnerSeconds; float partnerIntroSeconds;` (filled by the glue).
- Produces:
  - `Kind::Join = 5` (sent by the joiner to the first dancer, `To::Actor`; `key` = the emote's key) and
    `Kind::Accept = 6` (broadcast by the first dancer; `key` = the emote's key; the joiner's actor number in the flags'
    upper bits is not enough, so `x` carries it as a float of an integer; `unpack` refuses a non-integer or negative
    one). Message length stays 23 bytes.
  - `enum class Role { First, Partner }`.
  - `LocalSession`: `LocalOut join_request(int firstActor)` (sends Join only while this player could dance and is not
    dancing), `LocalOut accepted(int firstActor, int emote, double firstElapsed, Point spot, float yaw)` (starts the
    Partner role at `firstElapsed`, phase from the partner intro rule, yaw locked), `LocalOut partner_joined(int actor)`
    (First role: from now locked yaw, Accept sent), `std::optional<int> partner() const`, `Role role() const`.
    `LocalOut` gains `std::optional<int> acceptedActor` (glue answers Join with it) and `bool lockYaw`.
  - `LocalSession::on_join(int actor)` returns whether this client accepts: true only while dancing a synced emote, in
    the Loop phase (not Intro), with no partner yet.
  - `RemoteSessions`: an Accept pairs the named joiner with the sender, Partner role, timed from the sender's elapsed;
    an End from either pair member stops both (`RemoteOut::Stop` for each); `double partner_time(int actor)` =
    the first dancer's elapsed, wrapped to `partnerSeconds`.
  - Ending: the local dancer's session, on a remote End from its partner, returns `stop` (both end).
- [ ] **Step 1: Failing tests** (in `emotes_play_test.cpp` unless noted):
  `join_and_accept_round_trip` (emotes_test: pack/unpack keep kind, key and actor; a fractional or negative actor
  refused); `the_first_dancer_accepts_one_join` ; `the_first_join_wins_and_the_second_is_refused`;
  `no_join_during_the_intro`; `no_join_to_an_emote_that_is_not_synced`; `a_join_after_the_end_is_refused`;
  `an_accepted_partner_starts_at_the_first_dancers_point` (elapsed 20.0 s, partner 15.9 s loop: partner time 4.1);
  `the_partner_time_follows_the_first_dancers_clock` (after 600 s of 1/60 s ticks, partner time equals first elapsed
  mod 15.9 within 1e-6); `either_stopping_ends_both` (local partner gets a remote End from its first dancer: stop; and
  the reverse); `an_end_during_the_glide_ends_both`; `paired_bodies_keep_their_yaw` (camera yaw swinging 170 degrees
  sends no Facing while paired); `an_accept_for_an_unknown_dance_is_ignored` (a client with no session for the sender).
- [ ] **Step 2:** Build, run `emotes_play` and `emotes`: FAIL (compile).
- [ ] **Step 3:** Implement. **Step 4:** Pass, then `.\validate.ps1`.
- [ ] **Step 5:** Commit "Pair two dancers in one synced emote".

### Task 4: The prompt rule, the spot and the glide

**Owner: local-qwen**, Opus gates.

**Files:** Create `mods/emotes/sync_rule.h`; Test `tests/emotes_play_test.cpp`.

**Interfaces:**
- Produces, in `pm::emotes`:
  - `Point partner_spot(Point first, float firstYaw, float right, float forward)` (yaw in degrees, 0 along +z, 90 along
    +x, as `view_yaw`).
  - `struct PromptCheck { Point me, target; float lookYaw, lookPitch; bool canDance, targetSynced, partFree,
    targetInIntro, spotClear; }` and `bool shows_prompt(const PromptCheck&)`.
  - `inline constexpr float kJoinRange = 3.0f;` (metres), `kJoinLookDegrees = 15.0f;` (the angle off the look direction
    to the target's chest), `kGlideSeconds = 0.6f;` all starting values.
  - `struct Glide { Point from, to; float fromYaw, toYaw, seconds, t; }` with `Point at() const`, `float yaw() const`
    (shortest turn), `bool done() const`; eased with smoothstep.
- [ ] **Step 1: Failing tests:** `the_spot_is_beside_the_first_dancer` (yaw 0, right 1.3: (1.3, 0, 0) from the dancer;
  yaw 90: (0, 0, -1.3)); `the_prompt_needs_range_look_and_a_free_part` (each condition false alone hides it);
  `a_blocked_spot_shows_no_prompt`; `no_prompt_during_the_intro`; `the_glide_eases_into_the_spot` (t 0, half, full:
  from, midpoint, to; yaw takes the short way round from 350 to 10).
- [ ] **Step 2–4:** Fail, implement, pass. **Step 5:** Commit "Decide the join prompt, the partner's spot and the glide".

### Task 5: The keycap glyph and an animation start time

**Owner:** local-qwen (the glyph and the tests), Opus (the SDK addition).

**Files:** Create `mods/emotes/keycap_glyph.h`, Test `tests/emotes_wheel_test.cpp`; Modify `sdk/api/player_anim.h/.cpp`,
Test `tests/player_anim_test.cpp`.

**Interfaces:**
- Produces: `std::vector<uint8_t> keycap_glyph_rgba(int size)` (a white rounded square outline, bottom row first, as
  `scroll_glyph.h`); the key's name is drawn over it by the prompt's text, not baked in.
  `bool play_on_player(void* player, void* clip, float blendIn, double startSeconds)` (an overload; the clip starts at
  `startSeconds`, wrapped to its length when it loops), from the Playables entries Task 1 confirms
  (`PlayableHandle` time).
- [ ] **Steps:** tests first (`the_keycap_is_a_white_outline_on_clear`, bottom-row order; `a_clip_starts_where_asked`
  over the fake game), fail, implement, pass, `.\validate.ps1`, commit "Draw the keycap glyph and start a clip part-way".

### Task 6: Joining in the game

**Owner: Opus.**

**Files:** Modify `mods/emotes/emote.cpp`, `mods/emotes/wheel.*` only if the prompt reuses its canvas code,
`mods/emotes/i18n/default.json` (`emotes.join`: `Join`); Create `mods/emotes/join_prompt.cpp/.h` (the world-space
prompt: a world canvas over the target's head with the glyph image and the text).

- [ ] **Step 1:** The Interact action by name (as `read_actions` finds the ending actions), on the PC rig's input
  asset and the VR rig's (Task 1 finds the VR one); `WasPressedThisFrame` for the press. Its gamepad glyph from Task 1 Step 3 while `keyboard_in_use()` is false; else the keycap glyph with the
  key's display name.
- [ ] **Step 2:** Each tick: the nearest dancer that `shows_prompt` allows, the prompt over their head (Task 1 Step 5),
  facing the camera; hidden otherwise.
- [ ] **Step 3:** Press: `join_request`, sent `To::Actor`. On Accept: `accepted(...)`, the glide (Task 4) applied each
  frame through Task 1 Step 1's move, the camera eased out, then the partner clip via
  `play_on_player(player, clip, kBlendIn, partner_time)`.
- [ ] **Step 4:** The first dancer's side: `on_join` answers, the Accept broadcast, the synced song switch when the
  catalogue names one, both yaws locked.
- [ ] **Step 5:** Remote side: an Accept plays the partner clip on the joiner's body at `partner_time`; an End from
  either stops both. Only the first dancer's song plays and counts for the ghost.
- [ ] **Step 6:** `.\validate.ps1`; smoke items to the ledger; commit "Join a synced emote from a prompt over the dancer".

### Task 7: Sway's second part in the bundle

**Owner: Opus** (Unity), local-qwen (the tool's partner-clip support and its dotnet tests).

**Files:** `sdk/unity/com.phasmomods.emote-tools/Editor/{Catalogue.cs, EmotePackTool.cs, PackPaths.cs}`,
`tests/emote_tools/Program.cs`; `mods/emotes/assets/emotes.json` (Sway's `partner`), `tools/payload.psd1` (any
partner loudness); private repo `Assets/Emotes/Clips/SwayPartner.anim`.

- [ ] **Step 1:** The tool reads `partner` (clip checked humanoid and looped as `loops` says, `introClip` not looped,
  song measured into its loudness, every path checked as the others). Dotnet test `partner_paths_are_checked`.
- [ ] **Step 2:** Pose both Sway clips on a humanoid in the project at the reference pose; record which side the reach
  goes to; set `offset.right` to plus or minus 1.3 (forward 0). Record the finding in the ledger.
- [ ] **Step 3:** `tools/emote_pack.ps1 -FindLoops -Build`; publish (ask the user first) and pin; `.\validate.ps1`;
  commit "Give Sway its second part".

### Task 8: Docs and finish

**Owner:** local-qwen drafts, Opus reviews every claim.

- [ ] **Step 1:** `docs/emotes.md` (joining, ending, the prompt), `sdk/guides/emotes.md` ("An emote pack": the
  `partner` fields and rules), the example pack untouched.
- [ ] **Step 2:** `.\validate.ps1`, `gamecheck -Expect`; the whole-branch review on Opus; fix Critical and Important,
  ledger the rest.
- [ ] **Step 3:** The ledger's closing entry: the smoke list (join from each device, a blocked spot, either stopping,
  a third player watching, VR glide comfort), the calls made.
