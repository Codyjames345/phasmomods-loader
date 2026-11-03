#pragma once

// The Gremlin's rules, kept free of game calls so they can be tested offline. It switches off
// electronic equipment (gremlin.h electronic) that has been left running, never anything a player
// holds or stands beside.
namespace pm::custom {

constexpr float kGremlinGuard = 2.0f;    // metres: equipment this close to a living player is safe
constexpr float kGremlinChance = 0.5f;   // of its interactions with such equipment in its room, the share it sabotages
constexpr float kGremlinReach = 10.0f;   // metres: how far its player's "Short out" reaches

/** Whether a device that is `on`, `nearestPlayer` metres from the closest living player, may be switched off. */
inline bool gremlin_may_sabotage(bool on, float nearestPlayer) { return on && nearestPlayer > kGremlinGuard; }

}  // namespace pm::custom
