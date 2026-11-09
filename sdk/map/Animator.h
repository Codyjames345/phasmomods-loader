#pragma once

// UnityEngine.Animator: Unity's animation state machine component. Its parameter getters and setters
// are engine functions the runtime registers by icall name, so those entries resolve only in the
// running game. The type, its parameter list and StringToHash are by real name; the host uses them to
// check the AnimatorParam entries (Scene.h). A mod that sets one named parameter reads its hash from
// its AnimatorParam entry, so the name stays out of mod code. A mod that copies whatever an Animator
// has, parameter for parameter, without naming any (Custom Ghosts' apparition mirrors a ghost model's
// Animator onto a copy), lists them with get_parameters and reads each one's name and type.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* Animator_string[] = {"string"};
inline constexpr const char* Animator_float[] = {"float"};
inline constexpr const char* AnimatorControllerParameterType_values[] = {"Float", "Int", "Bool", "Trigger"};

/// Unity's animator component (UnityEngine.AnimationModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator{.key = "Animator", .name = "Animator",
                                             .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};

/// The Animator's parameters, a new AnimatorControllerParameter[] on every call (empty without a
/// controller). Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Animator_get_parameters{
    .key = "Animator.get_parameters", .kind = pm::sdk::Kind::Method, .owner = "Animator", .name = "get_parameters"};

/// The hash Animator's by-id setters take for a parameter's name. Static. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Animator_StringToHash{
    .key = "Animator.StringToHash", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "Animator",
    .name = "StringToHash", .type = "int", .params = Animator_string};

/// How fast the Animator plays, 1 as authored. The game plays a ghost model's walk at its agent's pace
/// through it (the hunt sets it from NavMeshAgent.speed every frame). Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc Animator_get_speed{.key = "Animator.get_speed", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Animator", .name = "get_speed"};

/// Sets how fast the Animator plays. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Animator_set_speed{.key = "Animator.set_speed", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Animator", .name = "set_speed",
                                                       .params = Animator_float};

/// One parameter of an Animator's controller, as get_parameters lists it (UnityEngine.AnimationModule).
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameter{
    .key = "AnimatorControllerParameter", .name = "AnimatorControllerParameter",
    .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};

/// The parameter's name, the field behind its `name` property. Inferred: the field's name is in the
/// dump of build 25811160.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameter_m_Name{
    .key = "AnimatorControllerParameter.m_Name", .kind = pm::sdk::Kind::Field,
    .owner = "AnimatorControllerParameter", .name = "m_Name", .type = "string"};

/// What a parameter holds (UnityEngine.AnimationModule): Float, Int, Bool or Trigger. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameterType{
    .key = "AnimatorControllerParameterType", .name = "AnimatorControllerParameterType",
    .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine", .values = AnimatorControllerParameterType_values};

/// A float parameter. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameterType_Float{
    .key = "AnimatorControllerParameterType.Float", .kind = pm::sdk::Kind::EnumValue,
    .owner = "AnimatorControllerParameterType", .name = "Float"};

/// An integer parameter. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameterType_Int{
    .key = "AnimatorControllerParameterType.Int", .kind = pm::sdk::Kind::EnumValue,
    .owner = "AnimatorControllerParameterType", .name = "Int"};

/// A bool parameter. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameterType_Bool{
    .key = "AnimatorControllerParameterType.Bool", .kind = pm::sdk::Kind::EnumValue,
    .owner = "AnimatorControllerParameterType", .name = "Bool"};

/// The parameter's type, the field behind its `type` property. Inferred: the field's name is in the
/// dump of build 25811160.
inline constexpr pm::sdk::EntryDesc AnimatorControllerParameter_m_Type{
    .key = "AnimatorControllerParameter.m_Type", .kind = pm::sdk::Kind::Field,
    .owner = "AnimatorControllerParameter", .name = "m_Type", .type = "@AnimatorControllerParameterType"};

// The runtime has no metadata for an icall, so its params are never matched: they fix the arity of
// its wrapper (sdk/api/game_api.h).
inline constexpr const char* Animator_int[] = {"int"};
inline constexpr const char* Animator_SetBoolID_params[] = {"int", "bool"};
inline constexpr const char* Animator_SetFloatID_params[] = {"int", "float"};
inline constexpr const char* Animator_SetIntegerID_params[] = {"int", "int"};

/// Sets an Animator's bool parameter by its hashed id (Animator.StringToHash): the engine function
/// behind UnityEngine.Animator.SetBool(int, bool), called as SetBoolID(animator, id, value). Custom
/// Ghosts' apparition calls it by this name. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_SetBoolID{.key = "Animator.SetBoolID", .kind = pm::sdk::Kind::Icall,
                                                       .name = "UnityEngine.Animator::SetBoolID",
                                                       .params = Animator_SetBoolID_params};

/// An Animator's bool parameter by its hashed id: the engine function behind GetBool(int). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_GetBoolID{.key = "Animator.GetBoolID", .kind = pm::sdk::Kind::Icall,
                                                       .name = "UnityEngine.Animator::GetBoolID",
                                                       .params = Animator_int};

/// Sets an Animator's float parameter by its hashed id: the engine function behind SetFloat(int,
/// float). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_SetFloatID{.key = "Animator.SetFloatID", .kind = pm::sdk::Kind::Icall,
                                                        .name = "UnityEngine.Animator::SetFloatID",
                                                        .params = Animator_SetFloatID_params};

/// An Animator's float parameter by its hashed id: the engine function behind GetFloat(int). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_GetFloatID{.key = "Animator.GetFloatID", .kind = pm::sdk::Kind::Icall,
                                                        .name = "UnityEngine.Animator::GetFloatID",
                                                        .params = Animator_int};

/// Sets an Animator's integer parameter by its hashed id: the engine function behind SetInteger(int,
/// int). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_SetIntegerID{.key = "Animator.SetIntegerID",
                                                          .kind = pm::sdk::Kind::Icall,
                                                          .name = "UnityEngine.Animator::SetIntegerID",
                                                          .params = Animator_SetIntegerID_params};

/// An Animator's integer parameter by its hashed id: the engine function behind GetInteger(int).
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_GetIntegerID{.key = "Animator.GetIntegerID",
                                                          .kind = pm::sdk::Kind::Icall,
                                                          .name = "UnityEngine.Animator::GetIntegerID",
                                                          .params = Animator_int};

inline constexpr const char* Animator_SetBool_params[] = {"string", "bool"};

/// Sets an Animator's bool parameter by its name: the overload with a string (the other takes the
/// hashed id). An Animator without the parameter changes nothing (Unity warns in its log). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_SetBool{.key = "Animator.SetBool", .kind = pm::sdk::Kind::Method,
                                                     .owner = "Animator", .name = "SetBool",
                                                     .params = Animator_SetBool_params};

// Playing a mod's own animation on a model (sdk/api/models.h, sdk/guides/custom-animation.md): a controller loaded from
// an AssetBundle (UnityAssets.h) replaces the model's, and the mod plays its states by name.

/// Unity's animator controller as the runtime holds it (UnityEngine.AnimationModule), the asset type an AssetBundle's
/// controller loads as. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RuntimeAnimatorController{
    .key = "RuntimeAnimatorController", .name = "RuntimeAnimatorController",
    .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};

inline constexpr const char* Animator_RuntimeAnimatorController[] = {"RuntimeAnimatorController"};
inline constexpr const char* Animator_Play_params[] = {"int", "int", "float"};
inline constexpr const char* Animator_HumanBodyBones[] = {"HumanBodyBones"};
inline constexpr const char* Animator_bool[] = {"bool"};
inline constexpr const char* Animator_AnimatorCullingMode[] = {"AnimatorCullingMode"};
inline constexpr const char* Animator_HasState_params[] = {"int", "int"};

/// Replaces the Animator's controller, which starts at its default state. Inferred: Unity's documented property
/// setter; the Squatter's engine test set a bundle's controller with it in game (2026-11-05).
inline constexpr pm::sdk::EntryDesc Animator_set_runtimeAnimatorController{
    .key = "Animator.set_runtimeAnimatorController", .kind = pm::sdk::Kind::Method, .owner = "Animator",
    .name = "set_runtimeAnimatorController", .params = Animator_RuntimeAnimatorController};

/// Plays a state now: Play(stateNameHash, layer, normalizedTime), the hash Animator.StringToHash of the state's name,
/// the layer -1 for the first that has it, the time 0 to 1 into the state. Inferred: Unity's documented API; the
/// Squatter's engine test played its states with it in game (2026-11-05).
inline constexpr pm::sdk::EntryDesc Animator_Play{.key = "Animator.Play(int,int,float)", .kind = pm::sdk::Kind::Method,
                                                  .owner = "Animator", .name = "Play",
                                                  .params = Animator_Play_params};

/// Whether the Animator's controller has a state, by its hash, on a layer: the engine function of
/// Animator.HasState(layerIndex, stateID). The answer is only good while the Animator is initialised (active, with a
/// controller). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_HasState{.key = "Animator.HasState", .kind = pm::sdk::Kind::Icall,
                                                      .name = "UnityEngine.Animator::HasState",
                                                      .params = Animator_HasState_params};

/// Whether the Animator is initialised: active, with a controller it has bound. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_get_isInitialized{.key = "Animator.get_isInitialized",
                                                               .kind = pm::sdk::Kind::Icall,
                                                               .name = "UnityEngine.Animator::get_isInitialized"};

/// Rebinds the Animator to its model and controller: the engine function of Animator.Rebind(writeDefaultValues),
/// whose managed method this build keeps private. An active Animator binds when it gets a controller; this binds one
/// that did not. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_Rebind{.key = "Animator.Rebind", .kind = pm::sdk::Kind::Icall,
                                                    .name = "UnityEngine.Animator::Rebind", .params = Animator_bool};

inline constexpr const char* Animator_CrossFade_params[] = {"int", "float", "int", "float", "float"};

/// Blends from the state playing into another: the engine function of Animator.CrossFade(stateHashName,
/// normalizedTransitionDuration, layer, normalizedTimeOffset, normalizedTransitionTime), the hash Animator.StringToHash of
/// the state's name, the blend's length as a fraction of the playing state's length, the layer, where to start in the
/// new state (0 to 1), and how far into the blend to begin (0). This build keeps only the hash overload's engine
/// function; the fixed-time CrossFadeInFixedTime is stripped. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_CrossFade{.key = "Animator.CrossFade", .kind = pm::sdk::Kind::Icall,
                                                       .name = "UnityEngine.Animator::CrossFade",
                                                       .params = Animator_CrossFade_params};

/// Steps the Animator by a time in seconds now, outside the game's own update: the engine function of
/// Animator.Update(deltaTime). With 0 it poses the model in the state it plays, without moving the state on, as
/// Unity's documentation shows for sampling a pose. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_Update{.key = "Animator.Update", .kind = pm::sdk::Kind::Icall,
                                                    .name = "UnityEngine.Animator::Update", .params = Animator_float};

/// The Transform of a humanoid bone of a humanoid Animator's model, or null when the avatar does not map it. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Animator_GetBoneTransform{
    .key = "Animator.GetBoneTransform", .kind = pm::sdk::Kind::Method, .owner = "Animator",
    .name = "GetBoneTransform", .params = Animator_HumanBodyBones};

/// Whether the clips' root motion moves the model's Transform. A mod that moves the model itself turns it off.
/// Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Animator_set_applyRootMotion{
    .key = "Animator.set_applyRootMotion", .kind = pm::sdk::Kind::Method, .owner = "Animator",
    .name = "set_applyRootMotion", .params = Animator_bool};

/// When the Animator stops animating a model no camera sees. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Animator_set_cullingMode{
    .key = "Animator.set_cullingMode", .kind = pm::sdk::Kind::Method, .owner = "Animator", .name = "set_cullingMode",
    .params = Animator_AnimatorCullingMode};

/// Unity's culling modes of an Animator (UnityEngine.AnimationModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimatorCullingMode{.key = "AnimatorCullingMode", .name = "AnimatorCullingMode",
                                                        .assembly = "UnityEngine.AnimationModule",
                                                        .ns = "UnityEngine"};

/// The Animator keeps animating while no camera sees the model, so cloth on it keeps moving. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc AnimatorCullingMode_AlwaysAnimate{
    .key = "AnimatorCullingMode.AlwaysAnimate", .kind = pm::sdk::Kind::EnumValue, .owner = "AnimatorCullingMode",
    .name = "AlwaysAnimate"};

/// Unity's humanoid bones (UnityEngine.AnimationModule), for Animator.GetBoneTransform. The investigator's body maps
/// every one of these (its avatar leaves only the eyes unmapped, read from the asset files). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones{.key = "HumanBodyBones", .name = "HumanBodyBones",
                                                   .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};
/// The hips, the root of a humanoid skeleton. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_Hips{.key = "HumanBodyBones.Hips", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "HumanBodyBones", .name = "Hips"};
/// The spine's lowest bone. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_Spine{.key = "HumanBodyBones.Spine",
                                                         .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
                                                         .name = "Spine"};
/// The chest. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_Chest{.key = "HumanBodyBones.Chest",
                                                         .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
                                                         .name = "Chest"};
/// The head, at the top of the neck. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_Head{.key = "HumanBodyBones.Head", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "HumanBodyBones", .name = "Head"};
/// The left upper arm. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftUpperArm{
    .key = "HumanBodyBones.LeftUpperArm", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "LeftUpperArm"};
/// The right upper arm. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightUpperArm{
    .key = "HumanBodyBones.RightUpperArm", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "RightUpperArm"};
/// The neck. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_Neck{
    .key = "HumanBodyBones.Neck", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "Neck"};
/// The left shoulder. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftShoulder{
    .key = "HumanBodyBones.LeftShoulder", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "LeftShoulder"};
/// The right shoulder. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightShoulder{
    .key = "HumanBodyBones.RightShoulder", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "RightShoulder"};
/// The left forearm. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftLowerArm{
    .key = "HumanBodyBones.LeftLowerArm", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "LeftLowerArm"};
/// The right forearm. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightLowerArm{
    .key = "HumanBodyBones.RightLowerArm", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "RightLowerArm"};
/// The left thigh. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftUpperLeg{
    .key = "HumanBodyBones.LeftUpperLeg", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "LeftUpperLeg"};
/// The right thigh. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightUpperLeg{
    .key = "HumanBodyBones.RightUpperLeg", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "RightUpperLeg"};
/// The left shin. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftLowerLeg{
    .key = "HumanBodyBones.LeftLowerLeg", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "LeftLowerLeg"};
/// The right shin. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightLowerLeg{
    .key = "HumanBodyBones.RightLowerLeg", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones",
    .name = "RightLowerLeg"};
/// The left hand. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftHand{
    .key = "HumanBodyBones.LeftHand", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones", .name = "LeftHand"};
/// The right hand. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightHand{
    .key = "HumanBodyBones.RightHand", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones", .name = "RightHand"};
/// The left foot. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_LeftFoot{
    .key = "HumanBodyBones.LeftFoot", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones", .name = "LeftFoot"};
/// The right foot. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HumanBodyBones_RightFoot{
    .key = "HumanBodyBones.RightFoot", .kind = pm::sdk::Kind::EnumValue, .owner = "HumanBodyBones", .name = "RightFoot"};

}  // namespace pm::map

// The game API for Animator (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build"
// once and returns its type's default (0, false, null) or does nothing.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's Animator component. `animator` below is the Animator object.
struct Animator {
  /// The type: klass(), is(obj), type_object() (for GetComponent). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Animator> type{};
  /// get_parameters(animator): an AnimatorControllerParameter[] (pm::api::RefArray). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Animator_get_parameters, void*(void*)> get_parameters{};
  /// StringToHash(name): the id of a parameter's name, a managed string. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Animator_StringToHash, int32_t(void*)> StringToHash{};
  /// get_speed(animator). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Animator_get_speed, float(void*)> get_speed{};
  /// set_speed(animator, speed). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_set_speed, void(void*, float)> set_speed{};
  /// Sets the bool parameter whose hashed id is `id`: SetBoolID(animator, id, value). Unresolved:
  /// does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_SetBoolID, void(void*, int32_t, bool)> SetBoolID{};
  /// GetBoolID(animator, id). Unresolved: false.
  static constexpr pm::api::Icall<pm::map::Animator_GetBoolID, bool(void*, int32_t)> GetBoolID{};
  /// SetFloatID(animator, id, value). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_SetFloatID, void(void*, int32_t, float)> SetFloatID{};
  /// GetFloatID(animator, id). Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::Animator_GetFloatID, float(void*, int32_t)> GetFloatID{};
  /// SetIntegerID(animator, id, value). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_SetIntegerID, void(void*, int32_t, int32_t)> SetIntegerID{};
  /// GetIntegerID(animator, id). Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::Animator_GetIntegerID, int32_t(void*, int32_t)> GetIntegerID{};
  /// SetBool(animator, name, value), with name a managed string. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_SetBool, void(void*, void*, bool)> SetBool{};
  /// set_runtimeAnimatorController(animator, controller). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_set_runtimeAnimatorController, void(void*, void*)>
      set_runtimeAnimatorController{};
  /// Play(animator, stateHash, layer, normalizedTime). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_Play, void(void*, int32_t, int32_t, float)> Play{};
  /// HasState(animator, layer, stateHash). Unresolved: false.
  static constexpr pm::api::Icall<pm::map::Animator_HasState, bool(void*, int32_t, int32_t)> HasState{};
  /// Rebind(animator, writeDefaultValues). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_Rebind, void(void*, bool)> Rebind{};
  /// CrossFade(animator, stateHash, blend, layer, startAt, blendStartAt): blend is a fraction of the playing state's
  /// length. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_CrossFade, void(void*, int32_t, float, int32_t, float, float)>
      CrossFade{};
  /// Update(animator, seconds): steps it now; 0 poses the model in its state. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::Animator_Update, void(void*, float)> Update{};
  /// get_isInitialized(animator). Unresolved: false.
  static constexpr pm::api::Icall<pm::map::Animator_get_isInitialized, bool(void*)> get_isInitialized{};
  /// GetBoneTransform(animator, bone), a HumanBodyBones number. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Animator_GetBoneTransform, void*(void*, int32_t)> GetBoneTransform{};
  /// set_applyRootMotion(animator, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_set_applyRootMotion, void(void*, bool)> set_applyRootMotion{};
  /// set_cullingMode(animator, mode), an AnimatorCullingMode number. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Animator_set_cullingMode, void(void*, int32_t)> set_cullingMode{};
};

/// An animator controller asset.
struct RuntimeAnimatorController {
  /// The type: type_object() for AssetBundle.LoadAsset_Internal. Unresolved: null.
  static constexpr pm::api::Type<pm::map::RuntimeAnimatorController> type{};
};

/// When an Animator stops animating an unseen model.
namespace AnimatorCullingMode {
/// AlwaysAnimate(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AnimatorCullingMode_AlwaysAnimate> AlwaysAnimate{};
}  // namespace AnimatorCullingMode

/// Humanoid bones, for Animator::GetBoneTransform. Required entries, so a mod that uses one runs only where it resolved.
namespace HumanBodyBones {
/// Hips(): the hips.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_Hips> Hips{};
/// Spine(): the spine's lowest bone.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_Spine> Spine{};
/// Chest(): the chest.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_Chest> Chest{};
/// Head(): the head.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_Head> Head{};
/// LeftUpperArm(): the left upper arm.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftUpperArm> LeftUpperArm{};
/// RightUpperArm(): the right upper arm.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightUpperArm> RightUpperArm{};
/// The neck: Neck(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_Neck> Neck{};
/// The left shoulder: LeftShoulder(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftShoulder> LeftShoulder{};
/// The right shoulder: RightShoulder(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightShoulder> RightShoulder{};
/// The left forearm: LeftLowerArm(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftLowerArm> LeftLowerArm{};
/// The right forearm: RightLowerArm(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightLowerArm> RightLowerArm{};
/// The left thigh: LeftUpperLeg(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftUpperLeg> LeftUpperLeg{};
/// The right thigh: RightUpperLeg(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightUpperLeg> RightUpperLeg{};
/// The left shin: LeftLowerLeg(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftLowerLeg> LeftLowerLeg{};
/// The right shin: RightLowerLeg(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightLowerLeg> RightLowerLeg{};
/// The left hand: LeftHand(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftHand> LeftHand{};
/// The right hand: RightHand(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightHand> RightHand{};
/// The left foot: LeftFoot(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_LeftFoot> LeftFoot{};
/// The right foot: RightFoot(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HumanBodyBones_RightFoot> RightFoot{};
}  // namespace HumanBodyBones

/// One parameter of an Animator's controller. `parameter` below is one.
struct AnimatorControllerParameter {
  /// Its name, a managed string: m_Name.get(parameter). Unresolved: null.
  static constexpr pm::api::Field<pm::map::AnimatorControllerParameter_m_Name, void*> m_Name{};
  /// Its type, an AnimatorControllerParameterType number: m_Type.get(parameter). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::AnimatorControllerParameter_m_Type, int32_t> m_Type{};
};

/// What a parameter holds: compare AnimatorControllerParameter::m_Type with these.
namespace AnimatorControllerParameterType {
/// Float(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AnimatorControllerParameterType_Float> Float{};
/// Int(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AnimatorControllerParameterType_Int> Int{};
/// Bool(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AnimatorControllerParameterType_Bool> Bool{};
}  // namespace AnimatorControllerParameterType

/// @}
}  // namespace phasmo
