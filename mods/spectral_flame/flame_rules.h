#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// The Spectral Flame rules: how blue a flame is, what that costs, and the message that tells every
// client. Pure arithmetic, free of game calls, so it is tested offline (tests/spectral_flame_test.cpp).
//
// A flame's blueness (0..1) builds while the ghost spends time in the flame's room, hunting or not,
// and drops back to yellow at half that speed otherwise. It answers where the ghost lingers, never
// where it is now: every second counts, so short visits add up, but a ghost walking past changes
// little. Any ghost takes a flame to the false-positive cap, a pale blue; only a ghost that gives
// Spectral Flame takes it beyond, to a deep blue. A false flame is half a true one in everything, so
// it is always half as blue as a true flame would be: players who know the speed tell them apart
// before the minute is up. A bluer flame is hotter and burns down faster.
namespace pg::flame {

constexpr int kEvidence = 8;  // the game's evidence value for Spectral Flame (after D.O.T.S., 7)

enum class Kind { Tier1, Tier2, Tier3, Candelabra };

constexpr float kFalseCap = 0.5f;       // as far as a ghost without the evidence takes a flame
constexpr float kComfortLimit = 0.5f;   // bluer than this, a flame stops easing sanity drain
constexpr float kBlowOutSeconds = 75;   // at full blue, the ghost's extra blow-out comes about this often

/**
 * Ghost time in the flame's room for a fully blue flame (with the evidence)
 * or one at the false-positive cap (without it). Higher Firelight tiers turn faster.
 */
inline float build_seconds(Kind kind) {
  switch (kind) {
    case Kind::Tier2: return 50;
    case Kind::Tier3: return 40;
    case Kind::Tier1:
    case Kind::Candelabra:
    default: return 60;
  }
}

struct Inputs {
  bool lit = false;          // the flame is burning
  bool placed = false;       // set down (a candelabra's candles always are); a held flame never reacts
  bool ghostInRoom = false;  // the ghost is in the flame's room
  bool hasEvidence = false;  // the ghost gives Spectral Flame (as the difficulty left its evidence)
  Kind kind = Kind::Tier1;
};

struct State {
  float blueness = 0;
  bool full = false;  // it has been fully blue since it was set down and lit: the test is over
};

/** A flame's state after `dt` seconds. */
inline State step(State s, const Inputs& in, float dt) {
  if (!in.lit || !in.placed) return {};  // picking it up or putting it out starts it over
  const float top = in.hasEvidence ? 1 : kFalseCap;
  const float rate = top / build_seconds(in.kind);  // per second of ghost time in the room
  float next = in.ghostInRoom ? s.blueness + dt * rate : s.blueness - dt * rate / 2;
  next = std::clamp(next, 0.0f, top);
  return {next, s.full || next >= 1};
}

/**
 * Whether the ghost may blow out a flame: a ghost that gives Spectral Flame cannot put out a flame
 * set down until it has been fully blue, so nothing interrupts the test; afterwards it may, though the
 * flame has faded a little since. Others blow candles out as the game's ghosts do.
 */
inline bool ghost_may_blow_out(bool placed, bool hasEvidence, State s) { return !placed || !hasEvidence || s.full; }

/** Whether a flame this blue still eases sanity drain as Firelight does. */
inline bool gives_comfort(float blueness) { return blueness <= kComfortLimit; }

/** How much faster than usual a flame this blue burns down. */
inline float burn_multiplier(float blueness) { return 1 + std::clamp(blueness, 0.0f, 1.0f); }

/** The chance that the ghost, in the flame's room, blows out a flame this blue in `dt` seconds. */
inline float blow_out_chance(float blueness, float dt) { return blueness >= 1 ? dt / kBlowOutSeconds : 0; }

/** Whether any of `ghosts` evidence triples (flattened, three ints each) holds Spectral Flame. */
inline bool active_for(const int* evidence, int ghosts) {
  for (int i = 0; evidence && i < ghosts * 3; ++i)
    if (evidence[i] == kEvidence) return true;
  return false;
}

struct Rgb {
  float r, g, b;
};

// The colours a flame passes through. A flame's tint is HDR (the candle's is 10.7, 3.6, 1.5; the
// lantern's 42.7, 12.3, 3.4), and a blue that bright tone-maps to white, so both blues are far dimmer,
// about as bright as the game's own blue flames (its "Gas Flame Blue" is 0, 0.55, 1, which shows as
// teal; the deep blue here has less green). The pale blue is twice as bright with red and green added
// back. Lights are LDR and keep their strongest channel, at the flame's own hues; a blue light looks
// darker than the warm one (about a quarter of its luminance at full blue), as a blue flame does,
// since a light bright enough to match would tone-map to the pale cyan the deep blue is meant to leave.
constexpr Rgb kFlameDeep{0.03f, 0.18f, 1.3f};  // on screen, a deep blue
constexpr Rgb kFlamePale{0.44f, 0.8f, 2};      // x the deep blue's brightness: on screen, a light sky blue
constexpr Rgb kLightPale{0.55f, 0.7f, 1};      // x the light's own brightness

inline float brightness(Rgb c) { return std::max({c.r, c.g, c.b}); }
inline Rgb scaled(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }

inline Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

/** A flame's pale blue, the false-positive cap, for a flame whose deep blue is `deep`. */
inline Rgb flame_pale(Rgb deep) { return scaled(kFlamePale, brightness(deep)); }

/** A light's colours at the false-positive cap and at full blue, for its brightness `b`. */
inline Rgb light_pale(float b) { return scaled(kLightPale, b); }
inline Rgb light_deep(float b) { return scaled(kFlameDeep, b / brightness(kFlameDeep)); }

/**
 * A flame or light colour at `blueness`, hotter as it goes: the warm colour itself at 0, white-hot
 * halfway to the false-positive cap, `pale` at the cap and `deep` at full blue; linear between them.
 * Only a true flame gets past the pale blue. Through white, never through pink as a straight blend
 * from orange to blue would.
 */
inline Rgb tint(Rgb warm, Rgb pale, Rgb deep, float blueness) {
  const float t = std::clamp(blueness, 0.0f, 1.0f);
  if (t == 0) return warm;
  const float white = (brightness(warm) + brightness(pale)) / 2;
  const Rgb hot{white, white, white};
  if (t <= kFalseCap / 2) return mix(warm, hot, t / (kFalseCap / 2));
  if (t <= kFalseCap) return mix(hot, pale, (t - kFalseCap / 2) / (kFalseCap / 2));
  return mix(pale, deep, (t - kFalseCap) / (1 - kFalseCap));
}

// The host's message to every client: byte 1 (its kind), a little-endian u16 count, then per flame
// its fire source's PhotonView id (i32, little-endian) and its blueness (u8, 0..255, rounded down so
// a flame at the false-positive cap never reads past it on a client).
constexpr uint8_t kFlamesMessage = 1;
constexpr size_t kMaxPerMessage = 100;  // 503 bytes, within the host's 511

inline std::vector<uint8_t> encode(const std::vector<std::pair<int, float>>& flames) {
  std::vector<uint8_t> out{kFlamesMessage, static_cast<uint8_t>(flames.size() & 0xFF),
                           static_cast<uint8_t>(flames.size() >> 8)};
  for (const auto& [view, blueness] : flames) {
    const uint32_t v = static_cast<uint32_t>(view);
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<uint8_t>(v >> shift));
    out.push_back(static_cast<uint8_t>(std::clamp(blueness, 0.0f, 1.0f) * 255));
  }
  return out;
}

inline bool decode(const uint8_t* data, size_t size, std::vector<std::pair<int, float>>& flames) {
  flames.clear();
  if (!data || size < 3 || data[0] != kFlamesMessage) return false;
  const size_t count = data[1] | (static_cast<size_t>(data[2]) << 8);
  if (size != 3 + count * 5) return false;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* p = data + 3 + i * 5;
    const uint32_t v = p[0] | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
                       (static_cast<uint32_t>(p[3]) << 24);
    flames.emplace_back(static_cast<int>(v), p[4] / 255.0f);
  }
  return true;
}

}  // namespace pg::flame
