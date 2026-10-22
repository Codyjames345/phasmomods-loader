# Lobby ghost controls visibility implementation plan

> **For agentic workers:** Use superpowers:executing-plans or superpowers:subagent-driven-development. Implement the measured correction, then obtain independent review and verify in a fresh game session.

**Goal:** Make the existing native-looking ghost controls visible and clickable in private/single-player lobbies, without changing Ready behavior, gameplay or rewards.

**Architecture:** Retain native control cloning. Correct the proven coordinate-unit and inherited-interactability defects, resolve the actual native label, and reject incomplete clones.

**Tech Stack:** Existing C++20/Unity IL2CPP helpers/CMake/CTest; no dependency or new canvas.

**Spec:** Existing lobby behavior in `docs/design.md`; user reports invisible Play as ghost despite successful native activation.

## Measured causes and remaining question

The user's latest private-lobby diagnostic run (PID 53780) establishes Ready enabled=1, interactable=0, activeSelf=1 and activeInHierarchy=1. Clones exist and share the active hierarchy. Ready's rect is roughly 0.1 native units wide; one-decimal formatting rounds its height to 0.0. Clone positions are approximately -8.1, -16.1 and -24.2 because `kGap=8` assumes pixels while this native hierarchy uses much smaller units. These positions are not a reasonable separation from the source control. The cloned controls also inherit interactable=false. Activating already-active clones cannot repair either cause.

Every cloned TMP label lookup returns null. That proves the current label lookup is wrong or the label is outside the cloned subtree; it does not prove which text class the native label uses. The parent is examining the original layout and GameAssembly offline. Consume that evidence before choosing the label binding. Do not invent a text object or broad TMP/legacy fallback to hide the unresolved hierarchy.

Existing helpers `ui::destroy`, `ui::set_text`, `game::game_object_of` and binding conventions are reusable. `lobby::tick` has one caller in `room.cpp`; `make_clone` serves all three controls. The latest implementation logs clone failure but still retains incomplete clones and polls them. Initial inherited isOn can be mistaken for a click, and failure to create a fresh event can retain Ready listeners. These defects belong to safe completion of this same clone operation.

## Task 1: Finish the one missing structural fact

**Modify only if required:** `src/lobby.cpp` diagnostics. Parent owns offline label investigation.

- [ ] Record source rect width/height, position, sizeDelta and parent rect with `%.6g` precision; never infer zero height from the rounded log.
- [ ] Identify the actual Ready label type and ownership path, including whether it is a child, a sibling or referenced elsewhere. Record the target that receives native Ready text and the component controlling localization. Inspect the smallest useful subtree, not the entire scene.
- [ ] Identify existing native row spacing and available space near the Ready control. Use the measured sibling pitch where one exists; otherwise derive spacing from the source's actual rendered height in the same coordinate system. Retain the existing native appearance.
- [ ] Write these concrete observations into the implementation handoff. No additional activation-state experiment is required for the already-proven active hierarchy.

## Task 2: Repair the native clone in one reviewed change

**Modify:** `src/lobby.cpp`, `CMakeLists.txt`.
**Create:** `tests/lobby_test.cpp`.
**Modify for named fields:** `src/il2cpp.h`, `src/il2cpp.cpp`, and the startup test's runtime-export fakes. Resolve `readyToggle`, separate `readyText` (TextMeshProUGUI), and `onValueChanged` by name through runtime field metadata. Read/write reference fields through the IL2CPP exports rather than guessing another instance offset. Existing `ui::set_text` already supports the proven label type.

**Interface:** Keep `lobby::resolve`, `lobby::tick(session::Session&)` and `lobby::teardown` unchanged. `make_clone` internally accepts the intended initial on/off state, and returns either a complete isolated control or an empty Clone. Preserve the three control semantics and session APIs.

- [ ] First add a deterministic fake-Unity boundary test that executes production clone/tick code, following `tests/startup_test.cpp`. Model an active native Ready control with interactable=false and a small nonzero rendered height. Assert the clone is interactable, in the intended native coordinate region, has its actual label, and does not mutate Ready. Run the new test and observe failure before implementing.
- [ ] Add scale-invariance assertions: multiply all source/parent/native spacing coordinates by a factor and require clone positions to scale by that factor. This catches any remaining pixel constant such as kGap=8. Test zero/invalid geometry rejects creation instead of generating off-board controls.
- [ ] Remove absolute pixel spacing. Use the measured native row pitch, or a dimensionless gap relative to actual source rect height when no native pitch exists. Select the available direction from the measured native layout; check all three rows fit. Do not merely replace sizeDelta with rect.height while keeping the 8-unit gap, and do not invent a screen-space overlay.
- [ ] Resolve the proven native label at the actual ownership location and clone the smallest complete native visual unit containing control and label. Keep native font, material, scale, raycast and localization styling. If label is external, clone and own that label explicitly with the control rather than rewriting the original. Destroy both owned pieces together.
- [ ] Make configuration transactional: require fresh ToggleEvent, valid label and valid geometry; detach from native ToggleGroup; set initial state silently; then set the clone interactable=true and activate/enable only the clone if necessary. Failure destroys all partial owned objects. Never fall through with Ready listeners still attached.
- [ ] Initialize shownOn from the provided initial session state before polling clicks. Test a source with isOn=true does not volunteer automatically, trigger a native Ready callback or alter a native group member. If source group membership creates OnEnable side effects during Instantiate, isolate creation before enabling the clone.
- [ ] Add tests for missing label, event allocation/constructor failure, null clone, host-role loss and room exit/re-entry. Failed creation must not log success or retry cloning every frame; bound retries to the existing lobby discovery cadence and log the cause once per source/failure state.
- [ ] Preserve process-scoped diagnostics at useful precision; log successful complete creation and final interaction state once. Do not call a control visible solely because its hierarchy is active.
- [ ] Run the full documented validation command, including all CTest tests, and obtain independent review of source, tests and ownership/cleanup.

## Runtime acceptance

Build the corrected DLL/package, close the prior pinned game process and start a fresh one. In the private lobby with one player, visually locate and click Play as ghost. Verify the selected state changes and ghost assignment appears, while native Ready/Start stays unchanged. Verify all three host controls fit and their labels remain correct after localization refresh. Repeat single-player and a second screen size; leave/rejoin the lobby and confirm no duplicate controls or repeated exceptions. Check private non-host with a second client when available and state that limit otherwise.

Keep the new screenshot and fresh PID-specific log together. Neither native active nor clone-created logs prove this acceptance. Preserve gameplay/rewards. FINDINGS entry 1 remains until full contract verification; unrelated existing findings, UI redesign, a new canvas, public-room behavior and broad binding refactors are outside scope. No Git/CI exists; report local checks rather than a pipeline result.
