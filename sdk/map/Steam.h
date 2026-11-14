#pragma once

// Steam's achievements as the game reaches them: Steamworks.NET's SteamUserStats (com.rlabrecque.steamworks.net,
// namespace Steamworks), trimmed in this build to these members and a display-text reader, so every achievement that
// reaches Steam passes through them (sdk/systems/achievements.md). Not obfuscated, so every entry is by real name. Notes
// read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// Steam's user stats, as the game's copy of Steamworks.NET gives them: a static class. The build keeps no call that sets
/// a stat, and no other Steam call that can unlock an achievement. Decompiled.
inline constexpr pm::sdk::EntryDesc SteamUserStats{.key = "SteamUserStats", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "SteamUserStats",
                                                   .assembly = "com.rlabrecque.steamworks.net", .ns = "Steamworks"};

/// Marks an achievement, by its Steam name, as unlocked: true when Steam took it. Nothing reaches the player's account
/// until StoreStats. Static. Every unlock the game makes calls it and then StoreStats. Decompiled.
inline constexpr pm::sdk::EntryDesc SteamUserStats_SetAchievement{
    .key = "SteamUserStats.SetAchievement", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "SteamUserStats",
    .name = "SetAchievement"};

/// Sends what SetAchievement marked to Steam: true when the call was made. Static. Only the game's unlock calls it, right
/// after SetAchievement. Decompiled.
inline constexpr pm::sdk::EntryDesc SteamUserStats_StoreStats{
    .key = "SteamUserStats.StoreStats", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "SteamUserStats", .name = "StoreStats"};

/// Reads whether Steam holds an achievement as unlocked, into its second argument: true when Steam could say. Static.
/// Reads only. Decompiled.
inline constexpr pm::sdk::EntryDesc SteamUserStats_GetAchievement{
    .key = "SteamUserStats.GetAchievement", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "SteamUserStats",
    .name = "GetAchievement"};

/// Asks Steam for this player's achievements: true when the call was made. The answer comes later, as a callback, and the
/// game then rereads every achievement's unlocked flag from GetAchievement. Static. Reads only. Decompiled.
inline constexpr pm::sdk::EntryDesc SteamUserStats_RequestCurrentStats{
    .key = "SteamUserStats.RequestCurrentStats", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "SteamUserStats",
    .name = "RequestCurrentStats"};

}  // namespace pm::map

/// @file
/// @brief The game API for Steam's achievements (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// Steam's achievements, as the game's unlock reaches them (sdk/systems/achievements.md). The loader leaves them alone.
struct SteamUserStats {
  /// SetAchievement(name): marks it unlocked, name an il2cpp string. Unresolved: false.
  static constexpr pm::api::Method<pm::map::SteamUserStats_SetAchievement, bool(pm::api::Object)> SetAchievement{};
  /// StoreStats(): sends what was marked. Unresolved: false.
  static constexpr pm::api::Method<pm::map::SteamUserStats_StoreStats, bool()> StoreStats{};
  /// GetAchievement(name, unlocked): reads one into *unlocked. Unresolved: false.
  static constexpr pm::api::Method<pm::map::SteamUserStats_GetAchievement, bool(pm::api::Object, bool*)> GetAchievement{};
  /// RequestCurrentStats(): asks Steam for the player's achievements. Unresolved: false.
  static constexpr pm::api::Method<pm::map::SteamUserStats_RequestCurrentStats, bool()> RequestCurrentStats{};
};

/// @}
}  // namespace phasmo
