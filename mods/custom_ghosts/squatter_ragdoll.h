#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "Transform.h"

// The Squatter's ragdoll: a Rigidbody, a collider and a CharacterJoint on the major bones of the figure under its
// sheet, laid out as Unity's Ragdoll Wizard (RagdollBuilder) lays them out. Built with the figure and kinematic, with
// its colliders off, while the figure is animated; physical only while it falls. Its parts sit on Layer.DeadPlayer,
// which meets only the house's fixed world (the layer's note in the map), so a falling body never moves a networked prop,
// an item, a player or the ghost. Each client builds and simulates its own.
namespace pm::custom::squatter::ragdoll {

/** The bones the ragdoll reads: an index into the HumanBodyBones values the game gives (read at build time). */
enum Part : int32_t {
  Hips,
  Spine,
  Chest,
  Head,
  LeftUpperLeg,
  LeftLowerLeg,
  LeftFoot,
  RightUpperLeg,
  RightLowerLeg,
  RightFoot,
  LeftUpperArm,
  LeftLowerArm,
  LeftHand,
  RightUpperArm,
  RightLowerArm,
  RightHand,
  kPartCount
};

/** A bone's collider. */
enum class RagdollShape { Box, Capsule, Sphere };

/** The body's own axes as it stands when the ragdoll is built: the Wizard's worldRight, worldUp and worldForward. */
enum class BodyAxis { Right, Up, Forward };

/** One bone of the ragdoll. */
struct RagdollBone {
  int32_t bone;        ///< the Part it is on
  int32_t parent;      ///< the index in kRagdollBones of the bone its joint holds to; -1 for the root
  RagdollShape shape;  ///< its collider
  int32_t end;         ///< the Part its shape reaches to: a capsule's far end, a box's top; -1 for the sphere
  int32_t left, right;  ///< a box's sides (Parts), whose span sets its depth; -1 for the others
  float width;          ///< capsule: radius over its length; box: depth over its sides' span; sphere: radius over the
                        ///< upper arms' span
  float mass;           ///< kilograms
  float twistLow, twistHigh, swing1, swing2;  ///< the joint's limits, degrees
  BodyAxis twist, swing;                      ///< the joint's twist and swing axes
};

/** The Wizard's whole body mass, kilograms. */
inline constexpr float kRagdollMass = 20.0f;

/**
 * The ragdoll's bones, each parent before its children, as Unity's Ragdoll Wizard builds them: a box for the hips and
 * one for the chest (the Wizard's middle spine), a sphere for the head, and a capsule for each upper and lower arm and
 * leg, with the Wizard's joint limits and axes and its masses (densities 2.5, 1.5, 1 scaled to 20 kg; swing2 left at
 * 0 as the Wizard leaves it). Starting values, judged with the user in game.
 */
inline constexpr RagdollBone kRagdollBones[] = {
    {Hips, -1, RagdollShape::Box, Spine, LeftUpperLeg, RightUpperLeg, 0.5f, 3.125f, 0, 0, 0, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {LeftUpperLeg, 0, RagdollShape::Capsule, LeftLowerLeg, -1, -1, 0.3f, 1.875f, -20, 70, 30, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {LeftLowerLeg, 1, RagdollShape::Capsule, LeftFoot, -1, -1, 0.25f, 1.875f, -80, 0, 0, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {RightUpperLeg, 0, RagdollShape::Capsule, RightLowerLeg, -1, -1, 0.3f, 1.875f, -20, 70, 30, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {RightLowerLeg, 3, RagdollShape::Capsule, RightFoot, -1, -1, 0.25f, 1.875f, -80, 0, 0, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {Chest, 0, RagdollShape::Box, Head, LeftUpperArm, RightUpperArm, 0.5f, 3.125f, -20, 20, 10, 0, BodyAxis::Right,
     BodyAxis::Forward},
    {LeftUpperArm, 5, RagdollShape::Capsule, LeftLowerArm, -1, -1, 0.25f, 1.25f, -70, 10, 50, 0, BodyAxis::Up,
     BodyAxis::Forward},
    {LeftLowerArm, 6, RagdollShape::Capsule, LeftHand, -1, -1, 0.2f, 1.25f, -90, 0, 0, 0, BodyAxis::Forward,
     BodyAxis::Up},
    {RightUpperArm, 5, RagdollShape::Capsule, RightLowerArm, -1, -1, 0.25f, 1.25f, -70, 10, 50, 0, BodyAxis::Up,
     BodyAxis::Forward},
    {RightLowerArm, 8, RagdollShape::Capsule, RightHand, -1, -1, 0.2f, 1.25f, -90, 0, 0, 0, BodyAxis::Forward,
     BodyAxis::Up},
    {Head, 5, RagdollShape::Sphere, -1, LeftUpperArm, RightUpperArm, 0.25f, 1.25f, -40, 25, 25, 0, BodyAxis::Right,
     BodyAxis::Forward},
};

/** The number of the ragdoll's bones. */
inline constexpr size_t kRagdollBoneCount = std::size(kRagdollBones);

/** The fastest a fall may launch the body, metres a second: a guard against a fall that starts inside furniture or
 * against a wall throwing it across the room (a starting value, judged with the user in game). */
inline constexpr float kFallLaunchCap = 6.0f;

/** `velocity` with its speed capped at kFallLaunchCap, its direction kept. */
inline phasmo::Vector3 capped_launch(phasmo::Vector3 velocity) {
  const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
  if (speed <= kFallLaunchCap) return velocity;
  const float scale = kFallLaunchCap / speed;
  return {velocity.x * scale, velocity.y * scale, velocity.z * scale};
}

/**
 * Builds the ragdoll on the bones of `animator`'s humanoid, the figure standing: every part kinematic with its collider
 * off, on Layer.DeadPlayer. Replaces one already built. False, with nothing kept, when a bone, the layer or a part's
 * type is missing (logged). Main thread.
 */
bool build(void* animator);

/** Whether the ragdoll is built and its parts are still in the scene. */
bool built();

/**
 * Turns every part physical (`on`: its collider on, no longer kinematic, moving at `velocity` capped at
 * kFallLaunchCap) or back to kinematic with its collider off. Does nothing when it is not built.
 */
void set_physical(bool on, phasmo::Vector3 velocity);

/** The fastest of its bodies, metres a second; 0 when it is not built. */
float fastest();

/** Each bone's rotation against its parent now, in kRagdollBones' order; empty when it is not built. */
std::vector<phasmo::Quaternion> local_pose();

/** Removes every part it added; the figure's bones stay. Safe when it is not built or the scene has gone. */
void destroy();

}  // namespace pm::custom::squatter::ragdoll
