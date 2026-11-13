#pragma once

// Unity's Cloth: a SkinnedMeshRenderer's mesh simulated as cloth, as a bedsheet or a curtain moves. The game's own scenes
// carry many Cloth components, and its ClothAttachCharacters sets their capsule colliders. The managed Cloth class keeps
// only its capsule colliders in this build; every other member is an engine function the runtime registers by its icall
// name, so those entries resolve only in the running game. sdk/internals/engine-experiments.md records what an in-game
// test showed of Cloth added at runtime; sdk/guides/custom-animation.md shows the helper that does it (sdk/api/models.h).
//
// Optional, every entry: the Cloth type lives in its own Unity module (UnityEngine.ClothModule), which a later build
// drops if the game stops using cloth. Only optional type entries name that module, so a build without it still
// resolves the map and only these entries stay unresolved (sdk/README.md, "The model is read from every assembly").
// A mod that needs Cloth checks available() at its start and fails it with a logged reason.

#include "entry.h"
#include "game_api.h"
#include "LevelRoom.h"

namespace pm::map {

inline constexpr const char* UnityCloth_array[] = {"Array"};
inline constexpr const char* UnityCloth_bool[] = {"bool"};
inline constexpr const char* UnityCloth_float[] = {"float"};
inline constexpr const char* UnityCloth_int[] = {"int"};
inline constexpr const char* UnityCloth_Vector3[] = {"Vector3"};
inline constexpr const char* UnityCloth_CapsuleColliders[] = {"CapsuleCollider[]"};

/// Unity's cloth component (UnityEngine.ClothModule). Add it to a GameObject that has a SkinnedMeshRenderer with a mesh,
/// after the mesh is set: it simulates that mesh's vertices. Decompiled: ClothAttachCharacters uses it.
inline constexpr pm::sdk::EntryDesc Cloth{.key = "Cloth", .confidence = pm::sdk::Confidence::Decompiled,
                                          .flags = pm::sdk::kOptional, .name = "Cloth",
                                          .assembly = "UnityEngine.ClothModule", .ns = "UnityEngine"};

/// Sets the capsules the cloth collides with, a CapsuleCollider[]. A capsule shorter than its diameter is a sphere.
/// The cloth reads only this list: a collider in it touches the cloth whatever its layer. Decompiled:
/// ClothAttachCharacters sets it.
inline constexpr pm::sdk::EntryDesc Cloth_set_capsuleColliders{
    .key = "Cloth.set_capsuleColliders", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kOptional, .owner = "Cloth", .name = "set_capsuleColliders", .params = UnityCloth_CapsuleColliders};

/// Sets each vertex's constraint, an array as long as the mesh's vertices whose elements are two floats: how far the
/// vertex may move from where the skinning puts it (0 pins it there), then the depth of the sphere it may not enter.
/// Unity's element type, ClothSkinningCoefficient, is stripped from this build; an array of Vector2 has its layout and
/// the engine took one in game. Never call the getter, which would have to make an array of the missing type. Live:
/// the Squatter's engine test pinned the vertices it set (2026-11-05).
inline constexpr pm::sdk::EntryDesc Cloth_set_coefficients{
    .key = "Cloth.set_coefficients", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Live,
    .flags = pm::sdk::kOptional, .name = "UnityEngine.Cloth::set_coefficients", .params = UnityCloth_array};

/// Whether gravity pulls the cloth. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_useGravity{
    .key = "Cloth.set_useGravity", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_useGravity", .params = UnityCloth_bool};

/// How stiffly the cloth resists bending, 0 to 1. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_bendingStiffness{
    .key = "Cloth.set_bendingStiffness", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_bendingStiffness", .params = UnityCloth_float};

/// How stiffly the cloth resists stretching, 0 to 1. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_stretchingStiffness{
    .key = "Cloth.set_stretchingStiffness", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_stretchingStiffness", .params = UnityCloth_float};

/// How much the cloth's motion is damped, 0 to 1. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_damping{.key = "Cloth.set_damping", .kind = pm::sdk::Kind::Icall,
                                                      .flags = pm::sdk::kOptional,
                                                      .name = "UnityEngine.Cloth::set_damping",
                                                      .params = UnityCloth_float};

/// How much the cloth answers its object's motion through the world, 0 (not at all) to 1: lower hangs heavier and sways
/// less as the body runs. Unity's default is 0.5. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_worldVelocityScale{
    .key = "Cloth.set_worldVelocityScale", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_worldVelocityScale", .params = UnityCloth_float};

/// How much the cloth answers its object's acceleration, 0 to 1: lower whips less as the body starts, stops and turns.
/// Unity's default is 1. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_worldAccelerationScale{
    .key = "Cloth.set_worldAccelerationScale", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_worldAccelerationScale", .params = UnityCloth_float};

/// Whether the cloth adds a virtual particle in each triangle for its collisions, 0 or 1: with it the colliders press
/// the cloth in smooth bulges rather than poking it at its points. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_useVirtualParticles{
    .key = "Cloth.set_useVirtualParticles", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_useVirtualParticles", .params = UnityCloth_float};

/// The cloth's points as simulated, a new Vector3[] of one per mesh vertex, in its SkinnedMeshRenderer's space (or the
/// world's: a reader checks). Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Cloth_get_vertices{.key = "Cloth.get_vertices", .kind = pm::sdk::Kind::Icall,
                                                       .flags = pm::sdk::kOptional,
                                                       .name = "UnityEngine.Cloth::get_vertices"};

/// The cloth's normals as simulated, a new Vector3[] beside get_vertices'. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Cloth_get_normals{.key = "Cloth.get_normals", .kind = pm::sdk::Kind::Icall,
                                                      .flags = pm::sdk::kOptional,
                                                      .name = "UnityEngine.Cloth::get_normals"};

/// Forgets the motion the cloth has accumulated from its GameObject moving through the world. Unity's own answer to
/// moving a cloth's transform a long way in one frame (a teleport, a warp): call it right after moving the transform,
/// and the cloth goes with it instead of being dragged after it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_ClearTransformMotion{
    .key = "Cloth.ClearTransformMotion", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::ClearTransformMotion"};

/// How close, in metres, two of the cloth's points may come before they push each other apart: its self-collision, which
/// keeps the cloth from folding through itself. 0, Unity's default, is off. It acts only between the points
/// SetSelfAndInterCollisionIndices names. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cloth_set_selfCollisionDistance{
    .key = "Cloth.set_selfCollisionDistance", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_selfCollisionDistance", .params = UnityCloth_float};

/// How hard the cloth's points push each other apart when they come too close, 0 to 1. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc Cloth_set_selfCollisionStiffness{
    .key = "Cloth.set_selfCollisionStiffness", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::set_selfCollisionStiffness", .params = UnityCloth_float};

inline constexpr const char* UnityCloth_ListUInt32[] = {"List<UInt32>"};

/// Which of the cloth's points take part in its self-collision, a List<UInt32> of mesh vertex indices. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Cloth_SetSelfAndInterCollisionIndices{
    .key = "Cloth.SetSelfAndInterCollisionIndices", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kOptional,
    .name = "UnityEngine.Cloth::SetSelfAndInterCollisionIndices", .params = UnityCloth_ListUInt32};

/// System.UInt32 (mscorlib). Inferred: .NET's documented type.
inline constexpr pm::sdk::EntryDesc UInt32{.key = "UInt32", .name = "UInt32", .assembly = "mscorlib", .ns = "System"};

inline constexpr const char* ListT_fieldTypes[] = {"T[]"};

/// The generic definition List<T> (mscorlib, System.Collections.Generic). Inferred: .NET's documented type.
inline constexpr pm::sdk::EntryDesc ListT{.key = "List`1", .name = "List", .assembly = "mscorlib",
                                          .ns = "System.Collections.Generic", .fieldTypes = ListT_fieldTypes};

inline constexpr const char* List_UInt32_args[] = {"@UInt32"};
inline constexpr const char* ListT_ctor_params[] = {"int"};
inline constexpr const char* ListT_Add_params[] = {"T"};

/// A List<UInt32>'s constructor taking its first capacity: List<UInt32>.ctor(list, capacity) on an object made with
/// new_object(). Optional: only the running game can say whether this build has code for the instance. Checked at
/// startup only. Inferred: .NET's documented API.
inline constexpr pm::sdk::EntryDesc List_UInt32_ctor{
    .key = "List<UInt32>.ctor", .kind = pm::sdk::Kind::Inflated, .flags = pm::sdk::kOptional, .owner = "List`1",
    .name = ".ctor", .params = ListT_ctor_params, .typeArgs = List_UInt32_args};

/// Adds a number to a List<UInt32>: List<UInt32>.Add(list, value). Optional, as the constructor. Checked at startup
/// only. Inferred: .NET's documented API.
inline constexpr pm::sdk::EntryDesc List_UInt32_Add{
    .key = "List<UInt32>.Add", .kind = pm::sdk::Kind::Inflated, .flags = pm::sdk::kOptional, .owner = "List`1",
    .name = "Add", .params = ListT_Add_params, .typeArgs = List_UInt32_args};

/// Unity's capsule collider (UnityEngine.PhysicsModule), a Collider: what a Cloth collides with. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc CapsuleCollider{.key = "CapsuleCollider", .name = "CapsuleCollider",
                                                    .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine",
                                                    .base = &Collider};

/// Sets the capsule's radius, metres. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_radius{
    .key = "CapsuleCollider.set_radius", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_radius", .params = UnityCloth_float};

/// Sets the capsule's height, metres, ends included; one shorter than its diameter is a sphere. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_height{
    .key = "CapsuleCollider.set_height", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_height", .params = UnityCloth_float};

/// Sets the capsule's centre in its object's space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_center{
    .key = "CapsuleCollider.set_center", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_center", .params = UnityCloth_Vector3};

/// Sets the object axis the capsule's length runs along: 0 for X, 1 for Y, 2 for Z. Inferred: Unity's documented
/// property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_direction{
    .key = "CapsuleCollider.set_direction", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_direction", .params = UnityCloth_int};

}  // namespace pm::map

// The game API for Cloth (sdk/api/game_api.h). Every member is called on Unity's main thread. One that did not resolve
// in the running game build logs "<key> is not available in this game build" once and returns its type's default
// (null) or does nothing. The Cloth members are optional: check Cloth::available() first.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's Cloth component. `cloth` below is one.
struct Cloth {
  /// The type: type_object() for GameObject.AddComponent. Optional: check available(). Unresolved: null.
  static constexpr pm::api::Type<pm::map::Cloth> type{};
  /// Whether the Cloth type resolved in this game build, whose module may be absent. Never logs.
  /// @return True when it resolved.
  template <int = 0>
  static bool available() {
    return decltype(type)::use::resolved();
  }
  /// set_capsuleColliders(cloth, capsules), a CapsuleCollider[]. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Cloth_set_capsuleColliders, void(void*, void*)> set_capsuleColliders{};
  /// set_coefficients(cloth, coefficients), a Vector2[] as long as the mesh's vertices (map note). Unresolved: does
  /// nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_coefficients, void(void*, void*)> set_coefficients{};
  /// set_useGravity(cloth, on). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_useGravity, void(void*, bool)> set_useGravity{};
  /// set_bendingStiffness(cloth, stiffness). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_bendingStiffness, void(void*, float)> set_bendingStiffness{};
  /// set_stretchingStiffness(cloth, stiffness). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_stretchingStiffness, void(void*, float)> set_stretchingStiffness{};
  /// set_damping(cloth, damping). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_damping, void(void*, float)> set_damping{};
  /// set_worldVelocityScale(cloth, share), 0 to 1. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_worldVelocityScale, void(void*, float)> set_worldVelocityScale{};
  /// set_worldAccelerationScale(cloth, share), 0 to 1. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_worldAccelerationScale, void(void*, float)>
      set_worldAccelerationScale{};
  /// set_useVirtualParticles(cloth, 0 or 1). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_useVirtualParticles, void(void*, float)> set_useVirtualParticles{};
  /// ClearTransformMotion(cloth): forgets the motion the cloth has accumulated from its GameObject moving through the
  /// world. Call it right after moving that GameObject a long way in one frame (a teleport, a warp), so the cloth goes
  /// with it instead of being dragged after it. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_ClearTransformMotion, void(void*)> ClearTransformMotion{};
  /// get_vertices(cloth): its simulated points, a new Vector3[]. Unresolved: null.
  static constexpr pm::api::Icall<pm::map::Cloth_get_vertices, void*(void*)> get_vertices{};
  /// get_normals(cloth): its simulated normals, a new Vector3[]. Unresolved: null.
  static constexpr pm::api::Icall<pm::map::Cloth_get_normals, void*(void*)> get_normals{};
  /// set_selfCollisionDistance(cloth, metres): how close two of the points SetSelfAndInterCollisionIndices names may
  /// come; 0 is off. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_selfCollisionDistance, void(void*, float)>
      set_selfCollisionDistance{};
  /// set_selfCollisionStiffness(cloth, stiffness), 0 to 1. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_set_selfCollisionStiffness, void(void*, float)>
      set_selfCollisionStiffness{};
  /// SetSelfAndInterCollisionIndices(cloth, list), a List<UInt32> of mesh vertex indices (ListUInt32 below makes one).
  /// Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Cloth_SetSelfAndInterCollisionIndices, void(void*, void*)>
      SetSelfAndInterCollisionIndices{};
};

/// A List<UInt32>, for Cloth.SetSelfAndInterCollisionIndices: `void* list = ListUInt32::ctor.new_object();
/// ListUInt32::ctor(list, capacity);` then `ListUInt32::Add(list, value)` for each number.
struct ListUInt32 {
  /// The constructor, ctor(list, capacity), and new_object() for the list it runs on. Unresolved: does nothing, and
  /// new_object() gives null.
  static constexpr pm::api::Method<pm::map::List_UInt32_ctor, void(void*, int32_t)> ctor{};
  /// Add(list, value). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::List_UInt32_Add, void(void*, uint32_t)> Add{};
};

/// Unity's capsule collider. `capsule` below is one.
struct CapsuleCollider {
  /// The type: type_object() for GameObject.AddComponent, make_array() for a CapsuleCollider[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::CapsuleCollider> type{};
  /// set_radius(capsule, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_radius, void(void*, float)> set_radius{};
  /// set_height(capsule, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_height, void(void*, float)> set_height{};
  /// set_center(capsule, &centre). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_center, void(void*, const Vector3*)> set_center{};
  /// set_direction(capsule, axis): 0 X, 1 Y, 2 Z. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_direction, void(void*, int32_t)> set_direction{};
};

/// @}
}  // namespace phasmo
