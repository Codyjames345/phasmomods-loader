#pragma once

// GhostAI: the ghost's brain, one per contract. It holds the ghost's state machine, its NavMeshAgent
// and the hunt flags. The type and its real-named methods are found by name; nearly every field and
// the state enum are obfuscated, so they are found by shape and, where shape is not enough, by what
// GhostAI.Hunting does to them. Notes read from decompiled code are from build 25811160.

#include "entry.h"

namespace pg::map {

inline constexpr const char* GhostAI_ChangeState_params[] = {"@GhostState", "PhotonObjectInteract", "bool"};
inline constexpr const char* GhostAI_Hunting_params[] = {"bool", "int", "PhotonMessageInfo"};
inline constexpr const char* GhostAI_LookAtPlayer_params[] = {"Player"};
inline constexpr const char* GhostAI_TemporarilyStopWander_params[] = {"float"};
inline constexpr const char* GhostState_values[] = {"idle", "wander", "hunting"};
// Hunting stores five bools, four of them from registers. Of those four, only isHunting's offset
// starts a store in Awake: Awake also clears the bool at 0x109, but inside a 16-bit store at 0x108,
// and the resolver keys a store by its start offset. Should Awake come to store at 0x109 on its own,
// the entry fails closed (Ambiguous), never to the wrong field.
inline constexpr pg::sdk::Fact GhostAI_isHunting_facts[] = {
    {.is = pg::sdk::Fact::Is::Stores, .method = "GhostAI.Hunting"},
    {.is = pg::sdk::Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1, .negate = true},
    {.is = pg::sdk::Fact::Is::Stores, .method = "GhostAI.Awake"}};
inline constexpr pg::sdk::Fact GhostAI_postHuntCooldown_facts[] = {
    {.is = pg::sdk::Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};

/// The ghost's AI component (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI{.key = "GhostAI", .confidence = pg::sdk::Confidence::Decompiled,
                                            .name = "GhostAI", .assembly = "Assembly-CSharp", .ns = ""};

/// The ghost's state, an enum nested in GhostAI (name obfuscated, value names real): idle, wander,
/// hunting, favouriteRoom and 20 more. ChangeState switches on it to build the state's object. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostState{.key = "GhostState", .confidence = pg::sdk::Confidence::Decompiled,
                                               .owner = "GhostAI", .hint = "ദശസറദബഹജഭ", .values = GhostState_values};

/// The ghost stands still. Inferred: from the value's name.
inline constexpr pg::sdk::EntryDesc GhostState_idle{.key = "GhostState.idle", .kind = pg::sdk::Kind::EnumValue,
                                                    .owner = "GhostState", .name = "idle"};

/// The ghost wanders: ChangeState gives this state the ghost's NavMeshAgent. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostState_wander{.key = "GhostState.wander", .kind = pg::sdk::Kind::EnumValue,
                                                      .confidence = pg::sdk::Confidence::Decompiled,
                                                      .owner = "GhostState", .name = "wander"};

/// The ghost hunts. ChangeState stores it (idle instead while a flag of LevelValues is set), then
/// starts its state object only when isHunting is clear or its third argument forces it. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostState_hunting{.key = "GhostState.hunting", .kind = pg::sdk::Kind::EnumValue,
                                                       .confidence = pg::sdk::Confidence::Decompiled,
                                                       .owner = "GhostState", .name = "hunting"};

/// The ghost's current state. ChangeState stores its first argument here. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_state{.key = "GhostAI.state", .kind = pg::sdk::Kind::Field,
                                                  .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .hint = "ഷമദയദസസധ഻", .type = "@GhostState"};

/// The NavMeshAgent the ghost moves with. Awake sets its speed to the base hunt speed. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_agent{.key = "GhostAI.agent", .kind = pg::sdk::Kind::Field,
                                                  .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .hint = "ഞതലസഩഠദര഻", .type = "NavMeshAgent"};

/// Unity's Awake: clears isHunting and postHuntCooldown, sets the agent's speed to 1.7 and keeps
/// that speed as the ghost's base hunt speed. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_Awake{.key = "GhostAI.Awake", .kind = pg::sdk::Kind::Method,
                                                  .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .name = "Awake"};

/// A Photon RPC that starts (true) or ends (false) a hunt on this client. It stores its first
/// argument to isHunting, and when that is true sets postHuntCooldown to 1. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_Hunting{.key = "GhostAI.Hunting", .kind = pg::sdk::Kind::Method,
                                                    .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                    .name = "Hunting", .params = GhostAI_Hunting_params};

/// Changes the ghost's state: it stores the state and starts that state's object, but only while a
/// flag of the ghost's PhotonView (+0x68) is set; otherwise it logs an error. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_ChangeState{.key = "GhostAI.ChangeState", .kind = pg::sdk::Kind::Method,
                                                        .confidence = pg::sdk::Confidence::Decompiled,
                                                        .owner = "GhostAI", .name = "ChangeState",
                                                        .params = GhostAI_ChangeState_params};

/// Ends an appearance: when the ghost's shown flag is set (the one Hunting clears as it hides the
/// model), it sends an RPC with two arguments to every client; otherwise it does nothing. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_UnAppear{.key = "GhostAI.UnAppear", .kind = pg::sdk::Kind::Method,
                                                     .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                     .name = "UnAppear"};

/// Turns the ghost's transform to face the player's position, on this client only. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_LookAtPlayer{.key = "GhostAI.LookAtPlayer", .kind = pg::sdk::Kind::Method,
                                                         .confidence = pg::sdk::Confidence::Decompiled,
                                                         .owner = "GhostAI", .name = "LookAtPlayer",
                                                         .params = GhostAI_LookAtPlayer_params};

/// Stops the ghost wandering for the given seconds: restarts the coroutine that times it. Tarot's
/// Hermit card and a smudged Yurei use it. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_TemporarilyStopWander{
    .key = "GhostAI.TemporarilyStopWander", .kind = pg::sdk::Kind::Method,
    .confidence = pg::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "TemporarilyStopWander",
    .params = GhostAI_TemporarilyStopWander_params};

/// Whether a hunt is on. GhostAI.Hunting stores its first argument there, and Awake clears it.
/// Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_isHunting{.key = "GhostAI.isHunting", .kind = pg::sdk::Kind::Field,
                                                      .confidence = pg::sdk::Confidence::Decompiled,
                                                      .owner = "GhostAI", .hint = "ബഺണസഷ഻ഡറബ", .type = "bool",
                                                      .facts = GhostAI_isHunting_facts};

/// GhostAI.Hunting sets it to 1 and Awake clears it. Earlier reverse engineering names it the
/// post-hunt cooldown. Decompiled.
inline constexpr pg::sdk::EntryDesc GhostAI_postHuntCooldown{
    .key = "GhostAI.postHuntCooldown", .kind = pg::sdk::Kind::Field, .confidence = pg::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .hint = "ടഴദബടഠഹടന", .type = "bool", .facts = GhostAI_postHuntCooldown_facts};

}  // namespace pg::map
