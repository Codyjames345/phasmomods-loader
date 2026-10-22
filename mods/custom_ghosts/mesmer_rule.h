#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// The Mesmer's rules, kept free of game calls so they can be tested offline. Each investigator sees
// its appearances differently (another form, standing elsewhere); cameras see it as it is; the truck's
// monitor shows the team saner than they are; and the lights in its room dim and brighten slowly.
namespace pg::custom {

constexpr float kMesmerLie = 20.0f;        // percent of sanity the monitor adds
constexpr float kMesmerOffsetMin = 1.5f;   // metres from where it really stands, at least
constexpr float kMesmerOffsetMax = 3.0f;   // and at most
constexpr float kMesmerDimPeriod = 20.0f;  // seconds for its room's lights to dim and come back
constexpr float kMesmerDimLowest = 0.55f;  // the share of their light at the dimmest
constexpr int kMesmerFakeLayer = 3;        // the one layer the game leaves unnamed: only a player's eye draws it

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
  uint32_t h = appearance * 0x9E3779B1u ^ (actor + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15;
  h *= 0xC2B2AE3Du;
  return h ^ (h >> 13);
}

/** Which of `count` forms a viewer sees, never `real` (when there is another). */
inline int mesmer_form(uint32_t seed, int count, int real) {
  if (count <= 1) return 0;
  int pick = static_cast<int>(seed % static_cast<uint32_t>(count - 1));
  return pick >= real && real >= 0 ? pick + 1 : pick;
}

/** Where a viewer sees it: an offset (x, z) from where it stands. */
inline void mesmer_offset(uint32_t seed, float& dx, float& dz) {
  const float angle = static_cast<float>(seed % 3600) / 3600.0f * 6.2831853f;
  const float r = kMesmerOffsetMin + (kMesmerOffsetMax - kMesmerOffsetMin) * static_cast<float>((seed / 3600) % 100) / 100.0f;
  dx = std::cos(angle) * r;
  dz = std::sin(angle) * r;
}

/** The insanity (0..100, 100 the most insane) the monitor is shown for a true `insanity`. */
inline float mesmer_shown_insanity(float insanity) { return std::max(0.0f, insanity - kMesmerLie); }

/** The share of their light its room's lights give `seconds` into the contract. */
inline float mesmer_dim(float seconds) {
  const float wave = 0.5f + 0.5f * std::cos(seconds / kMesmerDimPeriod * 6.2831853f);
  return kMesmerDimLowest + (1 - kMesmerDimLowest) * wave;
}

}  // namespace pg::custom
