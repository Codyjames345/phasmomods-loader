#pragma once

// LevelController: the controller of a contract's level, one per contract. The type is found by name;
// its static instance field is obfuscated, so it is found by shape. Notes read from decompiled code
// are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pg::map {

/// The level's controller (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pg::sdk::EntryDesc LevelController{.key = "LevelController",
                                                    .confidence = pg::sdk::Confidence::Decompiled,
                                                    .name = "LevelController", .assembly = "Assembly-CSharp", .ns = ""};

/// The current level's controller: the class's one static field of its own type. It outlives its
/// level: after a contract it still points at the destroyed controller, so check it with
/// pg::api::alive (core/game.cpp does). Decompiled.
inline constexpr pg::sdk::EntryDesc LevelController_instance{
    .key = "LevelController.instance", .kind = pg::sdk::Kind::Field, .confidence = pg::sdk::Confidence::Decompiled,
    .flags = pg::sdk::kStatic, .owner = "LevelController", .hint = "ബദഭഺധതഷഢഫ", .type = "@LevelController"};

}  // namespace pg::map

// The game API for LevelController (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this
// game build" once and returns its type's default (null, false) without touching the game.
namespace phasmo {

/// The controller of a contract's level.
struct LevelController {
  /// The LevelController type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved:
  /// null and false.
  static constexpr pg::api::Type<pg::map::LevelController> type{};
  /// The current level's controller: instance.get(), or null. A destroyed one remains after a
  /// contract ends, so check it with pg::api::alive. Reading it may run the class's static
  /// constructor, so read it while the game plays, never at startup. Unresolved: null.
  static constexpr pg::api::StaticField<pg::map::LevelController_instance, void*> instance{};
};

}  // namespace phasmo
