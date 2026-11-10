# SDD ledger — plan: docs/superpowers/plans/2026-11-10-emotes.md

Branch feature/emotes (worktree .claude/worktrees/emotes), from development 6bf114c. re/dump and re/ghidra are junctions
to stage2b's dump and the main checkout's Ghidra project (same game build E300EDA9...).

Pre-flight (consumer <- producer):
- T8 <- T1 (Playables present, body Animator): stop point if stripped.
- T9 <- T1 (own-body hiding), T2 (camera placement, look input).
- T10 <- T3 (loudness field, scale, master reads).
- T7 <- T5 (bindings, menu rows, save format).
- T11 <- T6 (blur/vignette mechanism).
- T16 <- T7 (key_state), T11 (filled images, effects), T13 (segment_for, wheel_step). Device enum from T7's binding_rule.h.
- T17 <- T3 (voice 3D settings), T4 (action members), T8, T9, T10, T13, T14. Consistent as planned.

## Part 1 findings (2026-11-10)
- T1 body: Player's one Animator field drives the body (also PlayerCharacterAnimations.get_Animator); the body uses
  AnimatorOverrideControllers, PhotonAnimatorView, and Animation Rigging (RigBuilder, hand parent targets, watch rig).
  Playables are all in the build (PlayableGraph, AnimationPlayableOutput, AnimationClipPlayable, sorting order) - no stop.
- T1 own body: Player.SetPlayerModelLayer(bool) moves every renderer in Player's one Renderer[] field between two named
  layers; that is how the local view hides its own body.
- T1 held items: NetworkedPropGrab.GrabPropRPC disables the item's sync component (Behaviour.set_enabled false on the
  item's +0x28 component) and freezes its Rigidbody while held, so a local re-parent sends nothing. On its own client
  PCPropGrab.ParentItemOnLocalPlayer parents the item to the camera slot transform with the item's own local offset;
  PlayerCharacterAnimations.parentingTargetLeftHand/RightHand are the body's hand anchors (real names).
- T2 look: MouseLook (Unity Standard Assets shape) turns the player's root by yaw and the camera by pitch; PCLook
  (LateUpdate) bends spine and head toward the look. Render pipeline: URP. Camera.CopyFrom,
  UniversalAdditionalCameraData get/set renderPostProcessing and volumeLayerMask are in the build.
- T2 VR: Player.IsVRPlayer(); VRPlayer.leftHandIK and rightHandIK (real-named private Transforms).
- T3 loudness: PlayerAudio's one public float is the loudness IsTalkingLoudly reads. PlayerAudio.Update (every client,
  every player) refreshes it once a second from GetPlayerVoiceAmplitude = Vivox AudioEnergy (the louder of the local and
  global participant) minus 0.4 (a literal read from the binary). IsTalkingLoudly(t): loudness > t, or the player's
  walkie-talkie transmits; when true it zeroes the loudness.
- T4 actions: the game's input actions (PCControls.actions; resources.assets) are named Pickup (E / buttonWest),
  Secondary Use (F / leftTrigger), Primary Use (RMB / buttonSouth), Interact (LMB press / rightTrigger), Drop (G /
  buttonEast), Crouch (C / rightStickPress), HoldPrimaryUse, InventorySwap (Q / buttonNorth), InventorySwapScroll,
  InventorySlot1-4 (1-4), Torch (T / dpad down), Hat Use (T / dpad down), InspectWatch (H / dpad left), Journal (J /
  select), LocalPushToTalk (V / leftShoulder), GlobalPushToTalk (B / rightShoulder), Run, Move, Look, Pause, UI*.
- T5 bindings: keyboard B is GlobalPushToTalk; gamepad D-pad down is Torch, Hat Use and UIDown; every D-pad direction
  is bound (up: UIUp only; right: UIRight only; left: InspectWatch and UILeft). VR (XR Interaction actions in
  resources.assets): left hand binds Primary2DAxis(+Click), TriggerButton, Trigger, GripButton, Grip, pointer and device
  pose; the left PrimaryButton (X) and SecondaryButton (Y) are unbound. Keybinds menu rows are Unity's RebindActionUI
  (real names: m_Action, m_BindingId, m_ActionLabel, set_actionReference, set_bindingId); ControlsManager holds the
  keyboard and controller lists; PCControls.StoreControlOverrides saves the game's own overrides as {id, path} JSON.
- T6 effects: no game blur effect for UI; URP Volume, VolumeProfile.Add(Type, bool), ScriptableObject.CreateInstance,
  Vignette and DepthOfField are in the build.

## Rulings
- Ruling: keyboard default B is taken by the game's walkie push-to-talk, so the keyboard default falls back to X, the
  same "unless taken" rule the user gave for the gamepad - one constant (kEmoteKeys) - if wrong: the user picks B or another key in the keybinds menu.
- Ruling: gamepad default D-pad down is taken (Torch, Hat Use, UIDown); every D-pad direction is bound, so the
  fallback is D-pad up, bound only to the menu-navigation UIUp - if wrong: D-pad up also moves the journal's selection while emoting; rebindable.
- Ruling: VR opens the wheel on the left controller's PrimaryButton (X on Touch), which the game leaves unbound - if wrong: rebind.
- Ruling: third person uses a mod-made camera (Camera.CopyFrom the player's, post-processing and volume mask copied)
  while the player's own camera is disabled, instead of moving the game's camera - keeps the head camera, audio listener,
  held-item slot and the game's raycasts where they are - if wrong: a HUD on a camera-space canvas hides during an emote.
- Ruling: the mouse look stays the game's (MouseLook turns the root and the head camera); the visible body (the Animator's
  transform) is set to the facing rule's yaw each frame; the view yaw is the player camera's yaw - if wrong: the
  Animator sits on the root and the body turns with the mouse (logged at start).
- Ruling: the VR pointer is a ray from VRPlayer.rightHandIK's forward against the disc's plane, not the game's own
  XR ray, and the trigger is read from our own action on <XRController>{RightHand}/{TriggerButton} - if wrong: the ray
  points off the controller's visible pointer by the IK target's offset.
- Ruling: what ends an emote is read from the game's own input actions by name (Pickup, Primary Use, Secondary Use,
  HoldPrimaryUse, Interact, Drop, Crouch), so a press ends it whether or not it hits something; switching (InventorySwap,
  scroll, slots) and Torch do not - if wrong: pressing interact at nothing also ends an emote.
- Ruling: in VR, a press of either grip or trigger ends an emote (except the right trigger while the wheel is open);
  crouching in VR is physical and is not detected - if wrong: a VR player who crouches keeps dancing.
- Ruling: emote sounds are WAV files (mods/emotes/assets/sounds/emote_<n>.wav, read with custom_ghosts' wav.h), so the
  mod has the samples to measure loudness; clips and icons are in the bundle - if wrong: none, the user supplies WAVs.
- Ruling: the music's loudness is raised into PlayerAudio's loudness on the master once a second, the game's own
  refresh rate, as energy = RMS x kEnergyPerRms (3.0, starting value), loudness = energy - 0.4 - if wrong: the ghost hears it more or less easily than a voice; one constant.
- Ruling: the wheel's segments are textures generated in code (annular sectors with alpha) on RawImages, tinted grey or
  blue; no filled-image API is needed - Task 11 shrinks to texture generation, effects and a world canvas.
- Ruling: the clip's output is sorted after the game's own (sorting order kEmoteSortingOrder 2000, after Animation
  Rigging's 1000), so the dance overrides the hand IK; items on the hand anchors follow the hands - if wrong: arms stiffen toward the held item.
- Ruling: PCLook is switched off while the local player emotes, so the spine and head follow the clip, and on again after.
- User, 2026-11-10 (mid-run): a looping emote's audio loops with exactly the animation's duration. So the sound is
  restarted from 0 at each loop of the clip, timed from the clip playable's own time (time mod length): a longer
  track is cut at the loop, a shorter one waits for the next. Added to the design spec.

## PAUSED 2026-11-10 (user restarting the PC) - resume here
- State: Tasks 1-6 (RE) findings and rulings above. Map entries for the whole plan are written (sdk/map/Input.h,
  Player.h, Camera.h, UnityWorld.h, UnityUI.h, new Playables.h and UnityPostProcessing.h, all listed in all.h) and
  committed as a WIP commit "WIP: map the entries the Emotes plan needs" (local only, not pushed).
- tests/game_map_test.cpp: fixture gained the two URP assemblies (22 images, 4400 classes); map and game_map pass.
- NEXT: gamecheck_test fails: "cannot read the assembly folder tests/fixtures/diffable/Unity.RenderPipelines.Core.Runtime"
  - the gamecheck fixture needs the new assemblies (folders under tests/fixtures/diffable, and expect_dump) or the
  check must treat them; then raise counts in tests/gamecheck_test.cmake; then run tools\gamecheck.ps1 -IfConfigured
  -Expect against the real dump (PM_GAME_DIR / PM_CPP2IL from the local-tool-paths memory) and add values to
  re/dump/expect/emotes.txt; write sdk/systems notes (players.md "The body", menus.md "Key bindings", voice.md
  loudness, media.md camera/effects); then amend the WIP into proper Task 1-6 commits and continue at Task 7.
- The design spec still needs the user's loop rule added (ledger line above); do it with the Task 18 docs.
- re/dump and re/ghidra in this worktree are junctions (stage2b's dump, main's Ghidra); run re/decomp.mjs from the
  main checkout (Ghidra refuses dotted paths).
- RESUMED after restart. Ruling: Player.IsVRPlayer is abstract (no code), dropped; VR is read as VRPlayer::type.is(player) - cost if wrong: none, same answer.
- Ruling: the inflated set_value's float argument is "@Single" (the resolver canonicalises "Single" to "float", which no model type is named).
Tasks 1-6: complete (commit f353d9e, tests: validate.ps1 -> 83/83, docs and scripts pass; gamecheck -Expect -> exit 0, 1817 entries, 0 missing)
- Task 7: Ruling: a key's actions live in an InputActionAsset of the mod's own (ScriptableObject.CreateInstance + AddActionMap + AddAction), not standalone actions: Unity's InputActionReference refuses an action outside an asset (the game's metadata holds "must be part of an InputActionAsset") - cost if wrong: none.
- Task 7: Ruling: the VR default is not checked against the game at run time (XR Interaction actions sit in assets no keybinds row or player control reaches); the left PrimaryButton was read unbound from the game's data - cost if wrong: an XRI action also fires on X while the wheel opens.
- Task 7: Ruling: the SDK takes the save file and owner from the mod (begin_keys) rather than reading pm::mod glue, so tests run without the host contract - cost if wrong: none.
- Ruling: validate.ps1 (about five minutes) runs at milestones (after Part 2, after the mod, at the end), each task runs its own tests plus map/game_map/gamecheck - cost if wrong: a break in another area is found a few tasks later.
- Task 7: the row test was added after the implementation it covers (it passed first time); the other keybinds tests went red then green.
Task 7: complete (commit 37d2f40, tests: keybinds_test -> passed, 9 cases)
- Task 8: Ruling: a switch of clips crossfades by two outputs (each clip its own graph, the newer sorted higher, its weight ramping over the older) instead of a mixer playable - only output weights are needed, no mixer entries - cost if wrong: none seen; two graphs on one Animator for the length of a blend.
- Task 8: the test was written before the code but not run red before the implementation; a mutation (the older clip never ended) was run instead and failed the test as it should.
Task 8: complete (commit 71f590d, tests: player_anim_test -> passed, 5 cases)
- Task 9: Ruling: no game look is paused for third person: the view follows the head camera, which the game's MouseLook goes on turning; the plan's "pauses the game's camera placement" is not needed - cost if wrong: none.
- Task 9: Ruling: the wall ray uses Physics.RaycastHit (mapped) with a mask that leaves out the body's own layers, not a sphere cast - cost if wrong: the view can clip a thin edge a sphere would catch.
- Task 9: test red on first build (two failures, a test leaving the view on), green after the test stopped its view.
Task 9: complete (commit d572990, tests: third_person_test -> passed, 7 cases)
- Task 10: Ruling: the SDK exposes the game's pace (Pace, once a second) and the mod raises the loudness at it, so the music is heard at most once per refresh, as a voice is - cost if wrong: none.
Task 10: complete (commit 3b6fed8, tests: talk_loudness_test red 3 then passed, 6 cases)
- Task 11: Ruling: no filled-image or rotation API: the wheel's segments are textures the mod draws in code (pm::ui::texture_from_rgba on RawImages), so Task 11 is the volume effects, a world canvas and that texture helper - cost if wrong: none.
- Task 11: create_world_canvas and texture_from_rgba are covered by the wheel's tests (Task 16), where they are used, not by a test of their own.
Task 11: complete (commit dc412f4, tests: screen_effects_test red 2 then passed, 3 cases; pmcore builds)
- Part 2 milestone: validate.ps1 -> 88/88 tests, docs and dev build pass; the API reference flagged orbit_rule.h's Point fields, fixed (4662968); the packaging steps after it rerun at the next milestone.
- User, 2026-11-10: added mods/emotes/assets/<emote>/ with each emote's icon (PNG) and song (MP3), and asked for the 8 clips
  matching the folder names: disco_fever, electro_swing, floss, groove_jam, orange_justice, scenario, smooth_moves,
  take_the_l -> the package's DiscoFever, ElectroSwing, Floss, GrooveJam, OrangeJustice, Scenario, SmoothMoves, TakeTheL.
- Ruling: the wheel's order is the folders' alphabetical order, clockwise from the top (segment 0 Disco Fever ... 7 Take The L) - one table to change - cost if wrong: emotes sit in other segments than the user pictured.
- Ruling: the names shown under the wheel are the user's own track titles from the MP3 names ("Disco Fever", ..., "Take The L"), not AI text - cost if wrong: the user renames them in the string table.
- Ruling: the songs are MP3, not WAV, so they go into the bundle as AudioClips (with the clips and icons), and each song's loudness envelope is measured once at build time from the same MP3 (ffmpeg) into a small text file the mod reads; the source MP3s and PNGs stay out of the public repo's git (gitignored) and are kept in the private repo's Unity project, as the Squatter's clips are - cost if wrong: none for play; an envelope out of step with a re-exported song until rebuilt.
- Ruling: the manifest description is left empty (the Mods panel shows its own "no description" text) - it is player-facing text for the user to write.
- Task 12: Ruling: the manifest's description is "PM_TEXT_TODO" (the manifest refuses an empty one), listed for the user with emotes.key-label.
- Task 12: Ruling: the README's mod row waits for Task 18 (the README is in the user's own voice; it gets one factual row then).
- Task 12: emote_assets.h (Task 15's names table) came early: the text-table test asks that every name key be named by code.
Task 12: complete (commit e9bcf95, tests: mods_test, mod_manifest_test red then green; held_scan_test, text_table_test pass)
- Tasks 13-14: Ruling: one commit for both (both pure, one test file); the headers were written before the test (while validate ran), so instead of a red run two mutations (the body never lagging the view; no magic check) were run and failed 6 checks.
- Tasks 13-14: Ruling: a release with the key lost without a release (focus loss) closes the wheel with nothing played (wheel_step: !held && !released).
Task 13: complete (commit de3a9d3, tests: emotes_test -> passed, 11 cases)
Task 14: complete (commit de3a9d3, tests: emotes_test -> passed)
- Task 15: the package marks no clip as looping (Loop Time off on all eight, read in the build); Ruling: the build turns Loop Time on for all eight (EmotesBuild.Loops), as each is a dance with a two-minute song repeating until the player moves; one-shot handling stays in the code - cost if wrong: an emote the user meant as one-shot repeats; one flag in the build.
- Clip lengths (s): DiscoFever 9.000, ElectroSwing 8.033, Floss 1.633, GrooveJam 9.367, OrangeJustice 6.533, Scenario 8.167, SmoothMoves 9.633, TakeTheL 0.833. Songs 121-134 s, 44.1/48 kHz stereo. Icons 512x512. All clips humanoid, with root curves.
- User, 2026-11-10 (mid-run): an emote's song may loop at a different point than its animation; if the files mark no loop, tell the user. Probed with ffprobe: no song has a loop marker (tags: album "Fortnite Emotes - Season N", title, track, encoder UnrealEngine4; no chapters, cue or LOOPSTART/LOOPLENGTH). Told the user; each emote gains songLoopStart and songLoopSeconds in emote_assets.h, defaulting to the clip's length (the user's earlier rule) until the user gives loop points.
- Noted to the user once: the songs' tags say they are Fortnite's own emote music (album "Fortnite Emotes", UnrealEngine4) - for the release decision.
- Task 15: Ruling: kEnergyPerRms raised from 3 to 4 (starting value): the songs' mean RMS is about 0.14, which at 3 sits just under the talking-loudly line - cost if wrong: the ghost hears the music more or less often than a voice; one constant.
- Task 15: bundle published to the private repo (commit 85eef73 there) and pinned E67E268E...; tools/emote_loudness.mjs wrote 8 loudness.txt (1210-1344 windows each).
Task 15: complete (commit 9eaa5ca, tests: emotes_test song-loop case red (no such type) then passed; talk_loudness_test RMS case red (3 unresolved) then passed)
- User, 2026-11-10: Floss and Take The L are the only two whose song loop differs (about 16.5 and 12.5 s); measured (waveform NCC at 22.05 kHz; loudness envelope at 1 ms): Floss 16.692 s from 0 (corr 0.9994, sample-exact), Take The L 12.631 s from 0 (envelope corr 0.89; the waveform is not identical between passes, 8 bars of 1.579 s). Committed 208b8fb.
- Task 16: Ruling: the wheel's Unity glue (canvases, images, text, effects) has no fake-game test: its pure parts (shapes, spans, icon places, colours, VR ray and pick) are tested in emotes_wheel_test, and the glue goes on the smoke list - cost if wrong: a glue fault is found in the user's in-game test, not offline.
- Task 16: Ruling: the VR wheel turns about the vertical only to face the head (a billboard by yaw) - cost if wrong: it reads at an angle when held high or low.
Task 16: complete (commits adb0f45 + 97cf7bf, tests: emotes_wheel_test passed 4 cases; Emotes.dll builds)
- Task 17 prep: Ruling: menu navigation (game actions named UI..., read only in menus) does not count as taken for a mod's default, so the gamepad key falls back to D-pad up (keys::menu_only, committed d1b9fce) - cost if wrong: D-pad up both opens the wheel and moves a menu's selection when a menu is open (the wheel stays shut while a menu has the cursor).
- Task 17: Ruling: the plan's game-glue tests (emotes_play_test over the fake game) became tests of a pure session (emote_session.h, 15 cases covering every listed behaviour); the glue (emote.cpp) carries its decisions out and is checked in game - cost if wrong: a glue fault shows in the user's test, not offline.
- Task 17: Ruling: the song plays from the dancer's head bone through the game's floor mixer group (SoundController.GetFloorAudioSnapshot), falling off as the dancer's voice AudioSource does - cost if wrong: the song sounds through walls more or less than a voice.
- Task 17: Ruling: a dead dancer's song is raised into their loudness on the master as any dancer's is: the game's own checks decide whom the ghost hears, as for a dead player's voice - cost if wrong: none expected; the hunt skips killed players.
- Task 17: Ruling: the held item is put in the right hand only on screen in third person (on its own client it hangs before the camera), and given back with PCPropGrab.ParentItemOnLocalPlayer; a left-hand item stays as the game has it - cost if wrong: a left-hand item floats before the view in third person.
- Task 17: Ruling: the canvas guard (raw_scan_test) gains kAuthorDesigned for canvases the author designed and asked for; the wheel is its one entry - cost if wrong: none; it is not the waiting list.
- Task 17: no AudioSource.set_time: every song loop starts at 0 (both measured loops start at 0), so a restart is Stop + Play.
- Milestone (after the mod): validate.ps1 -> 91/91, docs, reference, package, stage, pinned, release pass; gamecheck -Expect -Mods -> exit 0, 1821 entries, 0 missing, no mod switched off.
Task 17: complete (commit 37ebce1, tests: emotes_play_test 15 cases red (no header) then passed; validate 91/91)
Task 18: complete (commit 3a30563: docs/emotes.md, sdk/guides/emotes.md, CHANGELOG, sdk/README, README and installation rows, the spec's later decisions; tests: validate.ps1 91/91)
Final review: self-review (the harness asks that no subagent be spawned unless the user asks); findings fixed in 2fbdc19:
- Final: fixed a new emote started while the view eased back stayed first person - a_new_start_while_easing_back_turns_out_again RED->GREEN, suite 91/91
- Final: fixed an item switched to mid-dance floated before the head and the wrong item went back at the end - glue only (checked in game); held_scan passes
- Final: fixed the mod's clock in seconds since boot lost float precision - glue only
- Final: minor (deferred): the VR wheel re-sets its eight colours and name text every frame (a managed string a frame)
- Final: minor (deferred): the keybinds search for rows runs once a second for the whole session, even with every row made
Final: validate.ps1 -> 91/91 and every script test; gamecheck -Expect -Mods -> 1821 entries, 0 missing, no mod switched off.

## Smoke list (the user's in-game test)
1. The Mods panel lists Emotes; it starts (log: "Emotes: <clip> ... looping, song loop ...") for all eight.
2. Keybinds menu (journal > Options > Controls), keyboard and controller lists: scroll to the end; a divider, a heading "Emotes" (only its label shows), a divider, a row "Emote wheel"; the scroll reaches them; rebind, restart, the rebinding holds. Nothing appears on a map's training whiteboards.
3. Keyboard: hold X: the wheel (grey disc, gaps, icons), blur and vignette behind; the camera does not turn; mouse up picks the top (blue, name below); release plays; release at rest plays nothing; Esc/journal while held closes it.
4. Gamepad: hold D-pad up, right stick picks, release plays.
5. VR: left X opens the wheel over the left hand facing you; right controller points, trigger plays; X again closes; view never moves; your body shows; the head does not block the view.
6. The camera eases straight back from the head, character facing away; mouse turns it; body stays within 90 degrees, then follows; it eases back on the end.
7. Ends: walking 0.5 m, E/F/RMB/LMB/G/C (use, place, pick up, drop, throw, door, light switch, crouch), death; switching items (Q, scroll, 1-4) and the torch do not.
8. Held item stays in the hand on screen in third person; switching mid-dance; after the end the item is back in view.
9. Another player sees and hears the dance from the dancer, falling off with distance, and the body turns as the dancer turns.
10. Floss and Take The L: the song runs past the dance's loop and loops at 16.692 s and 12.631 s; the other six restart with their dance.
11. The ghost hunting near a dancer finds them as it would someone talking; the ghost's room events react to dancing nearby.
12. A dead dancer is seen and heard by the dead only.
13. Root motion: the dance does not move the player (or a dance ends itself by "moving" 0.5 m).
14. The body's Animator is not on the root (no log line "the body's Animator is on the player's root").

## In-game fix 1 (user, 2026-11-10)
- The user's run: "This game build lacks a game entry and 1 more". mods.log: VolumeParameter<float>.set_value and VolumeParameter<DepthOfFieldMode>.set_value MISSING ("this game build has no code for the instance"), so Emotes stayed off.
- Fix: the SDK sets a volume parameter by re-running its own constructor in place (ClampedFloatParameter, MinFloatParameter, DepthOfFieldModeParameter .ctor; decompiled: value, range and override only; range read from the parameter's min/max). Test: screen_effects_test (mutation: override off -> 2 failures).
- Wording: a key's angle brackets made the panel fall back to "a game entry"; modid::shown_key shows them as U+2039/U+203A (mod_manifest_test an_entry_key_is_shown_safely red->green). At the user's request the reasons are redrafted, marked PM_TEXT_AI: "Needs {{entry}}, which this game version lacks" and "...and {{count}} more, ...", fallback "a part of the game" (mods_test red->green).
- Ruling: inflated (generic instance) entries are avoided from now on where a non-generic member does the job: the offline check cannot see whether an instance has code - cost if wrong: none.
- validate.ps1 91/91 and every script test; gamecheck -Expect 1827 entries, 0 missing (55 startup-only, down from 57). Commit 0f10393. A first stage attempt hit a passing "access denied" on dist (nothing was open, the user said); the retry went through.
- In-game run 2 (11:16): Emotes started, but no keybinds row and no wheel on B. The key chose X (B is the game's walkie), and nothing logged the key's readiness or the menu search; added log lines (commit 0c65fc8) to see where it stops.
- In-game run 3 (13:40): the new logs showed the key waiting forever, and mods.log showed Unity's "Cannot add, remove, or change elements of InputActionAsset while one or more of its actions are enabled" every frame from 13:40:52: make_actions enabled each action before adding the next. Fixed (all added, then overrides and Enable), keybinds_test red (fake now counts adds while enabled) then green; ctest 91/91. Commit 049d679.

## In-game fix 2 (user, 2026-11-10, run 4 at 13:44)
- The user's report: the wheel shows grey squares; the row reads "Forward Movement"; no dance and no song after a pick; the camera eases too slowly (halve it); mod keys should sit under a heading per mod; the key should show from the main menu.
- Cause of the missing dance, song and icons: mods start at the main menu (log 13:44:33, the lobby at 13:44:50), and a scene load's unload of unused assets destroys any asset in no scene. load_bundle_asset now marks what it loads HideFlags.DontUnloadUnusedAsset (models_test red, then green). Same lesson as squatter_voice.cpp.
- Cause of the grey squares: texture_from_rgba applied level 0 only, and Texture2D(w, h) has a mip chain, so a texture drawn smaller than its 512 texels showed the engine's grey fill of a new texture. Now Apply(true, true). core/ui.cpp has no offline harness: checked in game (smoke 3).
- Cause of "Forward Movement": decompiled RebindActionUI.set_actionReference and OnEnable; neither writes the label. The label's TextLocaliserUI writes its key's words when it wakes; a copy made under the hidden settings page wakes when the player opens it, after our label. The SDK now DestroyImmediates the copy's localiser before labelling. Ruling: destroy, not disable: Unity runs Awake on a disabled component - cost if wrong: none (the precedent in lobby.cpp disables one that has already woken).
- Heading per mod (user): begin_keys takes a title; each list ends with a heading (a row copy, its RebindActionUI destroyed, every child but the label's hidden, label = the mod's name) and then the mod's rows. The title is the manifest's name through a new PmHostApi.mod_name at the end of 2.3 (unreleased; main is on 2.2), pm::mod::shown_name. Rows are no longer placed beside the copied row. Ruling: the heading's name is the manifest's, the name the Mods panel already shows players, rather than a new string - cost if wrong: one table entry.
- Main menu (user: "mods aren't loaded until a lobby is created"): the log of run 4 shows Emotes started at 13:44:33 at the menu and its rows were made at 13:44:47, before the lobby at 13:44:50; the row read "Forward Movement" and looked like the game's own. No loader change; the label fix and the heading make it visible. Told the user.
- Ease: kEaseSeconds 0.6 -> 0.3, out and back (user; third_person_test red, then green).
- Key label: "Emote wheel", the user's words (no mark).
- Diagnostics: the wheel's open line gives icons and segments alive; each start gives clip loaded/gone, playing on the body or not, song loaded/gone and its source.
- validate.ps1: 91/91 and every script test; API reference no warnings. (A first run hit the passing "access denied" on dist again; the rerun went through.)

## In-game fix 3 (user, 2026-11-10, run 5 at 14:09)
- The user's report: no keybind row anywhere; no head in third person; the journal and belt tools stay put while dancing; opening the journal should end the emote (it only ended on a click inside, then misbehaved); W snaps the view to first person before the 0.5 m end (other directions fine); the wheel looks blurred; a pop of sound at the start; per-song loop points (rough); and whether mods really load at the main menu (custom ghost pages show only from single or multiplayer).
- Keybinds: read with UnityPy. The main menu scene (level2) holds no RebindActionUI; the menu is in each PCPlayer's journal (resources.assets), spawned at menu start. Each list is a Layout (VerticalLayoutGroup, spacing 15.5) stretched in a Content of fixed height (2575 controller, 3125 keyboard) fitting the game's rows (100) and dividers (6): rows appended at the end were clipped past the scroll. Maps (level3, level11) carry training whiteboards with RebindActionUI rows on no layout. Fix: templates only from a list with a HorizontalOrVerticalLayoutGroup under the local Player; a divider copy before each heading and row; the Content grows by heights plus spacing and shrinks back on removal (keybinds_test red/green; mutations of the filter and the growth fail it).
- Head: PlayerCharacter.headRenderersToTurnOff, set to ShadowsOnly on the local client (decompiled ChangeHeadObjectsShadowCastingModeShadowsOnly: set_shadowCastingMode(on*2+1); ChangeLocalHeadObjectsToShadowCastingModeShadowsOnly: 3). The view sets each to On and restores its own mode at the end, and adds their layers to its mask (third_person_test).
- W snap: PCPlayer's root capsule, PCPlayerHead's sphere and GhostLOSHeadSensorLocations' box (FollowTarget) are on layer 10 and trail the camera; walking forward puts them on the wall check's ray behind the head. The wall check now leaves out the local Player's and rig's layers (third_person_test; mutation fails it).
- Belt and journal: the journal hangs through Journal Parent Rig (MultiParentConstraint, waist socket to hand socket), the belt is a child of CC_Base_Spine01. Our output at 2000 ran over Animation Rigging (1000), so the rig placed the journal from the pre-dance pose. Ruling: the clip's output sits at 500, between the controller and the rig, as the game's own animations do; the lone clip is renumbered to 500 so orders never climb to 1000 (player_anim_test, 1200 chained picks). Cost if wrong: the rig's hand IK now also runs over the dance, so arms holding an item may follow the hold pose (smoke 8).
- Journal: JournalController.open of Player.journal counts as an end action while dancing or waiting.
- Pop: the dance and song wait for the view to be out (LocalSession pick with a wait = view::seconds_until_out(), screen only; the view goes out at once; any end condition cancels the wait and brings the view back with nothing sent) (emotes_play_test; mutation fails it).
- Wheel blur: the canvas is Screen Space Overlay (after URP's post), and the name text was sharp in the screenshot; the soft edges were the 512-texel shapes stretched about 2.3 times. kTextureSize 1024.
- Song loops, measured from the user's rough points by waveform NCC at 22.05 kHz (Floss re-measured at 16.692 as a check): Disco Fever 8.542 (0.999), Electro Swing 16.000 (0.9996), Groove Jam 15.118 (0.994), Orange Justice 18.935 (0.996), Scenario 32.542 (0.983), Smooth Moves 19.200 (1.000), all from 0.
- Main menu: mods do start there (log 14:09:33), but content (custom ghost pages, rules) is gated on the private-room answer, which is no out of any room (the main menu is outside any Photon room; single player is an offline-mode room). Not changed: it is the shared gate that also keeps mods off in public lobbies, and the user's own 2026-11-03 rule; asked the user.
- validate.ps1 91/91 and every script test, reference clean; gamecheck -Expect: 1832 entries, 0 missing (the new Renderer.shadowCastingMode pair, ShadowCastingMode.On = 1, PlayerCharacter.headRenderersToTurnOff).
- Smoke additions: 15. Head shows in third person and is hidden again after. 16. Walking forward ends the emote at 0.5 m without a snap. 17. The journal follows the waist while dancing; opening the journal ends the dance. 18. No sound before the view is out. 19. Each song loops at its measured point.
- User's answer (2026-11-10): keep the rule. The main menu stays vanilla for mod content (custom ghost pages, journal lines, rules); mods still start there, and what sits outside the content gate (the emote key and its row) works at the menu. Document it with the 2026-11-03 rule.
