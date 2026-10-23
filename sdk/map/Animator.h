#pragma once

// UnityEngine.Animator: Unity's animation state machine component. Its parameter setters are engine
// functions the runtime registers by icall name, so those entries resolve only in the running game.

#include "entry.h"
#include "game_api.h"

namespace pg::map {

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
