#pragma once

// Unity's joints and the collider shapes a ragdoll is built from (UnityEngine.PhysicsModule): a Rigidbody, a collider and
// a CharacterJoint per bone, as Unity's Ragdoll Wizard lays them out. The game builds its dead players' ragdolls from the
// same parts (RagdollBone pairs a Collider with a CharacterJoint).

#include "LevelRoom.h"
#include "Transform.h"
#include "UnityWorld.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* UnityJoints_Vector3[] = {"Vector3"};
inline constexpr const char* UnityJoints_SoftJointLimit[] = {"SoftJointLimit"};
inline constexpr const char* UnityJoints_Rigidbody[] = {"Rigidbody"};

/// Unity's joint, the base of every joint: what it is attached to and about which axis. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc Joint{.key = "Joint", .name = "Joint", .assembly = "UnityEngine.PhysicsModule",
                                          .ns = "UnityEngine"};

/// Sets the Rigidbody the joint holds its own body to (null: the world). Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Joint_set_connectedBody{.key = "Joint.set_connectedBody", .kind = pm::sdk::Kind::Method,
                                                            .owner = "Joint", .name = "set_connectedBody",
                                                            .params = UnityJoints_Rigidbody};

/// Sets the joint's primary axis, in its own body's space: a CharacterJoint twists about it. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc Joint_set_axis{.key = "Joint.set_axis", .kind = pm::sdk::Kind::Method,
                                                   .owner = "Joint", .name = "set_axis", .params = UnityJoints_Vector3};

/// Unity's character joint: a ball and socket with a twist limit about its axis and two swing limits, the joint of a
/// ragdoll's limbs. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CharacterJoint{.key = "CharacterJoint", .name = "CharacterJoint",
                                                   .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine",
                                                   .base = &Joint};

/// Sets the axis the joint swings about, in its own body's space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CharacterJoint_set_swingAxis{
    .key = "CharacterJoint.set_swingAxis", .kind = pm::sdk::Kind::Method, .owner = "CharacterJoint",
    .name = "set_swingAxis", .params = UnityJoints_Vector3};

/// Sets the lowest twist about the axis, a SoftJointLimit in degrees. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CharacterJoint_set_lowTwistLimit{
    .key = "CharacterJoint.set_lowTwistLimit", .kind = pm::sdk::Kind::Method, .owner = "CharacterJoint",
    .name = "set_lowTwistLimit", .params = UnityJoints_SoftJointLimit};

/// Sets the highest twist about the axis, a SoftJointLimit in degrees. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CharacterJoint_set_highTwistLimit{
    .key = "CharacterJoint.set_highTwistLimit", .kind = pm::sdk::Kind::Method, .owner = "CharacterJoint",
    .name = "set_highTwistLimit", .params = UnityJoints_SoftJointLimit};

/// Sets the swing limit about the swing axis, a SoftJointLimit in degrees. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CharacterJoint_set_swing1Limit{
    .key = "CharacterJoint.set_swing1Limit", .kind = pm::sdk::Kind::Method, .owner = "CharacterJoint",
    .name = "set_swing1Limit", .params = UnityJoints_SoftJointLimit};

/// Sets the swing limit about the axis at right angles to both others, a SoftJointLimit in degrees. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc CharacterJoint_set_swing2Limit{
    .key = "CharacterJoint.set_swing2Limit", .kind = pm::sdk::Kind::Method, .owner = "CharacterJoint",
    .name = "set_swing2Limit", .params = UnityJoints_SoftJointLimit};

/// Unity's box collider (UnityEngine.PhysicsModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc BoxCollider{.key = "BoxCollider", .name = "BoxCollider",
                                                .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine",
                                                .base = &Collider};

/// Sets the box's size, metres, in its object's space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc BoxCollider_set_size{.key = "BoxCollider.set_size", .kind = pm::sdk::Kind::Method,
                                                         .owner = "BoxCollider", .name = "set_size",
                                                         .params = UnityJoints_Vector3};

/// Sets the box's centre in its object's space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc BoxCollider_set_center{.key = "BoxCollider.set_center", .kind = pm::sdk::Kind::Method,
                                                           .owner = "BoxCollider", .name = "set_center",
                                                           .params = UnityJoints_Vector3};

/// Unity's sphere collider (UnityEngine.PhysicsModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc SphereCollider{.key = "SphereCollider", .name = "SphereCollider",
                                                   .assembly = "UnityEngine.PhysicsModule", .ns = "UnityEngine",
                                                   .base = &Collider};

inline constexpr const char* UnityJoints_float[] = {"float"};

/// Sets the sphere's radius, metres. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc SphereCollider_set_radius{.key = "SphereCollider.set_radius",
                                                              .kind = pm::sdk::Kind::Method, .owner = "SphereCollider",
                                                              .name = "set_radius", .params = UnityJoints_float};

/// Sets the sphere's centre in its object's space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc SphereCollider_set_center{.key = "SphereCollider.set_center",
                                                              .kind = pm::sdk::Kind::Method, .owner = "SphereCollider",
                                                              .name = "set_center", .params = UnityJoints_Vector3};

}  // namespace pm::map

// The game API for joints and collider shapes (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build" once and does
// nothing.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's SoftJointLimit, as the joint's limit setters take it: the limit in degrees, its bounce (0 to 1), and how
/// near the limit the joint starts to resist. Passed by address, as the x64 convention passes a struct this size.
struct SoftJointLimit {
  float limit = 0.0f;            ///< the limit, degrees
  float bounciness = 0.0f;       ///< how much it bounces off the limit, 0 to 1
  float contactDistance = 0.0f;  ///< how near the limit, degrees, the joint starts to resist; 0 for Unity's own
};

/// Unity's joint. `joint` below is one, any kind.
struct Joint {
  /// set_connectedBody(joint, body): the body it holds to; null for the world. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Joint_set_connectedBody, void(pm::api::Ref<pm::map::Joint>, pm::api::Ref<pm::map::Rigidbody>)> set_connectedBody{};
  /// set_axis(joint, &axis): its primary axis in its body's space. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Joint_set_axis, void(pm::api::Ref<pm::map::Joint>, const Vector3*)> set_axis{};
};

/// Unity's character joint. `joint` below is one.
struct CharacterJoint {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::CharacterJoint> type{};
  /// set_swingAxis(joint, &axis). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CharacterJoint_set_swingAxis, void(pm::api::Ref<pm::map::CharacterJoint>, const Vector3*)> set_swingAxis{};
  /// set_lowTwistLimit(joint, &limit). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CharacterJoint_set_lowTwistLimit, void(pm::api::Ref<pm::map::CharacterJoint>, const SoftJointLimit*)>
      set_lowTwistLimit{};
  /// set_highTwistLimit(joint, &limit). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CharacterJoint_set_highTwistLimit, void(pm::api::Ref<pm::map::CharacterJoint>, const SoftJointLimit*)>
      set_highTwistLimit{};
  /// set_swing1Limit(joint, &limit). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CharacterJoint_set_swing1Limit, void(pm::api::Ref<pm::map::CharacterJoint>, const SoftJointLimit*)>
      set_swing1Limit{};
  /// set_swing2Limit(joint, &limit). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CharacterJoint_set_swing2Limit, void(pm::api::Ref<pm::map::CharacterJoint>, const SoftJointLimit*)>
      set_swing2Limit{};
};

/// Unity's box collider. `box` below is one.
struct BoxCollider {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::BoxCollider> type{};
  /// set_size(box, &size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::BoxCollider_set_size, void(pm::api::Ref<pm::map::BoxCollider>, const Vector3*)> set_size{};
  /// set_center(box, &centre). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::BoxCollider_set_center, void(pm::api::Ref<pm::map::BoxCollider>, const Vector3*)> set_center{};
};

/// Unity's sphere collider. `sphere` below is one.
struct SphereCollider {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SphereCollider> type{};
  /// set_radius(sphere, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SphereCollider_set_radius, void(pm::api::Ref<pm::map::SphereCollider>, float)> set_radius{};
  /// set_center(sphere, &centre). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SphereCollider_set_center, void(pm::api::Ref<pm::map::SphereCollider>, const Vector3*)> set_center{};
};

/// @}
}  // namespace phasmo
