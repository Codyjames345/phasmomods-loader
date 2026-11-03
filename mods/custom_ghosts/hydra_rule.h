#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "flame_net.h"

// The Hydra's rules, kept free of game calls so they can be tested offline. Each smudge in a hunt
// grows a false head (a harmless, silent apparition that throws things as the hunting ghost does), at
// most three; they vanish with the hunt. Some of
// its appearances outside hunts show a second apparition beside it that fades.
namespace pm::custom {

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

// A false head throws as the hunt throws for the ghost (the hunt update, and its copy):
//  - the tick: the hunt state's throw timer (GhostHuntState.throwTimer) counts down by deltaTime and, run
//    out, is set back up; the mod reads the timer, so the interval is the game's;
//  - the object: then GhostActivity.GetPropToThrow takes the Props its
//    interaction sensor (GhostActivity.interactionSensor, a RangeSensor) detects
//    (Sensor.GetDetectedComponents<Prop>: each signal object's own Prop), shuffles them
//    (OrderBy a new System.Random) and returns the first that its test (GhostActivity.throwable)
//    passes. Each head has its own RangeSensor, set up as the ghost's (its settings, detection mode and
//    signal filters copied) and posed on the head as the ghost's is on the ghost, pulsed on the tick as the
//    hunt pulses the ghost's;
//  - the roll: only when an object was found, and unless the hunt's ghost type (GhostHuntState.ghostType)
//    is the Poltergeist, RandomRangeInt(0, 2) == 1 means no throw this tick (the RandomRangeInt call that
//    returns past the type test). The mod rolls before it picks; the roll is independent of
//    the pick, so the odds of a throw are the same;
//  - the throw: Prop.TryThrow(true, true), as the hunt calls it; its direction, EMF and the
//    rest are the game's.
// One rule is the mod's own, approved by the user: one object is thrown once a tick, so a head skips an
// object another head or the ghost itself has thrown that tick.
// The hunt's roll, RandomRangeInt(0, 2), and the roll that means no throw this tick.
constexpr int kHuntThrowRollEnd = 2;  // pm:not-game-data: an immediate in the hunt update's code
constexpr int kHuntThrowSkip = 1;     // pm:not-game-data: an immediate in the hunt update's code

/** Whether this read of the hunt's throw timer is a tick: the same hunt's timer, set back up after running out. */
inline bool hydra_throw_tick(bool sameHunt, float before, float now) { return sameHunt && now > before; }

/**
 * Whether a tick throws for a hunt of `huntGhostType`, given the hunt's roll `roll`; `poltergeist` is the
 * game's number for the Poltergeist (GhostType, read from the game), whose throws the roll never holds back.
 */
inline bool hydra_throw_rolls(int huntGhostType, int poltergeist, int roll) {
  return huntGhostType == poltergeist || roll != kHuntThrowSkip;
}

/**
 * A head's pick on a tick: when its roll throws, the index of the first of the `shuffled` detected objects
 * not `taken` this tick that the game's test `passes` (asked in order, no further than the first that
 * passes, as GetPropToThrow asks); -1 for none.
 */
template <class Passes>
int hydra_head_pick(bool rollThrows, const std::vector<void*>& shuffled, const std::vector<void*>& taken, Passes passes) {
  if (!rollThrows) return -1;
  for (size_t i = 0; i < shuffled.size(); ++i) {
    if (!shuffled[i] || std::find(taken.begin(), taken.end(), shuffled[i]) != taken.end()) continue;
    if (passes(shuffled[i])) return static_cast<int>(i);
  }
  return -1;
}

struct Vec3 {
  float x, y, z;
};
struct Quat {
  float x, y, z, w;
};
struct SensorPose {
  Vec3 at;
  Quat rotation;
};

inline Quat quat_mul(Quat a, Quat b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat quat_inverse(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }  // a unit rotation's
inline Vec3 quat_rotate(Quat q, Vec3 v) {
  const Quat r = quat_mul(quat_mul(q, {v.x, v.y, v.z, 0}), quat_inverse(q));
  return {r.x, r.y, r.z};
}

/**
 * Where a head's sensor goes: placed and turned on the head (at `headAt`, turned `headRot`) as the ghost's
 * sensor (at `sensorAt`, turned `sensorRot`) is on the ghost (at `ghostAt`, turned `ghostRot`).
 */
inline SensorPose hydra_sensor_pose(Vec3 ghostAt, Quat ghostRot, Vec3 sensorAt, Quat sensorRot, Vec3 headAt, Quat headRot) {
  const Quat toGhost = quat_inverse(ghostRot);
  const Vec3 local = quat_rotate(toGhost, {sensorAt.x - ghostAt.x, sensorAt.y - ghostAt.y, sensorAt.z - ghostAt.z});
  const Vec3 offset = quat_rotate(headRot, local);
  return {{headAt.x + offset.x, headAt.y + offset.y, headAt.z + offset.z}, quat_mul(headRot, quat_mul(toGhost, sensorRot))};
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

}  // namespace pm::custom
