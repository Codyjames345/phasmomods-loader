#pragma once

// The Emotes wheel's geometry, free of game calls: its segments and centre drawn as white textures the images tint (a
// plain shape drawn in code, as the Squatter's sheet mesh is: no art), where each segment lies and its icon sits, the
// picked one's colour, and where a VR controller's ray meets the disc.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "emote_rule.h"
#include "wheel_style.h"

namespace pm::emotes::layout {

struct Vec2 {
  float x, y;
};
struct Vec3 {
  float x, y, z;
};

/// A segment's span in degrees, 0 straight up and clockwise: its kSegments-th of the circle, between the two boundary
/// rays it shares with its neighbours, centred on its direction. The gaps are cut from it by sector_rgba.
struct Span {
  float fromDeg, toDeg;
};

inline Span segment_span(int i) {
  const float width = 360.0f / kSegments, centre = i * width;
  return {centre - width / 2.0f, centre + width / 2.0f};
}

namespace detail {
// How far `value` lies inside [lo, hi], as a texel's coverage: 0 outside, 1 inside, a one-texel ramp across each edge
// (`soft` is one texel in `value`'s units), so the edges are smooth.
inline float edge(float value, float lo, float hi, float soft) {
  return std::clamp(std::min(value - lo, hi - value) / soft + 0.5f, 0.0f, 1.0f);
}
// Coverage of the half-plane at least `inset` texels on the far side of the ray from the centre at `deg` (0 up,
// clockwise): `side` +1 for the clockwise side, -1 for the anticlockwise. Its signed distance from the ray's line is
// the point's component along the ray's normal.
inline float beyond_ray(float dx, float dy, float deg, float side, float inset) {
  const float a = deg / 57.29577951f;
  const float across = (dx * std::cos(a) - dy * std::sin(a)) * side;
  return std::clamp(across - inset + 0.5f, 0.0f, 1.0f);
}
}  // namespace detail

/// A white ring segment on clear: `size` texels square, between the circles of `innerFrac` and `outerFrac` of the
/// half-size, and between the rays at `fromDeg` and `toDeg` (0 up, clockwise; less than 180 apart) less a band of
/// `gapFrac` of the half-size along each, half of each gap on either side of the ray. The gap is a constant width,
/// as wide at the rim as at the inner circle, and the same for every segment, whichever way it points. RGBA row by row
/// from the bottom.
inline std::vector<uint8_t> sector_rgba(int size, float innerFrac, float outerFrac, float fromDeg, float toDeg,
                                        float gapFrac) {
  std::vector<uint8_t> out(static_cast<size_t>(size) * size * 4, 0);
  const float half = size / 2.0f, inset = gapFrac * half / 2.0f;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      const float dx = x + 0.5f - half, dy = y + 0.5f - half;
      const float r = std::hypot(dx, dy);
      const float a = std::min({detail::edge(r, innerFrac * half, outerFrac * half, 1.0f),
                                detail::beyond_ray(dx, dy, fromDeg, 1.0f, inset),
                                detail::beyond_ray(dx, dy, toDeg, -1.0f, inset)});
      uint8_t* t = &out[(static_cast<size_t>(y) * size + x) * 4];
      t[0] = t[1] = t[2] = 255;
      t[3] = static_cast<uint8_t>(std::lround(a * 255.0f));
    }
  return out;
}

/// A white disc on clear, `radiusFrac` of the half-size, its edge smoothed over a texel.
inline std::vector<uint8_t> circle_rgba(int size, float radiusFrac) {
  std::vector<uint8_t> out(static_cast<size_t>(size) * size * 4, 0);
  const float half = size / 2.0f;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      const float r = std::hypot(x + 0.5f - half, y + 0.5f - half);
      uint8_t* t = &out[(static_cast<size_t>(y) * size + x) * 4];
      t[0] = t[1] = t[2] = 255;
      t[3] = static_cast<uint8_t>(std::lround(std::clamp(radiusFrac * half - r + 0.5f, 0.0f, 1.0f) * 255.0f));
    }
  return out;
}

/// Where segment `i`'s icon sits, from the disc's centre: half way across its ring, in its direction.
inline Vec2 icon_centre(int i, float innerRadius, float outerRadius) {
  const float r = (innerRadius + outerRadius) / 2.0f, rad = i * (360.0f / kSegments) / 57.29577951f;
  return {std::sin(rad) * r, std::cos(rad) * r};
}

/// Segment `i`'s colour: blue when it is the one picked, grey otherwise.
inline style::Rgba segment_colour(int i, int picked) { return i == picked ? style::kPickedBlue : style::kDiscGrey; }

/// Where a ray meets a plane, or nothing when it runs along it or points away.
inline std::optional<Vec3> ray_plane(Vec3 origin, Vec3 dir, Vec3 point, Vec3 normal) {
  const float d = dir.x * normal.x + dir.y * normal.y + dir.z * normal.z;
  if (std::fabs(d) < 1e-6f) return std::nullopt;
  const float t = ((point.x - origin.x) * normal.x + (point.y - origin.y) * normal.y + (point.z - origin.z) * normal.z) / d;
  if (t <= 0.0f) return std::nullopt;
  return Vec3{origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t};
}

/// The segment under a point of the disc in its own units (x right, y up from its centre): by direction within the
/// ring, nothing in the centre or past the rim.
inline int segment_at(Vec2 p, float innerRadius, float outerRadius) {
  const float r = std::hypot(p.x, p.y);
  if (r < innerRadius || r > outerRadius) return -1;
  return segment_for(p.x, p.y, 0.0f);
}

}  // namespace pm::emotes::layout
