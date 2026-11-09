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

/// A segment's span in degrees, 0 straight up and clockwise: its kSegments-th of the circle less the gap, centred on
/// its direction.
struct Span {
  float fromDeg, toDeg;
};

inline Span segment_span(int i, float gapDegrees) {
  const float width = 360.0f / kSegments, centre = i * width;
  return {centre - width / 2.0f + gapDegrees / 2.0f, centre + width / 2.0f - gapDegrees / 2.0f};
}

namespace detail {
// How much of the texel at (x, y) lies in [lo, hi], softened over one texel for a smooth edge.
inline float edge(float value, float lo, float hi, float soft) {
  return std::clamp(std::min(value - lo, hi - value) / soft + 0.5f, 0.0f, 1.0f);
}
inline float angle_of(float dx, float dy) {
  float deg = std::atan2(dx, dy) * 57.29577951f;  // 0 up, clockwise
  return deg;
}
}  // namespace detail

/// A white ring sector on clear: `size` texels square, from `innerFrac` to `outerFrac` of the half-size and from
/// `fromDeg` to `toDeg` (0 up, clockwise; the span may cross 0), RGBA row by row from the bottom.
inline std::vector<uint8_t> sector_rgba(int size, float innerFrac, float outerFrac, float fromDeg, float toDeg) {
  std::vector<uint8_t> out(static_cast<size_t>(size) * size * 4, 0);
  const float half = size / 2.0f, mid = (fromDeg + toDeg) / 2.0f, halfSpan = (toDeg - fromDeg) / 2.0f;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      const float dx = x + 0.5f - half, dy = y + 0.5f - half;
      const float r = std::hypot(dx, dy) / half;
      const float off = wrap_degrees(detail::angle_of(dx, dy) - mid);  // degrees from the span's middle
      const float arcSoft = 57.29577951f / std::max(std::hypot(dx, dy), 1.0f);  // one texel, in degrees, at this radius
      const float a = std::min(detail::edge(r, innerFrac, outerFrac, 1.0f / half),
                               detail::edge(off, -halfSpan, halfSpan, arcSoft));
      uint8_t* t = &out[(static_cast<size_t>(y) * size + x) * 4];
      t[0] = t[1] = t[2] = 255;
      t[3] = static_cast<uint8_t>(std::lround(a * 255.0f));
    }
  return out;
}

/// A white disc on clear, `radiusFrac` of the half-size.
inline std::vector<uint8_t> circle_rgba(int size, float radiusFrac) {
  return sector_rgba(size, -1.0f, radiusFrac, -360.0f, 360.0f);
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
