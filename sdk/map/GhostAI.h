#pragma once

// GhostAI: the ghost's brain, one per contract. It holds the ghost's state machine, its NavMeshAgent
// and the hunt flags. The type and its real-named methods are found by name; nearly every field and
// the state enum are obfuscated, so they are found by shape and, where shape is not enough, by what
// GhostAI.Hunting does to them. Notes read from decompiled code are from build 25811160.

#include <bit>
#include <cstdint>

#include "entry.h"
#include "game_api.h"
#include "Transform.h"

namespace pm::map {

inline constexpr const char* GhostAI_ChangeState_params[] = {"@GhostState", "PhotonObjectInteract", "bool"};
inline constexpr const char* GhostAI_Hunting_params[] = {"bool", "int", "PhotonMessageInfo"};
inline constexpr const char* GhostAI_LookAtPlayer_params[] = {"Player"};
inline constexpr const char* GhostAI_TemporarilyStopWander_params[] = {"float"};
inline constexpr const char* GhostAI_Init_params[] = {"@GhostTraits"};
inline constexpr const char* GhostState_values[] = {"idle", "wander", "hunting"};
// Hunting stores five bools through rdi, a copy of this, four of them from registers. Of those four,
// ChangeState reads only isHunting, through rsi, its own copy of this, before it starts a state's
// object. Awake clears isHunting too, through rbx (a copy of this it restores in its epilogue), but it
// clears postHuntCooldown and other bools with it, so an Awake fact would not single isHunting out.
// Should ChangeState come to read another of the four, the entry fails closed (Ambiguous).
inline constexpr pm::sdk::Fact GhostAI_isHunting_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostAI.Hunting"},
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1, .negate = true},
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostAI.ChangeState"}};
inline constexpr pm::sdk::Fact GhostAI_postHuntCooldown_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostAI.Hunting", .value = 1}};

/// The ghost's AI component (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI{.key = "GhostAI", .confidence = pm::sdk::Confidence::Decompiled,
                                            .name = "GhostAI", .assembly = "Assembly-CSharp", .ns = ""};

/// The ghost's state, an enum nested in GhostAI (name obfuscated, value names real): idle, wander,
/// hunting, favouriteRoom and 20 more. ChangeState switches on it to build the state's object. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState{.key = "GhostState", .confidence = pm::sdk::Confidence::Decompiled,
                                               .owner = "GhostAI", .values = GhostState_values};

/// The ghost stands still. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_idle{.key = "GhostState.idle", .kind = pm::sdk::Kind::EnumValue,
                                                    .owner = "GhostState", .name = "idle"};

/// The ghost wanders: ChangeState gives this state the ghost's NavMeshAgent. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState_wander{.key = "GhostState.wander", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostState", .name = "wander"};

/// The ghost hunts. ChangeState stores it (idle instead while a flag of LevelValues is set), then
/// starts its state object only when isHunting is clear or its third argument forces it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState_hunting{.key = "GhostState.hunting", .kind = pm::sdk::Kind::EnumValue,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "GhostState", .name = "hunting"};

/// A state GhostActivity sets through ChangeState with an object to act on: its choice among the
/// targets its sensor detects (one call in it) and one branch of its interaction with an object.
/// ChangeState stores it but starts no state object for it: its switch has no case for the value, so
/// it falls to the default, which logs it as an error. What the game shows for it was not traced.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState_appear{.key = "GhostState.appear", .kind = pm::sdk::Kind::EnumValue,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostState", .name = "appear"};

/// The ghost's events. ChangeState stores it and runs GhostAI's private event pick, always passing -1
/// for the event. Unless forced, some ghost types may go back to idle instead (one by the players'
/// average insanity); otherwise it starts one of five event state objects, chosen by chance, the
/// ghost's type and whether its audio plays: event 0 while an AudioSource plays, a fixed array for
/// the Kormos (28), event 1 perhaps forced for the Banshee (4), and events 1, 3 and 4 remapped for
/// the Shade (8) and the Oni (11). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState_randomEvent{.key = "GhostState.randomEvent",
                                                           .kind = pm::sdk::Kind::EnumValue,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostState", .name = "randomEvent"};

/// The ghost's current state. ChangeState stores its first argument here. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_state{.key = "GhostAI.state", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .type = "@GhostState"};

/// The interface of the ghost's state objects, GhostWanderState and the others (Assembly-CSharp, global namespace, name
/// obfuscated): the one interface the wander declares. Decompiled.
inline constexpr pm::sdk::EntryDesc IGhostState{.key = "IGhostState", .confidence = pm::sdk::Confidence::Decompiled,
                                                .assembly = "Assembly-CSharp", .ns = "",
                                                .within = "GhostWanderState"};

inline constexpr const char* GhostStateMachine_fieldTypes[] = {"@IGhostState"};

/// The ghost's state machine (Assembly-CSharp, global namespace, name obfuscated), one per GhostAI: the class whose one
/// field is a state object. Changing state runs the old state's exit, stores the new state and runs its enter;
/// GhostAI.ChangeState does it for the states with an object. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostStateMachine{.key = "GhostStateMachine",
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .flags = pm::sdk::kExactFields,
                                                      .assembly = "Assembly-CSharp", .ns = "",
                                                      .fieldTypes = GhostStateMachine_fieldTypes};

/// The state object the machine runs now: its one field. Null before its first state. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostStateMachine_current{.key = "GhostStateMachine.current",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostStateMachine",
                                                              .type = "@IGhostState"};

/// The ghost's state machine: GhostAI's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_stateMachine{.key = "GhostAI.stateMachine", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostAI",
                                                         .type = "@GhostStateMachine"};

/// The NavMeshAgent the ghost moves with. Awake sets its speed to the base hunt speed. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_agent{.key = "GhostAI.agent", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .type = "NavMeshAgent"};

/// The ghost's GhostInfo: the class's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_info{.key = "GhostAI.info", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                 .type = "@GhostInfo"};

/// Sets the ghost up on every client from the host's traits: GhostController.CreateGhost (an RPC)
/// calls it with its own traits, and it copies them whole into the GhostInfo's, shows the ghost and
/// runs StartGhost, which on the master calls GhostInfo.SyncValues. It has an alias (see
/// sdk/systems/ghost-ai.md). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_Init{.key = "GhostAI.Init", .kind = pm::sdk::Kind::Method,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                 .name = "Init", .params = GhostAI_Init_params};

/// Unity's Awake: clears isHunting and postHuntCooldown, sets the agent's speed to 1.7 and keeps
/// that speed as the ghost's base hunt speed. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_Awake{.key = "GhostAI.Awake", .kind = pm::sdk::Kind::Method,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .name = "Awake"};

/// Unity's Update, every frame. On the master it runs the ghost's state machine (unless a flag of Network,
/// not traced, is set) and once a second records where the ghost stands while its agent moves; while
/// isHunting is set it also makes the model blink on a timer. Outside a hunt it does nothing else to the
/// agent (sdk/systems/ghost-ai.md, "Moving the ghost outside a hunt"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_Update{.key = "GhostAI.Update", .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                   .name = "Update"};

// Awake stores four floats of the ghost: three are the constant 1, this one the agent's speed it has
// just set (NavMeshAgent.get_speed after set_speed(1.7)).
inline constexpr pm::sdk::Fact GhostAI_baseSpeed_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostAI.Awake"},
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostAI.Awake", .value = std::bit_cast<int32_t>(1.0f),
     .negate = true}};

/// The ghost's base hunt speed in metres a second: Awake sets the agent's speed to 1.7 and keeps it
/// here. The hunt's RefreshBase gives the hunt this base for every type but the Moroi and the Deogen.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_baseSpeed{.key = "GhostAI.baseSpeed", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostAI", .type = "float",
                                                      .facts = GhostAI_baseSpeed_facts};

/// A Photon RPC that starts (true) or ends (false) a hunt on this client. It stores its first
/// argument to isHunting, and when that is true sets postHuntCooldown to 1. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_Hunting{.key = "GhostAI.Hunting", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                    .name = "Hunting", .params = GhostAI_Hunting_params};

/// Changes the ghost's state: it stores the state and starts that state's object, but only while a
/// flag of the ghost's PhotonView is set; otherwise it logs an error. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_ChangeState{.key = "GhostAI.ChangeState", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostAI", .name = "ChangeState",
                                                        .params = GhostAI_ChangeState_params};

/// Ends an appearance: when the ghost's shown flag is set (the one Hunting clears as it hides the
/// model), it sends an RPC with two arguments to every client; otherwise it does nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_UnAppear{.key = "GhostAI.UnAppear", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                     .name = "UnAppear"};

/// Turns the ghost's transform to face the player's position, on this client only. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_LookAtPlayer{.key = "GhostAI.LookAtPlayer", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostAI", .name = "LookAtPlayer",
                                                         .params = GhostAI_LookAtPlayer_params};

/// Stops the ghost wandering for the given seconds: restarts the coroutine that times it. Tarot's
/// Hermit card and a smudged Yurei use it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_TemporarilyStopWander{
    .key = "GhostAI.TemporarilyStopWander", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "TemporarilyStopWander",
    .params = GhostAI_TemporarilyStopWander_params};

/// The Banshee's new target: on the master, for a ghost whose behaviour type is the Banshee's (4), it
/// chooses a living player other than the one given and sets the ghost's target to them through an
/// RPC. The hunt's kill calls it with each victim (sdk/systems/ghost-ai.md, "The Banshee's one
/// target"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_SetNewBansheeTarget{
    .key = "GhostAI.SetNewBansheeTarget", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "SetNewBansheeTarget",
    .params = GhostAI_LookAtPlayer_params};

// AttemptGhostTypeStateChange flips one GhostAI bool, through LevelController.currentGhost, for the
// Obambo; it stores no other.
inline constexpr pm::sdk::Fact GhostAI_obamboAggressive_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostController.AttemptGhostTypeStateChange"}};

/// The Obambo's mood, aggressive when set: GhostController.AttemptGhostTypeStateChange flips it at each
/// of the Obambo's switches. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_obamboAggressive{
    .key = "GhostAI.obamboAggressive", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .type = "bool", .facts = GhostAI_obamboAggressive_facts,
    .access = pm::sdk::Visibility::Public};

/// The ghost's hearing: GhostAI's one GhostHearing field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostAI_hearing{.key = "GhostAI.hearing", .kind = pm::sdk::Kind::Field,
                                                    .owner = "GhostAI", .type = "@GhostHearing"};

/// The parts of players its sight keeps, nearest first, a List<PlayerLOSTarget>: GhostAI's one list of
/// them, which DayanPlayerChecks fills by distance and reads the first of. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_sightTargets{.key = "GhostAI.sightTargets", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostAI",
                                                         .type = "List<PlayerLOSTarget>"};

inline constexpr const char* GhostAI_EventPick_params[] = {"bool", "int"};
// Of GhostAI's void(bool, int) methods only the event pick is called by ChangeState (its look-alikes by
// ChangeState's two look-alikes).
inline constexpr pm::sdk::Fact GhostAI_EventPick_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostAI.ChangeState"}};

/// GhostAI's private event pick, which ChangeState runs for GhostState.randomEvent: by chance, the
/// ghost's type and its audio it starts one of five event states; for the Thaye it reads the age step
/// from GhostController.instance. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_EventPick{.key = "GhostAI.EventPick", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostAI", .type = "void",
                                                      .params = GhostAI_EventPick_params,
                                                      .facts = GhostAI_EventPick_facts};

// Of GhostAI's three public Transforms the hunt's voice test reads only this one, through the hunt's
// ghost.
inline constexpr pm::sdk::Fact GhostAI_head_facts[] = {{.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.Hears"}};

/// Where the ghost hears from: the Transform the hunt's voice test measures a talking player's distance
/// from. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_head{.key = "GhostAI.head", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                 .type = "Transform", .facts = GhostAI_head_facts};

// GhostAI's two Player fields; SetNewBansheeTarget reads this one through this.
inline constexpr pm::sdk::Fact GhostAI_bansheeTarget_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostAI.SetNewBansheeTarget"}};

/// The player the ghost is set on, a Banshee's target (sdk/systems/ghost-ai.md, "The Banshee's one
/// target"), or null: SetNewBansheeTarget sets it through an RPC, the idle state's hunt insanity measures
/// this player's insanity instead of the team's, and the hunt keeps to them. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_bansheeTarget{.key = "GhostAI.bansheeTarget", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "GhostAI", .type = "Player",
                                                          .facts = GhostAI_bansheeTarget_facts};

// Of GhostAI's bools Awake stores the constant 1 in this one only (a 16-bit store that also clears the
// bool beside it, keyed by its start).
inline constexpr pm::sdk::Fact GhostAI_canWander_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostAI.Awake", .value = 1}};

/// Whether the ghost may wander: Awake sets it, and the coroutine TemporarilyStopWander starts clears it
/// for its time; the wander's enter sends a ghost without it to its favourite room. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_canWander{.key = "GhostAI.canWander", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                      .type = "bool",
                                                      .facts = GhostAI_canWander_facts};

// UpdateSpeed reads this bool of the hunt's ghost, the Obambo's mood, and at the same offset as the cooldown
// its own searchCloset.
inline constexpr pm::sdk::Fact GhostAI_speedFlag_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.UpdateSpeed"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.postHuntCooldown"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.obamboAggressive"}};

/// Whether the ghost acts as under a Blood Moon whatever the weather: the Winter's Jest event's RPC that
/// makes Krampus stronger (WintersJestManager.RPC_MakeKrampusStronger) sets it on the contract's ghost.
/// GhostHuntState.UpdateSpeed and GhostActivity.GhostAbility take their Blood Moon branch when it is set or
/// the difficulty's actual weather is the Blood Moon (Difficulty.actualWeather, WeatherValueType.BloodMoon):
/// the hunt's speed multiplier and the ability's second roll. Nothing the suite reads; it is mapped so that
/// the entries beside it can be told from it. The key keeps its first name. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_speedFlag{.key = "GhostAI.speedFlag", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                      .type = "bool",
                                                      .facts = GhostAI_speedFlag_facts};

// GhostAbility reads two bools of the ghost, this one and speedFlag.
inline constexpr pm::sdk::Fact GhostAI_stayHome_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostActivity.GhostAbility"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.speedFlag"}};

/// While set, the ghost's idle roll and its wanders go to its favourite room instead: GhostAbility and the
/// wander's enter read it, and a coroutine of a Monkey Paw wish sets it for a time. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_stayHome{.key = "GhostAI.stayHome", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                     .type = "bool",
                                                     .facts = GhostAI_stayHome_facts};

// The one writer of this bool is the coroutine RemoveInteractionBlock starts, which no entry can name; its
// readers are the idle state's methods. The idle roll reads this and the four named here.
inline constexpr pm::sdk::Fact GhostAI_interactionBoost_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostIdleState.Execute"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.postHuntCooldown"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.huntsHeldOff"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.speedFlag"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "GhostAI.stayHome"}};

/// While set, the idle roll's actions are not held back: the coroutine RemoveInteractionBlock starts (the
/// Tower card, a Monkey Paw wish) sets it for the time given and then clears it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_interactionBoost{
    .key = "GhostAI.interactionBoost", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .type = "bool", .facts = GhostAI_interactionBoost_facts};

/// The props the Deildegast has moved, a HashSet<PhotonObjectInteract>: GhostAI's one field of that type;
/// its speed falls with their count. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostAI_movedProps{.key = "GhostAI.movedProps", .kind = pm::sdk::Kind::Field,
                                                       .owner = "GhostAI",
                                                       .type = "HashSet<PhotonObjectInteract>"};

/// Whether a hunt is on. GhostAI.Hunting stores its first argument there, ChangeState reads it, and
/// Awake clears it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_isHunting{.key = "GhostAI.isHunting", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "GhostAI", .type = "bool",
                                                      .facts = GhostAI_isHunting_facts};

/// GhostAI.Hunting sets it to 1 and Awake clears it. Earlier reverse engineering names it the
/// post-hunt cooldown. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_postHuntCooldown{
    .key = "GhostAI.postHuntCooldown", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .type = "bool", .facts = GhostAI_postHuntCooldown_facts};

/// The Dayan's checks on the players near it, every frame of a hunt: the hunt state's speed update calls
/// it (GhostHuntState.UpdateSpeed's fact). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_DayanPlayerChecks{
    .key = "GhostAI.DayanPlayerChecks", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .name = "DayanPlayerChecks"};

// The ghost's other states, by their real value names. Each of these is a value a state change of the
// ghost's own may carry; Ghost Tells holds some of them back (sdk/systems/ghost-ai.md, "The ghost's own
// attempts"). Inferred: from the value's name.
/// The ghost goes to its favourite room. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_favouriteRoom{.key = "GhostState.favouriteRoom",
                                                             .kind = pm::sdk::Kind::EnumValue,
                                                             .owner = "GhostState", .name = "favouriteRoom"};
/// A light interaction. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_light{.key = "GhostState.light", .kind = pm::sdk::Kind::EnumValue,
                                                     .owner = "GhostState", .name = "light"};
/// A door interaction. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_door{.key = "GhostState.door", .kind = pm::sdk::Kind::EnumValue,
                                                    .owner = "GhostState", .name = "door"};
/// A throw. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_throwing{.key = "GhostState.throwing", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "GhostState", .name = "throwing"};
/// A fuse box interaction. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_fusebox{.key = "GhostState.fusebox", .kind = pm::sdk::Kind::EnumValue,
                                                       .owner = "GhostState", .name = "fusebox"};
/// A knock on a door. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_doorKnock{.key = "GhostState.doorKnock", .kind = pm::sdk::Kind::EnumValue,
                                                         .owner = "GhostState", .name = "doorKnock"};
/// A knock on a window. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_windowKnock{.key = "GhostState.windowKnock",
                                                           .kind = pm::sdk::Kind::EnumValue, .owner = "GhostState",
                                                           .name = "windowKnock"};
/// A car alarm. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_carAlarm{.key = "GhostState.carAlarm", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "GhostState", .name = "carAlarm"};
/// A light flicker. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_flicker{.key = "GhostState.flicker", .kind = pm::sdk::Kind::EnumValue,
                                                       .owner = "GhostState", .name = "flicker"};
/// A camera interaction. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_cctv{.key = "GhostState.cctv", .kind = pm::sdk::Kind::EnumValue,
                                                    .owner = "GhostState", .name = "cctv"};
/// The ghost walks for a D.O.T.S. sighting: GhostAI.ChangeState makes a GhostDotsState for it, and the wander's
/// enter goes to it by chance. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostState_dots{.key = "GhostState.dots", .kind = pm::sdk::Kind::EnumValue,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "GhostState", .name = "dots"};
/// The ghost's ability. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_GhostAbility{.key = "GhostState.GhostAbility",
                                                            .kind = pm::sdk::Kind::EnumValue, .owner = "GhostState",
                                                            .name = "GhostAbility"};
/// A mannequin interaction. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_mannequin{.key = "GhostState.mannequin", .kind = pm::sdk::Kind::EnumValue,
                                                         .owner = "GhostState", .name = "mannequin"};
/// An object teleported. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_teleportObject{.key = "GhostState.teleportObject",
                                                              .kind = pm::sdk::Kind::EnumValue, .owner = "GhostState",
                                                              .name = "teleportObject"};
/// An interaction with an object. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_interact{.key = "GhostState.interact", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "GhostState", .name = "interact"};
/// The summoning circle's pull on the ghost. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_summoningCircle{.key = "GhostState.summoningCircle",
                                                               .kind = pm::sdk::Kind::EnumValue,
                                                               .owner = "GhostState", .name = "summoningCircle"};
/// The music box's pull on the ghost. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_musicBox{.key = "GhostState.musicBox", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "GhostState", .name = "musicBox"};
/// A flame lit. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc GhostState_ignite{.key = "GhostState.ignite", .kind = pm::sdk::Kind::EnumValue,
                                                      .owner = "GhostState", .name = "ignite"};

inline constexpr const char* GhostAI_GetNearestPlayer_params[] = {"out Player"};

// Mapped for GhostDotsState.GoToNearest's fact.
/// Gives the living player nearest the ghost, and whether there is one. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_GetNearestPlayer{
    .key = "GhostAI.GetNearestPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .name = "GetNearestPlayer", .type = "bool", .params = GhostAI_GetNearestPlayer_params};

inline constexpr const char* GhostAI_Appear_params[] = {"int"};
inline constexpr const char* GhostAI_MakeGhostAppear_params[] = {"bool", "int", "PhotonMessageInfo"};
// Of the class's bools, MakeGhostAppear stores only the shown flag, from its first argument. (Appear and
// UnAppear read it too, through registers the reader cannot prove hold `this`.)
inline constexpr pm::sdk::Fact GhostAI_isAppeared_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostAI.MakeGhostAppear"}};

/// Shows the ghost: when it is not showing itself already (isAppeared clear) it sends an RPC through
/// its PhotonView, after which, for the Shade (8) and the Oni (11), a Random.RandomRangeInt(0, 2)
/// roll may change the form asked for. It does nothing while the ghost shows itself. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_Appear{.key = "GhostAI.Appear", .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                   .name = "Appear", .params = GhostAI_Appear_params};

/// The RPC that shows (true) or hides (false) the ghost on every client: it stores its first
/// argument in isAppeared when it shows the ghost or the ghost was shown. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_MakeGhostAppear{
    .key = "GhostAI.MakeGhostAppear", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostAI", .name = "MakeGhostAppear", .params = GhostAI_MakeGhostAppear_params};

/// Whether the ghost shows itself: MakeGhostAppear stores it on every client, Appear does nothing
/// while it is set, and UnAppear sends its RPC only while it is set. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_isAppeared{.key = "GhostAI.isAppeared", .kind = pm::sdk::Kind::Field,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "GhostAI", .type = "bool",
                                                       .facts = GhostAI_isAppeared_facts};

/// Makes the coroutine that holds the ghost's hunts off for a while. Its only callers are the incense's
/// smudge of the ghost (in WhiteSage) and a Monkey Paw wish. The coroutine does nothing when a hold is
/// already on; otherwise it sets the hold, waits a time chosen by the ghost's behaviour type (one for
/// the Spirit (0), another for the Demon (9), a third for the rest), and clears it. Calling it starts nothing: the
/// caller starts the coroutine. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_StopGhostFromHunting{
    .key = "GhostAI.StopGhostFromHunting", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI", .name = "StopGhostFromHunting"};

// FireSource's count of the Onryo's blow-outs reads two bools of the ghost (cross-type: matched at
// their offsets through any register), isHunting and the hold; ChangeState reads isHunting through this
// but never the hold.
inline constexpr pm::sdk::Fact GhostAI_huntsHeldOff_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "FireSource.CountGhostBlowOut"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostAI.ChangeState", .negate = true}};

/// While set, the ghost's hunts are held off: the coroutine StopGhostFromHunting makes (its MoveNext)
/// returns at once while it is set, and otherwise sets it, waits a time chosen by the behaviour type
/// and clears it. FireSource's count of the Onryo's blow-outs (FireSource.CountGhostBlowOut) reads it
/// beside isHunting. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_huntsHeldOff{.key = "GhostAI.huntsHeldOff", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostAI", .type = "bool",
                                                         .facts = GhostAI_huntsHeldOff_facts};

/// The incense that smudged the ghost in a hunt: GhostAI's one WhiteSage field. The coroutine of
/// StopHuntingForTime (its MoveNext) clears it, stores its incense there and reads that incense's
/// StoreItem. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_smudge{.key = "GhostAI.smudge", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                   .type = "@WhiteSage"};

// FireSource's count of the Onryo's blow-outs stores one int of the ghost's, reached through
// LevelController: another type's method, so the fact matches that offset through any register.
inline constexpr pm::sdk::Fact GhostAI_flamesBlownOut_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "FireSource.CountGhostBlowOut"}};

/// The Onryo's count of flames blown out, an int. Once it reaches 3, the idle state starts a hunt.
/// FireSource.CountGhostBlowOut adds one for each blow-out by the Onryo (16), and clears a count of
/// 3 reached while the ghost hunts or its hunts are held off. GhostIdleState.Execute starts the hunt:
/// when its idle timer runs out with the count above 2, it clears the count and changes the state to
/// GhostState.hunting, without the sanity roll (see that entry). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_flamesBlownOut{.key = "GhostAI.flamesBlownOut",
                                                           .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostAI", .type = "int",
                                                           .facts = GhostAI_flamesBlownOut_facts};

/// The ghost's GhostActivity component: GhostAI's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_activity{.key = "GhostAI.activity", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                     .type = "@GhostActivity"};

/// The model the ghost wears now, a GhostModel: GhostAI's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_model{.key = "GhostAI.model", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                  .type = "@GhostModel"};

/// The ghost's GhostInteraction (its EMF and prints at what it touches): GhostAI's one field of that
/// type, which GhostActivity.InteractWithAProp passes to CreateThrowingEMF. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_interaction{.key = "GhostAI.interaction", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostAI",
                                                        .type = "@GhostInteraction"};

/// What the ghost leaves where it touches things: EMF and prints (Assembly-CSharp, global namespace), a
/// MonoBehaviour beside its GhostAI. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInteraction{.key = "GhostInteraction",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .name = "GhostInteraction", .assembly = "Assembly-CSharp",
                                                     .ns = ""};

/// Leaves an EMF reading at a thrown or moved object: CreateThrowingEMF(object's PhotonObjectInteract,
/// the reading's EMF type, a flag, and a range in metres; the game's defaults are objectMoved, false and
/// 20). GhostActivity.InteractWithAProp calls it on GhostAI.interaction. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInteraction_CreateThrowingEMF{
    .key = "GhostInteraction.CreateThrowingEMF", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostInteraction", .name = "CreateThrowingEMF"};

inline constexpr const char* GhostInteraction_CreateInteractionEMF_params[] = {"Vector3", "Quaternion", "int",
                                                                                 "@EMFType"};

/// Leaves an EMF reading at a point, facing a rotation, through GhostInteraction's own EMF spawn: the overload
/// of (position, rotation, a number, the reading's EMFType). Door.CreateDoorNoise calls it on the contract ghost's
/// GhostInteraction with the door's PhotonView number and the door-moved type. Its two other overloads (a
/// position alone, an object's PhotonObjectInteract) are not mapped. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInteraction_CreateInteractionEMF{
    .key = "GhostInteraction.CreateInteractionEMF", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostInteraction", .name = "CreateInteractionEMF",
    .params = GhostInteraction_CreateInteractionEMF_params};

inline constexpr const char* GhostInteraction_Vector3[] = {"Vector3"};

/// Leaves an EMF reading at a point in the world through GhostInteraction's own EMF spawn: the hunt state's
/// start calls it on GhostAI.interaction at the ghost's position, on the master. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostInteraction_CreateAppearedEMF{
    .key = "GhostInteraction.CreateAppearedEMF", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostInteraction", .name = "CreateAppearedEMF",
    .params = GhostInteraction_Vector3};

/// An EMF reader's reading (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc EMF{.key = "EMF", .name = "EMF", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* EMFType_values[] = {"interaction", "objectMoved", "windowKnock", "lightSwitched"};

// Found by its values.
/// What made an EMF reading (nested in EMF, name obfuscated): interaction, objectMoved, windowKnock and
/// six more. Inferred: from the values' names.
inline constexpr pm::sdk::EntryDesc EMFType{.key = "EMFType", .owner = "EMF",
                                            .values = EMFType_values};

/// A thrown or moved object's reading, CreateThrowingEMF's default. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc EMFType_objectMoved{.key = "EMFType.objectMoved", .kind = pm::sdk::Kind::EnumValue,
                                                        .owner = "EMFType", .name = "objectMoved"};

/// One of the ghost's bodies (Assembly-CSharp, global namespace): its renderers, its Animator and its
/// sounds. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostModel{.key = "GhostModel", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "GhostModel", .assembly = "Assembly-CSharp", .ns = ""};

/// The model's Animator, which plays its walk: GhostModel's one Animator field. It may sit on a child
/// of the model. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostModel_animator{.key = "GhostModel.animator", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostModel",
                                                        .type = "Animator"};

/// The ghost's interactions and abilities (Assembly-CSharp, global namespace), a MonoBehaviour beside
/// its GhostAI. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity{.key = "GhostActivity", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .name = "GhostActivity", .assembly = "Assembly-CSharp", .ns = ""};

/// Whether the Raiju's electronics sensor finds running electronics near the ghost, which its hunt speed
/// reads: GhostActivity's one public bool. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostActivity_nearElectronics{
    .key = "GhostActivity.nearElectronics", .kind = pm::sdk::Kind::Field, .owner = "GhostActivity",
    .type = "bool", .access = pm::sdk::Visibility::Public};

/// The Raiju's electronics sensor, a RangeSensor. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc GhostActivity_electronicsSensor{
    .key = "GhostActivity.electronicsSensor", .kind = pm::sdk::Kind::Field, .owner = "GhostActivity",
    .name = "electronicsSensor"};

/// Pulses the electronics sensor (GhostActivity.electronicsSensor) through its virtual PulseAll, so that
/// GhostActivity.nearElectronics is fresh. The hunt's start (after it sends GhostAI.Hunting and locks the
/// exits) and the hunt's speed update run it on the master, and so do helpers of the idle state outside
/// hunts. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_UpdateElectronics{
    .key = "GhostActivity.UpdateElectronics", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostActivity", .name = "UpdateElectronics"};

inline constexpr const char* GhostActivity_bool[] = {"bool"};
// GhostActivity's private bool(bool) methods roll its interactions. Two that are called compare the
// behaviour type with the Mare's (the second roll keeps the Mare from flickering); only the light chance
// calls GhostAI.ChangeState, its look-alike look-alikes of it.
inline constexpr pm::sdk::Fact GhostActivity_lightChance_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Mare"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.ChangeState"}};

/// The ghost's light chance, run from its sensor's pulse handlers: its first draw is a roll of
/// Random.RandomRangeInt(0, 100) against 25, or 10 for the Deildegast; past it, it takes a light switch its
/// sensor detects, and a second draw (0 to 4) chooses GhostState.light or, unless a Mare, GhostState.flicker.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_lightChance{.key = "GhostActivity.lightChance",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostActivity",
                                                              .type = "bool", .params = GhostActivity_bool,
                                                              .facts = GhostActivity_lightChance_facts};

/// The ghost's prop chance: with props in its sensor, its first draw is a roll of
/// Random.RandomRangeInt(0, 100) against 100 (20 for the Poltergeist, 85 for the Deildegast); past it the
/// Poltergeist throws one and other types interact with one. True when it did. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_InteractWithARandomProp{
    .key = "GhostActivity.InteractWithARandomProp", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostActivity", .name = "InteractWithARandomProp",
    .params = GhostActivity_bool};

/// The ghost tries one interaction of its own. Inferred: from the method's name.
inline constexpr pm::sdk::EntryDesc GhostActivity_TryInteract{.key = "GhostActivity.TryInteract",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .owner = "GhostActivity", .name = "TryInteract"};

/// One pulse of the ghost's interaction sensor, NormalInteraction(force): it keeps `force`, gives the sensor
/// its box (GhostActivity.sensorBox) and pulses it, and the pulse's handlers run the light, door and prop
/// chances. TryInteract runs the same steps in its own body for every ghost but the Twins, whose roll calls
/// this; the voodoo doll's pins call it forced, and Playable Ghost calls it for a ghost player's aimed
/// interaction. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_NormalInteraction{
    .key = "GhostActivity.NormalInteraction", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostActivity", .name = "NormalInteraction",
    .params = GhostActivity_bool};

/// A door the ghost's door sensor (interactionSensor) detects now, at random, never the front door (in a
/// hunt only one marked as a hunt door), or null. Its callers are the idle door chance (doorChance) and
/// the hunt state. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_GetDoorToOpen{.key = "GhostActivity.GetDoorToOpen",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "GhostActivity", .name = "GetDoorToOpen"};

// GhostActivity's private bool() methods roll its interactions. Two call GetDoorToOpen or a twin of it:
// the door chance, which the sensor's pulse handlers call, and an uncalled copy of it.
inline constexpr pm::sdk::Fact GhostActivity_doorChance_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostActivity.GetDoorToOpen"}, {.is = pm::sdk::Fact::Is::Called}};

/// The ghost's idle door chance, run from its sensor's pulse handlers after TryInteract: by a roll and the
/// ghost's type it asks GetDoorToOpen for a door and, when one is found, runs GhostAI.ChangeState(door) on
/// the door's PhotonObjectInteract. True when it did. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_doorChance{.key = "GhostActivity.doorChance",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "GhostActivity",
                                                             .type = "bool", .facts = GhostActivity_doorChance_facts};

/// The ghost's interaction sensor, a RangeSensor: GhostActivity's one public RangeSensor (the other,
/// electronicsSensor, is private). TryInteract pulses it where the ghost stands; GetDoorToOpen and the
/// light switch and prop picks read its detections. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_interactionSensor{
    .key = "GhostActivity.interactionSensor", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostActivity",
    .type = "@RangeSensor", .access = pm::sdk::Visibility::Public};

// Four Vector3 fields; TryInteract reads one, the box it gives the sensor before each pulse.
inline constexpr pm::sdk::Fact GhostActivity_sensorBox_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostActivity.TryInteract"}};

/// The half extents of the interaction sensor's box, a Vector3: GhostActivity.Start sets them by the
/// map's size, and TryInteract passes them to RangeSensor.SetBoxShape before it pulses the sensor.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_sensorBox{.key = "GhostActivity.sensorBox",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "GhostActivity",
                                                            .type = "Vector3", .facts = GhostActivity_sensorBox_facts};

/// The ghost decides what its ability does: by its behaviour type, its difficulty and a roll it
/// changes the ghost's state, its last path to randomEvent only while LevelValues.eventsBlocked is
/// clear. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_GhostAbility{
    .key = "GhostActivity.GhostAbility", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostActivity", .name = "GhostAbility"};

/// The Prop the ghost throws: the Props its interaction sensor detects (each detected object's own
/// Prop, Sensor.GetDetectedComponents<Prop>), shuffled, the first that its test
/// (GhostActivity.throwable) passes; null for none. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_GetPropToThrow{.key = "GhostActivity.GetPropToThrow",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .confidence = pm::sdk::Confidence::Decompiled,
                                                                 .owner = "GhostActivity", .name = "GetPropToThrow"};

inline constexpr const char* GhostActivity_throwable_params[] = {"@Prop"};
inline constexpr pm::sdk::Fact GhostActivity_throwable_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostActivity.GetPropToThrow"}};

/// GetPropToThrow's test of a Prop: a live Prop whose PhotonObjectInteract allows a throw and is not
/// held, and which is not of a kind the ghost never throws. The bool(Prop) GetPropToThrow calls.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_throwable{.key = "GhostActivity.throwable",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "GhostActivity",
                                                            .type = "bool", .params = GhostActivity_throwable_params,
                                                            .facts = GhostActivity_throwable_facts};

/// The ghost's vision outside its hunt's own sight test: GhostAI's one LOSSensor field, a sensor
/// that tests its input RangeSensor's signals for line of sight. No game code pulses it directly: it
/// pulses by its own pulse routine, whose mode is in the ghost's prefab, and outside hunts it
/// detects nobody (seen live 2026-10-25). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostAI_vision{.key = "GhostAI.vision", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostAI",
                                                   .type = "@LOSSensor"};

/// The model's renderers, a Renderer[]: GhostModel.SetVisibility switches each one's enabled. Inferred:
/// from its name.
inline constexpr pm::sdk::EntryDesc GhostModel_renderers{.key = "GhostModel.renderers", .kind = pm::sdk::Kind::Field,
                                                         .owner = "GhostModel", .name = "myRends"};

/// The model's own sounds, a List<AudioClip>: GhostModel's one field of that type. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc GhostModel_sounds{.key = "GhostModel.sounds", .kind = pm::sdk::Kind::Field,
                                                      .owner = "GhostModel",
                                                      .type = "List<AudioClip>"};

/// Sets the model up for a GhostAI: the master rolls a number, and it sets the hunting sound (GhostAudio.SetHuntingSound)
/// and the model's original materials.
inline constexpr pm::sdk::EntryDesc GhostModel_Setup{
    .key = "GhostModel.Setup", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostModel", .name = "Setup"};

/// The Twins' second interaction, a coroutine (an IEnumerator to start on the GhostActivity): a second or
/// two later the twin pulses the interaction sensor around its own box. GhostActivity.TryInteract starts
/// it on Random.RandomRangeInt(0, 10) of 5 and up. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostActivity_TwinInteraction{
    .key = "GhostActivity.TwinInteraction", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostActivity", .name = "TwinInteraction", .params = GhostActivity_bool};

}  // namespace pm::map

/// @file
/// @brief The game API for GhostAI (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The ghost's state machine. `machine` below is GhostAI::stateMachine's value.
struct GhostStateMachine {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostStateMachine> type{};
  /// The state object it runs now: current.get(machine), such as a GhostWanderState or GhostHuntState. Valid for
  /// this frame. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostStateMachine_current, void*> current{};
};

/// The ghost's AI component, one per contract. `ghost` below is the GhostAI object.
struct GhostAI {
  /// The Obambo's mood, aggressive when set: obamboAggressive.get(ghost). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostAI_obamboAggressive, bool> obamboAggressive{};
  /// The props the Deildegast has moved, a HashSet (count them with pm::api::count): movedProps.get(ghost).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_movedProps, void*> movedProps{};
  /// Where it hears from, a Transform: head.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_head, void*> head{};
  /// A Banshee's target, or null: bansheeTarget.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_bansheeTarget, void*> bansheeTarget{};
  /// Whether it may wander: canWander.get(ghost). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostAI_canWander, bool> canWander{};
  /// Whether its roll and wanders go to its favourite room instead: stayHome.get(ghost). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostAI_stayHome, bool> stayHome{};
  /// Whether the idle roll's actions are not held back (the Tower card): interactionBoost.get(ghost).
  /// Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostAI_interactionBoost, bool> interactionBoost{};
  /// Its hearing, a GhostHearing: hearing.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_hearing, void*> hearing{};
  /// The parts of players its sight keeps, nearest first, a List<PlayerLOSTarget>: sightTargets.get(ghost).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_sightTargets, void*> sightTargets{};
  /// The GhostAI type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null
  /// and false.
  static constexpr pm::api::Type<pm::map::GhostAI> type{};
  /// The ghost's current state, a GhostState number: state.get(ghost), state.set(ghost, n).
  /// ChangeState also starts the new state's behaviour. Unresolved: reads 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_state, int32_t> state{};
  /// The ghost's NavMeshAgent, a managed reference: agent.get(ghost); agent.set goes through the GC
  /// write barrier. Unresolved: reads null, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_agent, void*> agent{};
  /// Its state machine: stateMachine.get(ghost), for GhostStateMachine::current. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_stateMachine, void*> stateMachine{};
  /// Whether a hunt is on: isHunting.get(ghost). Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_isHunting, bool> isHunting{};
  /// Set when a hunt starts, cleared by Awake (the post-hunt cooldown): postHuntCooldown.get(ghost).
  /// Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_postHuntCooldown, bool> postHuntCooldown{};
  /// The ghost's GhostInfo, a managed reference: info.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_info, void*> info{};
  /// The ghost's base hunt speed in metres a second (1.7 from Awake): baseSpeed.get(ghost).
  /// Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostAI_baseSpeed, float> baseSpeed{};
  /// Sets the ghost up from the host's traits: Init(ghost, traits), `traits` pointing at a GhostTraits
  /// (passed by address, as the x64 convention passes a struct). A mod usually hooks it. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_Init, void(void*, void*)> Init{};
  /// Unity's Awake: Awake(ghost). The game calls it once; a mod usually hooks it rather than calls
  /// it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_Awake, void(void*)> Awake{};
  /// The Photon RPC that starts (true) or ends (false) a hunt on this client, its body run directly:
  /// Hunting(ghost, start, number, info), `info` pointing at a PhotonMessageInfo. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_Hunting, void(void*, bool, int32_t, const void*)> Hunting{};
  /// Changes the ghost's state: ChangeState(ghost, state, interact, force), with a GhostState number,
  /// a PhotonObjectInteract or null, and whether to force the change. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_ChangeState, void(void*, int32_t, void*, bool)> ChangeState{};
  /// Ends an appearance on every client (an RPC) when the ghost is showing itself: UnAppear(ghost).
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_UnAppear, void(void*)> UnAppear{};
  /// Turns the ghost to face a player, on this client only: LookAtPlayer(ghost, player). Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_LookAtPlayer, void(void*, void*)> LookAtPlayer{};
  /// Stops the ghost wandering for `seconds`: TemporarilyStopWander(ghost, seconds). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_TemporarilyStopWander, void(void*, float)> TemporarilyStopWander{};
  /// The Banshee's new target, other than `player`: SetNewBansheeTarget(ghost, player), on the master.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_SetNewBansheeTarget, void(void*, void*)> SetNewBansheeTarget{};
  /// Shows the ghost on every client unless it shows itself already: Appear(ghost, form), -1 for the
  /// game's pick of form. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_Appear, void(void*, int32_t)> Appear{};
  /// The RPC that shows or hides the ghost, its body run directly: MakeGhostAppear(ghost, show, form,
  /// info), `info` pointing at a PhotonMessageInfo. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostAI_MakeGhostAppear, void(void*, bool, int32_t, const void*)>
      MakeGhostAppear{};
  /// Makes the coroutine that holds the ghost's hunts off for a while: StopGhostFromHunting(ghost), an
  /// IEnumerator its caller starts. A mod usually hooks it. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostAI_StopGhostFromHunting, void*(void*)> StopGhostFromHunting{};
  /// Whether the ghost shows itself: isAppeared.get(ghost). Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_isAppeared, bool> isAppeared{};
  /// Whether the ghost's hunts are held off (incense, a Monkey Paw wish): huntsHeldOff.get(ghost).
  /// Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_huntsHeldOff, bool> huntsHeldOff{};
  /// The incense that smudged it in this hunt, a WhiteSage, or null: smudge.get(ghost). Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::GhostAI_smudge, void*> smudge{};
  /// The Onryo's count of flames blown out; at 3 the idle state starts a hunt (see
  /// GhostIdleState.Execute): flamesBlownOut.get(ghost). Unresolved: reads 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostAI_flamesBlownOut, int32_t> flamesBlownOut{};
  /// Its GhostActivity: activity.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_activity, void*> activity{};
  /// The model it wears now, a GhostModel: model.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_model, void*> model{};
  /// Its GhostInteraction: interaction.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_interaction, void*> interaction{};
  /// Its vision, a LOSSensor: vision.get(ghost). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostAI_vision, void*> vision{};
};

/// What the ghost leaves where it touches things. `interaction` below is the GhostInteraction.
struct GhostInteraction {
  /// CreateThrowingEMF(interaction, objectInteract, emfType, flag, range). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostInteraction_CreateThrowingEMF, void(void*, void*, int32_t, bool, float)>
      CreateThrowingEMF{};
};

/// What made an EMF reading.
namespace EMFType {
/// objectMoved(): a thrown or moved object. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::EMFType_objectMoved> objectMoved{};
}  // namespace EMFType

/// One of the ghost's bodies. `model` below is the GhostModel.
struct GhostModel {
  /// The type: is(obj), and the field type of GhostAI's model arrays (GhostAI::type.fields_of).
  /// Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostModel> type{};
  /// The model's Animator: animator.get(model). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostModel_animator, void*> animator{};
  /// Its renderers, a Renderer[]: renderers.get(model). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostModel_renderers, void*> renderers{};
  /// Its own sounds, a List<AudioClip>: sounds.get(model). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostModel_sounds, void*> sounds{};
};

/// The ghost's interactions. `activity` below is the GhostActivity.
struct GhostActivity {
  /// The light chance: lightChance(activity, force). A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostActivity_lightChance, bool(void*, bool)> lightChance{};
  /// The prop chance: InteractWithARandomProp(activity, force). A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostActivity_InteractWithARandomProp, bool(void*, bool)>
      InteractWithARandomProp{};
  /// Whether the Raiju's sensor finds running electronics: nearElectronics.get(activity). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostActivity_nearElectronics, bool> nearElectronics{};
  /// The Raiju's electronics sensor, a RangeSensor: electronicsSensor.get(activity). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostActivity_electronicsSensor, void*> electronicsSensor{};
  /// UpdateElectronics(activity): pulses the electronics sensor, as a hunt's start and the idle state do. A mod
  /// usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostActivity_UpdateElectronics, void(void*)> UpdateElectronics{};
  /// One interaction attempt of the ghost's own: TryInteract(activity). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostActivity_TryInteract, void(void*)> TryInteract{};
  /// One pulse of its interaction sensor: NormalInteraction(activity, force). A mod usually hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostActivity_NormalInteraction, void(void*, bool)> NormalInteraction{};
  /// GetDoorToOpen(activity): a door, or null. A mod usually hooks it. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostActivity_GetDoorToOpen, void*(void*)> GetDoorToOpen{};
  /// The idle door chance: doorChance(activity). A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostActivity_doorChance, bool(void*)> doorChance{};
  /// Its interaction sensor, a RangeSensor: interactionSensor.get(activity). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostActivity_interactionSensor, void*> interactionSensor{};
  /// The sensor's box half extents: sensorBox.get(activity). Unresolved: zeros.
  static constexpr pm::api::Field<pm::map::GhostActivity_sensorBox, Vector3> sensorBox{};
  /// GetPropToThrow(activity): the Prop the ghost would throw now, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostActivity_GetPropToThrow, void*(void*)> GetPropToThrow{};
  /// throwable(activity, prop): GetPropToThrow's test of one Prop. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostActivity_throwable, bool(void*, void*)> throwable{};
  /// TwinInteraction(activity, flag): the Twins' second interaction, an IEnumerator to start with
  /// phasmo::MonoBehaviour::StartCoroutine. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GhostActivity_TwinInteraction, void*(void*, bool)> TwinInteraction{};
};

/// The ghost's states: the numbers GhostAI::state holds and GhostAI::ChangeState takes.
namespace GhostState {
/// The GhostState enum type: klass(), is(obj), type_object() and make_array(n). Unresolved: null and
/// false.
inline constexpr pm::api::Type<pm::map::GhostState> type{};
/// The ghost stands still: idle(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_idle> idle{};
/// The ghost wanders: wander(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_wander> wander{};
/// The ghost hunts: hunting(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_hunting> hunting{};
/// The state GhostActivity sets to act on an object: appear(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_appear> appear{};
/// The ghost's events: randomEvent(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_randomEvent> randomEvent{};
/// The other states by their names: favouriteRoom(), light(), ... Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GhostState_favouriteRoom> favouriteRoom{};
inline constexpr pm::api::EnumValue<pm::map::GhostState_light> light{};  ///< light(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_door> door{};  ///< door(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_throwing> throwing{};  ///< throwing(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_fusebox> fusebox{};  ///< fusebox(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_doorKnock> doorKnock{};  ///< doorKnock(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_windowKnock> windowKnock{};  ///< windowKnock(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_carAlarm> carAlarm{};  ///< carAlarm(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_flicker> flicker{};  ///< flicker(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_cctv> cctv{};  ///< cctv(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_dots> dots{};  ///< dots(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_GhostAbility> GhostAbility{};  ///< GhostAbility(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_mannequin> mannequin{};  ///< mannequin(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_teleportObject> teleportObject{};  ///< teleportObject(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_interact> interact{};  ///< interact(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_summoningCircle> summoningCircle{};  ///< summoningCircle(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_musicBox> musicBox{};  ///< musicBox(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::GhostState_ignite> ignite{};  ///< ignite(): the value's number in this game build
}  // namespace GhostState

/// @}
}  // namespace phasmo
