#pragma once

#include <algorithm>
#include <cmath>

#include "flame_rules.h"

// What a colour looks like on the player's screen in Phasmophobia, for the offline tests: the game
// renders in linear colour space (PlayerSettings m_ActiveColorSpace 1), Unity converts material and
// light colours to linear light (pow 2.2 above 1, from UnityPlayer's GammaToLinearSpace), and the
// game's volume tonemaps with ACES (sharedassets2 "Tonemapping", mode 2). This is URP 2022.3's own
// ACES (Color.hlsl AcesTonemap, the fitted RRT and ODT, from Unity-Technologies/Graphics) after the
// HDR LUT's LogC range (scene values above 58.86 clamp), with no grading; `ev` tries the picture a
// few stops brighter or darker. Bloom and the room around the flame are left out.
namespace pg::flame::urp {

struct M3 {
  float m[3][3];
  Rgb operator*(Rgb v) const {
    return {m[0][0] * v.r + m[0][1] * v.g + m[0][2] * v.b, m[1][0] * v.r + m[1][1] * v.g + m[1][2] * v.b,
            m[2][0] * v.r + m[2][1] * v.g + m[2][2] * v.b};
  }
};

constexpr M3 kSrgbToAp0{{{0.4397010f, 0.3829780f, 0.1773350f}, {0.0897923f, 0.8134230f, 0.0967616f}, {0.0175440f, 0.1115440f, 0.8707040f}}};
constexpr M3 kAp0ToAp1{{{1.4514393161f, -0.2365107469f, -0.2149285693f}, {-0.0765537734f, 1.1762296998f, -0.0996759264f}, {0.0083161484f, -0.0060324498f, 0.9977163014f}}};
constexpr M3 kAp1ToXyz{{{0.6624541811f, 0.1340042065f, 0.1561876870f}, {0.2722287168f, 0.6740817658f, 0.0536895174f}, {-0.0055746495f, 0.0040607335f, 1.0103391003f}}};
constexpr M3 kXyzToAp1{{{1.6410233797f, -0.3248032942f, -0.2364246952f}, {-0.6636628587f, 1.6153315917f, 0.0167563477f}, {0.0117218943f, -0.0082844420f, 0.9883948585f}}};
constexpr M3 kXyzToRec709{{{3.2409699419f, -1.5373831776f, -0.4986107603f}, {-0.9692436363f, 1.8759675015f, 0.0415550574f}, {0.0556300797f, -0.2039769589f, 1.0569715142f}}};
constexpr M3 kD60ToD65{{{0.98722400f, -0.00611327f, 0.0159533f}, {-0.00759836f, 1.00186000f, 0.0053302f}, {0.00307257f, -0.00509595f, 1.0816800f}}};
constexpr float kLogCMax = 58.85666f;

inline float ap1_y(Rgb c) { return 0.272229f * c.r + 0.674082f * c.g + 0.0536895f * c.b; }
inline Rgb toward_grey(Rgb c, float keep) {
  const float y = ap1_y(c);
  return {y + (c.r - y) * keep, y + (c.g - y) * keep, y + (c.b - y) * keep};
}

/** URP's AcesTonemap of a scene-linear colour (sRGB primaries): display-linear Rec.709. */
inline Rgb aces(Rgb x) {
  Rgb a = kSrgbToAp0 * x;
  const float lo = std::min({a.r, a.g, a.b}), hi = std::max({a.r, a.g, a.b});
  const float saturation = (std::max(hi, 1e-4f) - std::max(lo, 1e-4f)) / std::max(hi, 1e-2f);
  const float k = std::max(a.b * (a.b - a.g) + a.g * (a.g - a.r) + a.r * (a.r - a.b), 0.0f);
  const float yc = (a.b + a.g + a.r + 1.75f * std::sqrt(k)) / 3;
  const float xs = (saturation - 0.4f) / 0.2f, t = std::max(1 - std::fabs(xs / 2), 0.0f);
  const float s = (1 + (xs > 0 ? 1.0f : xs < 0 ? -1.0f : 0.0f) * (1 - t * t)) / 2;
  const float gain = 0.05f * s;
  const float glow = yc <= 2.0f / 3 * 0.08f ? gain : yc >= 2 * 0.08f ? 0 : gain * (0.08f / yc - 0.5f);
  a = scaled(a, 1 + glow);
  float hue = a.r == a.g && a.g == a.b ? 0 : std::atan2(std::sqrt(3.0f) * (a.g - a.b), 2 * a.r - a.g - a.b) * 57.2957795f;
  if (hue < 0) hue += 360;
  const float centred = hue > 180 ? hue - 360 : hue;
  float w = std::clamp(1 - std::fabs(2 * centred / 135), 0.0f, 1.0f);
  w = w * w * (3 - 2 * w);
  w *= w;
  a.r += w * saturation * (0.03f - a.r) * (1 - 0.82f);
  Rgb cg = kAp0ToAp1 * a;
  cg = toward_grey({std::max(cg.r, 0.0f), std::max(cg.g, 0.0f), std::max(cg.b, 0.0f)}, 0.96f);
  auto curve = [](float v) { return (v * (v + 0.0245786f) - 0.000090537f) / (v * (0.983729f * v + 0.4329510f) + 0.238081f); };
  const Rgb post{curve(cg.r), curve(cg.g), curve(cg.b)};
  Rgb xyz = kAp1ToXyz * post;
  const float sum = std::max(xyz.r + xyz.g + xyz.b, 1e-4f);
  const float cx = xyz.r / sum, cy = xyz.g / sum, y = std::pow(std::clamp(xyz.g, 0.0f, 65504.0f), 0.9811f);
  const float m = y / std::max(cy, 1e-4f);
  xyz = {cx * m, y, (1 - cx - cy) * m};
  const Rgb lin = toward_grey(kXyzToAp1 * xyz, 0.93f);
  return kXyzToRec709 * (kD60ToD65 * (kAp1ToXyz * lin));
}

inline float encode(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
}

/** A scene-linear HDR colour as the screen shows it (sRGB, 0..1), `ev` stops brighter or darker. */
inline Rgb display(Rgb sceneLinear, float ev = 0) {
  const float k = std::pow(2.0f, ev);
  auto range = [k](float v) { return std::clamp(v * k, 0.0f, kLogCMax); };
  const Rgb out = aces({range(sceneLinear.r), range(sceneLinear.g), range(sceneLinear.b)});
  return {encode(out.r), encode(out.g), encode(out.b)};
}

/** A colour set on a material or light (as Material.SetColor takes it) in linear light. */
inline Rgb linear_light(Rgb set) { return {to_linear(set.r), to_linear(set.g), to_linear(set.b)}; }

}  // namespace pg::flame::urp
