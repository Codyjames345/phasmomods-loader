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
};

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
