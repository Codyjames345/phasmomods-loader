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

namespace pm::map {

inline constexpr const char* UnityCloth_array[] = {"Array"};
inline constexpr const char* UnityCloth_bool[] = {"bool"};
inline constexpr const char* UnityCloth_float[] = {"float"};
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

/// Unity's capsule collider (UnityEngine.PhysicsModule), a Collider: what a Cloth collides with. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc CapsuleCollider{.key = "CapsuleCollider", .name = "CapsuleCollider",
                                                    .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine"};

/// Sets the capsule's radius, metres. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_radius{
    .key = "CapsuleCollider.set_radius", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_radius", .params = UnityCloth_float};

/// Sets the capsule's height, metres, ends included; one shorter than its diameter is a sphere. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc CapsuleCollider_set_height{
    .key = "CapsuleCollider.set_height", .kind = pm::sdk::Kind::Method, .owner = "CapsuleCollider",
    .name = "set_height", .params = UnityCloth_float};

}  // namespace pm::map

// The game API for Cloth (sdk/api/game_api.h). Every member is called on Unity's main thread. One that did not resolve
// in the running game build logs "<key> is not available in this game build" once and returns its type's default
// (null) or does nothing. The Cloth members are optional: check Cloth::type.available() first.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's Cloth component. `cloth` below is one.
struct Cloth {
  /// The type: type_object() for GameObject.AddComponent. Optional: check available(). Unresolved: null.
  static constexpr pm::api::Type<pm::map::Cloth> type{};
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
};

/// Unity's capsule collider. `capsule` below is one.
struct CapsuleCollider {
  /// The type: type_object() for GameObject.AddComponent, make_array() for a CapsuleCollider[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::CapsuleCollider> type{};
  /// set_radius(capsule, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_radius, void(void*, float)> set_radius{};
  /// set_height(capsule, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CapsuleCollider_set_height, void(void*, float)> set_height{};
};

/// @}
}  // namespace phasmo
