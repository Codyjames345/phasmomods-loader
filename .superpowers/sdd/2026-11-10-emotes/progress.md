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
