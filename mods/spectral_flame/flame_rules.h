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
// A flame's ghost time builds while the ghost spends time in the flame's room, hunting or not, and
// drains at half that speed otherwise. It answers where the ghost lingers, never where it is now:
// every second counts, so short visits add up, but a ghost walking past changes little. The colour
// comes late: the flame looks its own for most of the build and turns most in its last seconds, so
// it is not obvious straight away. It fades evenly once the ghost is away and holds while the ghost
// is back until the ghost time catches up, so it never follows the ghost in and out of the room. Any
// ghost takes a flame to the false-positive cap, a pale blue; only a ghost that gives Spectral Flame
// takes it beyond, to a deep blue. A false flame is half a true one in everything, so it is always
// half as blue as a true flame would be: players who know the speed tell them apart. A bluer flame
// is hotter and burns down faster. The Hydra's flame turns green instead, pale green at the cap, so
// it names the Hydra even where its evidence is withheld. Only the time differs between Firelight
// tiers.
namespace pg::flame {

constexpr int kEvidence = 8;  // the game's evidence value for Spectral Flame (after D.O.T.S., 7)

enum class Kind { Tier1, Tier2, Tier3, Candelabra };

constexpr float kFalseCap = 0.5f;       // as far as a ghost without the evidence takes a flame
constexpr float kComfortLimit = 0.5f;   // bluer than this, a flame stops easing sanity drain
constexpr float kBlowOutSeconds = 75;   // at full blue, the ghost's extra blow-out comes about this often
constexpr float kLate = 4;              // the colour follows the ghost time to this power: most comes last

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
  float ghostTime = 0;  // the ghost's time in the room, built up and drained, as blueness (0..the flame's top)
  float blueness = 0;   // how blue the flame shows
  bool full = false;    // it has been fully blue since it was set down and lit: the test is over
};

/** How far along its colours a flame is, for the share (0..1) of its build the ghost time has reached. */
inline float late(float share) { return std::pow(std::clamp(share, 0.0f, 1.0f), kLate); }

/** A flame's state after `dt` seconds. */
inline State step(State s, const Inputs& in, float dt) {
  if (!in.lit || !in.placed) return {};  // picking it up or putting it out starts it over
  const float top = in.hasEvidence ? 1 : kFalseCap;
  const float rate = top / build_seconds(in.kind);  // per second of ghost time in the room
  const float time = std::clamp(in.ghostInRoom ? s.ghostTime + dt * rate : s.ghostTime - dt * rate / 2, 0.0f, top);
  const float built = top * late(time / top);
  const float faded = s.blueness - dt * rate / 2;  // as fast as the ghost time drains
  const float blueness = std::min(top, in.ghostInRoom ? std::max(s.blueness, built) : std::max(built, faded));
  return {time, blueness, s.full || blueness >= 1};
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

inline float brightness(Rgb c) { return std::max({c.r, c.g, c.b}); }
inline Rgb scaled(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }

inline Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

/**
 * A colour channel as set on a material or light (Material.SetColor, Light.color) in linear light, as
 * Unity hands it to the shader in a linear colour space project like this one: the sRGB curve below
 * 1 and pow 2.2 above (UnityPlayer's GammaToLinearSpace); and back.
 */
inline float to_linear(float v) {
  if (v <= 0.04045f) return v / 12.92f;
  if (v < 1) return std::pow((v + 0.055f) / 1.055f, 2.4f);
  return std::pow(v, 2.2f);
}
inline float to_gamma(float v) {
  if (v <= 0.0031308f) return v * 12.92f;
  if (v < 1) return 1.055f * std::pow(v, 1 / 2.4f) - 0.055f;
  return std::pow(v, 1 / 2.2f);
}

// The colours a flame passes through, as set on its material. Unity takes them to linear light (pow
// 2.2 above 1) before the shader multiplies them by the flame's grey sprite, and the game's volume
// tonemaps with ACES. Chosen through that pipeline (tests/urp_display.h): the pale blue shows as a
// light sky blue; the deep blue as a fully saturated deep blue, core to rim, a stop darker or brighter
// too; the Hydra's pale green as a mint, red under blue, never a candle's warm white; its deep green as
// a saturated green. Any brighter and ACES turns the deep blue's core purple and washes the green out
// to lime. The game's own "Gas Flame Blue" (0, 0.55, 1) shows teal.
constexpr Rgb kFlamePale{0.572f, 1.04f, 2.6f};
constexpr Rgb kFlameDeep{0.02f, 0.15f, 1};
constexpr Rgb kFlamePaleGreen{0.4f, 1.5f, 0.9f};
constexpr Rgb kFlameDeepGreen{0, 0.7f, 0.15f};

// A light takes the colour its flame shows on screen (the flame's body, through the same pipeline),
// at the light's own brightness, so a wall it lights shows the flame's own hue. A blue or green light
// is darker than the warm one, as such a flame is.
constexpr Rgb kLightPale{0.59f, 0.71f, 1};
constexpr Rgb kLightDeep{0, 0.01f, 1};
constexpr Rgb kLightPaleGreen{0.59f, 1, 0.74f};
constexpr Rgb kLightDeepGreen{0, 1, 0.01f};

/** The two stops a flame or light passes through: at the false-positive cap and at full. */
struct FlameColours {
  Rgb pale;
  Rgb deep;
};

/** A flame's stops: blue, or the Hydra's greens (`green`). The same for candles and the lantern. */
inline FlameColours flame_colours(bool green) {
  return green ? FlameColours{kFlamePaleGreen, kFlameDeepGreen} : FlameColours{kFlamePale, kFlameDeep};
}

/** A light's stops for its brightness `b`: blue, or the Hydra's greens, as its flame shows them. */
inline FlameColours light_colours(float b, bool green) {
  return green ? FlameColours{scaled(kLightPaleGreen, b), scaled(kLightDeepGreen, b)}
               : FlameColours{scaled(kLightPale, b), scaled(kLightDeep, b)};
}

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

// The candle flame's sprite. The game's "Candle flame" sprite is coloured (a white core, a yellow body,
// an orange tip) and its shader multiplies it by the tint, so no tint could turn it blue: its yellow
// and orange stay olive. A candle flame always shows a grey copy instead, with the tint that shows it
// as the game's own sprite did; the tint alone then gives the whole flame its colour.

/** Make a texel (8-bit RGBA as the texture stores it) grey, of its own brightness; alpha is kept. */
inline void to_grey(uint8_t* rgba) {
  const float y = 0.2126f * rgba[0] + 0.7152f * rgba[1] + 0.0722f * rgba[2];
  for (int i = 0; i < 3; ++i) rgba[i] = static_cast<uint8_t>(std::clamp(std::lround(y), 0L, 255L));
}

/**
 * What a sprite's grey copy needs multiplied in, channel by channel, to show as the sprite did: the
 * least-squares fit of the sprite to its grey copy, in linear light (as the shader samples both),
 * each texel weighted by how much it shows (its alpha, squared, as the flame is alpha-blended).
 * (1, 1, 1) for a grey sprite, or one where nothing shows.
 */
inline Rgb grey_match(const uint8_t* rgba, size_t texels) {
  double cross[3] = {0, 0, 0}, grey = 0;
  for (size_t i = 0; i < texels; ++i) {
    const uint8_t* t = rgba + i * 4;
    uint8_t g[4] = {t[0], t[1], t[2], t[3]};
    to_grey(g);
    const double w = (t[3] / 255.0) * (t[3] / 255.0), y = to_linear(g[0] / 255.0f);
    for (int c = 0; c < 3; ++c) cross[c] += w * to_linear(t[c] / 255.0f) * y;
    grey += w * y * y;
  }
  if (grey <= 0) return {1, 1, 1};
  return {static_cast<float>(cross[0] / grey), static_cast<float>(cross[1] / grey), static_cast<float>(cross[2] / grey)};
}

/** The tint (as set on the material) that shows a grey copy as `tint` showed the sprite `match` fits. */
inline Rgb grey_tint(Rgb tint, Rgb match) {
  return {to_gamma(to_linear(tint.r) * match.r), to_gamma(to_linear(tint.g) * match.g),
          to_gamma(to_linear(tint.b) * match.b)};
}

// The host's message to every client: byte 1 (its kind), a little-endian u16 count, then per flame
// its fire source's PhotonView id (i32, little-endian) and how blue it shows (u8, 0..255, rounded down
// so a flame at the false-positive cap never reads past it on a client).
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
