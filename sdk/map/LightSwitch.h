#pragma once

// LightSwitch: a room's light switch, which the players and the ghost use. Its type and its methods
// keep their real names; two of its bools are obfuscated and found by facts. Notes read from
// decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* LightSwitch_UseLightSwitch_params[] = {"bool", "bool", "bool"};
// UseLightSwitch reads four bools of the switch: isFuseboxSwitch and the on flag, which get_IsSwitchOn
// also reads, one it also stores, and the one that refuses a ghost's use.
inline constexpr pm::sdk::Fact LightSwitch_ghostUseRefused_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LightSwitch.UseLightSwitch"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "LightSwitch.UseLightSwitch", .negate = true},
    {.is = pm::sdk::Fact::Is::Loads, .method = "LightSwitch.get_IsSwitchOn", .negate = true}};

/// A light switch (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch{.key = "LightSwitch", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "LightSwitch", .assembly = "Assembly-CSharp", .ns = ""};

/// Whether the switch is the fuse box's: UseLightSwitch returns at once while it is set, and
/// get_IsSwitchOn answers the on flag only while it is clear. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_isFuseboxSwitch{
    .key = "LightSwitch.isFuseboxSwitch", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LightSwitch", .name = "isFuseboxSwitch"};

/// While set, UseLightSwitch refuses a use whose third argument is set (the ghost's use). No method of
/// LightSwitch's own stores it. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_ghostUseRefused{
    .key = "LightSwitch.ghostUseRefused", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LightSwitch", .type = "bool", .facts = LightSwitch_ghostUseRefused_facts};

/// Whether the switch is on: false for the fuse box's, else its on flag. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_get_IsSwitchOn{
    .key = "LightSwitch.get_IsSwitchOn", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LightSwitch", .name = "get_IsSwitchOn"};

/// Whether the switch's light is lit: get_IsSwitchOn and the switch's fuse not blown (LightSwitch's own
/// get_FuseBlown). LevelRoom.AreRoomLightsOn reads it (sdk/systems/ghost-ai.md). Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_get_isOn{
    .key = "LightSwitch.get_isOn", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LightSwitch", .name = "get_isOn"};

/// Uses the switch, its arguments read in the decompiled code as: the use's own flag, a flag that lets a
/// dead local player use it, and the ghost's use. It returns at once for the fuse box's switch, for a
/// ghost's use while ghostUseRefused is set, while the ghost hunts, and for a dead local player; it
/// toggles the switch on every client (an RPC in a room). Then, while a flag of the switch is set and
/// its second argument is clear, when the ghost's behaviour type is 6, it restarts a coroutine of the
/// switch with StartCoroutine, whose first step (its MoveNext) rolls Random.RandomRangeInt(0, 7) while
/// the switch is on and its fuse not blown, and on a 1 waits and may switch it off again. Unity runs a
/// coroutine's first step inside StartCoroutine, so that roll happens inside this call. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_UseLightSwitch{
    .key = "LightSwitch.UseLightSwitch", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LightSwitch", .name = "UseLightSwitch", .params = LightSwitch_UseLightSwitch_params};

inline constexpr const char* LightSwitch_Use_params[] = {"bool", "bool", "bool", "bool"};

/// What every client runs for each use of the switch, the RPC UseLightSwitch sends: its arguments
/// read as on, prints, byGhost and counted. A player's use (its interact handler) clears every
/// flag; the ghost's uses set byGhost; the switch's own sensor handler clears byGhost but sets
/// counted, which adds a ghost interaction to LevelStats. Decompiled (2026-10-22).
inline constexpr pm::sdk::EntryDesc LightSwitch_Use{.key = "LightSwitch.Use", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "LightSwitch", .name = "Use",
                                                    .params = LightSwitch_Use_params};

/// The room the switch is in, a LevelRoom: LightSwitch's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_room{.key = "LightSwitch.room", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "LightSwitch",
                                                     .type = "@LevelRoom"};

/// A light a switch controls (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc LightSource{.key = "LightSource", .name = "LightSource", .assembly = "Assembly-CSharp",
                                                .ns = ""};

/// The light's Unity Light. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc LightSource_lightSource{.key = "LightSource.lightSource", .kind = pm::sdk::Kind::Field,
                                                            .owner = "LightSource", .name = "lightSource"};

/// The lights the switch controls, a List<LightSource>: LightSwitch's one field of that type.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LightSwitch_sources{.key = "LightSwitch.sources", .kind = pm::sdk::Kind::Field,
                                                        .owner = "LightSwitch",
                                                        .type = "List<@LightSource>"};

/// The flicker RPC every client runs: it starts a visual flicker coroutine on the switch's lights
/// and does nothing else. A ghost's flicker interaction sends it. Decompiled.
inline constexpr pm::sdk::EntryDesc LightSwitch_FlickerNetworked{.key = "LightSwitch.FlickerNetworked",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .confidence = pm::sdk::Confidence::Decompiled,
                                                                 .owner = "LightSwitch", .name = "FlickerNetworked"};

}  // namespace pm::map

/// @file
/// @brief The game API for light switches (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (false) without touching the game.
namespace phasmo {
/// @addtogroup rooms
/// @{

/// A light switch. `light` below is the LightSwitch.
struct LightSwitch {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::LightSwitch> type{};
  /// Whether it is the fuse box's: isFuseboxSwitch.get(light). Unresolved: reads false.
  static constexpr pm::api::Field<pm::map::LightSwitch_isFuseboxSwitch, bool> isFuseboxSwitch{};
  /// Whether it refuses the ghost's use now: ghostUseRefused.get(light). Unresolved: reads false.
  static constexpr pm::api::Field<pm::map::LightSwitch_ghostUseRefused, bool> ghostUseRefused{};
  /// get_IsSwitchOn(light). Unresolved: false.
  static constexpr pm::api::Method<pm::map::LightSwitch_get_IsSwitchOn, bool(void*)> get_IsSwitchOn{};
  /// UseLightSwitch(light, use, deadMayUse, byGhost). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LightSwitch_UseLightSwitch, void(void*, bool, bool, bool)> UseLightSwitch{};
  /// Use(lightSwitch, on, prints, byGhost, counted). A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LightSwitch_Use, void(void*, bool, bool, bool, bool)> Use{};
  /// Its room: room.get(lightSwitch). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LightSwitch_room, void*> room{};
  /// The lights it controls, a List<LightSource>: sources.get(lightSwitch). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LightSwitch_sources, void*> sources{};
  /// The flicker RPC, FlickerNetworked(lightSwitch, &info). A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::LightSwitch_FlickerNetworked, void(void*, const void*)> FlickerNetworked{};
};

/// A light a switch controls. `source` below is one.
struct LightSource {
  /// Its Unity Light: lightSource.get(source). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LightSource_lightSource, void*> lightSource{};
};

/// @}
}  // namespace phasmo
