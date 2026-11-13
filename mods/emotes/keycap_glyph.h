#pragma once

// The keycap the join prompt shows a keyboard key in (docs/superpowers/specs/2026-11-13-synced-emotes-design.md): a
// white rounded square outline on clear, in the scroll glyphs' style (scroll_glyph.h), with its middle left clear for
// the key's name, which the prompt's text draws over it. Free of game calls.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pm::emotes {

/// The keycap as `size` x `size` RGBA texels, row by row from the bottom as Unity's textures are (texture_from_rgba):
/// white everywhere, its outline opaque and the rest clear, the edges smoothed over one texel. Laid out on a square
/// 100 units wide: the outline's middle runs 6 units in from each side, with corners rounded 16 units.
inline std::vector<uint8_t> keycap_glyph_rgba(int size) {
  if (size <= 0) return {};
  std::vector<uint8_t> rgba(static_cast<size_t>(size) * size * 4, 255);
  const float half = 44.0f, radius = 16.0f, strokeHalf = 3.0f;
  const float texel = 100.0f / static_cast<float>(size);
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      // Distance to the rounded square's outline, from its centre (50, 50).
      const float px = std::fabs((static_cast<float>(x) + 0.5f) * texel - 50.0f) - (half - radius);
      const float py = std::fabs((static_cast<float>(y) + 0.5f) * texel - 50.0f) - (half - radius);
      const float outside = std::hypot(std::max(px, 0.0f), std::max(py, 0.0f)) + std::min(std::max(px, py), 0.0f);
      const float d = std::fabs(outside - radius) - strokeHalf;
      const float alpha = std::clamp(0.5f - d / texel, 0.0f, 1.0f);
      const size_t row = static_cast<size_t>(size - 1 - y);  // the drawing's top is the texture's last row
      rgba[(row * size + x) * 4 + 3] = static_cast<uint8_t>(std::lround(alpha * 255.0f));
    }
  return rgba;
}

}  // namespace pm::emotes
