#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

// The Mesmer's rules, kept free of game calls so they can be tested offline. Each investigator sees
// it as one other form through the whole contract, a little way off in its appearances; cameras see
// it as it is; the truck's monitor shows the team saner than they are; and the lights in its room dim
// and brighten slowly.
namespace pm::custom {

constexpr float kMesmerLie = 20.0f;        // percent of sanity the monitor adds
constexpr float kMesmerOffsetMin = 1.5f;   // metres from where it really stands, at least
constexpr float kMesmerOffsetMax = 3.0f;   // and at most
constexpr float kMesmerDimPeriod = 20.0f;  // seconds for its room's lights to dim and come back
constexpr float kMesmerDimLowest = 0.55f;  // the share of their light at the dimmest
constexpr float kMesmerWalkSpeed = 0.2f;   // metres a second the ghost must cover to be walking, not settling
// The one layer the game leaves unnamed (the scene's TagManager): only a player's eye draws it.
constexpr int kMesmerFakeLayer = 3;  // pm:not-game-data: a Unity layer with no name, so no Layer entry can find it

/** A culling-mask bit for a Unity layer; none for a layer out of range. */
inline uint32_t mesmer_layer_bit(int layer) { return layer >= 0 && layer < 32 ? 1u << layer : 0; }

/**
 * Whether the true form, drawn on `layers` (a mask), can be taken out of a player's eye by layer:
 * not on Default (the scene) nor on the fake's own layer, and on some layer at all.
 */
inline bool mesmer_layers_safe(uint32_t layers) {
  return layers && !(layers & mesmer_layer_bit(0)) && !(layers & mesmer_layer_bit(kMesmerFakeLayer));
}

/** A well-mixed number from an appearance's count and the viewing player's actor. */
inline uint32_t mesmer_seed(uint32_t appearance, uint32_t actor) {
  uint32_t h = appearance * 0x9E3779B1u ^ (actor + 0x7F4A7C15u) * 0x85EBCA77u;  // pm:not-game-data: hash mixing
  h ^= h >> 15;
  h *= 0xC2B2AE3Du;  // pm:not-game-data: hash mixing
  return h ^ (h >> 13);
}

/** Which of `count` forms a viewer sees, never `real` (when there is another). */
inline int mesmer_form(uint32_t seed, int count, int real) {
  if (count <= 1) return 0;
  int pick = static_cast<int>(seed % static_cast<uint32_t>(count - 1));
  return pick >= real && real >= 0 ? pick + 1 : pick;
}

/**
 * A number every client of a contract shares and the next contract changes, from the ghost's name (GhostInfo.name, which
 * GhostInfo.SyncValuesNetworked builds on every client from the same two numbers). FNV-1a.
 */
inline uint32_t mesmer_contract_seed(std::string_view ghostName) {
  uint32_t h = 2166136261u;  // pm:not-game-data: hash mixing
  for (const char c : ghostName) {
    h ^= static_cast<uint8_t>(c);
    h *= 16777619u;  // pm:not-game-data: hash mixing
  }
  return h;
}

/**
 * The one form a viewer sees in every appearance and hunt of a contract, never `real` (when there is another). Viewers whose
 * actors differ by less than `count - 1` see different forms, so in practice no two players in a room agree.
 */
inline int mesmer_viewer_form(uint32_t contract, uint32_t actor, int count, int real) {
  return mesmer_form(contract + actor, count, real);
}

/** Whether the ghost is walking, having covered `moved` metres in `seconds`: no time measured says no. */
inline bool mesmer_walking(float moved, float seconds) { return seconds > 0 && moved > kMesmerWalkSpeed * seconds; }

/** Where a viewer sees it: an offset (x, z) from where it stands. */
inline void mesmer_offset(uint32_t seed, float& dx, float& dz) {
  const float angle = static_cast<float>(seed % 3600) / 3600.0f * 6.2831853f;
  const float r = kMesmerOffsetMin + (kMesmerOffsetMax - kMesmerOffsetMin) * static_cast<float>((seed / 3600) % 100) / 100.0f;
  dx = std::cos(angle) * r;
  dz = std::sin(angle) * r;
}

/** The yaw in degrees that turns it at (x, z) to face a viewer's eye at (eyeX, eyeZ): 0 looks along +z, 90 along +x. */
inline float mesmer_facing(float x, float z, float eyeX, float eyeZ) {
  return std::atan2(eyeX - x, eyeZ - z) * 180.0f / 3.14159265f;
}

/** The insanity (0..100, 100 the most insane) the monitor is shown for a true `insanity`. */
inline float mesmer_shown_insanity(float insanity) { return std::max(0.0f, insanity - kMesmerLie); }

/** The share of their light its room's lights give `seconds` into the contract. */
inline float mesmer_dim(float seconds) {
  const float wave = 0.5f + 0.5f * std::cos(seconds / kMesmerDimPeriod * 6.2831853f);
  return kMesmerDimLowest + (1 - kMesmerDimLowest) * wave;
}

}  // namespace pm::custom
