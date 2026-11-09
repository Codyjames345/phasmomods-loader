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

/// Unity's IK goals.
namespace AvatarIKGoal {
/// LeftHand(), RightHand(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AvatarIKGoal_LeftHand> LeftHand{};
inline constexpr pm::api::EnumValue<pm::map::AvatarIKGoal_RightHand> RightHand{};
}  // namespace AvatarIKGoal

/// @}
}  // namespace phasmo
