#pragma once

// The keyboard and mouse page glyphs on the wheel's page row: a mouse with its scroll wheel, and an arrow above it
// (scroll up, the previous page) or below it (scroll down, the next page). A vector trace of the user's reference image
// (2026-11-13): white lines on clear, as the rest of the wheel, drawn at any size as stroked shapes. Free of game calls.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pm::emotes {

enum class ScrollArrow { Up, Down };

namespace glyph_detail {

struct P {
  float x, y;
};

// Distance from p to the segment a-b.
inline float to_segment(P p, P a, P b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len2 = dx * dx + dy * dy;
  const float t = len2 > 0.0f ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0f, 1.0f) : 0.0f;
  return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

// Distance from p to the outline of an upright capsule (a rectangle whose short sides are half circles) centred at
// c, `w` wide and `h` tall: the distance to its middle line less its radius, made unsigned.
inline float to_capsule_outline(P p, P c, float w, float h) {
  const float r = w * 0.5f;
  const float half = std::max(0.0f, h * 0.5f - r);
  return std::fabs(to_segment(p, {c.x, c.y - half}, {c.x, c.y + half}) - r);
}

}  // namespace glyph_detail

/// The glyph as `size` x `size` RGBA texels, top row first: white everywhere, its lines opaque and the rest clear, with
/// the lines' edges smoothed over one texel. The drawing is laid out on a square 100 units wide.
inline std::vector<uint8_t> scroll_glyph_rgba(int size, ScrollArrow which) {
  using namespace glyph_detail;
  std::vector<uint8_t> rgba(static_cast<size_t>(size) * size * 4, 255);
  if (size <= 0) return {};
  const bool up = which == ScrollArrow::Up;
  // The mouse: a capsule 44 wide and 70 tall, below the arrow for scrolling up and above it for scrolling down.
  const float top = up ? 26.0f : 4.0f;
  const P body{50.0f, top + 35.0f};
  // Its wheel: a capsule 8 wide and 16 tall, joined to the mouse's top by a short line.
  const P wheel{50.0f, top + 15.0f};
  // The arrow: a shaft and a head of two strokes, its tip away from the mouse.
  const P tip = up ? P{50.0f, 5.0f} : P{50.0f, 94.0f};
  const P tail = up ? P{50.0f, 19.0f} : P{50.0f, 80.0f};  // a gap before the mouse, as in the reference
  const float back = up ? 9.0f : -9.0f;
  const float bodyHalf = 2.25f, fineHalf = 2.0f;
  const float texel = 100.0f / static_cast<float>(size);
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      const P p{(static_cast<float>(x) + 0.5f) * texel, (static_cast<float>(y) + 0.5f) * texel};
      float d = to_capsule_outline(p, body, 44.0f, 70.0f) - bodyHalf;
      d = std::min(d, to_capsule_outline(p, wheel, 8.0f, 16.0f) - fineHalf);
      d = std::min(d, to_segment(p, {50.0f, top}, {50.0f, wheel.y - 8.0f}) - fineHalf);
      d = std::min(d, to_segment(p, tip, tail) - bodyHalf);
      d = std::min(d, to_segment(p, tip, {tip.x - 8.0f, tip.y + back}) - bodyHalf);
      d = std::min(d, to_segment(p, tip, {tip.x + 8.0f, tip.y + back}) - bodyHalf);
      const float alpha = std::clamp(0.5f - d / texel, 0.0f, 1.0f);
      rgba[(static_cast<size_t>(y) * size + x) * 4 + 3] = static_cast<uint8_t>(std::lround(alpha * 255.0f));
    }
  return rgba;
}

}  // namespace pm::emotes
