#pragma once

// What a ghost senses of the players: its hearing (GhostHearing and the PlayerNoise it hears), the
// players its sight keeps (PlayerLOSTarget) and how a player moves (NetworkedStamina). Playable Ghost
// shows its player these as the game's own AI has them. Notes read from the class dumps and decompiled
// code of build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The ghost's hearing (Assembly-CSharp, global namespace), a MonoBehaviour on the ghost. Inferred: from
/// the class dump.
inline constexpr pm::sdk::EntryDesc GhostHearing{.key = "GhostHearing", .name = "GhostHearing",
                                                 .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* HeardNoise_fieldTypes[] = {"PlayerNoise", "float"};

/// A noise the ghost hears, a struct nested in GhostHearing: the PlayerNoise and how far it is, exactly
/// (its constructor takes the two). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc HeardNoise{.key = "HeardNoise", .flags = pm::sdk::kExactFields,
                                               .owner = "GhostHearing",
                                               .fieldTypes = HeardNoise_fieldTypes};

/// What the ghost hears now, a List<HeardNoise>: GhostHearing's one list of them. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc GhostHearing_heard{.key = "GhostHearing.heard", .kind = pm::sdk::Kind::Field,
                                                       .owner = "GhostHearing",
                                                       .type = "List<@HeardNoise>"};

/// The noise heard: HeardNoise's one PlayerNoise field (unboxed offset 0). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc HeardNoise_noise{.key = "HeardNoise.noise", .kind = pm::sdk::Kind::Field,
                                                     .owner = "HeardNoise",
                                                     .type = "PlayerNoise"};

/// A player's footstep noise (Assembly-CSharp, global namespace), a Noise. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerNoise{.key = "PlayerNoise", .name = "PlayerNoise",
                                                .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* NoiseKind_values[] = {"WalkFootstep", "SprintFootstep", "CrouchFootstep"};

// Found by its values.
/// The kind of a player's footstep, an enum nested in PlayerNoise (name obfuscated, value names real):
/// WalkFootstep 0, SprintFootstep 1, CrouchFootstep 2. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc NoiseKind{.key = "NoiseKind", .owner = "PlayerNoise",
                                              .values = NoiseKind_values};
/// A sprinting player's footstep. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc NoiseKind_SprintFootstep{.key = "NoiseKind.SprintFootstep",
                                                             .kind = pm::sdk::Kind::EnumValue, .owner = "NoiseKind",
                                                             .name = "SprintFootstep"};
/// A crouching player's footstep. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc NoiseKind_CrouchFootstep{.key = "NoiseKind.CrouchFootstep",
                                                             .kind = pm::sdk::Kind::EnumValue, .owner = "NoiseKind",
                                                             .name = "CrouchFootstep"};

/// The footstep's kind: PlayerNoise's one NoiseKind field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerNoise_kind{.key = "PlayerNoise.kind", .kind = pm::sdk::Kind::Field,
                                                     .owner = "PlayerNoise", .type = "@NoiseKind"};
/// The player making it: PlayerNoise's one Player field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerNoise_player{.key = "PlayerNoise.player", .kind = pm::sdk::Kind::Field,
                                                       .owner = "PlayerNoise", .type = "Player"};

/// A part of a player the ghost's sight can see (Assembly-CSharp, global namespace). Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc PlayerLOSTarget{.key = "PlayerLOSTarget", .name = "PlayerLOSTarget",
                                                    .assembly = "Assembly-CSharp", .ns = ""};
/// Its player: PlayerLOSTarget's one Player field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerLOSTarget_player{.key = "PlayerLOSTarget.player",
                                                           .kind = pm::sdk::Kind::Field, .owner = "PlayerLOSTarget",
                                                           .type = "Player"};

/// A player's stamina and movement, sent to every client (Assembly-CSharp, global namespace). Inferred:
/// from the class dump.
inline constexpr pm::sdk::EntryDesc NetworkedStamina{.key = "NetworkedStamina", .name = "NetworkedStamina",
                                                     .assembly = "Assembly-CSharp", .ns = ""};

// The class's two sync methods, NetworkSyncSprint and NetworkSyncWalk, store their argument in the sprint
// and walk bools and set moving while either is set, clearing it when neither is; but they are leaves the
// fact reader cannot read in full, so they are not mapped. Of the class's five bools
// GhostAI.DayanPlayerChecks reads only moving, of the player its sight keeps nearest.
inline constexpr pm::sdk::Fact NetworkedStamina_moving_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostAI.DayanPlayerChecks"}};

/// Whether the player moves, sprinting or walking: what empowers a Dayan (GhostAI.DayanPlayerChecks
/// reads it of the player its sight keeps nearest). Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkedStamina_moving{
    .key = "NetworkedStamina.moving", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "NetworkedStamina", .type = "bool", .facts = NetworkedStamina_moving_facts};

}  // namespace pm::map

/// @file
/// @brief The game API for what a ghost senses (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null, 0, false) without touching the
/// game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The ghost's hearing.
struct GhostHearing {
  /// What it hears now, a List<HeardNoise>: read it with pm::api::List{heard.get(hearing)}.values(), each
  /// element's noise through HeardNoise::noise. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHearing_heard, void*> heard{};
};

/// A noise the ghost hears, read in place in a List's array (pm::api::ValueArray::at).
struct HeardNoise {
  /// The PlayerNoise: noise.get(element). Unresolved: null.
  static constexpr pm::api::Field<pm::map::HeardNoise_noise, void*> noise{};
};

/// A player's footstep noise.
struct PlayerNoise {
  /// Its kind, a NoiseKind number: kind.get(noise). Unresolved: 0 (a walk).
  static constexpr pm::api::Field<pm::map::PlayerNoise_kind, int32_t> kind{};
  /// The player making it: player.get(noise). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerNoise_player, void*> player{};
};

/// The kinds of footstep.
namespace NoiseKind {
/// SprintFootstep(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::NoiseKind_SprintFootstep> SprintFootstep{};
/// CrouchFootstep(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::NoiseKind_CrouchFootstep> CrouchFootstep{};
}  // namespace NoiseKind

/// A part of a player the ghost's sight can see.
struct PlayerLOSTarget {
  /// Its player: player.get(target). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerLOSTarget_player, void*> player{};
};

/// A player's stamina and movement.
struct NetworkedStamina {
  /// Whether the player moves: moving.get(stamina). Unresolved: false.
  static constexpr pm::api::Field<pm::map::NetworkedStamina_moving, bool> moving{};
};

/// @}
}  // namespace phasmo
