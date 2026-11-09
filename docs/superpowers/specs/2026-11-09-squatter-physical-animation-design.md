# The Squatter's physical animation: design

Decided with the user on 2026-11-09; approved by the user on 2026-11-10. Items marked **(builder's call)** are not the user's decisions and may be changed by
whoever plans this. Everything else is the user's.

## Why

The Squatter's animations play as separate clips with obvious start and end points that do not mesh, and they do not
connect to the house around it. One trip hovers in mid-air for half a second without moving. The user wants a smooth,
seamless set of animations that responds to the environment, as modern games do, and named a missing step that blending
alone does not fix. Research (2026-11-09) found three techniques that modern games use for that step, and the user asked
for all three:

1. **Physical falls (ragdoll).** As in GTA's Euphoria, the body is simulated rather than played when it falls, so it
   lands against the real floor and furniture. Recovering is a known pattern: capture the landed pose, check whether it
   lies face up or face down, and blend from that exact pose into the matching get-up clip, with no snap.
2. **Warping clips to the world.** A clip's own travel (root motion) is bent so the move ends where the world needs it:
   a jump lands past the prop it clears, a dive ends at its target, as Unreal's motion warping does.
3. **Inertialization.** A new clip starts at once, and the difference between the old pose and the new one dies away over
   a moment, so the body keeps its momentum, as Gears of War does. It replaces cross-fades, which float between poses.

Motion matching, the fourth common technique, needs a large motion-capture library; with about thirty Mixamo clips it does
not fit, and it is out of scope.

## Goals

- **Every trip ends in a ragdoll before it gets up** (user, 2026-11-09). The trip's clip plays until the body loses its
  footing; the body then falls under physics, onto the floor and against what is there; once it settles, it gets up from
  the pose it landed in, with the get-up that matches that pose (face down: Standing Up; face up: Getting Up, both
  already in the bundle).
- **The clumsy moves land where the world needs them:** the jump over a knee-high prop lands past it, the dive ends at the
  player it dives at, the duck and the weave carry it past what is beside it or through the gap.
- **No pose ever snaps or floats** between any two of its clips, its acts (kick, bang, door, sit, get up) and its moves.
- **The hover is gone, from its cause.** The cause is to be proven first (see "Known problem").

## Non-goals

- Motion matching.
- New clips. The design uses the bundle's clips and the user's downloads. A gap that needs a new clip goes to the user.
- Any change to what the Squatter does, decides or says. Only how its body moves changes.

## Anti-goals

- **A ragdoll that pushes the house about.** Props and equipment are networked: a body that knocked them over on one
  client would leave them in different places on each. The ragdoll must not move them.
- A body that sinks into the floor, tangles in walls, stands up somewhere other than where the ghost is, or ends in
  different places for different players by more than a little.
- Anything that reads as a hunt, or that touches the game's death ragdoll for players.

## Where ragdolls go

All chosen by the user (2026-11-09 and 2026-11-10); each falls, settles and gets up as a trip does:

- **Every trip.**
- **A hard bump** into a wall or furniture it stalls against (today's Bump act, which plays Jogging Stumble).
- **Landing from a jump down a drop.**
- **A door swinging into it.**
- **An object a player throws hitting it.**

How hard a bump, a door's swing or a thrown object must be to knock it over, and whether a lighter one only staggers it,
are starting values judged with the user in game **(builder's call)**. What each of these reads from the game (a door's
swing toward it, a thrown object's hit) is reverse engineered and cited by map key, never assumed.

## Constraints

- The project's rules apply: map entries for every game member used, decompiled or checked; no invented game behaviour;
  vanilla UI only; offline tests for every rule; `validate.ps1` and the game check pass.
- **The figure is each client's own.** Every client builds and plays its own copy of the body under the sheet; only the
  ghost's position and the master's Shown are shared. Physics on each client gives each a slightly different fall. The
  master's fall decides where the ghost stands up; clients blend to it.
- **No scripts can ship in the bundle.** An AssetBundle carries assets, not code, so everything new is native code in the
  mod and the controller's own features (states, transitions, blend trees).
- **What the game provides:** Final IK (already used: full-body IK, look-at, grounding), Unity's Animation Rigging
  package, and Unity's physics joints (the game builds dead players' ragdolls from CharacterJoints: `RagdollBone`,
  `DeadPlayer`). It does not ship PuppetMaster or any active-ragdoll tool.
- **The investigator prefab has no ragdoll of its own** (no CharacterJoint on it, read from the asset files), so the
  Squatter's is built at runtime from Unity's parts: a Rigidbody, a collider and a CharacterJoint per major bone, as
  Unity's Ragdoll Wizard lays them out.

## How it works

### A pass after the Animator

Inertialization and the ragdoll's blend back into animation both change the pose after the Animator has written it and
before Final IK reads it. The builder finds the frame point for that **(builder's call)**. The preferred route: a hook on
Final IK's `IKExecutionOrder.LateUpdate`, already mapped, which the figure's own IK runs from, so the pass runs for the
figure alone, every frame, after its animation and before its IK.

### Inertialization

When the figure changes clip, it records each bone's offset from the old pose to the new clip's first pose and decays
that offset to zero over a short time with a critically damped curve, applied in the pass after the Animator. The new
clip starts at once. Cross-fades between the figure's own states are replaced by it **(builder's call: which transitions
in the bundle stay as Animator transitions, such as the trip into its get-up)**. The decay time is a starting value,
judged with the user in game.

### Motion warping

For each clumsy move that carries the body by its clip, the master knows where the move should end: past the prop for a
jump, at the target for a dive, past the furniture or through the gap for a duck or weave. It scales and turns the clip's
travel each frame so the move ends there, as Unreal's skew warp does, and only to a point on the navigation mesh. A move
with no valid end point plays as recorded. Each clip's own travel is read from the bundle's build (its average speed and
length), as the build already logs **(builder's call: where those numbers live)**.

### The ragdoll trip

1. **Built once** with the figure: a Rigidbody, a collider sized from the bone lengths, and a CharacterJoint with limits
   per bone (hips, spine, chest, head, both upper and lower arms and legs), all kinematic while it is animated. They sit
   on a layer that collides with the house's static world (floors, walls, stairs, fixed furniture) and with nothing
   networked or living: no props, equipment, players or ghost. The builder finds that layer in the game's physics
   matrix, or another way to get the same collisions **(builder's call)**. The sheet's cloth spheres stay on their own
   layer and follow the bones as now.
2. **The trip begins** as now (the master picks a trip; every client plays its clip) and plays until the body loses its
   footing, a moment read from each trip clip **(builder's call: the moment, per clip)**.
3. **It falls.** The Animator stops driving the body; every bone body turns physical, with the velocity it had, so it
   carries its run into the fall; the figure is no longer placed at the ghost each frame.
4. **It settles** once the bodies have slowed below a threshold, or after a time limit, both starting values.
5. **It gets up.** The landed pose is captured; the hips' up vector says face up or face down, which picks the get-up
   clip; the figure's root is placed and turned to the landed body (the hips on the floor, the heading from hips to
   head), so the clip begins where the body lies; the bones blend from the captured pose into the clip in the pass after
   the Animator, then the clip plays on into the run as any move does.
6. **The ghost follows.** On the master, the ghost's agent moves to where its fall left the hips, taken onto the
   navigation mesh, before it stands; every other client's figure blends from where its own fall landed to the ghost
   while it gets up.

The trips' chained get-up states in the bundle (`StandUp`, `TripGetUp`) and the turns the client takes up after them are
replaced by this.

## Known problem: the hover

One trip hovers in mid-air for about half a second without moving (user, 2026-11-09). The likely cause: the trip clip
carries the body about four metres forward, but that travel reaches the ghost only through the master's warp of its
agent, so when the warp does not land (blocked, or off the mesh), the falling pose plays in place. Prove the cause before
the rework (log each frame's clip travel against the agent's movement during a trip), and record what it was; the ragdoll
fall then replaces the travel that failed.

## Left to the builder

The frame point for the pass after the Animator, the ragdoll's layer, the bone shapes and joint limits, the moment each
trip clip loses its footing, the settle threshold and time limit, the inertialization decay time, where clip travel
numbers live, and which bundle transitions stay. Each starting value is judged with the user in game.
