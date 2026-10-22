#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "flame_net.h"

// The Hydra's rules, kept free of game calls so they can be tested offline. Each smudge in a hunt
// grows a false head (a harmless, silent apparition), at most three; they vanish with the hunt. Some of
// its appearances outside hunts show a second apparition beside it that fades.
namespace pg::custom {

constexpr int kHydraMaxHeads = 3;
constexpr float kHydraWander = 8.0f;        // metres: how far a false head walks before choosing again
constexpr float kHydraDoubleChance = 0.5f;  // of its appearances, the share that show a second apparition
constexpr float kHydraDoubleSide = 1.5f;    // metres to the side of the ghost the second one stands
constexpr float kHydraFade = 2.0f;          // seconds the second one takes to fade away
constexpr float kHydraFlicker = 8.0f;       // its flickers per second while fading

/** False heads after `smudges` smudges in one hunt. */
inline int hydra_heads(int smudges) { return std::clamp(smudges, 0, kHydraMaxHeads); }

/** Whether the fading second apparition shows `seconds` after it appeared (flickering out). */
inline bool hydra_double_visible(float seconds) {
  if (seconds < 0 || seconds >= kHydraFade) return false;
  const float duty = 1 - seconds / kHydraFade;
  return std::fmod(seconds * kHydraFlicker, 1.0f) < duty;
}

struct Head {
  float x, y, z, yaw;
};

inline std::vector<uint8_t> encode_heads(const std::vector<Head>& heads) {
  std::vector<uint8_t> out = net_msg::header(net_msg::kDecoys);
  const size_t n = std::min<size_t>(heads.size(), kHydraMaxHeads);
  out.push_back(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i)
    for (float v : {heads[i].x, heads[i].y, heads[i].z, heads[i].yaw}) net_msg::put(out, v);
  return out;
}

inline bool decode_heads(const uint8_t* data, size_t size, std::vector<Head>& heads) {
  heads.clear();
  if (net_msg::kind_of(data, size) != net_msg::kDecoys || size < 5 || data[4] > kHydraMaxHeads) return false;
  size_t at = 5;
  for (int i = 0; i < data[4]; ++i) {
    Head h{};
    if (!net_msg::take(data, size, at, h.x) || !net_msg::take(data, size, at, h.y) || !net_msg::take(data, size, at, h.z) ||
        !net_msg::take(data, size, at, h.yaw))
      return false;
    heads.push_back(h);
  }
  return at == size;
}

inline std::vector<uint8_t> encode_double(const Head& at) {
  std::vector<uint8_t> out = net_msg::header(net_msg::kDouble);
  for (float v : {at.x, at.y, at.z, at.yaw}) net_msg::put(out, v);
  return out;
}

inline bool decode_double(const uint8_t* data, size_t size, Head& h) {
  size_t at = 4;
  return net_msg::kind_of(data, size) == net_msg::kDouble && net_msg::take(data, size, at, h.x) &&
         net_msg::take(data, size, at, h.y) && net_msg::take(data, size, at, h.z) && net_msg::take(data, size, at, h.yaw) &&
         at == size;
}

}  // namespace pg::custom
