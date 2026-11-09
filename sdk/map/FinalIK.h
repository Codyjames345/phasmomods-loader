#pragma once

// RootMotion's Final IK (Assembly-CSharp-firstpass, namespace RootMotion.FinalIK), the inverse kinematics library the
// game ships: components that bend a model's bones after its Animator has posed them, so a hand reaches a point or a
// head turns to look at one. Each component holds a solver; set the solver's bones once, then each frame its target
// position (IKSolver.IKPosition) and how much it counts (IKSolver.IKPositionWeight, 0 to 1). The components run in
// LateUpdate, after the Animator. sdk/guides/custom-animation.md shows them on a mod's own model.

#include "Animator.h"
#include "Transform.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* FinalIK_SetChain_limb[] = {"Transform", "Transform", "Transform", "Transform"};
inline constexpr const char* FinalIK_SetChain_lookAt[] = {"Transform[]", "Transform", "Transform[]", "Transform"};
inline constexpr const char* AvatarIKGoal_values[] = {"LeftFoot", "RightFoot", "LeftHand", "RightHand"};
inline constexpr const char* FinalIK_SetReferences[] = {"BipedReferences", "Transform"};
inline constexpr const char* FinalIK_Transform[] = {"Transform"};

/// The bones of a two-legged body (RootMotion.BipedReferences), a class with a field per bone: root, pelvis, each leg's
/// thigh, calf and foot, each arm's upper arm, forearm and hand, the head, and the spine and eyes as Transform[]. Make
/// one, fill it, and give it to FullBodyBipedIK.SetReferences. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences{.key = "BipedReferences", .name = "BipedReferences",
                                                    .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion"};
/// Its constructor: every bone unset. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_ctor{.key = "BipedReferences.ctor", .kind = pm::sdk::Kind::Method,
                                                         .owner = "BipedReferences", .name = ".ctor"};
/// The model's root Transform. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_root{.key = "BipedReferences.root", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "root"};
/// The hips. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_pelvis{.key = "BipedReferences.pelvis", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "pelvis"};
/// The left thigh. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftThigh{.key = "BipedReferences.leftThigh", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftThigh"};
/// The left shin. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftCalf{.key = "BipedReferences.leftCalf", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftCalf"};
/// The left foot. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftFoot{.key = "BipedReferences.leftFoot", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftFoot"};
/// The right thigh. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightThigh{.key = "BipedReferences.rightThigh", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightThigh"};
/// The right shin. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightCalf{.key = "BipedReferences.rightCalf", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightCalf"};
/// The right foot. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightFoot{.key = "BipedReferences.rightFoot", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightFoot"};
/// The left upper arm. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftUpperArm{.key = "BipedReferences.leftUpperArm", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftUpperArm"};
/// The left forearm. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftForearm{.key = "BipedReferences.leftForearm", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftForearm"};
/// The left hand. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_leftHand{.key = "BipedReferences.leftHand", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "leftHand"};
/// The right upper arm. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightUpperArm{.key = "BipedReferences.rightUpperArm", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightUpperArm"};
/// The right forearm. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightForearm{.key = "BipedReferences.rightForearm", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightForearm"};
/// The right hand. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_rightHand{.key = "BipedReferences.rightHand", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "rightHand"};
/// The head. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_head{.key = "BipedReferences.head", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "head"};
/// The spine bones from the hips up, a Transform[]. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_spine{.key = "BipedReferences.spine", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "spine"};
/// The eyes, a Transform[] (may be empty). Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc BipedReferences_eyes{.key = "BipedReferences.eyes", .kind = pm::sdk::Kind::Field,
                                                   .owner = "BipedReferences", .name = "eyes"};

/// Final IK's full-body solver for a two-legged body (RootMotion.FinalIK.FullBodyBipedIK): hands, feet, shoulders,
/// thighs and the body each reach for a point, and the rest of the body follows as a body does. Add it to the model's
/// GameObject. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc FullBodyBipedIK{.key = "FullBodyBipedIK", .name = "FullBodyBipedIK",
                                                    .assembly = "Assembly-CSharp-firstpass",
                                                    .ns = "RootMotion.FinalIK"};
/// Its solver, an IKSolverFullBodyBiped. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc FullBodyBipedIK_solver{.key = "FullBodyBipedIK.solver",
                                                           .kind = pm::sdk::Kind::Field, .owner = "FullBodyBipedIK",
                                                           .name = "solver"};
/// Sets the body's bones (a BipedReferences) and the spine bone the body turns about (null: Final IK finds it).
/// Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc FullBodyBipedIK_SetReferences{
    .key = "FullBodyBipedIK.SetReferences", .kind = pm::sdk::Kind::Method, .owner = "FullBodyBipedIK",
    .name = "SetReferences", .params = FinalIK_SetReferences};

/// The full-body solver (RootMotion.FinalIK.IKSolverFullBodyBiped). Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverFullBodyBiped{.key = "IKSolverFullBodyBiped", .name = "IKSolverFullBodyBiped",
                                                          .assembly = "Assembly-CSharp-firstpass",
                                                          .ns = "RootMotion.FinalIK"};
/// The left hand's effector, an IKEffector. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverFullBodyBiped_get_leftHandEffector{
    .key = "IKSolverFullBodyBiped.get_leftHandEffector", .kind = pm::sdk::Kind::Method,
    .owner = "IKSolverFullBodyBiped", .name = "get_leftHandEffector"};
/// The right hand's effector, an IKEffector. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverFullBodyBiped_get_rightHandEffector{
    .key = "IKSolverFullBodyBiped.get_rightHandEffector", .kind = pm::sdk::Kind::Method,
    .owner = "IKSolverFullBodyBiped", .name = "get_rightHandEffector"};

/// One point of the full-body solver a bone reaches for (RootMotion.FinalIK.IKEffector). Inferred: Final IK's own
/// documentation.
inline constexpr pm::sdk::EntryDesc IKEffector{.key = "IKEffector", .name = "IKEffector",
                                               .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// Where it reaches, world space. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKEffector_position{.key = "IKEffector.position", .kind = pm::sdk::Kind::Field,
                                                        .owner = "IKEffector", .name = "position"};
/// How much it reaches there, 0 to 1. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKEffector_positionWeight{.key = "IKEffector.positionWeight",
                                                              .kind = pm::sdk::Kind::Field, .owner = "IKEffector",
                                                              .name = "positionWeight"};

/// Starts a solver on the model whose root Transform is given; a solver its component has not started yet must be,
/// before IKSolver.Update. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolver_Initiate{.key = "IKSolver.Initiate", .kind = pm::sdk::Kind::Method,
                                                      .owner = "IKSolver", .name = "Initiate",
                                                      .params = FinalIK_Transform};

/// Every Final IK component's base (RootMotion.FinalIK.IK), the element type of IKExecutionOrder's list. Inferred:
/// Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc FinalIK_IK{.key = "IK", .name = "IK", .assembly = "Assembly-CSharp-firstpass",
                                               .ns = "RootMotion.FinalIK"};

/// Runs Final IK components one after another in a set order each frame, as Final IK asks when two of them bend the
/// same bones (a full-body solver, then a look-at): it switches them off and updates their solvers itself, after the
/// Animator. Add it to the model's GameObject. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKExecutionOrder{.key = "IKExecutionOrder", .name = "IKExecutionOrder",
                                                     .assembly = "Assembly-CSharp-firstpass",
                                                     .ns = "RootMotion.FinalIK"};
/// The components, an IK[], in the order they run. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKExecutionOrder_IKComponents{.key = "IKExecutionOrder.IKComponents",
                                                                  .kind = pm::sdk::Kind::Field,
                                                                  .owner = "IKExecutionOrder", .name = "IKComponents"};
/// The model's Animator. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKExecutionOrder_animator{.key = "IKExecutionOrder.animator",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "IKExecutionOrder", .name = "animator"};

/// What every Final IK grounder shares (RootMotion.FinalIK.Grounder): foot placement on the ground under each foot,
/// the hips lowered to the lower foot, as games keep feet on stairs and slopes. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc Grounder{.key = "Grounder", .name = "Grounder",
                                             .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// Its solver, a Grounding. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc Grounder_solver{.key = "Grounder.solver", .kind = pm::sdk::Kind::Field,
                                                    .owner = "Grounder", .name = "solver"};
/// The grounder's solver (RootMotion.FinalIK.Grounding). Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc Grounding{.key = "Grounding", .name = "Grounding",
                                              .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// The layers it takes for ground, a LayerMask (a bit per layer); none until set. Inferred: Final IK's own
/// documentation.
inline constexpr pm::sdk::EntryDesc Grounding_layers{.key = "Grounding.layers", .kind = pm::sdk::Kind::Field,
                                                     .owner = "Grounding", .name = "layers"};
/// The grounder for a FullBodyBipedIK body (RootMotion.FinalIK.GrounderFBBIK): add it beside one and set its ik. It
/// starts itself once the body's solver has. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc GrounderFBBIK{.key = "GrounderFBBIK", .name = "GrounderFBBIK",
                                                  .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// The FullBodyBipedIK it grounds. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc GrounderFBBIK_ik{.key = "GrounderFBBIK.ik", .kind = pm::sdk::Kind::Field,
                                                     .owner = "GrounderFBBIK", .name = "ik"};

/// What every Final IK solver shares (RootMotion.FinalIK.IKSolver). Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolver{.key = "IKSolver", .name = "IKSolver",
                                             .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// The point the solver reaches for, in world space. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolver_IKPosition{.key = "IKSolver.IKPosition", .kind = pm::sdk::Kind::Field,
                                                        .owner = "IKSolver", .name = "IKPosition"};
/// How much the solver counts, 0 (the animation alone) to 1. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolver_IKPositionWeight{.key = "IKSolver.IKPositionWeight",
                                                              .kind = pm::sdk::Kind::Field, .owner = "IKSolver",
                                                              .name = "IKPositionWeight"};

/// A three-bone solver (RootMotion.FinalIK.IKSolverTrigonometric): an upper limb, the limb below and its end. Inferred:
/// Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverTrigonometric{.key = "IKSolverTrigonometric", .name = "IKSolverTrigonometric",
                                                          .assembly = "Assembly-CSharp-firstpass",
                                                          .ns = "RootMotion.FinalIK"};
/// Sets the solver's bones (upper, lower, end, and the model's root) and starts it; true when the chain is valid.
/// Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverTrigonometric_SetChain{
    .key = "IKSolverTrigonometric.SetChain", .kind = pm::sdk::Kind::Method, .owner = "IKSolverTrigonometric",
    .name = "SetChain", .params = FinalIK_SetChain_limb};
/// How much the end bone turns to IKRotation, 0 to 1; Final IK starts it at 1. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverTrigonometric_IKRotationWeight{
    .key = "IKSolverTrigonometric.IKRotationWeight", .kind = pm::sdk::Kind::Field, .owner = "IKSolverTrigonometric",
    .name = "IKRotationWeight"};

/// A humanoid limb's solver (RootMotion.FinalIK.IKSolverLimb), an IKSolverTrigonometric that bends the limb as a human
/// limb bends. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLimb{.key = "IKSolverLimb", .name = "IKSolverLimb",
                                                 .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// Which limb the solver bends, an AvatarIKGoal value, for the way the elbow or knee turns. Inferred: Final IK's own
/// documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLimb_goal{.key = "IKSolverLimb.goal", .kind = pm::sdk::Kind::Field,
                                                      .owner = "IKSolverLimb", .name = "goal"};

/// Final IK's limb component (RootMotion.FinalIK.LimbIK): an arm or a leg reaching for a point. Add it to the model's
/// GameObject. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc LimbIK{.key = "LimbIK", .name = "LimbIK", .assembly = "Assembly-CSharp-firstpass",
                                           .ns = "RootMotion.FinalIK"};
/// Its solver, an IKSolverLimb the component makes. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc LimbIK_solver{.key = "LimbIK.solver", .kind = pm::sdk::Kind::Field,
                                                  .owner = "LimbIK", .name = "solver"};

/// A look-at solver (RootMotion.FinalIK.IKSolverLookAt): spine bones, a head and eyes turned toward a point. Inferred:
/// Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLookAt{.key = "IKSolverLookAt", .name = "IKSolverLookAt",
                                                   .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// Sets the solver's bones (spine bones, the head, eyes, and the model's root) and starts it; true when the chain is
/// valid. An empty array is none. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLookAt_SetChain{.key = "IKSolverLookAt.SetChain",
                                                            .kind = pm::sdk::Kind::Method, .owner = "IKSolverLookAt",
                                                            .name = "SetChain", .params = FinalIK_SetChain_lookAt};
/// How much the spine turns, 0 to 1. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLookAt_bodyWeight{.key = "IKSolverLookAt.bodyWeight",
                                                              .kind = pm::sdk::Kind::Field, .owner = "IKSolverLookAt",
                                                              .name = "bodyWeight"};
/// How much the head turns, 0 to 1. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLookAt_headWeight{.key = "IKSolverLookAt.headWeight",
                                                              .kind = pm::sdk::Kind::Field, .owner = "IKSolverLookAt",
                                                              .name = "headWeight"};
/// How far it may turn before it eases off, 0 (freely) to 1 (not at all past straight ahead). Inferred: Final IK's own
/// documentation.
inline constexpr pm::sdk::EntryDesc IKSolverLookAt_clampWeight{.key = "IKSolverLookAt.clampWeight",
                                                               .kind = pm::sdk::Kind::Field, .owner = "IKSolverLookAt",
                                                               .name = "clampWeight"};

/// Final IK's look-at component (RootMotion.FinalIK.LookAtIK). Add it to the model's GameObject. Inferred: Final IK's
/// own documentation.
inline constexpr pm::sdk::EntryDesc LookAtIK{.key = "LookAtIK", .name = "LookAtIK",
                                             .assembly = "Assembly-CSharp-firstpass", .ns = "RootMotion.FinalIK"};
/// Its solver, an IKSolverLookAt the component makes. Inferred: Final IK's own documentation.
inline constexpr pm::sdk::EntryDesc LookAtIK_solver{.key = "LookAtIK.solver", .kind = pm::sdk::Kind::Field,
                                                    .owner = "LookAtIK", .name = "solver"};

/// Unity's IK goals (UnityEngine.AnimationModule): which hand or foot. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AvatarIKGoal{.key = "AvatarIKGoal", .name = "AvatarIKGoal",
                                                 .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine",
                                                 .values = AvatarIKGoal_values};
/// The left hand. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AvatarIKGoal_LeftHand{.key = "AvatarIKGoal.LeftHand", .kind = pm::sdk::Kind::EnumValue,
                                                          .owner = "AvatarIKGoal", .name = "LeftHand"};
/// The right hand. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AvatarIKGoal_RightHand{.key = "AvatarIKGoal.RightHand",
                                                           .kind = pm::sdk::Kind::EnumValue, .owner = "AvatarIKGoal",
                                                           .name = "RightHand"};

}  // namespace pm::map

// The game API for Final IK (sdk/api/game_api.h). Main thread. An entry that did not resolve logs once and returns its
// type's default (null, 0, false) or does nothing.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Any Final IK solver. `solver` below is one.
struct IKSolver {
  /// Initiate(solver, root): starts it on the model whose root Transform this is. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::IKSolver_Initiate, void(void*, void*)> Initiate{};
  /// The point it reaches for, world space: IKPosition.get(solver), IKPosition.set(solver, at). Unresolved: zero.
  static constexpr pm::api::Field<pm::map::IKSolver_IKPosition, Vector3> IKPosition{};
  /// How much it counts, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolver_IKPositionWeight, float> IKPositionWeight{};
};

/// A three-bone solver. `solver` below is one.
struct IKSolverTrigonometric {
  /// SetChain(solver, upper, lower, end, root): true when the chain is valid. Unresolved: false.
  static constexpr pm::api::Method<pm::map::IKSolverTrigonometric_SetChain, bool(void*, void*, void*, void*, void*)>
      SetChain{};
  /// How much the end bone turns to the solver's rotation, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolverTrigonometric_IKRotationWeight, float> IKRotationWeight{};
};

/// A humanoid limb's solver. `solver` below is one.
struct IKSolverLimb {
  /// The limb it bends, an AvatarIKGoal value. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolverLimb_goal, int32_t> goal{};
};

/// Final IK's limb component. `limb` below is one.
struct LimbIK {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::LimbIK> type{};
  /// Its solver: solver.get(limb). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LimbIK_solver, void*> solver{};
};

/// A look-at solver. `solver` below is one.
struct IKSolverLookAt {
  /// SetChain(solver, spine, head, eyes, root): spine and eyes are Transform[]. True when valid. Unresolved: false.
  static constexpr pm::api::Method<pm::map::IKSolverLookAt_SetChain, bool(void*, void*, void*, void*, void*)> SetChain{};
  /// How much the spine turns, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolverLookAt_bodyWeight, float> bodyWeight{};
  /// How much the head turns, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolverLookAt_headWeight, float> headWeight{};
  /// How far it may turn before it eases off, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKSolverLookAt_clampWeight, float> clampWeight{};
};

/// Final IK's look-at component. `lookAt` below is one.
struct LookAtIK {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::LookAtIK> type{};
  /// Its solver: solver.get(lookAt). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LookAtIK_solver, void*> solver{};
};

/// The bones of a two-legged body. `refs` below is one: type.make(), then ctor(refs), then each field.
struct BipedReferences {
  /// The type: make() for a new one. Unresolved: null.
  static constexpr pm::api::Type<pm::map::BipedReferences> type{};
  /// ctor(refs). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::BipedReferences_ctor, void(void*)> ctor{};
  /// A Transform each; spine and eyes a Transform[]. Unresolved: null.
  static constexpr pm::api::Field<pm::map::BipedReferences_root, void*> root{};
  static constexpr pm::api::Field<pm::map::BipedReferences_pelvis, void*> pelvis{};  ///< the pelvis, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftThigh, void*> leftThigh{};  ///< the left thigh, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftCalf, void*> leftCalf{};  ///< the left calf, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftFoot, void*> leftFoot{};  ///< the left foot, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightThigh, void*> rightThigh{};  ///< the right thigh, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightCalf, void*> rightCalf{};  ///< the right calf, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightFoot, void*> rightFoot{};  ///< the right foot, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftUpperArm, void*> leftUpperArm{};  ///< the left upper arm, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftForearm, void*> leftForearm{};  ///< the left forearm, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_leftHand, void*> leftHand{};  ///< the left hand, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightUpperArm, void*> rightUpperArm{};  ///< the right upper arm, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightForearm, void*> rightForearm{};  ///< the right forearm, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_rightHand, void*> rightHand{};  ///< the right hand, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_head, void*> head{};  ///< the head, a Transform
  static constexpr pm::api::Field<pm::map::BipedReferences_spine, void*> spine{};  ///< the spine, a Transform[]
  static constexpr pm::api::Field<pm::map::BipedReferences_eyes, void*> eyes{};  ///< the eyes, a Transform[]
};

/// Final IK's full-body solver for a two-legged body. `ik` below is one.
struct FullBodyBipedIK {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::FullBodyBipedIK> type{};
  /// Its solver, an IKSolverFullBodyBiped. Unresolved: null.
  static constexpr pm::api::Field<pm::map::FullBodyBipedIK_solver, void*> solver{};
  /// SetReferences(ik, refs, rootNode): rootNode null lets Final IK find it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::FullBodyBipedIK_SetReferences, void(void*, void*, void*)> SetReferences{};
};

/// The full-body solver. `solver` below is one.
struct IKSolverFullBodyBiped {
  /// get_leftHandEffector(solver), get_rightHandEffector(solver): an IKEffector. Unresolved: null.
  static constexpr pm::api::Method<pm::map::IKSolverFullBodyBiped_get_leftHandEffector, void*(void*)>
      get_leftHandEffector{};
  static constexpr pm::api::Method<pm::map::IKSolverFullBodyBiped_get_rightHandEffector, void*(void*)>
      get_rightHandEffector{};  ///< the right hand's IKEffector
};

/// A full-body solver's effector. `effector` below is one.
struct IKEffector {
  /// Where it reaches, world space. Unresolved: zero.
  static constexpr pm::api::Field<pm::map::IKEffector_position, Vector3> position{};
  /// How much it reaches there, 0 to 1. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::IKEffector_positionWeight, float> positionWeight{};
};

/// Final IK's base component, for an IK[] (IK::type.make_array).
struct IK {
  /// The type. Unresolved: null.
  static constexpr pm::api::Type<pm::map::FinalIK_IK> type{};
};

/// Final IK's execution order. `order` below is one.
struct IKExecutionOrder {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::IKExecutionOrder> type{};
  /// The components it runs, an IK[]. Unresolved: null.
  static constexpr pm::api::Field<pm::map::IKExecutionOrder_IKComponents, void*> IKComponents{};
  /// The model's Animator. Unresolved: null.
  static constexpr pm::api::Field<pm::map::IKExecutionOrder_animator, void*> animator{};
};

/// A grounder's solver. `grounding` below is one.
struct Grounding {
  /// The ground's layers, a bit per layer. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Grounding_layers, int32_t> layers{};
};

/// Any Final IK grounder. `grounder` below is one.
struct Grounder {
  /// Its solver, a Grounding. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Grounder_solver, void*> solver{};
};

/// The grounder for a FullBodyBipedIK body. `grounder` below is one.
struct GrounderFBBIK {
  /// The type: type_object() for GameObject.AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::GrounderFBBIK> type{};
  /// The FullBodyBipedIK it grounds. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GrounderFBBIK_ik, void*> ik{};
};

/// Unity's IK goals.
namespace AvatarIKGoal {
/// LeftHand(), RightHand(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AvatarIKGoal_LeftHand> LeftHand{};
inline constexpr pm::api::EnumValue<pm::map::AvatarIKGoal_RightHand> RightHand{};  ///< the right hand's goal
}  // namespace AvatarIKGoal

/// @}
}  // namespace phasmo
