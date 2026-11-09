// Offline tests for the Squatter's ragdoll (mods/custom_ghosts/squatter_ragdoll.cpp) over the fake backend: the table
// laid out as Unity's Ragdoll Wizard lays it out, the parts it adds to the figure's bones, kinematic on the DeadPlayer
// layer until a fall, and nothing left behind when it or the figure goes. Run: build\squatter_ragdoll_test.exe
#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "Animator.h"
#include "LevelRoom.h"
#include "Scene.h"
#include "UnityCloth.h"
#include "UnityJoints.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"
#include "squatter_ragdoll.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};  // the count of hooked bodies running, which hooks.cpp keeps in the game
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace ragdoll = pm::custom::squatter::ragdoll;
using ragdoll::kRagdollBones;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};

// A component the ragdoll added, with what it was told.
struct Comp {
  Obj obj;
  void* go = nullptr;
  void* type = nullptr;
  bool kinematic = false, enabled = true;
  float mass = 0.0f;
  phasmo::Vector3 velocity{};
  void* connected = nullptr;
  float size = 0.0f;  // a capsule's height, a box's largest side, a sphere's radius
};

// Made-up HumanBodyBones values, so the code must read them from the map: Part p is kBoneBase + p.
constexpr int32_t kBoneBase = 100;
constexpr int32_t kDeadPlayerLayer = 15;

int kRigidbodyClass, kCapsuleClass, kBoxClass, kSphereClass, kJointClass;
Obj g_animator, g_root, g_bones[ragdoll::kPartCount], g_gos[ragdoll::kPartCount];
std::deque<Comp> g_comps;
std::vector<int32_t> g_layers;  // each bone GameObject's layer, -1 untouched
std::set<const void*> g_destroyed;

// A standing body, metres, facing +z: the hips at 1 m.
phasmo::Vector3 bone_position(int part) {
  using namespace ragdoll;
  switch (part) {
    case Hips: return {0, 1.0f, 0};
    case Spine: return {0, 1.1f, 0};
    case Chest: return {0, 1.3f, 0};
    case Head: return {0, 1.6f, 0};
    case LeftUpperLeg: return {-0.1f, 0.95f, 0};
    case LeftLowerLeg: return {-0.1f, 0.5f, 0};
    case LeftFoot: return {-0.1f, 0.08f, 0};
    case RightUpperLeg: return {0.1f, 0.95f, 0};
    case RightLowerLeg: return {0.1f, 0.5f, 0};
    case RightFoot: return {0.1f, 0.08f, 0};
    case LeftUpperArm: return {-0.2f, 1.45f, 0};
    case LeftLowerArm: return {-0.2f, 1.15f, 0};
    case LeftHand: return {-0.2f, 0.9f, 0};
    case RightUpperArm: return {0.2f, 1.45f, 0};
    case RightLowerArm: return {0.2f, 1.15f, 0};
    case RightHand: return {0.2f, 0.9f, 0};
    default: return {};
  }
}

int part_of(const void* t) {
  for (int i = 0; i < ragdoll::kPartCount; ++i)
    if (t == &g_bones[i]) return i;
  return -1;
}

int go_part(const void* go) {
  for (int i = 0; i < ragdoll::kPartCount; ++i)
    if (go == &g_gos[i]) return i;
  return -1;
}

Comp* comp(const void* c) {
  for (Comp& k : g_comps)
    if (&k.obj == c) return &k;
  return nullptr;
}

void* bone_transform(void* animator, int32_t which, const void*) {
  const int part = which - kBoneBase;
  return animator == &g_animator && part >= 0 && part < ragdoll::kPartCount ? &g_bones[part] : nullptr;
}
void* component_go(void* c, const void*) {
  const int part = part_of(c);
  if (part >= 0) return &g_gos[part];
  Comp* k = comp(c);
  return k ? k->go : nullptr;
}
void* component_transform(void* c, const void*) { return c == &g_animator ? &g_root : nullptr; }
phasmo::Vector3 position(void* t, const void*) { return part_of(t) >= 0 ? bone_position(part_of(t)) : phasmo::Vector3{}; }
phasmo::Quaternion rotation(void*, const void*) { return {0, 0, 0, 1}; }
phasmo::Quaternion local_rotation(void* t, const void*) {
  const float h = 0.01f * static_cast<float>(part_of(t));
  return {std::sin(h), 0, 0, std::cos(h)};
}
phasmo::Vector3 inverse_point(void* t, const phasmo::Vector3* p, const void*) {
  const phasmo::Vector3 at = bone_position(part_of(t));
  return {p->x - at.x, p->y - at.y, p->z - at.z};
}
void set_layer(void* go, int32_t layer, const void*) {
  const int part = go_part(go);
  if (part >= 0) g_layers[part] = layer;
}
void* add_component(void* go, void* type, const void*) {
  Comp& k = g_comps.emplace_back();
  k.go = go;
  k.type = type;
  return &k.obj;
}
void set_mass(void* b, float m, const void*) { comp(b)->mass = m; }
void set_kinematic(void* b, bool on, const void*) { comp(b)->kinematic = on; }
void set_velocity(void* b, const phasmo::Vector3* v, const void*) { comp(b)->velocity = *v; }
phasmo::Vector3 get_velocity(void* b, const void*) { return comp(b)->velocity; }
void set_enabled(void* c, bool on, const void*) { comp(c)->enabled = on; }
void set_size_float(void* c, float v, const void*) { comp(c)->size = std::max(comp(c)->size, v); }
void set_size_vector(void* c, const phasmo::Vector3* v, const void*) {
  comp(c)->size = std::max({comp(c)->size, v->x, v->y, v->z});
}
void set_vector(void*, const phasmo::Vector3*, const void*) {}
void set_int(void*, int32_t, const void*) {}
void set_connected(void* j, void* body, const void*) { comp(j)->connected = body; }
void set_limit(void*, const phasmo::SoftJointLimit*, const void*) {}
void destroy_immediate(void* obj, bool) {
  g_destroyed.insert(obj);
  fake::destroy(obj);
}

void start(bool withLayer = true) {
  fake::reset();
  g_comps.clear();
  g_destroyed.clear();
  g_layers.assign(ragdoll::kPartCount, -1);
  fake::type(map::Rigidbody, &kRigidbodyClass);
  fake::type(map::CapsuleCollider, &kCapsuleClass);
  fake::type(map::BoxCollider, &kBoxClass);
  fake::type(map::SphereCollider, &kSphereClass);
  fake::type(map::CharacterJoint, &kJointClass);
  const pm::sdk::EntryDesc* const bones[] = {
      &map::HumanBodyBones_Hips,          &map::HumanBodyBones_Spine,         &map::HumanBodyBones_Chest,
      &map::HumanBodyBones_Head,          &map::HumanBodyBones_LeftUpperLeg,  &map::HumanBodyBones_LeftLowerLeg,
      &map::HumanBodyBones_LeftFoot,      &map::HumanBodyBones_RightUpperLeg, &map::HumanBodyBones_RightLowerLeg,
      &map::HumanBodyBones_RightFoot,     &map::HumanBodyBones_LeftUpperArm,  &map::HumanBodyBones_LeftLowerArm,
      &map::HumanBodyBones_LeftHand,      &map::HumanBodyBones_RightUpperArm, &map::HumanBodyBones_RightLowerArm,
      &map::HumanBodyBones_RightHand};
  for (int i = 0; i < ragdoll::kPartCount; ++i) fake::value(*bones[i], kBoneBase + i);
  fake::method(map::Animator_GetBoneTransform, reinterpret_cast<void*>(&bone_transform));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&component_go));
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&component_transform));
  fake::method(map::Transform_get_position, reinterpret_cast<void*>(&position));
  fake::method(map::Transform_get_rotation, reinterpret_cast<void*>(&rotation));
  fake::method(map::Transform_get_localRotation, reinterpret_cast<void*>(&local_rotation));
  fake::method(map::Transform_InverseTransformPoint, reinterpret_cast<void*>(&inverse_point));
  fake::method(map::GameObject_set_layer, reinterpret_cast<void*>(&set_layer));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&add_component));
  fake::method(map::Rigidbody_set_mass, reinterpret_cast<void*>(&set_mass));
  fake::method(map::Rigidbody_set_isKinematic, reinterpret_cast<void*>(&set_kinematic));
  fake::method(map::Rigidbody_set_velocity, reinterpret_cast<void*>(&set_velocity));
  fake::method(map::Rigidbody_get_velocity, reinterpret_cast<void*>(&get_velocity));
  fake::method(map::Collider_set_enabled, reinterpret_cast<void*>(&set_enabled));
  fake::method(map::CapsuleCollider_set_radius, reinterpret_cast<void*>(&set_size_float));
  fake::method(map::CapsuleCollider_set_height, reinterpret_cast<void*>(&set_size_float));
  fake::method(map::CapsuleCollider_set_center, reinterpret_cast<void*>(&set_vector));
  fake::method(map::CapsuleCollider_set_direction, reinterpret_cast<void*>(&set_int));
  fake::method(map::BoxCollider_set_size, reinterpret_cast<void*>(&set_size_vector));
  fake::method(map::BoxCollider_set_center, reinterpret_cast<void*>(&set_vector));
  fake::method(map::SphereCollider_set_radius, reinterpret_cast<void*>(&set_size_float));
  fake::method(map::SphereCollider_set_center, reinterpret_cast<void*>(&set_vector));
  fake::method(map::Joint_set_connectedBody, reinterpret_cast<void*>(&set_connected));
  fake::method(map::Joint_set_axis, reinterpret_cast<void*>(&set_vector));
  fake::method(map::CharacterJoint_set_swingAxis, reinterpret_cast<void*>(&set_vector));
  fake::method(map::CharacterJoint_set_lowTwistLimit, reinterpret_cast<void*>(&set_limit));
  fake::method(map::CharacterJoint_set_highTwistLimit, reinterpret_cast<void*>(&set_limit));
  fake::method(map::CharacterJoint_set_swing1Limit, reinterpret_cast<void*>(&set_limit));
  fake::method(map::CharacterJoint_set_swing2Limit, reinterpret_cast<void*>(&set_limit));
  fake::icall(map::UnityObject_DestroyImmediate, reinterpret_cast<void*>(&destroy_immediate));
  std::vector<std::string> names(16, "unused");
  names[0] = "Default";
  names[kDeadPlayerLayer] = "DeadPlayer";
  fake::layer_names(names);
  if (withLayer) fake::layer(map::Layer_DeadPlayer);
  std::string missing;
  pm::api::bind(fake::host(), missing);
}

std::vector<Comp*> of_type(void* type) {
  std::vector<Comp*> found;
  for (Comp& k : g_comps)
    if (k.type == type) found.push_back(&k);
  return found;
}

Comp* on(void* type, int part) {
  for (Comp& k : g_comps)
    if (k.type == type && k.go == &g_gos[part]) return &k;
  return nullptr;
}

float length(phasmo::Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// The Wizard's layout: one root (the hips), every parent before its child, limits in order, 20 kg in all.
void the_ragdoll_table_is_whole() {
  int roots = 0;
  float mass = 0.0f;
  std::set<int32_t> bones;
  for (size_t i = 0; i < ragdoll::kRagdollBoneCount; ++i) {
    const ragdoll::RagdollBone& b = kRagdollBones[i];
    if (b.parent < 0) {
      ++roots;
      CHECK(b.bone == ragdoll::Hips);
    } else {
      CHECK(b.parent < static_cast<int32_t>(i));
    }
    CHECK(b.twistLow <= b.twistHigh);
    CHECK(b.swing1 >= 0.0f && b.swing2 >= 0.0f);
    CHECK(b.mass > 0.0f && b.width > 0.0f);
    CHECK(b.bone >= 0 && b.bone < ragdoll::kPartCount);
    CHECK(b.shape == ragdoll::RagdollShape::Sphere || (b.end >= 0 && b.end < ragdoll::kPartCount));
    CHECK(b.shape != ragdoll::RagdollShape::Box || (b.left >= 0 && b.right >= 0));
    CHECK(bones.insert(b.bone).second);  // each bone once
    mass += b.mass;
  }
  CHECK(roots == 1);
  CHECK(std::fabs(mass - ragdoll::kRagdollMass) < 0.01f);
  CHECK(ragdoll::kRagdollBoneCount == 11);
  // A fall never launches the body faster than the cap, and keeps its direction.
  const phasmo::Vector3 fast = ragdoll::capped_launch({30, 0, 40});
  CHECK(std::fabs(length(fast) - ragdoll::kFallLaunchCap) < 1e-4f && std::fabs(fast.x / fast.z - 0.75f) < 1e-4f);
  const phasmo::Vector3 slow = ragdoll::capped_launch({1, 2, 0});
  CHECK(slow.x == 1 && slow.y == 2 && slow.z == 0);
}

// The ragdoll is built kinematic with its colliders off, every part on the DeadPlayer layer, every joint held to its
// parent's body; a fall turns it physical at the figure's speed, capped; destroying it leaves nothing behind.
void the_ragdoll_is_built_kinematic_on_the_dead_player_layer() {
  start();
  void* const body = phasmo::Rigidbody::type.type_object();
  void* const joint = phasmo::CharacterJoint::type.type_object();
  CHECK(ragdoll::build(&g_animator));
  CHECK(ragdoll::built());
  CHECK(of_type(body).size() == ragdoll::kRagdollBoneCount);
  CHECK(of_type(joint).size() == ragdoll::kRagdollBoneCount - 1);
  CHECK(of_type(phasmo::BoxCollider::type.type_object()).size() == 2);
  CHECK(of_type(phasmo::CapsuleCollider::type.type_object()).size() == 8);
  CHECK(of_type(phasmo::SphereCollider::type.type_object()).size() == 1);
  for (const ragdoll::RagdollBone& b : kRagdollBones) {
    CHECK(g_layers[b.bone] == kDeadPlayerLayer);
    Comp* rb = on(body, b.bone);
    CHECK(rb && rb->kinematic && std::fabs(rb->mass - b.mass) < 1e-6f);
    Comp* j = on(joint, b.bone);
    if (b.parent < 0) {
      CHECK(!j);
    } else {
      CHECK(j && j->connected == &on(body, kRagdollBones[b.parent].bone)->obj);
    }
  }
  for (Comp& k : g_comps) {
    if (k.type == body || k.type == joint) continue;
    CHECK(!k.enabled);       // a collider stays off while the figure is animated
    CHECK(k.size > 0.01f);  // sized from its bones
  }
  for (size_t i = 0; i < ragdoll::kPartCount; ++i) {
    bool inTable = false;
    for (const ragdoll::RagdollBone& b : kRagdollBones) inTable = inTable || b.bone == static_cast<int32_t>(i);
    if (!inTable) CHECK(g_layers[i] == -1);  // only the ragdoll's own bones change layer
  }

  ragdoll::set_physical(true, {1, 0, 2});
  for (Comp* rb : of_type(body)) CHECK(!rb->kinematic && rb->velocity.x == 1 && rb->velocity.z == 2);
  for (Comp& k : g_comps)
    if (k.type != body && k.type != joint) CHECK(k.enabled);
  ragdoll::set_physical(true, {0, -40, 0});
  CHECK(std::fabs(ragdoll::fastest() - ragdoll::kFallLaunchCap) < 1e-4f);
  CHECK(ragdoll::local_pose().size() == ragdoll::kRagdollBoneCount);
  CHECK(std::fabs(ragdoll::local_pose()[0].x - std::sin(0.01f * static_cast<float>(ragdoll::Hips))) < 1e-6f);
  ragdoll::set_physical(false, {});
  for (Comp* rb : of_type(body)) CHECK(rb->kinematic);
  for (Comp& k : g_comps)
    if (k.type != body && k.type != joint) CHECK(!k.enabled);

  ragdoll::destroy();
  CHECK(!ragdoll::built());
  for (Comp& k : g_comps) CHECK(g_destroyed.contains(&k.obj));
  CHECK(ragdoll::fastest() == 0.0f && ragdoll::local_pose().empty());
  ragdoll::destroy();  // twice is safe
}

// The figure destroyed mid-fall (a scene load, the contract's end): its parts go with its bones, the ragdoll then
// reads as not built, and nothing it does touches what has gone (Review Focus 4).
void a_ragdoll_whose_figure_has_gone_is_not_built() {
  start();
  CHECK(ragdoll::build(&g_animator));
  ragdoll::set_physical(true, {0, 0, 3});
  for (Comp& k : g_comps) fake::destroy(&k.obj);  // the scene took the bones, and the parts on them
  CHECK(!ragdoll::built());
  CHECK(ragdoll::fastest() == 0.0f);
  ragdoll::set_physical(false, {});
  ragdoll::destroy();
  CHECK(g_destroyed.empty());  // nothing destroyed twice
}

// With no DeadPlayer layer in this build the ragdoll could collide with what is networked: none is built.
void no_ragdoll_without_its_layer() {
  start(false);
  CHECK(!ragdoll::build(&g_animator));
  CHECK(g_comps.empty());
  CHECK(!ragdoll::built());
}

}  // namespace

int main() {
  the_ragdoll_table_is_whole();
  the_ragdoll_is_built_kinematic_on_the_dead_player_layer();
  a_ragdoll_whose_figure_has_gone_is_not_built();
  no_ragdoll_without_its_layer();
  return g_failures ? 1 : 0;
}
