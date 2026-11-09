# The Squatter's Physical Animation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. (The user runs plans inline: superpowers:executing-plans.)

**Goal:** The Squatter's body falls as a ragdoll and gets up from where it lands, its clumsy moves land where the world
needs them, and its clips change by inertialization, so its animation connects to the house.

**Architecture:** A pass after the Animator and before Final IK (a hook on `IKExecutionOrder.LateUpdate` for the figure's
own instance) applies inertialization offsets and the ragdoll's blend back into animation. A ragdoll of Unity's own parts
(Rigidbody, collider, CharacterJoint per bone) on the game's `DeadPlayer` layer is built with the figure and stays
kinematic until a fall. The master decides falls and warped moves and shares them in the Shown; every client simulates its
own fall; the master's fall decides where the ghost stands up.

**Tech Stack:** C++20 mod (`mods/custom_ghosts`), the game map and API (`sdk/map`, `sdk/api`), the fake game for offline
tests (`sdk/fake`, `tests/`), Unity 2022.3.40f1 for the bundle (`C:\Dev\phasmomods-private\unity\squatter-bundle`).

**Spec:** `docs/superpowers/specs/2026-11-09-squatter-physical-animation-design.md`

## Global Constraints

- Every game member used is a map entry, cited by key, decompiled or checked; `tools/gamecheck.ps1 -IfConfigured -Expect`
  passes with 0 missing; raw addresses only in gitignored `re/dump/expect` and `re/dump/notes`.
- Never invent game behaviour: what a door's swing or a thrown object reads from the game is decompiled or Unity's
  documented API, and the note says which.
- The ragdoll never moves anything networked: its colliders are on `Layer.DeadPlayer`, which collides only with Default,
  Floor, Wall, Camera, DefaultNoNavigation, StaticLights, InvisibleWalls and StaticProps (the game's physics matrix).
- Tests first, red then green; `.\validate.ps1` passes after each task; commit messages per CLAUDE.md, ending with the
  Co-Authored-By line.
- Every starting value is a named constant with a comment saying it is a starting value, judged with the user in game.
- The bundle is rebuilt only in the private repository's project and published with `tools/publish_assets.ps1`.
- After each task: append to `.superpowers/sdd/2026-11-04-squatter/progress.md`.

## Review Focus

1. A fall that starts inside furniture, on stairs or against a wall: the body must not sink into the floor or launch.
   Owned by Task 5 (settle limit and a launch guard) and Task 6 (the ghost placed onto the navigation mesh).
2. A second knock while already down, or a knock during an act (kick, door, bang): it must not stack or restart. Owned by
   Task 7 (`may_fall`).
3. The master changing mid-fall: the new master must not leave the ghost where its own fall did not land. Owned by Task 7.
4. The figure destroyed mid-fall (scene load, contract end): its ragdoll parts go with it, nothing physical is left.
   Owned by Task 5.
5. A body landing on its side, where face up and face down are close: it must still pick one get-up and not flip.
   Owned by Task 6 (`get_up_for` tie rule).

---

### Task 1: Prove the hover's cause

**Files:**
- Modify: `mods/custom_ghosts/squatter.cpp` (the root-motion warp in `run()`, "A roll, a trip or a landing carries it")

- [ ] **Step 1:** While a clip carries the body (`moves_by_clip`), log once every 250 ms: the move, the clip's step
  length this frame (`figure::clip_step()`), the agent's actual movement since the last frame, and whether `Warp`
  returned to a point off the mesh (`NavMesh::SamplePosition` within 0.1 m of the target fails).
  `PM_LOG("squatter: carried by %s: clip %.3f m, agent %.3f m, off the mesh %d", ...)`.
- [ ] **Step 2:** `.\validate.ps1` passes; commit "Log what carries the Squatter during a clip, to find the hover".
- [ ] **Step 3:** Record in the ledger that the user's next test settles the cause; Task 8's warp onto the mesh is the
  fix if the log shows steps off the mesh.

### Task 2: Map the physics the ragdoll and its knocks need

**Files:**
- Modify: `sdk/map/UnityWorld.h`, `sdk/map/UnityCloth.h`, `sdk/map/LevelRoom.h`, `sdk/map/Scene.h`, `sdk/map/FinalIK.h`,
  `sdk/map/all.h`
- Test: `tests/game_map_test.cpp` and `tests/gamecheck_test.cmake` (counts), the game check

**Interfaces:**
- Produces: `phasmo::CharacterJoint` (`type`, `set_swingAxis`, `set_lowTwistLimit`, `set_highTwistLimit`,
  `set_swing1Limit`, `set_swing2Limit`), `phasmo::Joint` (`set_connectedBody`, `set_axis`), `phasmo::SoftJointLimit`
  (struct: `float limit, bounciness, contactDistance`, passed by address), `phasmo::Rigidbody::set_mass`,
  `set_useGravity`, `get_angularVelocity`, `phasmo::BoxCollider` (`type`, `set_size`, `set_center`),
  `phasmo::SphereCollider` (`type`, `set_radius`, `set_center`), `phasmo::CapsuleCollider::set_center`, `set_direction`,
  `phasmo::Physics::OverlapSphere(const Vector3*, float, int32_t, int32_t) -> void* (Collider[])`,
  `phasmo::Collider::get_attachedRigidbody`, `phasmo::Layers::DeadPlayer`,
  `phasmo::IKExecutionOrder::LateUpdate` (a method to hook).

- [ ] **Step 1:** Add each entry as `Inferred: Unity's documented API` (or Decompiled where read), with params as the
  dump declares them (`re/dump/cs/DiffableCs/UnityEngine.PhysicsModule/UnityEngine/`). `Layer.DeadPlayer`'s note states
  its collision row above, read from the asset files. `IKExecutionOrder.LateUpdate`'s note: Final IK runs its solvers
  from it, after the Animator.
- [ ] **Step 2:** Raise the fixture counts the new layer entry changes (`game_map_test` layers 13 -> 14, `gamecheck_test`
  scene facts 53 -> 54, its written count 53 -> 54).
- [ ] **Step 3:** `.\validate.ps1` and `tools\gamecheck.ps1 -IfConfigured -Expect`: 0 missing, each new entry OK; add
  the values to `re/dump/expect/squatter.txt`.
- [ ] **Step 4:** Commit "Map the physics a ragdoll needs and Final IK's frame".

### Task 3: A pass after the Animator, before the IK

**Files:**
- Modify: `mods/custom_ghosts/squatter_figure.h`, `mods/custom_ghosts/squatter_figure.cpp`, `mods/custom_ghosts/squatter.cpp`
  (`create_hooks`)
- Test: `tests/squatter_test.cpp`

**Interfaces:**
- Produces: `bool figure::create_hooks()` (hooks `IKExecutionOrder.LateUpdate`); `using figure::AfterAnimation =
  void (*)(float dt)`; `void figure::set_after_animation(AfterAnimation fn)`; inside the detour, `fn(dt)` runs only when
  the instance is the figure's own `IKExecutionOrder`, then the original runs.
- Pure: `bool figure::is_own_ik(const void* instance, const void* own)` (non-null and equal).

- [ ] **Step 1:** Test `the_pass_after_the_animator_is_the_figures_own`: `is_own_ik(a, a)`; `!is_own_ik(a, b)`;
  `!is_own_ik(nullptr, nullptr)`.
- [ ] **Step 2:** Run it red; implement; run green.
- [ ] **Step 3:** Call `figure::create_hooks()` from `squatter::create_hooks()`; `.\validate.ps1`; commit "Give the
  figure a pass after its animation, before its IK".

### Task 4: Inertialization

**Files:**
- Create: `mods/custom_ghosts/inertia.h` (pure, no game calls)
- Modify: `mods/custom_ghosts/squatter_figure.cpp` (`blend_to`, the after-animation pass)
- Test: `tests/squatter_test.cpp` (`#include "inertia.h"`)

**Interfaces:**
- Produces: `struct Quat { float x, y, z, w; }` (reuse `phasmo::Quaternion` if `inertia.h` may include `Transform.h`;
  else its own); `inline constexpr float kInertiaSeconds = 0.25f;` (starting value);
  `class PoseInertia { void begin(std::span<const Quat> from, std::span<const Quat> to, float now); Quat offset(size_t
  bone, float now) const; bool active(float now) const; void clear(); }`. The offset is `from * inverse(to)` per bone,
  scaled toward identity by the decay; the decay is the quintic of Bollo's inertialization (GDC 2018) over
  `kInertiaSeconds`, value 1 at `begin` and 0 with zero velocity at the end.

- [ ] **Step 1:** Test `inertialization_starts_from_the_old_pose_and_dies_away`: at `now == begin`, `offset * to == from`
  per bone (within 1e-4); at `begin + kInertiaSeconds`, `offset` is identity and `active` is false; the angle of the
  offset never grows over time (sampled every 10 ms); a `begin` with mismatched spans does nothing.
- [ ] **Step 2:** Red; implement `inertia.h`; green.
- [ ] **Step 3:** In the figure: `blend_to` reads every mapped humanoid bone's `localRotation` (the old pose), plays the
  state outright (`play_state` + `pose_now`), reads the new pose, `begin`s; the after-animation pass sets each bone's
  `localRotation` to `offset * animated` while `active`. `fade_to_state` is no longer used for the figure's own states.
- [ ] **Step 4:** `.\validate.ps1`; commit "Change the Squatter's clips by inertialization, not cross-fades".

### Task 5: The ragdoll

**Files:**
- Create: `mods/custom_ghosts/squatter_ragdoll.h`, `mods/custom_ghosts/squatter_ragdoll.cpp`
- Modify: `mods/custom_ghosts/squatter_figure.cpp` (build with the figure, destroy with it), `CMakeLists.txt`
- Test: `tests/squatter_ragdoll_test.cpp` (fake game, as `tests/squatter_door_test.cpp`), registered in `CMakeLists.txt`

**Interfaces:**
- Produces (pure, in the header): `enum class RagdollShape { Box, Capsule, Sphere };` `struct RagdollBone { int32_t
  bone; int32_t parent; RagdollShape shape; float width; float mass; float twistLow, twistHigh, swing1, swing2; };`
  `inline constexpr RagdollBone kRagdollBones[11]` for Hips, Spine, Chest, Head, Left/Right UpperArm, LowerArm,
  UpperLeg, LowerLeg (Unity's Ragdoll Wizard layout and limits: thighs twist -20..70 swing 30; calves -80..0 swing 0;
  upper arms -70..10 swing 50; forearms -90..0 swing 0; spine and chest -20..20 swing 10; head -40..25 swing 25; mass
  shares of a 20 kg body as the Wizard gives them). Bone ids are `phasmo::HumanBodyBones` values read at build time, so
  the table holds an index into a parallel list of their entries, not raw numbers.
- Produces (game): `bool ragdoll::build(void* animator)` (all kinematic, all on `Layer.DeadPlayer`, colliders sized from
  each bone to its child); `void ragdoll::set_physical(bool on, phasmo::Vector3 velocity)`;
  `float ragdoll::fastest()` (the fastest body's speed, m/s); `std::vector<phasmo::Quaternion> ragdoll::local_pose()`;
  `void ragdoll::destroy()`.

- [ ] **Step 1:** Test `the_ragdoll_table_is_whole`: every parent appears before its child; every limit low <= high;
  masses sum to 20 within 0.01; exactly one root (Hips).
- [ ] **Step 2:** Test `the_ragdoll_is_built_kinematic_on_the_dead_player_layer` (fake game): `build` adds one Rigidbody,
  one collider and (but for the root) one CharacterJoint per table bone, each kinematic, each GameObject on the
  `DeadPlayer` layer, each joint connected to its parent's body; `set_physical(true, v)` clears kinematic and sets `v` on
  every body; `destroy` removes every part (Review Focus 4).
- [ ] **Step 3:** Red; implement; green. Guard (Review Focus 1): `set_physical(true, v)` caps `|v|` at
  `kFallLaunchCap = 6.0f` m/s (starting value).
- [ ] **Step 4:** Build the ragdoll in `figure::create` after the IK; destroy it in `clear()`; `.\validate.ps1`; commit
  "Build a ragdoll for the Squatter from Unity's own parts".

### Task 6: Falling, settling and getting up (every client)

**Files:**
- Create: `mods/custom_ghosts/squatter_fall.h` (pure)
- Modify: `mods/custom_ghosts/squatter.cpp` (`play()`), `mods/custom_ghosts/squatter_figure.cpp`
- Test: `tests/squatter_chase_cases.inc` (included in `squatter_test.cpp`)

**Interfaces:**
- Produces: `enum class FallPhase { Up, Falling, GettingUp };` `class FallState { void begin(uint64_t now); FallPhase
  update(uint64_t now, float fastest); ... }`: Falling turns GettingUp once `fastest < kSettleSpeed` (0.15 m/s) for
  `kSettleMs` (300) or after `kFallMaxMs` (2500); GettingUp turns Up after the get-up clip's length.
  `enum class GetUp { FaceDown, FaceUp };` `GetUp get_up_for(Point chestFront)`: FaceDown when the chest's front points
  down (`y < 0`), FaceUp otherwise; a front within `kSideBand` (0.2) of level keeps the last choice (Review Focus 5).
  `struct RootPose { Point at; float yaw; };` `RootPose get_up_root(Point hips, Point head, GetUp g)`: at the hips on the
  flat, turned so the clip's own start pose lies along hips -> head; the clips' start head directions are
  `kGetUpHeadYaw = {5.0f /* StandUp */, 175.0f /* TripGetUp, Getting Up's rise */}` degrees (the probe of 2026-11-09).
- Consumes: Task 3's pass, Task 4's `PoseInertia` (the landed pose blends into the get-up over `kRecoverSeconds` 0.4),
  Task 5's `ragdoll::*`.

- [ ] **Step 1:** Tests `a_fall_settles_then_gets_up` (settle on slow bodies, on the time limit, not before),
  `the_get_up_follows_how_it_landed` (FaceDown/FaceUp, side band keeps the last), `it_gets_up_where_it_lies` (root at the
  hips, yaw along hips -> head less the clip's start direction).
- [ ] **Step 2:** Red; implement; green.
- [ ] **Step 3:** In `play()`: a Fall cue (Task 7) `begin`s, makes the ragdoll physical with the ghost's velocity plus
  the cue's push, and stops placing the figure at the ghost; on GettingUp, capture `local_pose()`, place the root by
  `get_up_root`, make the ragdoll kinematic, play the get-up state outright and `begin` inertia from the captured pose;
  while getting up, the figure eases from its own landing to the ghost's position (on the master the ghost is moved
  there, Task 7). The chained trip get-ups and their turns (`kRiseTurnDegrees`, `risen_facing`, `stood_up`,
  `tripSince`) are removed.
- [ ] **Step 4:** `.\validate.ps1`; commit "Let the Squatter fall, settle and get up from where it lands".

### Task 7: Falls in the Shown, and what knocks it over (master)

**Files:**
- Modify: `mods/custom_ghosts/squatter_rule.h` (append `Move::Fall` after `Sneak`), `mods/custom_ghosts/squatter_net.h`
  (the Shown gains `fallYaw` and `fallCause`, one byte each; `kMoveValues` follows), `mods/custom_ghosts/squatter_chase.h`,
  `mods/custom_ghosts/squatter.cpp` (`run()`)
- Test: `tests/squatter_test.cpp` (`shown_round_trips`, `bad_shown_is_dropped`), `tests/squatter_chase_cases.inc`

**Interfaces:**
- Produces: `enum class FallCause : uint8_t { Trip, Bump, Drop, Door, Thrown };` `bool may_fall(Move current, int8_t act,
  bool down)` (false while down, mid-act, or in a fall: Review Focus 2); `bool knocks_over(FallCause c, float speed)`
  with `kBumpFallPace` 2.5 m/s, `kDoorKnockSpeed` 1.5 m/s, `kThrownKnockSpeed` 4.0 m/s (Trip and Drop always);
  `inline constexpr float kFooting[] = {0.45f /* Stumble */, 0.95f /* Trip */}` seconds into each trip clip where the body
  loses its footing (the probe: hips fall below 0.6 m); `kDoorKnockReach` 0.6 m, `kThrownReach` 0.6 m.
- Consumes: Task 2's `OverlapSphere`, `attachedRigidbody`, `Rigidbody::get_velocity`, `GetPointVelocity`.

- [ ] **Step 1:** Tests: `a_fall_is_shared` (the Shown round-trips `Fall`, `fallYaw`, `fallCause`; a cause past the last
  is dropped); `what_knocks_it_over` (each cause's threshold, at and either side); `a_fall_never_stacks` (`may_fall`
  false while down, mid-act, in a fall).
- [ ] **Step 2:** Red; implement; green.
- [ ] **Step 3:** In `run()`: a trip goes to `Fall` (cause Trip) at `kFooting[variant]`; the stuck bump while its pace is
  at least `kBumpFallPace` falls (cause Bump, pushed back from its heading) instead of the Bump act; a Parkour picked for
  a drop falls (cause Drop) as the jump clip ends; a door whose leaf (`Door.body`) point nearest the ghost within
  `kDoorKnockReach` moves toward it at `kDoorKnockSpeed` falls (cause Door, pushed along the leaf's motion); a prop or
  item within `kThrownReach` of the chest moving at `kThrownKnockSpeed` falls (cause Thrown, pushed along its motion).
  While down, the master holds the agent still; when its own figure stands, it warps the agent to its figure's hips taken
  onto the mesh (`NavMesh.SamplePosition` within 1 m), then the chase goes on. A new master mid-fall takes its own
  figure's landing (Review Focus 3).
- [ ] **Step 4:** `.\validate.ps1` and the game check; commit "Knock the Squatter over on a trip, a bump, a drop, a door
  or a thrown object".

### Task 8: Motion warping

**Files:**
- Modify: `mods/custom_ghosts/squatter_chase.h`, `mods/custom_ghosts/squatter.cpp` (the root-motion warp),
  `mods/custom_ghosts/squatter_figure.h` (`kClipTravel`)
- Test: `tests/squatter_chase_cases.inc`

**Interfaces:**
- Produces: `inline constexpr StateTravel kClipTravel[]` (state, metres): Duck_A 1.28, Duck_B 1.18, Weave_A 1.28,
  Weave_B 1.28, Roll_A 5.52, Roll_B 4.31 (the bundle build's speed x length); `Point warp_step(Point step, float
  clipLeft, Point toTarget)`: the step turned toward `toTarget` and scaled by `|toTarget| / clipLeft`, the scale clamped
  to 0.5..1.5 (starting values); zero `clipLeft` returns the step.
- Consumes: the targets: a dive's is the target player's position less `kKickReach`; a duck's or weave's is along its way
  by its clip's travel; each taken onto the mesh (`NavMesh.SamplePosition` within 0.5 m), a move with none playing as
  recorded. Each frame's warped point is itself taken onto the mesh before `Warp`; a frame whose point is off it holds
  (the hover's suspected cause, Task 1).

- [ ] **Step 1:** Tests `a_move_lands_where_the_world_needs_it` (turned toward the target; scaled to reach it; clamped;
  zero clip left returns the step) and `the_clips_travel_as_built` (each table entry > 0 and every `moves_by_clip` state
  but the trips and the jump has one).
- [ ] **Step 2:** Red; implement; green.
- [ ] **Step 3:** `.\validate.ps1`; commit "Warp the Squatter's clumsy moves to land where the world needs them".

### Task 9: The bundle without the chained get-ups

**Files:**
- Modify: `C:\Dev\phasmomods-private\unity\squatter-bundle\Assets\Editor\SquatterBuild.cs`;
  `mods/custom_ghosts/squatter_figure.h` (`kStateClips`: Stumble 1.6000, Trip 2.1000)
- Test: `tests/squatter_test.cpp` (`a_trip_gets_up_with_the_users_clip` becomes `the_get_ups_stand_alone`)

- [ ] **Step 1:** Test: `seconds_of("Stumble") == 1.6f`, `seconds_of("Trip") == 2.1f`, `StandUp` and `TripGetUp` still in
  `all_states()`.
- [ ] **Step 2:** In the build: drop the Stumble -> StandUp and Trip -> TripGetUp transitions and both rotation offsets
  (`StandUpTurn`, `TripGetUpTurn`); keep both trims. Build in Unity (batch mode, `SquatterBuild.Build`), copy
  `bundle-out\squatter.bundle` into `mods/custom_ghosts/assets/`, commit the project in the private repository, run
  `tools\publish_assets.ps1 -Private C:\Dev\phasmomods-private`.
- [ ] **Step 3:** `.\validate.ps1`; commit "Let the Squatter's get-ups stand alone for the ragdoll" with the new pin.

### Task 10: Docs, merge and staging

**Files:**
- Modify: `sdk/guides/custom-animation.md` (sections: a ragdoll from Unity's parts on `Layer.DeadPlayer`; a pass after
  the Animator through `IKExecutionOrder.LateUpdate`; inertialization; motion warping), `sdk/CHANGELOG.md`,
  `docs/superpowers/specs/2026-11-04-squatter-design.md` (the falls and their causes), the ledger

- [ ] **Step 1:** Write the sections, citing map keys only.
- [ ] **Step 2:** `.\validate.ps1`, `tools\gamecheck.ps1 -IfConfigured -Expect`; commit "Document the Squatter's physical
  animation for mod authors"; push `feature/squatter`.
- [ ] **Step 3:** Merge into `development` locally (`--no-ff`), place the pinned bundle there, validate and stage
  `development`'s copy. Do not push `development`: the user tests first.
