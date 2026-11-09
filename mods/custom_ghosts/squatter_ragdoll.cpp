#include "squatter_ragdoll.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "Animator.h"
#include "LevelRoom.h"
#include "Scene.h"
#include "UnityCloth.h"
#include "UnityJoints.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "held.h"
#include "log.h"
#include "quat.h"
#include "runtime.h"

namespace pm::custom::squatter::ragdoll {
namespace {

namespace api = pm::api;

// What the ragdoll added to one bone: its body, its collider and (but for the root) its joint.
struct Added {
  api::Held bone, body, collider, joint;
};

std::vector<Added> g_parts;  // in kRagdollBones' order; empty when not built

// The game's HumanBodyBones value of each Part.
std::array<int32_t, kPartCount> bone_ids() {
  namespace B = phasmo::HumanBodyBones;
  return {B::Hips(),          B::Spine(),         B::Chest(),        B::Head(),         B::LeftUpperLeg(),
          B::LeftLowerLeg(),  B::LeftFoot(),      B::RightUpperLeg(), B::RightLowerLeg(), B::RightFoot(),
          B::LeftUpperArm(),  B::LeftLowerArm(),  B::LeftHand(),     B::RightUpperArm(), B::RightLowerArm(),
          B::RightHand()};
}

float axis_of(phasmo::Vector3 v, int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

// The local axis `v` lies most along (0 x, 1 y, 2 z): the Wizard's CalculateDirection.
int largest_axis(phasmo::Vector3 v) {
  const float x = std::fabs(v.x), y = std::fabs(v.y), z = std::fabs(v.z);
  return x >= y && x >= z ? 0 : y >= z ? 1 : 2;
}

// `v` snapped to the local axis it lies most along, with its sign: the Wizard's CalculateDirectionAxis.
phasmo::Vector3 snapped(phasmo::Vector3 v) {
  const int axis = largest_axis(v);
  const float sign = axis_of(v, axis) < 0.0f ? -1.0f : 1.0f;
  return {axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f};
}

phasmo::Vector3 minus(phasmo::Vector3 a, phasmo::Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float length(phasmo::Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// Where `other` lies in `bone`'s own space.
phasmo::Vector3 local_of(void* bone, void* other) {
  const phasmo::Vector3 at = phasmo::Transform::get_position(other);
  return phasmo::Transform::InverseTransformPoint(bone, &at);
}

// The body's own axis in `bone`'s space, snapped to the nearest bone axis.
phasmo::Vector3 joint_axis(void* bone, phasmo::Quaternion body, BodyAxis which) {
  const phasmo::Vector3 world = qrot(body, which == BodyAxis::Right ? phasmo::Vector3{1, 0, 0}
                                           : which == BodyAxis::Up  ? phasmo::Vector3{0, 1, 0}
                                                                    : phasmo::Vector3{0, 0, 1});
  return snapped(qrot(qinv(phasmo::Transform::get_rotation(bone)), world));
}

// Adds and sizes the collider of `b` on `bone`, as the Wizard sizes it. Null when it could not be added.
void* add_collider(const RagdollBone& b, void* go, const std::array<void*, kPartCount>& t) {
  void* const bone = t[b.bone];
  if (b.shape == RagdollShape::Capsule) {
    void* c = phasmo::GameObject::AddComponent(go, phasmo::CapsuleCollider::type.type_object());
    if (!c) return nullptr;
    const phasmo::Vector3 end = local_of(bone, t[b.end]);
    const int axis = largest_axis(end);
    const float height = std::fabs(axis_of(end, axis));
    phasmo::Vector3 centre{};
    (axis == 0 ? centre.x : axis == 1 ? centre.y : centre.z) = axis_of(end, axis) / 2;
    phasmo::CapsuleCollider::set_direction(c, axis);
    phasmo::CapsuleCollider::set_height(c, height);
    phasmo::CapsuleCollider::set_radius(c, height * b.width);
    phasmo::CapsuleCollider::set_center(c, &centre);
    return c;
  }
  const float span = length(minus(local_of(bone, t[b.left]), local_of(bone, t[b.right])));
  if (b.shape == RagdollShape::Sphere) {
    void* c = phasmo::GameObject::AddComponent(go, phasmo::SphereCollider::type.type_object());
    if (!c) return nullptr;
    const float radius = span * b.width;
    // One radius from the head bone, away from the hips: the Wizard's head.
    const phasmo::Vector3 hips = local_of(bone, t[Hips]);
    const int axis = largest_axis(hips);
    phasmo::Vector3 centre{};
    (axis == 0 ? centre.x : axis == 1 ? centre.y : centre.z) = axis_of(hips, axis) > 0.0f ? -radius : radius;
    phasmo::SphereCollider::set_radius(c, radius);
    phasmo::SphereCollider::set_center(c, &centre);
    return c;
  }
  void* c = phasmo::GameObject::AddComponent(go, phasmo::BoxCollider::type.type_object());
  if (!c) return nullptr;
  // The box round the bone, its top and its two sides, at least as deep as its sides' span times its width.
  const phasmo::Vector3 points[] = {{0, 0, 0}, local_of(bone, t[b.end]), local_of(bone, t[b.left]),
                                    local_of(bone, t[b.right])};
  phasmo::Vector3 lo = points[0], hi = points[0];
  for (const phasmo::Vector3& p : points) {
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
  const float depth = span * b.width;
  const phasmo::Vector3 size{std::max(hi.x - lo.x, depth), std::max(hi.y - lo.y, depth), std::max(hi.z - lo.z, depth)};
  const phasmo::Vector3 centre{(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2};
  phasmo::BoxCollider::set_size(c, &size);
  phasmo::BoxCollider::set_center(c, &centre);
  return c;
}

void set_limit(void* joint, auto setter, float degrees) {
  const phasmo::SoftJointLimit limit{degrees, 0.0f, 0.0f};
  setter(joint, &limit);
}

}  // namespace

bool build(void* animator) {
  destroy();
  if (!animator) return false;
  const int32_t layer = phasmo::Layers::DeadPlayer.index();
  void* const bodyType = phasmo::Rigidbody::type.type_object();
  void* const jointType = phasmo::CharacterJoint::type.type_object();
  if (layer < 0 || !bodyType || !jointType) {
    PM_LOG("squatter: no ragdoll: %s", layer < 0 ? "the DeadPlayer layer is missing" : "a part's type is missing");
    return false;
  }
  std::array<void*, kPartCount> t{};
  const auto ids = bone_ids();
  for (int i = 0; i < kPartCount; ++i) {
    t[i] = phasmo::Animator::GetBoneTransform(animator, ids[i]);
    if (!t[i]) {
      PM_LOG("squatter: no ragdoll: the figure lacks humanoid bone %d", i);
      return false;
    }
  }
  const phasmo::Quaternion body = phasmo::Transform::get_rotation(phasmo::Component::get_transform(animator));
  for (const RagdollBone& b : kRagdollBones) {
    Added& added = g_parts.emplace_back();
    added.bone = t[b.bone];
    void* go = phasmo::Component::get_gameObject(t[b.bone]);
    void* rb = go ? phasmo::GameObject::AddComponent(go, bodyType) : nullptr;
    void* collider = rb ? add_collider(b, go, t) : nullptr;
    if (!collider) {
      PM_LOG("squatter: no ragdoll: a part could not be added to bone %d", b.bone);
      destroy();
      return false;
    }
    phasmo::GameObject::set_layer(go, layer);
    phasmo::Rigidbody::set_mass(rb, b.mass);
    phasmo::Rigidbody::set_isKinematic(rb, true);
    phasmo::Collider::set_enabled(collider, false);
    added.body = rb;
    added.collider = collider;
    if (b.parent < 0) continue;
    void* joint = phasmo::GameObject::AddComponent(go, jointType);
    if (!joint) {
      PM_LOG("squatter: no ragdoll: a joint could not be added to bone %d", b.bone);
      destroy();
      return false;
    }
    added.joint = joint;
    phasmo::Joint::set_connectedBody(joint, g_parts[static_cast<size_t>(b.parent)].body);
    const phasmo::Vector3 twist = joint_axis(t[b.bone], body, b.twist);
    const phasmo::Vector3 swing = joint_axis(t[b.bone], body, b.swing);
    phasmo::Joint::set_axis(joint, &twist);
    phasmo::CharacterJoint::set_swingAxis(joint, &swing);
    set_limit(joint, phasmo::CharacterJoint::set_lowTwistLimit, b.twistLow);
    set_limit(joint, phasmo::CharacterJoint::set_highTwistLimit, b.twistHigh);
    set_limit(joint, phasmo::CharacterJoint::set_swing1Limit, b.swing1);
    set_limit(joint, phasmo::CharacterJoint::set_swing2Limit, b.swing2);
  }
  return true;
}

bool built() {
  if (g_parts.empty()) return false;
  for (const Added& a : g_parts)
    if (!api::alive(a.body) || !api::alive(a.collider)) return false;
  return true;
}

void set_physical(bool on, phasmo::Vector3 velocity) {
  if (!built()) return;
  const phasmo::Vector3 launch = capped_launch(velocity);
  for (const Added& a : g_parts) {
    phasmo::Collider::set_enabled(a.collider, on);
    phasmo::Rigidbody::set_isKinematic(a.body, !on);
    if (on) phasmo::Rigidbody::set_velocity(a.body, &launch);
  }
}

float fastest() {
  if (!built()) return 0.0f;
  float best = 0.0f;
  for (const Added& a : g_parts) best = std::max(best, length(phasmo::Rigidbody::get_velocity(a.body)));
  return best;
}

std::vector<phasmo::Quaternion> local_pose() {
  std::vector<phasmo::Quaternion> pose;
  if (!built()) return pose;
  pose.reserve(g_parts.size());
  for (const Added& a : g_parts) pose.push_back(phasmo::Transform::get_localRotation(a.bone));
  return pose;
}

void destroy() {
  // Joints first, then colliders and bodies: Unity refuses to remove a Rigidbody a joint still needs.
  for (auto it = g_parts.rbegin(); it != g_parts.rend(); ++it)
    if (api::alive(it->joint)) phasmo::Object::DestroyImmediate(it->joint, false);
  for (auto it = g_parts.rbegin(); it != g_parts.rend(); ++it) {
    if (api::alive(it->collider)) phasmo::Object::DestroyImmediate(it->collider, false);
    if (api::alive(it->body)) phasmo::Object::DestroyImmediate(it->body, false);
  }
  g_parts.clear();
}

}  // namespace pm::custom::squatter::ragdoll
