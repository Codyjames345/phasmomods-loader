#pragma once

// Unity's world queries the mods use: a ray through the physics scene, a point on the navigation mesh,
// a NavMeshAgent's destination, and Unity's random integers. Not obfuscated, so every entry is by real
// name.

#include "entry.h"
#include "game_api.h"
#include "Camera.h"
#include "Transform.h"

namespace pm::map {

inline constexpr const char* Physics_Raycast_params[] = {"Vector3", "Vector3", "float", "int",
                                                         "@QueryTriggerInteraction"};
inline constexpr const char* NavMesh_SamplePosition_params[] = {"Vector3", "out NavMeshHit", "float", "int"};
inline constexpr const char* NavMeshAgent_SetDestination_params[] = {"Vector3"};
inline constexpr const char* NavMeshAgent_CalculatePath_params[] = {"Vector3", "NavMeshPath"};
inline constexpr const char* NavMeshPathStatus_values[] = {"PathComplete", "PathPartial", "PathInvalid"};
inline constexpr const char* Random_RandomRangeInt_params[] = {"int", "int"};
inline constexpr const char* QueryTriggerInteraction_values[] = {"UseGlobal", "Ignore", "Collide"};

/// Unity's physics queries (UnityEngine.PhysicsModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Physics{.key = "Physics", .name = "Physics", .assembly = "UnityEngine.PhysicsModule",
                                            .ns = "UnityEngine"};

/// Whether a ray from an origin along a direction hits a collider within a distance, on the layers of
/// a mask, with triggers counted as the last argument says. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Physics_Raycast{.key = "Physics.Raycast", .kind = pm::sdk::Kind::Method,
                                                    .flags = pm::sdk::kStatic, .owner = "Physics", .name = "Raycast",
                                                    .type = "bool", .params = Physics_Raycast_params};

/// Whether a physics query counts trigger colliders (UnityEngine.PhysicsModule). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc QueryTriggerInteraction{.key = "QueryTriggerInteraction",
                                                            .name = "QueryTriggerInteraction",
                                                            .assembly = "UnityEngine.PhysicsModule",
                                                            .ns = "UnityEngine",
                                                            .values = QueryTriggerInteraction_values};

/// Trigger colliders are not hit. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc QueryTriggerInteraction_Ignore{.key = "QueryTriggerInteraction.Ignore",
                                                                   .kind = pm::sdk::Kind::EnumValue,
                                                                   .owner = "QueryTriggerInteraction",
                                                                   .name = "Ignore"};

/// Unity's navigation mesh (UnityEngine.AIModule, namespace UnityEngine.AI). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc NavMesh{.key = "NavMesh", .name = "NavMesh", .assembly = "UnityEngine.AIModule",
                                            .ns = "UnityEngine.AI"};

/// The nearest point of the navigation mesh within a distance of a position, on the areas of a mask,
/// written to a NavMeshHit; false when there is none. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMesh_SamplePosition{.key = "NavMesh.SamplePosition",
                                                           .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
                                                           .owner = "NavMesh", .name = "SamplePosition",
                                                           .params = NavMesh_SamplePosition_params};

/// A path over the navigation mesh (UnityEngine.AIModule, namespace UnityEngine.AI), as GhostWanderState.PickDestination
/// makes one to test a long roam: a new one, its agent's CalculatePath, then its status. Decompiled.
inline constexpr pm::sdk::EntryDesc NavMeshPath{.key = "NavMeshPath", .confidence = pm::sdk::Confidence::Decompiled,
                                                .flags = pm::sdk::kOptional, .name = "NavMeshPath",
                                                .assembly = "UnityEngine.AIModule", .ns = "UnityEngine.AI"};

/// Its constructor. Decompiled.
inline constexpr pm::sdk::EntryDesc NavMeshPath_ctor{.key = "NavMeshPath.ctor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .flags = pm::sdk::kOptional, .owner = "NavMeshPath",
                                                     .name = ".ctor"};

/// How far the path got (NavMeshPathStatus). Decompiled.
inline constexpr pm::sdk::EntryDesc NavMeshPath_get_status{.key = "NavMeshPath.get_status",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .flags = pm::sdk::kOptional, .owner = "NavMeshPath",
                                                           .name = "get_status"};

/// The path's corners, a Vector3[], its start first. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshPath_get_corners{.key = "NavMeshPath.get_corners",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .flags = pm::sdk::kOptional, .owner = "NavMeshPath",
                                                            .name = "get_corners"};

/// How far a path got (UnityEngine.AIModule, namespace UnityEngine.AI). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshPathStatus{.key = "NavMeshPathStatus", .flags = pm::sdk::kOptional,
                                                      .name = "NavMeshPathStatus",
                                                      .assembly = "UnityEngine.AIModule", .ns = "UnityEngine.AI",
                                                      .values = NavMeshPathStatus_values};

/// The path reaches its target. GhostWanderState.PickDestination compares the status with it. Decompiled.
inline constexpr pm::sdk::EntryDesc NavMeshPathStatus_PathComplete{.key = "NavMeshPathStatus.PathComplete",
                                                                   .kind = pm::sdk::Kind::EnumValue,
                                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                                   .flags = pm::sdk::kOptional,
                                                                   .owner = "NavMeshPathStatus",
                                                                   .name = "PathComplete"};

/// The component that walks an object over the navigation mesh (UnityEngine.AIModule, namespace
/// UnityEngine.AI); the ghost's GhostAI.agent is one. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent{.key = "NavMeshAgent", .name = "NavMeshAgent",
                                                 .assembly = "UnityEngine.AIModule", .ns = "UnityEngine.AI"};

/// Sends the agent towards a point; false when it cannot. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_SetDestination{.key = "NavMeshAgent.SetDestination",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .owner = "NavMeshAgent", .name = "SetDestination",
                                                                .params = NavMeshAgent_SetDestination_params};

/// The path the agent would walk from where it stands to a point, into a NavMeshPath, as
/// GhostWanderState.PickDestination asks it of the ghost's agent. Decompiled.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_CalculatePath{.key = "NavMeshAgent.CalculatePath",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .flags = pm::sdk::kOptional, .owner = "NavMeshAgent",
                                                               .name = "CalculatePath",
                                                               .params = NavMeshAgent_CalculatePath_params};

/// The agent's velocity in world space, metres a second. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_velocity{.key = "NavMeshAgent.get_velocity",
                                                              .kind = pm::sdk::Kind::Method, .owner = "NavMeshAgent",
                                                              .name = "get_velocity"};

inline constexpr const char* NavMeshAgent_float[] = {"float"};

/// Sets how fast the agent turns, degrees a second. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_set_angularSpeed{
    .key = "NavMeshAgent.set_angularSpeed", .kind = pm::sdk::Kind::Method, .owner = "NavMeshAgent",
    .name = "set_angularSpeed", .params = NavMeshAgent_float};

/// Moves the agent to a point of the navigation mesh at once; false when it cannot. The game's own
/// ghost teleports (GhostController.WarpGhostToFavouriteRoom among them) use it. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_Warp{.key = "NavMeshAgent.Warp", .kind = pm::sdk::Kind::Method,
                                                      .owner = "NavMeshAgent", .name = "Warp",
                                                      .params = NavMeshAgent_SetDestination_params};

// The runtime has no metadata for an icall, so its params are never matched: they fix the arity of
// its wrapper.
inline constexpr const char* NavMeshAgent_out_Vector3[] = {"out Vector3"};

/// The velocity the agent wants to move at, written to the Vector3 given: the engine function behind
/// the desiredVelocity getter, whose managed body this build strips. Inferred: Unity's documented API
/// (Custom Ghosts has called it by this name since 2026-10-20).
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_desiredVelocity_Injected{
    .key = "NavMeshAgent.get_desiredVelocity_Injected", .kind = pm::sdk::Kind::Icall,
    .name = "UnityEngine.AI.NavMeshAgent::get_desiredVelocity_Injected", .params = NavMeshAgent_out_Vector3};

/// How fast the agent turns, degrees a second: the engine function behind the angularSpeed getter,
/// whose managed body this build strips. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_angularSpeed{.key = "NavMeshAgent.get_angularSpeed",
                                                                  .kind = pm::sdk::Kind::Icall,
                                                                  .name = "UnityEngine.AI.NavMeshAgent::get_angularSpeed"};

/// Unity's rigid body (UnityEngine.PhysicsModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody{.key = "Rigidbody", .name = "Rigidbody",
                                              .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine"};

/// The body's centre of mass in world space. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Rigidbody_get_worldCenterOfMass{
    .key = "Rigidbody.get_worldCenterOfMass", .kind = pm::sdk::Kind::Method, .owner = "Rigidbody",
    .name = "get_worldCenterOfMass"};

/// Whether physics leaves the body alone (a velocity set on a kinematic body does nothing). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody_get_isKinematic{.key = "Rigidbody.get_isKinematic",
                                                              .kind = pm::sdk::Kind::Method, .owner = "Rigidbody",
                                                              .name = "get_isKinematic"};

/// The body's velocity. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody_get_velocity{.key = "Rigidbody.get_velocity", .kind = pm::sdk::Kind::Method,
                                                           .owner = "Rigidbody", .name = "get_velocity"};

/// Sets the body's velocity. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody_set_velocity{.key = "Rigidbody.set_velocity", .kind = pm::sdk::Kind::Method,
                                                           .owner = "Rigidbody", .name = "set_velocity",
                                                           .params = NavMeshAgent_SetDestination_params};

/// Sets the body's spin. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody_set_angularVelocity{
    .key = "Rigidbody.set_angularVelocity", .kind = pm::sdk::Kind::Method, .owner = "Rigidbody",
    .name = "set_angularVelocity", .params = NavMeshAgent_SetDestination_params};

/// Moves the body there at once. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Rigidbody_set_position{.key = "Rigidbody.set_position", .kind = pm::sdk::Kind::Method,
                                                           .owner = "Rigidbody", .name = "set_position",
                                                           .params = NavMeshAgent_SetDestination_params};

/// Sets the agent's top speed. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_set_speed{.key = "NavMeshAgent.set_speed", .kind = pm::sdk::Kind::Method,
                                                           .owner = "NavMeshAgent", .name = "set_speed",
                                                           .params = NavMeshAgent_float};

/// Sets how fast the agent may gain speed, in metres a second each second. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_set_acceleration{
    .key = "NavMeshAgent.set_acceleration", .kind = pm::sdk::Kind::Method, .owner = "NavMeshAgent",
    .name = "set_acceleration", .params = NavMeshAgent_float};

inline constexpr const char* DOTween_IsTweening_params[] = {"object", "bool"};

/// DOTween's static API (the DOTween assembly, namespace DG.Tweening), the tween library the game
/// animates objects with. Inferred: DOTween's documented API.
inline constexpr pm::sdk::EntryDesc DOTween{.key = "DOTween", .name = "DOTween", .assembly = "DOTween",
                                            .ns = "DG.Tweening"};

/// Whether a tween of the target (or id) exists, and with the second argument true, whether one is
/// playing. Static. Inferred: DOTween's documented API.
inline constexpr pm::sdk::EntryDesc DOTween_IsTweening{.key = "DOTween.IsTweening", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "DOTween",
                                                       .name = "IsTweening", .params = DOTween_IsTweening_params};

/// Unity's random numbers (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Random{.key = "Random", .name = "Random", .assembly = "UnityEngine.CoreModule",
                                           .ns = "UnityEngine"};

/// A random integer from the first argument up to, not including, the second: the body of
/// Random.Range(int, int), which the game's own code calls directly (the Mare's switch-off roll in
/// the light switch's switch-off coroutine among them). Static. Decompiled.
inline constexpr pm::sdk::EntryDesc Random_RandomRangeInt{
    .key = "Random.RandomRangeInt", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "Random", .name = "RandomRangeInt", .params = Random_RandomRangeInt_params};

/// The agent's top speed, in metres a second. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_speed{
    .key = "NavMeshAgent.get_speed", .kind = pm::sdk::Kind::Method, .owner = "NavMeshAgent", .name = "get_speed"};

/// How far the agent still has to walk along its path, in metres; Infinity while that is unknown.
/// Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_remainingDistance{
    .key = "NavMeshAgent.get_remainingDistance", .kind = pm::sdk::Kind::Method, .owner = "NavMeshAgent",
    .name = "get_remainingDistance"};

/// Whether the agent's path is still being worked out. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_pathPending{.key = "NavMeshAgent.get_pathPending",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .owner = "NavMeshAgent", .name = "get_pathPending"};

/// Where the agent walks to, in world space. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc NavMeshAgent_get_destination{.key = "NavMeshAgent.get_destination",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .owner = "NavMeshAgent", .name = "get_destination"};

/// What a physics ray hit (UnityEngine.PhysicsModule), a struct the query fills in. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc RaycastHit{.key = "RaycastHit", .name = "RaycastHit",
                                               .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine"};

/// The Transform of what the ray hit: the hit collider's own, or its Rigidbody's when it has one. An
/// instance method of the struct: it takes the struct's address. Inferred: Unity's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc RaycastHit_get_transform{.key = "RaycastHit.get_transform",
                                                             .kind = pm::sdk::Kind::Method, .owner = "RaycastHit",
                                                             .name = "get_transform"};

inline constexpr const char* Physics_RaycastHit_params[] = {"Ray", "out RaycastHit", "float", "int",
                                                            "QueryTriggerInteraction"};

/// Casts a ray, the first collider it hits within the distance into the RaycastHit, on the layers of the
/// mask, counting trigger colliders as the last argument says; whether it hit. Static. Another
/// five-parameter overload exists (origin and direction apart), so the parameters tell them apart.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Physics_RaycastHit{.key = "Physics.RaycastHit", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "Physics", .name = "Raycast",
                                                       .type = "bool", .params = Physics_RaycastHit_params};

}  // namespace pm::map

// The game API for Unity's world queries (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this game
// build" once and returns its type's default (0, false) without touching the game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's NavMeshHit: where SamplePosition found the mesh. Its layout is Unity's own.
struct NavMeshHit {
  Vector3 position, normal;
  float distance;
  int32_t mask;
  int32_t hit;  // non-zero when something was found
};

/// Unity's physics queries.
struct Physics {
  /// Raycast(&origin, &direction, maxDistance, layerMask, triggers): whether anything is hit, the
  /// vectors passed by address as the x64 convention passes them. Unresolved: false.
  static constexpr pm::api::Method<pm::map::Physics_Raycast, bool(const Vector3*, const Vector3*, float, int32_t,
                                                                  int32_t)>
      Raycast{};
  /// RaycastHit(&ray, &hit, maxDistance, layerMask, queryTriggers): whether the ray hit a collider within
  /// maxDistance, filling `hit`. `hit` must be room for the game's RaycastHit: size it from the game
  /// (RaycastHit::type.make_array(1) and pm::api::ValueArray::stride). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Physics_RaycastHit, bool(const Ray*, void*, float, int32_t, int32_t)>
      RaycastHit{};
};

/// Whether a physics query counts triggers.
namespace QueryTriggerInteraction {
/// Ignore(): triggers are not hit. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::QueryTriggerInteraction_Ignore> Ignore{};
}  // namespace QueryTriggerInteraction

/// Unity's navigation mesh.
struct NavMesh {
  /// SamplePosition(&position, &hit, maxDistance, areaMask): the nearest point of the mesh into `hit`.
  /// Unresolved: false.
  static constexpr pm::api::Method<pm::map::NavMesh_SamplePosition, bool(const Vector3*, NavMeshHit*, float, int32_t)>
      SamplePosition{};
};

/// How far a path got.
namespace NavMeshPathStatus {
/// The path reaches its target.
inline constexpr pm::api::EnumValue<pm::map::NavMeshPathStatus_PathComplete> PathComplete{};
}  // namespace NavMeshPathStatus

/// A NavMeshPath. `path` below is one.
struct NavMeshPath {
  /// The type: make() for a new one, which ctor then sets up. Unresolved: null.
  static constexpr pm::api::Type<pm::map::NavMeshPath> type{};
  /// ctor(path). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::NavMeshPath_ctor, void(void*)> ctor{};
  /// get_status(path), a NavMeshPathStatus. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::NavMeshPath_get_status, int32_t(void*)> get_status{};
  /// get_corners(path), a Vector3[] read as a pm::api::Array<Vector3>. Unresolved: null.
  static constexpr pm::api::Method<pm::map::NavMeshPath_get_corners, void*(void*)> get_corners{};
};

/// A NavMeshAgent. `agent` below is one.
struct NavMeshAgent {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::NavMeshAgent> type{};
  /// steer(agent, dx, dz): walks the agent in that world direction this frame, the game's speed and destination
  /// its own (pm::api::AgentSteering says all it does). steer.available(): whether this game build allows it. The
  /// host does the work and needs entries of its own, which a mod never names or requires.
  static constexpr pm::api::AgentSteering steer{};
  /// SetDestination(agent, &target). Unresolved: false.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_SetDestination, bool(void*, const Vector3*)> SetDestination{};
  /// CalculatePath(agent, &target, path): the path the agent would walk from where it stands, into `path`, a
  /// NavMeshPath; false when it cannot. Unresolved: false.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_CalculatePath, bool(void*, const Vector3*, void*)> CalculatePath{};
  /// get_velocity(agent). Unresolved: zeros.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_get_velocity, Vector3(void*)> get_velocity{};
  /// set_angularSpeed(agent, degreesPerSecond). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_set_angularSpeed, void(void*, float)> set_angularSpeed{};
  /// Warp(agent, &position): false when it cannot. Unresolved: false.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_Warp, bool(void*, const Vector3*)> Warp{};
  /// set_speed(agent, metresPerSecond). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_set_speed, void(void*, float)> set_speed{};
  /// set_acceleration(agent, metresPerSecondSquared). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_set_acceleration, void(void*, float)> set_acceleration{};
  /// get_desiredVelocity_Injected(agent, &out): the velocity it wants into `out`. Unresolved: does
  /// nothing.
  static constexpr pm::api::Icall<pm::map::NavMeshAgent_get_desiredVelocity_Injected, void(void*, Vector3*)>
      get_desiredVelocity_Injected{};
  /// get_angularSpeed(agent). Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::NavMeshAgent_get_angularSpeed, float(void*)> get_angularSpeed{};
  /// get_speed(agent): its top speed in metres a second. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_get_speed, float(void*)> get_speed{};
  /// get_remainingDistance(agent): how far it still walks, Infinity while unknown. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_get_remainingDistance, float(void*)> get_remainingDistance{};
  /// get_pathPending(agent): whether its path is still being worked out. Unresolved: false.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_get_pathPending, bool(void*)> get_pathPending{};
  /// get_destination(agent): where it walks to. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::NavMeshAgent_get_destination, Vector3(void*)> get_destination{};
};

/// Unity's rigid body. `body` below is one.
struct Rigidbody {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Rigidbody> type{};
  /// get_worldCenterOfMass(body). Unresolved: zeros.
  static constexpr pm::api::Method<pm::map::Rigidbody_get_worldCenterOfMass, Vector3(void*)> get_worldCenterOfMass{};
  /// get_isKinematic(body). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Rigidbody_get_isKinematic, bool(void*)> get_isKinematic{};
  /// get_velocity(body). Unresolved: zeros.
  static constexpr pm::api::Method<pm::map::Rigidbody_get_velocity, Vector3(void*)> get_velocity{};
  /// set_velocity(body, &velocity). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Rigidbody_set_velocity, void(void*, const Vector3*)> set_velocity{};
  /// set_angularVelocity(body, &spin). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Rigidbody_set_angularVelocity, void(void*, const Vector3*)>
      set_angularVelocity{};
  /// set_position(body, &position). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Rigidbody_set_position, void(void*, const Vector3*)> set_position{};
};

/// DOTween's static API.
struct DOTween {
  /// IsTweening(target, alsoCheckIfIsPlaying). Unresolved: false.
  static constexpr pm::api::Method<pm::map::DOTween_IsTweening, bool(void*, bool)> IsTweening{};
};

/// Unity's random numbers.
struct Random {
  /// RandomRangeInt(min, max): an integer in [min, max). A mod usually hooks it. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Random_RandomRangeInt, int32_t(int32_t, int32_t)> RandomRangeInt{};
};

/// What a physics ray hit, a struct. `hit` below is its address.
struct RaycastHit {
  /// The type: klass(), make_array(n) (an array's stride is the struct's size). Unresolved: null.
  static constexpr pm::api::Type<pm::map::RaycastHit> type{};
  /// get_transform(hit): the Transform of what it hit. Unresolved: null.
  static constexpr pm::api::Method<pm::map::RaycastHit_get_transform, void*(void*)> get_transform{};
};

/// @}
}  // namespace phasmo
