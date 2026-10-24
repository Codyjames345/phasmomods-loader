#pragma once

// UnityEngine.Animator: Unity's animation state machine component. Its parameter setters are engine
// functions the runtime registers by icall name, so those entries resolve only in the running game.
// The type, its parameter list and StringToHash are by real name; the host uses them to check the
// AnimatorParam entries (Scene.h), and a mod gets no wrapper for them: it reads a parameter's hash
// from its AnimatorParam entry.

#include "entry.h"
#include "game_api.h"

namespace pg::map {

inline constexpr const char* Animator_string[] = {"string"};

/// Unity's animator component (UnityEngine.AnimationModule). Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc Animator{.key = "Animator", .name = "Animator",
                                             .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};

/// The Animator's parameters, a new AnimatorControllerParameter[] on every call (empty without a
/// controller). Inferred: Unity's documented property getter.
inline constexpr pg::sdk::EntryDesc Animator_get_parameters{
    .key = "Animator.get_parameters", .kind = pg::sdk::Kind::Method, .owner = "Animator", .name = "get_parameters"};

/// The hash Animator's by-id setters take for a parameter's name. Static. Inferred: Unity's
/// documented API.
inline constexpr pg::sdk::EntryDesc Animator_StringToHash{
    .key = "Animator.StringToHash", .kind = pg::sdk::Kind::Method, .flags = pg::sdk::kStatic, .owner = "Animator",
    .name = "StringToHash", .type = "int", .params = Animator_string};

/// One parameter of an Animator's controller, as get_parameters lists it (UnityEngine.AnimationModule).
/// Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc AnimatorControllerParameter{
    .key = "AnimatorControllerParameter", .name = "AnimatorControllerParameter",
    .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine"};

/// The parameter's name, the field behind its `name` property. Inferred: the field's name is in the
/// dump of build 25811160.
inline constexpr pg::sdk::EntryDesc AnimatorControllerParameter_m_Name{
    .key = "AnimatorControllerParameter.m_Name", .kind = pg::sdk::Kind::Field,
    .owner = "AnimatorControllerParameter", .name = "m_Name", .type = "string"};

// The runtime has no metadata for an icall, so its params are never matched: they fix the arity of
// its wrapper (sdk/api/game_api.h).
inline constexpr const char* Animator_SetBoolID_params[] = {"int", "bool"};

/// Sets an Animator's bool parameter by its hashed id (Animator.StringToHash): the engine function
/// behind UnityEngine.Animator.SetBool(int, bool), called as SetBoolID(animator, id, value). Custom
/// Ghosts' apparition calls it by this name. Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc Animator_SetBoolID{.key = "Animator.SetBoolID", .kind = pg::sdk::Kind::Icall,
                                                       .name = "UnityEngine.Animator::SetBoolID",
                                                       .params = Animator_SetBoolID_params};

}  // namespace pg::map

// The game API for Animator (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build"
// once and does nothing.
namespace phasmo {

/// Unity's Animator component. `animator` below is the Animator object.
struct Animator {
  /// Sets the bool parameter whose hashed id is `id`: SetBoolID(animator, id, value). Unresolved:
  /// does nothing.
  static constexpr pg::api::Icall<pg::map::Animator_SetBoolID, void(void*, int32_t, bool)> SetBoolID{};
};

}  // namespace phasmo
