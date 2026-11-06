# Navigation

What a mod author needs to find a walkable route through the level, as the ghost does. The entries are in
`sdk/map/UnityWorld.h` (Unity's navigation, `UnityEngine.AI`) and `sdk/map/GhostAI.h`; their `///` notes are the
authority. Every call here is local to the client that makes it and runs on Unity's main thread.

## The navigation mesh

The level has a navigation mesh, the floor the ghost's `NavMeshAgent` (`GhostAI.agent`) walks on. Use `-1` as the
area mask for every area.

- **`NavMesh.SamplePosition`** finds the nearest point of the mesh to a position, within a distance in metres. Use
  it to find the floor below a thing before asking for a route from there. It answers false when nothing is in
  reach.
- **`NavMesh.CalculatePath`** works out a route between two points into a `NavMeshPath`. Make the path once with
  `NavMeshPath.type.make()` and `NavMeshPath.ctor`, keep it in an `api::Held`, and reuse it.
- **`NavMeshPath.get_status`** says how far the route got. Compare it with `NavMeshPathStatus.PathComplete`; only
  a complete route reaches the end point.
- **`NavMeshPath.get_corners`** is the route itself, a `Vector3[]` read as an `api::Array<Vector3>`. The first
  corner is the start and the last is the end, all on the floor.

## The ghost's agent

- **`NavMeshAgent.CalculatePath`** works out a route from where the agent stands, as the ghost's wander does
  (`GhostWanderState.PickDestination` tests a long roam this way).
- `NavMeshAgent.SetDestination`, `NavMeshAgent.get_destination`, `NavMeshAgent.get_remainingDistance` and
  `NavMeshAgent.get_pathPending` steer and follow the agent. `NavMeshAgent.Warp` moves it at once.
- **`NavMeshAgent.get_speed`** is the agent's top speed in metres a second. Outside a hunt it is the pace the ghost
  walks at. In a hunt the game sets it from the hunt's speed rules ([ghost AI](ghost-ai.md), "Hunt speed").

## Moving a thing along the floor

Custom Ghosts' Draugr drags a thing back to where it lay this way (`mods/custom_ghosts/carry.cpp`,
`mods/custom_ghosts/drag_rule.h`):

1. Take the thing over through its `PhotonView`, as a player picking it up does, so every client sees the owner
   move it.
2. Find the floor below the thing and below its goal with `NavMesh.SamplePosition`, and the route between them
   with `NavMesh.CalculatePath`. Keep the thing's height above the floor along the route's corners.
3. Make its body kinematic and move it a step along the route each frame (`Transform.set_position` and
   `Rigidbody.set_position`), at the ghost's pace from `NavMeshAgent.get_speed`.
4. At the goal, set it down, stop its motion, and give its body back its kinematic state.

With no floor in reach or no complete route, it goes straight to its goal.
