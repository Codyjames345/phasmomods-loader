# Emotes: pages, emote packs, the loop tool and walking emotes. Implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Opus runs this plan inline as the
> orchestrator and hands the tasks marked **Owner: local-qwen** to the `local-qwen` agent with the brief each one
> gives (see "How Opus delegates"). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the Emotes mod with paged wheels, emotes that other mods can add as content packs, a tool that finds an
emote's clip and song loops from a rough guess, and emotes with a variant that plays while walking forward.

**Architecture:** The loader learns SMAPI-style content packs: a mod folder with a manifest that names the mod it is
`contentPackFor`, its data files and no DLL. The loader lists, copies, matches across the lobby and hands those packs to
the mod they are for, through one new host function. Emotes reads one catalogue format, `assets/emotes.json`, from its own
folder and from each pack, and its eight built-in emotes become the first catalogue. The wheel pages through every emote
eight at a time. The session rules gain a walking state and their own clock, and a Unity editor package in `sdk/unity`
finds loops, cuts songs, measures loudness and builds a pack's bundle.

**Tech Stack:** C++20 (mod, loader, offline tests in `tests/`), Unity 2022.3.40f1 editor C# (C# 9) for the pack tool,
.NET SDK console runner for the tool's pure-C# tests, PowerShell wrappers.

**Spec:** `docs/superpowers/specs/2026-11-10-emotes-design.md`, plus "Decisions for this round" below. Task 1 appends
those decisions to the spec, so the spec stays the one source.

## Decisions for this round

The user's (2026-11-12):

1. **Pages.** The wheel has more than one page. The scroll wheel or the left and right bumpers turn the page. The wheel
   opens on the page last viewed in this session. Nothing is saved: a restart opens on the first page.
2. **Showing the page.** Page number text at the top of the wheel, with a glyph for each page control on either side of
   it. Reuse the game's own glyphs if they exist. The wording is the user's: `PM_TEXT_TODO`.
3. **Other mods add emotes.** A community author ships a bundle and a catalogue, and their emotes join the wheel's pages.
4. **No props or particles this round.** The dance package stores none: True Heart and Ride The Pony are body-only
   humanoid clips (muscle curves only, no prop bones, animation events or object curves; read from the package's
   serialised clips on 2026-11-12). Leave props out entirely.
5. **Walking emotes.** Walking forward blends to the moving variant at the game's own walk speed, and stopping blends
   back. Sideways or backward past 0.5 m, sprinting, crouching and the usual actions still end the emote. The camera
   stays in third person. **Forward is the player's input** (the user, 2026-11-12): the W key, or up on the move stick,
   not the direction the body happens to travel. Forward only: a diagonal (W with A or D) is not forward.
6. **Orchestration.** Opus orchestrates and delegates to `local-qwen` with a clear task outline wherever it can.
7. **Intro and loop emotes.** An emote may have an intro clip that plays once before its looping clip, and an intro
   song before its looping song. Both packages have them: `BoogieDownIntro` and `BoogieDownLoop`, `Never Gonna Start`
   and `Never Gonna Loop`, with songs such as `Emote_Dreadful_Intro.wav` and `Emote_Dreadful_Loop.wav`.
8. **Eight more bundled emotes** (Task 15): Build Up, Get Griddy, Lavish, Never Gonna, Running Man, Savor The W,
   Sprinkler and Stride. Their names under the wheel are the user's words as given. The user supplied every icon and
   the five songs the first package lacks (2026-11-12). Never Gonna uses the second package's separate start and loop
   clips and its song.

Claude's calls, listed for the user to check (each marked where it is used):

- **Packs follow SMAPI's content packs.** Field `contentPackFor` (SMAPI's `ContentPackFor`, in this manifest's lower camel
  case, with `id` and `minimumVersion` as `dependencies` already has). A pack has no `dll`. It implies a required dependency
  on its framework mod. A pack counts in the host's list like a mod, so a joiner lacking it is removed. Its build is a
  hash of its files.
- **Ordering.** Emotes' own emotes come first in catalogue order, then each pack's emotes in catalogue order, with the
  packs sorted by id. Messages name an emote by a 32-bit hash of its full id `<pack id>.<name>`, never by its slot.
- **Empty segments.** The last page's unused segments are drawn bare and pick nothing. With a single page, the page row
  (text and glyphs) is hidden.
- **Turning pages.** Scrolling down or the right bumper goes to the next page, and scrolling up or the left bumper goes
  back. Pages wrap round. One step per frame with any scroll. The controls are fixed, not rebindable, and read directly as
  the right stick already is. While the wheel is open, the game's item swap (`InventorySwap`) and whatever the game binds
  to the bumpers are paused, as Look is now.
- **Paging in VR** is done by pointing the right controller at a glyph and pulling the trigger, so no free button is needed.
- **The last page** is held in memory while the mod runs, and clamped if it is past the end.
- **Walking forward** means the game's move input (its Move action's value, the same one W and the move stick drive)
  is at least `kForwardInput` (0.5) up and less than `kForwardInput` to either side. So W alone or the stick pushed
  roughly up counts. W with A or D (a diagonal of about 0.71 each way) does not, and moves as a sideways step. Walking
  forward moves the 0.5 m anchor along with the player. Any other move is measured from where the forward walk stopped. A
  release shorter than `kStopHoldSeconds` (0.2 s) does not switch back. Sprinting ends a walking emote.
- **Intros.** The intro clip plays once, then the loop clip blends in over `kIntroBlendSeconds` (0.1 s, a starting
  value). The intro song plays once and the loop song starts exactly when it ends, scheduled on the audio clock so there
  is no gap. An emote with an intro clip but no intro song starts its loop song with the intro clip. Walking forward during
  the intro skips the rest of it and goes to the moving clip, and stopping goes to the loop clip. Every client times the
  intro from its own clock, started by the Start message, so no extra message is sent.
- **Song timing** follows the emote's own clock (seconds since it started), not the clip's time, because the clip
  restarts when a walking emote switches between its clips.
- **The tool** is a Unity editor package (`sdk/unity/com.phasmomods.emote-tools`, UPM layout). An emote author already
  needs Unity to build a bundle. Its pure-C# core is tested by a .NET console runner that `validate.ps1` runs, so the .NET
  SDK becomes a requirement of `validate.ps1`.
- **A broken pack** (an unreadable catalogue, a bundle that won't load, an asset missing) is switched off by the loader,
  with the reason in the Mods panel, through a new host function the framework mod calls (`content_pack_failed`). A broken
  catalogue in Emotes' own folder keeps Emotes off, as any load-time error does.

## Global Constraints

- Every rule in `CLAUDE.md` applies. In particular: no invented game behaviour (each game fact cited by map key), map
  entries for every game member used, no addresses in SDK docs, no obfuscated names, player-facing text only in string
  tables (`PM_TEXT_TODO` for anything the user has not written), no AI-made assets or visual design beyond what the user
  decided above, private lobbies only, the loader owns all logistics, the host contract only grows at the end, and every
  content struct begins with `uint32_t size`.
- The bundle carries no scripts. A pack carries no code.
- Tests first (red, then green), then `.\validate.ps1`. Any task that moves map values also runs
  `.\tools\gamecheck.ps1 -IfConfigured -Expect`.
- Never launch the game. Live checks go on the smoke list for the user.
- Commit messages: an imperative subject under about 72 characters, then prose, ending with the attribution line.
- User-facing docs: no em dash, active voice.
- The tool's C# compiles as C# 9 (Unity 2022.3): no file-scoped namespaces, records or other newer syntax.

## Review Focus

1. **A pack that fails to load** (bad JSON, a missing clip, a bundle another pack already loaded). Expect: that pack's
   emotes are absent, the Mods panel shows why, every other emote still works, and Emotes stays on. Test in Task 4
   (`failed_switches_the_pack_off_with_its_reason`), Task 5 (`a_broken_pack_is_reported_and_skipped`) and the smoke list.
2. **Scroll or bumpers while the wheel is open.** Expect: the page turns, the held item never swaps and no game action
   fires, and both work again when the wheel closes for any reason (release, death, menu, the mod stopping). Test in
   Task 8 (`paused_actions_come_back_on_every_close`), Task 9's close paths, and the smoke list.
3. **Fewer emotes than last time** (a pack removed). Expect: the remembered page is clamped, empty segments pick nothing, and a
   peer's message for an unknown emote is ignored. Tests in Task 6 and Task 7.
4. **Walking forward, stopping, then stepping sideways.** Expect: the emote ends once the sideways step passes 0.5 m from
   where the walk stopped, never from where the emote began. Test in Task 10 (`sideways_after_walking_measures_from_the_stop`).
5. **A song across clip switches.** Expect: the ghost's loudness and the song's loop follow the emote's clock without a
   jump when the clip switches. Test in Task 10 (`song_clock_runs_on_across_a_clip_switch`).

## How Opus delegates

Opus runs everything marked **Owner: Opus** itself: all reverse engineering, every file that calls the game, the
loader's lobby and identity code, and every review. A task marked **Owner: local-qwen** goes to
`Agent(subagent_type: "local-qwen")` with a brief that holds, in this order:

1. **Goal**, in one line.
2. **Files**: exact paths to create or modify, and "touch no other file".
3. **Interface**: the task's Interfaces block, copied verbatim from this plan.
4. **Tests**: each test's name and assertions, copied from the task.
5. **House style**: comments are plain prose sentences describing behaviour, as in `mods/emotes/emote_rule.h`. No em
   dash. C++20 with tests in `tests/<name>_test.cpp` using `tests/check.h` (`pm_test(<name>)` in `CMakeLists.txt`). For the
   C# tool, C# 9 with no UnityEngine reference in `LoopFinder.cs`.
6. **Commands**: `.\build.ps1`, then `build\<name>_test.exe`. For C#, `dotnet run --project tests/emote_tools -c Release`.
   Report the last 30 lines of each command's output.
7. **Stop conditions**: "If a test cannot pass without changing a file outside your list, or the interface seems wrong,
   stop and report; do not work round it."

**The gate.** After every delegated task Opus reads `git diff` for the task's files, runs the tests itself, and checks
the house rules. Opus fixes small issues inline. If the work is wrong twice, Opus does the task itself. If the agent
reports that Strata is not running, Opus does the task itself. Long build or test output goes to a log file, and Opus asks
`mcp__local-delegate__ask_files` about the log rather than reading it whole.

**The ledger** is `.superpowers/sdd/2026-11-12-emotes-2/progress.md` (gitignored, force-added like the others). Opus
appends one entry per task: what was done, who did it, the gate's result, any difference from the plan, and any
`PM_TEXT_TODO` or `PM_UI_TODO` added.

**Where:** a worktree `.claude/worktrees/emotes-2` on a new branch `feature/emotes-2` from `development`, so the dev
menu's uncommitted work on `feature/dev-menu` is untouched.

---

### Task 1: Reverse engineer what this round needs

**Owner: Opus.** The game facts every later task cites. Nothing is built here.

**Files:**
- Modify: `docs/superpowers/specs/2026-11-10-emotes-design.md` (append "Decisions for this round (2026-11-12)" from this
  plan)
- Modify: `sdk/map/Input.h`, `sdk/map/Animator.h`, `sdk/map/all.h` (entries as found), `sdk/systems/menus.md`,
  `sdk/systems/players.md`
- Modify: `tests/raw_scan_test.cpp` or the map's own tests, as each new entry requires
- Create: the ledger

**Interfaces:**
- Produces, in the ledger and the docs, the answers below, each cited by map key.

- [ ] **Step 1: The game's input glyphs.** Look for sprites the game shows for mouse scroll, LB and RB (and any keyboard
  or VR prompts) with `tools/assetcheck.py`, the approved offline asset check. Record each sprite's asset name and how to
  load it (a map entry, or a `Resources`/sprite lookup the SDK already has). If the game has none for a control, stop and
  ask the user for that glyph (`PM_UI_TODO`). AI never draws one.
- [ ] **Step 2: Scroll and bumpers.** Read the game's action asset for what `<Mouse>/scroll` and
  `<Gamepad>/leftShoulder`, `<Gamepad>/rightShoulder` are bound to (`InputActionAsset` entries in `Input.h`). Confirm
  that disabling those actions while the wheel is open and enabling them again is enough, as `set_look` does for Look.
- [ ] **Step 3: Sprint and move.** The sprint action's name in the game's controls, and whether it is held or toggled.
  The move action's name and value (a `Vector2`, up positive), for keyboard (W) and gamepad (the move stick), and where
  the mod reads it while the wheel and the third-person view are active.
- [ ] **Step 3b: Joining two songs without a gap.** Map `AudioSource.PlayScheduled`, `AudioSource.SetScheduledEndTime`
  and `AudioSettings.dspTime` (Unity engine members, read through the map like the other engine entries), so the loop
  song can start on the sample the intro song ends.
- [ ] **Step 4: Root motion on the live body.** Whether `Player.animator` applies root motion (`Animator.applyRootMotion`),
  and so whether a walking clip's `RootT` would move the player under the clip from `sdk/api/player_anim.h`. If it would,
  record how the SDK keeps the clip in place.
- [ ] **Step 5: The current device.** How the game knows whether the player last used the mouse or the gamepad (for which
  glyphs to show). If the game keeps nothing, the mod uses the device of the last input it read (Claude's call).
- [ ] **Step 6: Walking candidates.** In the private bundle project, add `Assets/Editor/EmotesClipProbe.cs` that logs, for
  `Conga`, `Stride`, `LlamaConga`, `LlamaCongaMove`, `Fishin'`, `Fishin'Traversal`, `Focused`, `FocusedTraversal`,
  `Pandemonium`, `PandemoniumTraversal`, `GuitarWalk`, `Jaywalking`, `MarshWalk`, and the eight of Task 15, each clip's
  length, loop setting and forward travel of `RootT.z` per second. Run it in batch mode. Report to the user which clips
  walk and which stand in place, and ask which clips, songs, icons and names make up the walking emotes. Those assets
  are the user's. Until they arrive, the test pack uses `LlamaConga` and `LlamaCongaMove`.
- [ ] **Step 7: Validate and commit.** Run `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`, then
  commit: "Map the input and animation facts the emote pages and walks need".

### Task 2: Content packs in the manifest and the dependency graph

**Owner: Opus** for the parser (it reads untrusted files). **local-qwen** writes the tests from Step 1.

**Files:**
- Modify: `core/mod_manifest.h` (`Manifest`, `read_manifest`, `kKnown`)
- Modify: `host/mod_deps.cpp`, `host/mod_deps.h`
- Modify: `i18n/default.json` (new `manifest.*` reasons; their wording is `PM_TEXT_TODO`)
- Test: `tests/mod_manifest_test.cpp`, `tests/mod_deps_test.cpp`

**Interfaces:**
- Produces: `struct Manifest` gains `std::optional<Dependency> contentPackFor;` and `bool is_pack() const`.
  `read_manifest` accepts `"contentPackFor": {"id": "<mod id>", "minimumVersion": "x.y.z"}` (minimumVersion optional) and,
  for a pack, refuses a `dll` field and needs no DLL. In `mod_deps`, a pack depends on its framework mod as a required
  dependency, so it starts after it, stays off when it is missing, and is named in the Mods panel's reason.

- [ ] **Step 1: Write the failing tests.** In `mod_manifest_test.cpp`: `a_pack_names_its_framework_and_no_dll`
  (`is_pack()`, `contentPackFor->id == "phasmomods.emotes"`, `dll.empty()`), `a_pack_with_a_dll_is_refused`,
  `a_pack_without_an_id_is_refused`, `a_pack_minimum_version_is_parsed`, `a_mod_without_contentPackFor_is_no_pack`. In
  `mod_deps_test.cpp`: `a_pack_starts_after_its_framework`, `a_pack_without_its_framework_stays_off_with_the_reason`,
  `a_pack_for_an_older_framework_stays_off`.
- [ ] **Step 2: Run them to see them fail.** `.\build.ps1`, `build\mod_manifest_test.exe`, `build\mod_deps_test.exe`.
- [ ] **Step 3: Implement** the fields and rules above. The framework id goes through `modid::migrated_id`, as
  dependencies do.
- [ ] **Step 4: Run them to see them pass,** then `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Let a manifest declare a content pack for another mod".

### Task 3: Packs in the launcher, the Mods panel and the lobby's list

**Owner: Opus.** This touches every guarantee in "The host's list". **local-qwen** may write test bodies from Step 1.

**Files:**
- Modify: `tools/launcher.cpp` and its copy helpers (`tools/stage.ps1` only if packs need staging), `core/mod_list.h`
- Modify: `host/mods.cpp` (`build_of`, `started_builds`), `host/presence.h` as needed, `host/menu_panel.cpp`
- Modify: `i18n/default.json` (a pack's line in the Mods panel: `PM_TEXT_TODO`)
- Test: `tests/launcher_test.cpp`, `tests/mods_test.cpp`, `tests/presence_test.cpp`, `tests/join_guard_test.cpp`,
  `tests/mods_panel_test.cpp`

**Interfaces:**
- Consumes: `Manifest::is_pack()`, `contentPackFor` (Task 2).
- Produces: `std::string pack_build_of(const std::filesystem::path& packFolder)`, the first 16 hex characters of a SHA-256
  over every file under `assets/` and `i18n/` plus `manifest.json`, taken in sorted relative-path order, each path and
  each length included so that a renamed file changes the result. A started pack appears in `started_builds()` with that
  build. The launcher copies a pack's `assets/` and `i18n/` into the per-process copy, as it does for a mod. The host
  keeps, for each started pack, `{id, name, version, folder}`, where `folder` is the copied `assets/`.

- [ ] **Step 1: Write the failing tests:** `a_pack_build_changes_with_any_file`, `a_pack_build_ignores_file_order`,
  `a_pack_is_copied_with_assets_and_i18n`, `a_started_pack_is_in_the_presence`, `a_joiner_lacking_a_host_pack_is_removed`,
  `a_joiner_with_an_extra_pack_has_it_switched_off`, `a_pack_row_names_its_framework` (Mods panel).
- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.** A pack never loads a DLL and has no `pmmod_*` exports. It is "started" when its framework is
  started and it is enabled. It stops when its framework stops.
- [ ] **Step 4: Run them to see them pass,** then `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Copy, list and match content packs as the lobby's mods".

### Task 4: The host function that hands a mod its packs

**Owner: Opus** for the contract. **local-qwen** drafts the test and the docs section.

**Files:**
- Modify: `core/mod_api.h` (end of `PmHostApi`; minor version plus one over what `development` holds at the time)
- Create: `sdk/api/packs.h`, `sdk/api/packs.cpp`
- Modify: `host/` where `PmHostApi` is filled, `sdk/fake/fake_game.cpp` (fake packs for tests), `CMakeLists.txt`
- Modify: `sdk/README.md` (contract history and a "Content packs" section), `sdk/guides/mod-files.md` (a pack's layout)
- Test: `tests/content_packs_test.cpp`

**Interfaces:**
- Produces, in `core/mod_api.h`:
  ```c
  typedef struct PmContentPack {
    uint32_t size;          // sizeof(PmContentPack)
    const char* id;         // the pack's mod id
    const char* name;       // its manifest name
    const char* version;    // its manifest version
    const wchar_t* folder;  // its copied assets folder for this game process
  } PmContentPack;
  // @since Contract 2.N: the started packs whose contentPackFor is the calling mod, in id order.
  uint32_t (*content_packs)(PmContentPack* out, uint32_t max);
  // @since Contract 2.N: switches a pack off for this session with a reason shown in the Mods panel.
  void (*content_pack_failed)(const char* packId, const char* why);
  ```
- Produces, in `sdk/api/packs.h`, namespace `pm::api::packs`: `struct Pack { std::string id, name, version;
  std::filesystem::path folder; };`, `std::vector<Pack> owned();` (empty before bind or for a host without the
  function), and `void failed(std::string_view packId, std::string_view why);`.

- [ ] **Step 1: Write the failing test** `content_packs_test.cpp`: `owned_lists_only_my_packs_in_id_order`,
  `owned_is_empty_on_an_older_host`, `failed_switches_the_pack_off_with_its_reason`,
  `a_failed_pack_leaves_the_presence_at_the_next_start` (out of a room only).
- [ ] **Step 2: Run it to see it fail.**
- [ ] **Step 3: Implement** the contract additions at the end of `PmHostApi`, the wrapper, the host side and the fake.
  Document both functions as interfaces (what, parameters, return, thread, phase, every client, side effects).
- [ ] **Step 4: Run it to see it pass,** then `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Hand a mod the content packs made for it".

### Task 5: The emote catalogue

**Owner: local-qwen.** Pure parsing, no game calls.

**Files:**
- Create: `mods/emotes/catalogue.h`
- Create: `tests/emotes_catalogue_test.cpp`, and `pm_test(emotes_catalogue)` with `mods/emotes` included, in `CMakeLists.txt`

**Interfaces:**
- Produces, in namespace `pm::emotes`:
  ```cpp
  struct EmoteDef {
    std::string id;        // "<pack id>.<name>"
    std::string name;      // [a-z0-9-], 1 to 40 characters, unique in its pack
    std::string clip;      // project path in the bundle, "Assets/....anim"
    std::string moveClip;  // the same for the walking variant, or empty
    std::string introClip; // played once before `clip`, or empty
    std::string song;      // "Assets/....wav" or ".ogg"
    std::string introSong; // played once before `song`, or empty
    std::string icon;      // "Assets/....png"
    std::string label;     // key in the pack's i18n/default.json
    std::string loudness;  // path under the pack's assets folder, "loudness/<name>.txt"
    std::string introLoudness;  // "loudness/<name>.intro.txt" when introSong is set, else empty
    bool loops = true;
    SongLoop songLoop;     // from emote_rule.h; length 0 loops with the clip
    uint32_t key = 0;      // emote_key(id)
  };
  struct Catalogue { std::string packId; std::string bundle; std::vector<EmoteDef> emotes; };
  bool parse_catalogue(std::string_view json, std::string_view packId, Catalogue& out, std::string& why);
  std::string bundle_path(std::string_view projectPath);  // lower case, as Unity names a bundle's assets
  uint32_t emote_key(std::string_view fullId);            // FNV-1a, 32-bit
  // Emotes' own catalogue first, then the rest by packId; each in its own order. Refuses (why set) two emotes with one key.
  bool wheel_order(const Catalogue& own, std::vector<Catalogue> packs, std::vector<EmoteDef>& out, std::string& why);
  ```
- The file format (`assets/emotes.json`):
  ```json
  { "format": 1, "bundle": "emotes.bundle",
    "emotes": [ { "name": "floss", "clip": "Assets/Emotes/Clips/Floss.anim", "song": "Assets/Emotes/Loops/floss.wav",
                  "icon": "Assets/Emotes/Icons/floss.png", "label": "emotes.name.floss",
                  "loudness": "loudness/floss.txt", "loops": true, "songLoop": { "start": 0, "seconds": 16.692 },
                  "moveClip": "Assets/Emotes/Clips/FlossWalk.anim",
                  "introClip": "Assets/Emotes/Clips/FlossStart.anim", "introSong": "Assets/Emotes/Songs/floss_intro.wav",
                  "introLoudness": "loudness/floss.intro.txt" } ] }
  ```
  Tool-only fields, read by the Unity tool and ignored by the mod: `clipSource`, `clipLoopGuess`, `songSource`,
  `songLoopGuess`. Any other unknown field is listed in `why` as a warning, and parsing still succeeds, as the manifest's
  `unknownFields` does.

- [ ] **Step 1: Write the failing tests:** `the_example_parses` (the JSON above: one emote, `id == "p.x.floss"` for pack
  `p.x`, `songLoop.length == 16.692f`, `key == emote_key("p.x.floss")`), `format_other_than_1_is_refused`,
  `a_bundle_name_with_a_path_is_refused`, `a_path_outside_Assets_or_with_dotdot_is_refused`,
  `a_loudness_path_with_dotdot_is_refused`, `a_duplicate_name_is_refused`, `a_bad_name_is_refused` (upper case, a space,
  41 characters), `a_moveClip_on_a_one_shot_is_refused`, `an_introClip_on_a_one_shot_is_refused`,
  `an_introSong_without_introLoudness_is_refused`, `intro_fields_are_optional`, `a_negative_or_infinite_songLoop_is_refused`,
  `tool_fields_are_accepted_silently`, `an_unknown_field_is_a_warning`, `more_than_64_emotes_is_refused`,
  `bundle_path_lowercases`, `wheel_order_puts_own_first_then_packs_by_id`, `wheel_order_refuses_a_key_collision`, and
  `a_broken_pack_is_reported_and_skipped` (`wheel_order` gets only the good ones; the caller reports the bad one).
- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement** with nlohmann::json (already vendored; see `core/text_table.h`), comments allowed as
  `text_table` allows them.
- [ ] **Step 4: Run them to see them pass,** then `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Read emotes from a catalogue file".

### Task 6: Session, messages and rules without a fixed eight

**Owner: local-qwen.** Pure headers and their tests.

**Files:**
- Modify: `mods/emotes/emote_session.h`, `mods/emotes/emote_net.h`, `mods/emotes/emote_rule.h`
- Create: `mods/emotes/wheel_pages.h`
- Modify: `tests/emotes_test.cpp`, `tests/emotes_play_test.cpp`
- Create: `tests/emotes_pages_test.cpp`, and `pm_test(emotes_pages)` in `CMakeLists.txt`

**Interfaces:**
- Consumes: `EmoteDef`, `emote_key` (Task 5).
- Produces:
  - `using EmoteTable = std::vector<EmoteInfo>;`, and `EmoteInfo` gains `uint32_t key; bool hasMove;`. Every emote
    index is `int` into that table.
  - `emote_net.h`: `EmoteMsg { Kind kind; uint32_t key; bool moving; float x, y, z, yaw; }`. `Kind` gains `Move = 4`.
    The bytes are magic, kind, flags (bit 0 moving), key as little-endian u32, then x, y, z, yaw as little-endian floats:
    `kMessageBytes = 23`. `unpack` no longer checks the emote against the wheel. `RemoteSessions::receive` looks the key up
    in its table and ignores a key it does not hold.
  - `wheel_pages.h`, namespace `pm::emotes::pages`: `int page_count(int emotes)` (at least 1),
    `int clamp_page(int page, int emotes)`, `int turn(int page, int steps, int emotes)` (wraps both ways),
    `int emote_at(int page, int segment, int emotes)` (-1 for an empty segment), and `int scroll_step(float scrollY)`
    (+1 for scrolling down, -1 for up, 0 for none). `kSegments` stays the per-page count.
  - `wheel_step` keeps its signature. The glue turns its segment into an emote with `emote_at`.

- [ ] **Step 1: Write the failing tests.** Pages: `page_count_rounds_up` (0 to 1, 8 to 1, 9 to 2, 17 to 3),
  `turn_wraps_both_ways`, `emote_at_is_minus_one_past_the_end`, `clamp_page_after_emotes_shrink`,
  `scroll_down_is_next_page`. Net: `pack_unpack_round_trips_every_kind`, `a_short_or_long_message_is_refused`,
  `a_non_finite_number_is_refused`. Session: `an_unknown_key_from_a_peer_is_ignored`, `a_table_of_twenty_emotes_plays_the_last`,
  and every existing test kept green with tables built through a helper `table_of(n)`.
- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run them to see them pass,** then `.\validate.ps1`. The mod's glue does not compile until Task 7; if
  `.\build.ps1` stops on `mods/emotes/emote.cpp`, report it and stop (Task 7 follows at once).
- [ ] **Step 5: Commit** together with Task 7.

### Task 7: Emotes loads its catalogue and its packs

**Owner: Opus.** The glue in `emote.cpp`, which calls the game.

**Files:**
- Create: `mods/emotes/assets/emotes.json` (the eight emotes, with `songLoop` values from today's `kEmotes` and labels
  `emotes.name.<name>`)
- Modify: `mods/emotes/i18n/default.json` (keys renamed to `emotes.name.<name>`, the user's text unchanged)
- Move: `mods/emotes/assets/<folder>/loudness.txt` to `mods/emotes/assets/loudness/<name>.txt`
- Modify: `mods/emotes/emote_assets.h` (keep only the key labels and volume label; `kEmotes` goes), `mods/emotes/emote.cpp`,
  `mods/emotes/wheel.cpp`, `mods/emotes/wheel.h`, `tools/emote_loudness.mjs` (new paths, until Task 12 replaces it)
- Modify: `tools/payload_files.ps1` or `tools/stage.ps1` if they list the loudness files
- Test: `tests/emotes_test.cpp` (the shipped catalogue parses and every label has a string-table entry),
  `tests/text_table_test.cpp` as needed

**Interfaces:**
- Consumes: `parse_catalogue`, `wheel_order` (Task 5), `pm::api::packs::owned`, `failed` (Task 4), `EmoteTable`, message
  format (Task 6).
- Produces: in `emote.cpp`, `bool load_catalogues()`, which loads its own catalogue (any error makes `start()` fail) and
  each pack's: its catalogue, its bundle (from the pack's folder), every clip, song and icon, its loudness file, and its
  labels from the pack's `i18n/default.json` with `pm::i18n::Table::load`. A pack with any failure is unloaded, reported
  with `packs::failed(id, why)` and left out. `wheel::load` takes the icons in wheel order.

- [ ] **Step 1: Write the failing test** `the_shipped_catalogue_parses_with_a_label_for_each` (reads
  `mods/emotes/assets/emotes.json` and `mods/emotes/i18n/default.json` through `PM_TEXT_DIR`).
- [ ] **Step 2: Run it to see it fail.**
- [ ] **Step 3: Implement** the glue. The private repo's `EmotesBuild.cs` keeps building today's bundle until Task 13
  replaces it. The bundle paths are unchanged, so the pinned bundle still serves.
- [ ] **Step 4: Run** `.\validate.ps1`. Add to the smoke list: with the example pack (Task 14) installed, its emote is
  the first on page 2; with its bundle deleted, the Mods panel shows why and the eight still play.
- [ ] **Step 5: Commit** (with Task 6): "Load emotes from catalogues and from emote packs".

### Task 8: Paused actions always come back

**Owner: local-qwen.** A pure rule that holds which actions are paused, so every close path is tested offline.

**Files:**
- Create: `mods/emotes/paused_actions.h`; Test: `tests/emotes_pages_test.cpp`

**Interfaces:**
- Produces: `class PausedActions { public: void pause(std::span<void* const> actions); std::vector<void*> resume();
  bool paused() const; };`. `pause` remembers only the actions that were enabled. `resume` returns them once and empties
  the set. A second `pause` without a `resume` adds nothing twice.
- [ ] **Step 1: Write the failing tests:** `paused_actions_come_back_on_every_close` (pause, then resume returns
  exactly those), `a_disabled_action_is_not_reenabled` (the caller passes only enabled ones; resume returns none extra),
  `double_pause_is_one_pause`, `resume_twice_returns_nothing_the_second_time`.
- [ ] **Step 2: Fail. Step 3: Implement. Step 4: Pass,** then Opus switches `emote.cpp`'s Look handling to it
  (`set_look` goes) and runs `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Pause the game's actions under the wheel in one place".

### Task 9: Pages on the wheel

**Owner: Opus.** UI objects and input in the game. **local-qwen** writes the layout maths and its tests (Step 1 and the
`wheel_layout.h` additions).

**Files:**
- Modify: `mods/emotes/wheel_layout.h` (page row geometry), `mods/emotes/wheel_style.h` (placeholders: `kPageAbove`,
  `kPageFontSize`, `kGlyphSize`, `kGlyphGap`), `mods/emotes/wheel.cpp`, `mods/emotes/wheel.h`, `mods/emotes/emote.cpp`
- Modify: `mods/emotes/i18n/default.json` (`"emotes.page": "PM_TEXT_TODO"`, tokens `{{page}}` and `{{pages}}`)
- Test: `tests/emotes_wheel_test.cpp`

**Interfaces:**
- Consumes: `pages::*` (Task 6), `PausedActions` (Task 8), the glyph sprites and the actions to pause (Task 1).
- Produces: `wheel::show_page(int page, int pages, api::keys::Device device)`, which sets the text and glyphs or hides the row when
  `pages == 1`. `wheel::show_icons(std::span<void* const> icons)` gives the page's eight icons (null for empty).
  `layout::glyph_at(Vec2 p) -> int` returns -1 for the previous glyph, +1 for next, and 0 otherwise. `wheel::pointed_glyph(void* hand)`
  is for VR.

- [ ] **Step 1: Write the failing tests:** `glyphs_sit_either_side_of_the_page_text`, `glyph_at_hits_each_glyph_and_misses_between`,
  `page_row_sits_above_the_disc`.
- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.** The wheel opens on the page it last showed, held in memory (the first page after the mod
  starts). Scroll and bumpers turn it through `pages::turn`. While the wheel is open, the actions from Task 1 Step 2 are paused through `PausedActions`, and they are resumed on
  every path that closes it (`close_wheel`, `end_at_once`, `detour_death_start`, `stop()`). Segments past the end show bare and pick
  nothing. In VR, pointing at a glyph and pulling the right trigger turns the page.
- [ ] **Step 4: Run** `.\validate.ps1`. Add to the ledger's smoke list: paging by scroll, bumpers and VR, the held item
  never swaps, the bumpers' game actions never fire while open, the page is remembered on reopening and back to the first after a restart, and the row is
  hidden with eight emotes.
- [ ] **Step 5: Commit:** "Page through the emote wheel with the scroll wheel or bumpers".

### Task 10: The walking rule, intros and the emote's clock

**Owner: local-qwen.** Pure session rules.

**Files:**
- Modify: `mods/emotes/emote_rule.h`, `mods/emotes/emote_session.h`; Test: `tests/emotes_play_test.cpp`

**Interfaces:**
- Produces:
  - `emote_rule.h`: `kForwardInput = 0.5f`, `kStopHoldSeconds = 0.2f`, `kMoveBlendSeconds = 0.25f` (all starting
    values), and `bool walking_forward(float moveX, float moveY)`: `moveY >= kForwardInput && std::fabs(moveX) < kForwardInput`.
    It reads input only, so W against a wall still plays the moving clip.
  - `LocalFrame` gains `float moveX, moveY` (the game's move input) and `bool sprinting`. `LocalOut` gains `std::optional<bool> moving` (switch to the walking clip, or
    back). `LocalSession` gains `double elapsed() const`. The session sends `Kind::Move` on each switch.
  - For an emote with `hasMove`: walking forward sets moving and re-anchors the 0.5 m start to the current position each
    frame. Not walking forward for `kStopHoldSeconds` switches back. Moving otherwise is measured from the last anchor.
    `sprinting` while it plays ends it. An emote without `hasMove` behaves exactly as today.
  - `RemoteSessions::tick(frames, receiverDown, dt)` keeps each peer's elapsed time, and `RemoteOut` gains `Move` with
    `bool moving`. `double elapsed(int actor) const`.
  - Intros: `EmoteInfo` gains `float introSeconds` (0 for none) and `float introSongSeconds` (0 for none).
    `kIntroBlendSeconds = 0.1f`. Which clip plays is one enum, `enum class Phase { Intro, Loop, Moving };`, and `LocalOut`'s
    `moving` becomes `std::optional<Phase> phase`. An emote with an intro starts in `Intro` and goes to `Loop` when
    `elapsed() >= introSeconds`. Walking forward in `Intro` goes to `Moving`, and stopping from `Moving` goes to `Loop`
    (never back to `Intro`). `RemoteSessions` makes the same intro switch from each peer's own elapsed time and reports it
    as `RemoteOut::Move` with the phase. A one-shot never has an intro (the catalogue refuses it).
  - `emote_rule.h`: `double song_position(double elapsed, float introSongSeconds, SongLoop loop)` gains the intro: during
    the intro it returns `elapsed` and marks it as intro time, and after it, the loop song's position from
    `elapsed - introSongSeconds`. It returns `struct SongAt { bool intro; double seconds; }`, so the glue picks the right
    loudness envelope.
- [ ] **Step 1: Write the failing tests:** `w_alone_is_forward` (0, 1), `stick_roughly_up_is_forward` (0.3, 0.8),
  `a_diagonal_is_not_forward` (0.707, 0.707), `backward_and_sideways_are_not_forward`,
  `walking_forward_switches_to_the_moving_clip`,
  `a_short_stop_does_not_switch_back`, `a_stop_longer_than_the_hold_switches_back`,
  `walking_forward_past_half_a_metre_does_not_end_it`, `sideways_after_walking_measures_from_the_stop`,
  `backward_past_half_a_metre_ends_it`, `sprinting_ends_a_walking_emote`, `a_standing_emote_ignores_walking_forward`
  (it still ends past 0.5 m), `song_clock_runs_on_across_a_clip_switch` (`elapsed()` grows by `dt` through a switch),
  `a_peer_move_message_switches_their_clip`, `a_peer_elapsed_follows_dt`, `an_intro_plays_once_then_loops`,
  `walking_forward_during_the_intro_skips_to_moving`, `stopping_after_a_skipped_intro_goes_to_the_loop`,
  `a_peer_intro_switches_on_their_own_clock`, `song_at_is_intro_then_loop` (intro 2.0 s, loop 4.0 s: elapsed 1.0 is intro
  1.0; 7.0 is loop 1.0), and `no_intro_song_starts_the_loop_song_at_once`.
- [ ] **Step 2: Fail. Step 3: Implement. Step 4: Pass,** then `.\validate.ps1` (the glue must still build; the new
  fields have defaults).
- [ ] **Step 5: Commit:** "Let an emote walk forward with its moving clip".

### Task 11: Walking and intro emotes in the game

**Owner: Opus.**

**Files:**
- Modify: `mods/emotes/emote.cpp`; Test: covered by Task 10; the smoke list

**Interfaces:**
- Consumes: Task 10's outputs, the sprint action and root-motion finding (Task 1), `play_on_player` (a second clip
  blends over the first), `EmoteDef::moveClip`.
- [ ] **Step 1: Implement.** Read `moveX`, `moveY` from the game's move action and `sprinting` from its sprint action
  (Task 1 Step 3). On a `phase`, call `play_on_player` with that phase's clip, locally and for peers on
  `RemoteOut::Move`. The song keeps playing. `raise_loudness` uses `elapsed()` in place of `clip_time`. If Task 1 Step 4 found that root motion
  applies, keep the clip in place as recorded there. On a phase change, play the phase's clip (intro, loop or moving)
  with `kIntroBlendSeconds` or `kMoveBlendSeconds`. With an intro song, start it with `PlayScheduled` and schedule the
  loop song (`loop = true`) on a second source from the moment it ends (Task 1 Step 3b). The ghost's loudness reads the
  intro's envelope or the loop's, by `song_position`'s `intro` flag.
- [ ] **Step 2: Run** `.\validate.ps1`. Add to the smoke list: walking forward with the test pack's walking emote, others
  seeing the switch, sprinting ending it, sideways and backward steps, and the song not jumping. Also, for an intro
  emote: the intro plays once, the loop follows without a visible snap, the songs join with no gap or click, and walking
  during the intro skips it.
- [ ] **Step 3: Commit:** "Play an emote's intro and its moving clip when its dancer walks".

### Task 12: The loop finder's core

**Owner: local-qwen.** Pure C#, no Unity.

**Files:**
- Create: `sdk/unity/com.phasmomods.emote-tools/Editor/LoopFinder.cs`
- Create: `tests/emote_tools/EmoteToolsTest.csproj` (net10.0 console, `<LangVersion>9.0</LangVersion>`, no packages,
  `<Compile Include="../../sdk/unity/com.phasmomods.emote-tools/Editor/LoopFinder.cs" />`), `tests/emote_tools/Program.cs`
- Modify: `validate.ps1` (runs `dotnet run --project tests/emote_tools -c Release` and fails on a non-zero exit)

**Interfaces:**
- Produces, `namespace PhasmoMods.EmoteTools`, `public static class LoopFinder`:
  - `public static LoopResult SongLoop(float[] mono, int rate, double guessSeconds, double searchSeconds = 1.0, double compareSeconds = 4.0)`:
    the loop length in samples that best repeats the song's start. This is the normalised cross-correlation of the first
    `compareSeconds` against the audio at each lag within `guess ± search`, coarse at every 8th sample, then exact within
    ±16 samples of the best. `LoopResult { int At; double Score; }`.
  - `public static LoopResult ClipLoop(float[][] frames, float fps, double guessSeconds, double searchSeconds = 1.0, float motionWeight = 4f)`:
    the frame that best returns to frame 0, by summed squared pose difference plus `motionWeight` times the summed squared
    difference of the frame-to-frame change, as `EmotesLoopCheck.FindCycle` in the private repo does. Lower is better,
    and `Score` is that cost.
  - `public static float[] RmsWindows(float[] mono, int rate, double windowSeconds = 0.1)`, the format `tools/emote_loudness.mjs`
    writes (RMS 0 to 1 per window; a final partial window counts).
  - `public static float[] Mono(float[] interleaved, int channels)`.
- [ ] **Step 1: Write the failing tests** in `Program.cs` (each a method; exit code 1 on any failure, printing its name):
  `song_loop_found_to_the_sample` (a seeded random signal repeated with a period of 59259 samples at 48 kHz; a guess of
  1.2 s finds 59259 with a score of at least 0.99), `song_loop_with_noise_still_found` (adds ±1% noise; within 2 samples),
  `song_guess_outside_search_reports_low_score`, `clip_loop_found_on_periodic_curves` (a 37-frame period at 30 fps, guess
  1.2 s, gives 37), `clip_motion_term_breaks_a_pose_tie` (a mirrored half-cycle matches the pose but not the motion),
  `rms_of_a_full_scale_square_is_one`, `rms_counts_a_partial_last_window`, `mono_averages_channels`.
- [ ] **Step 2: Fail. Step 3: Implement. Step 4: Pass,** then `.\validate.ps1`.
- [ ] **Step 5: Commit:** "Find an emote's song and clip loops from a rough guess".

### Task 13: The Unity tool: find loops and build a pack

**Owner: Opus.** **local-qwen** drafts `EmotePackTool.cs` and `tools/emote_pack.ps1` from this outline, and Opus checks
them in Unity batch mode.

**Files:**
- Create: `sdk/unity/com.phasmomods.emote-tools/package.json` (UPM: name `com.phasmomods.emote-tools`, Unity `2022.3`),
  `Editor/PhasmoMods.EmoteTools.Editor.asmdef` (Editor only), `Editor/EmotePackTool.cs`, `Editor/Catalogue.cs`
  (reads and writes `emotes.json` with `JsonUtility`-compatible types), `Editor/WavWriter.cs`
- Create: `tools/emote_pack.ps1 -Unity <Unity.exe> -Project <unity project> -Pack <pack assets folder> [-FindLoops] [-Build]`
- Delete: `tools/emote_loudness.mjs` (the tool writes loudness now; record the change in the ledger)
- Private repo: `unity/emotes-bundle/Packages/manifest.json` gains the package by `file:` path. `EmotesBuild.cs`,
  `EmotesLoopCheck.cs` and `cut_loops.ps1` are removed in favour of the tool.

**Interfaces:**
- Consumes: `LoopFinder` (Task 12), the catalogue format (Task 5).
- Produces: batch entry points `PhasmoMods.EmoteTools.EmotePackTool.FindLoopsBatch` and `.BuildBatch`, each reading
  `-pack <folder>` from the command line, plus the same two as menu items under `PhasmoMods/Emotes`.
  - **Find loops**, for each emote with `songSource` and `songLoopGuess`: decode it (import as PCM, `AudioClip.GetData`),
    run `SongLoop`, write the song cut to its first loop as 16-bit WAV at `song`, and set `songLoop`. For each emote with
    `clipSource` and `clipLoopGuess`: sample every curve at the clip's frame rate, then run `ClipLoop`. If its score
    passes the threshold, cut the clip there. Otherwise crossfade its tail into its start for the song's loop length, as
    `EmotesBuild.MakeLoop` does today. Write it at `clip` with Loop Time on. Log `EMLOOP <name> song <samples> (<score>) clip <frame> (<score>|crossfaded)`.
  - **Songs that are loops already** (the second package's `_Loop` files): with no `songLoopGuess`, the tool sets
    `songLoop` to `{0, the file's length}` and cuts nothing. An `introSong` is never cut. A clip with Loop Time already on
    and no `clipLoopGuess` is used as it is.
  - **Build**: check every path the catalogue names (intro clips and songs included), set import settings as `EmotesBuild.cs` does today (Vorbis at quality
    1.0, icons uncompressed and clamped, humanoid clips only), write `loudness/<name>.txt` from each cut song and
    `loudness/<name>.intro.txt` from each intro song with `RmsWindows`, build the one bundle named by `bundle`, check that it holds exactly the named assets, and copy it into the
    pack folder.
- [ ] **Step 1:** Draft (qwen), then run in batch mode on the private project against `mods/emotes/assets`. The eight
  loops must match today's sample counts in `cut_loops.ps1` within 2 samples, and Disco Fever must crossfade.
- [ ] **Step 2:** Build the bundle. Compare its asset list with the pinned one, and compare the new loudness files with
  the old ones (they may differ slightly by decoder; record the largest difference in the ledger).
- [ ] **Step 3:** Publish with `tools/publish_assets.ps1 -Private C:\Dev\phasmomods-private` only if the bundle bytes
  changed, then commit the pin with the code.
- [ ] **Step 4:** `.\validate.ps1`. Commit: "Find loops and build emote bundles with one Unity tool".

### Task 14: An example pack and the author's guide

**Owner: local-qwen** drafts. **Opus** reviews every claim against the code.

**Files:**
- Create: `sdk/examples/example_emote_pack/manifest.json` (`contentPackFor: phasmomods.emotes`),
  `assets/emotes.json` (one emote, with the tool-only fields shown), `i18n/default.json` (`PM_TEXT_TODO`), `README.md`
- Modify: `sdk/guides/emotes.md` ("An emote pack": layout, catalogue fields, the tool, walking clips, what fails and how
  it shows), `docs/emotes.md` (pages, walking emotes, packs; controls table gains paging), `sdk/README.md` index,
  `docs/mod-files.md` if it lists file kinds
- Test: `tests/emotes_catalogue_test.cpp` gains `the_example_pack_parses`; `text_table_test` covers the new keys

- [ ] **Step 1:** Write the test. **Step 2:** Fail. **Step 3:** Draft the files. **Step 4:** Pass, then `.\validate.ps1`
  (the docs checks: no addresses, no obfuscated names, no em dash).
- [ ] **Step 5: Commit:** "Show authors how to make an emote pack".

### Task 15: Eight more bundled emotes

**Owner: Opus** (assets and the private bundle project). **local-qwen** adds the catalogue lines and string-table entries
from the table below.

They fill page 2 exactly, clockwise from the top in alphabetical order, as the first eight do. Icons and the five MP3s
are in `mods/emotes/assets/<folder>/` (gitignored). Icons are 512×512 lossless WebP named `.png`, as the first eight are.
"First package" is `dance-animations.unitypackage` (private repo, `local/.mixamo/`). "Second package" is
`10893-almost-every-icon-series-rarity-dance-4.unitypackage` (the loader repo's ignored `.mixamo/`; copy it into the private
repo's `local/.mixamo/` so `sync.ps1` backs it up).

| Folder | Name | `introClip` | `clip` | `introSong` | `song` |
|---|---|---|---|---|---|
| `build_up` | Build Up | second: `Build Up Start` | second: `Build Up Loop` | second: `Emote_BluePhoto_MX_Intro.wav` | second: `Emote_BluePhoto_MX_Loop.wav` |
| `get_griddy` | Get Griddy | | second: `Get Griddy` | | second: `Emote_Griddles_Music_Loop_01.wav` |
| `lavish` | Lavish | | first: `Lavish` | | user: `08-21. Lavish.mp3` |
| `never_gonna` | Never Gonna | second: `Never Gonna Start` | second: `Never Gonna Loop` | | second: `Emote_NeverGonna_Loop_01.wav` |
| `running_man` | Running Man | | first: `RunningMan` | | user: `06-01. Running Man.mp3` |
| `savor_the_w` | Savor The W | | first: `SavorTheW` | | user: `09-22. Savor The W.mp3` |
| `sprinkler` | Sprinkler | | first: `Sprinkler` | | user: `06-09. Sprinkler.mp3` |
| `stride` | Stride | | first: `Stride` | | user: `10-03. Stride.mp3` |

The second package's loop songs and loop clips are loops already (their clips have Loop Time on), so the tool uses them
as they are. Never Gonna has no intro song, so its loop song starts with its start clip. The five MP3s (123 to 141 s)
need a loop cut like the first eight's.

**The user's rough loop points** (2026-11-12), each the song's loop length from its start, in seconds: Lavish 28,
Running Man 15, Savor The W 15, Sprinkler 22.5, Stride 17. These are the `songLoopGuess` values. The tool finds the
exact loop within a second of each; if the best score there is poor, report it to the user rather than widening the
search silently.

**Files:**
- Modify: `mods/emotes/assets/emotes.json` (the eight after the first eight), `mods/emotes/i18n/default.json`
  (`emotes.name.<name>` for each, with the names in the table)
- Private repo: the clips, songs and icons under `unity/emotes-bundle/Assets/Emotes/` (the clips keep their package
  names; the bundle paths follow the catalogue)
- Modify: `tools/payload.psd1` (the new pin), `docs/emotes.md` (the dances list and the second page)
- Test: `tests/emotes_test.cpp` (`the_shipped_catalogue_parses_with_a_label_for_each` covers all sixteen)

- [ ] **Step 1:** Import the assets and write the catalogue lines. If Task 1 Step 6 shows that Stride walks, ask the
  user whether it is a walking emote and which clip it stands with.
- [ ] **Step 2:** Set `songLoopGuess` for the five MP3s from the user's points above, and `clipLoopGuess` for the first
  package's five clips (their own lengths unless the probe shows otherwise). Run `tools/emote_pack.ps1 -FindLoops -Build`.
  Check that every `EMLOOP` score is good, and send any crossfaded clip to the user to judge.
- [ ] **Step 3:** Run the test, then `.\validate.ps1`. Publish and pin the bundle.
- [ ] **Step 4:** Add to the smoke list: all sixteen play, page 2 holds the new eight with no bare segment, Build Up's
  intro song joins its loop with no gap, and Never Gonna's start clip leads into its loop.
- [ ] **Step 5: Commit:** "Add eight more emotes to the wheel".

### Task 16: Finish

**Owner: Opus.**

- [ ] **Step 1:** `.\validate.ps1` and `.\tools\gamecheck.ps1 -IfConfigured -Expect`. Show both results in the ledger.
- [ ] **Step 2:** Run superpowers:requesting-code-review over the branch, on Opus (local models don't give verdicts).
  Fix what it finds.
- [ ] **Step 3:** The ledger's closing entry lists the smoke list, every `PM_TEXT_TODO` (the page text, the Mods panel's
  pack line, any manifest reasons), any `PM_UI_TODO` (glyphs the game lacks), the assets the user still owes (walking
  emotes' clips, songs, icons and names), and every Claude's call above.
- [ ] **Step 4:** Merge into `development` locally. Push only after the user's in-game test, as the Emotes rule in memory
  says.
