// Offline tests for the Emotes wheel's layout (mods/emotes/wheel_layout.h): the shapes drawn as textures, each segment's
// span with its gap, where each icon sits, the picked segment's colour, and the VR pointer's hit on the disc. The wheel's
// Unity objects themselves are checked in game (the smoke list). The design is
// docs/superpowers/specs/2026-11-10-emotes-design.md, "The wheel". Run: build\emotes_wheel_test.exe
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "check.h"
#include "emote_rule.h"
#include "wheel_layout.h"
#include "wheel_style.h"

namespace {

using namespace pm::emotes;

bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

// The alpha of the texel at (x, y), x right and y up from the bottom-left.
uint8_t alpha_at(const std::vector<uint8_t>& rgba, int size, int x, int y) { return rgba[(y * size + x) * 4 + 3]; }

// A texel at `degrees` (0 up, clockwise) and `radiusFrac` of the half-size from the centre.
uint8_t alpha_polar(const std::vector<uint8_t>& rgba, int size, float degrees, float radiusFrac) {
  const float rad = degrees / 57.29577951f, half = size / 2.0f;
  const int x = static_cast<int>(half + std::sin(rad) * radiusFrac * half);
  const int y = static_cast<int>(half + std::cos(rad) * radiusFrac * half);
  return alpha_at(rgba, size, x, y);
}

void the_wheel_shows_eight_segments_with_gaps() {
  for (int i = 0; i < kSegments; ++i) {
    const layout::Span s = layout::segment_span(i, style::kGapDegrees);
    CHECK(near(s.toDeg - s.fromDeg, 45.0f - style::kGapDegrees));
    CHECK(near((s.fromDeg + s.toDeg) / 2.0f, i * 45.0f));  // centred on its direction, the first straight up
  }
  // Neighbours leave the gap between them.
  CHECK(near(layout::segment_span(1, 2.0f).fromDeg - layout::segment_span(0, 2.0f).toDeg, 2.0f));
  // The drawn segment: solid inside its span and ring, clear outside, and clear in the gap.
  const int n = 128;
  const layout::Span s0 = layout::segment_span(0, 10.0f);
  const std::vector<uint8_t> tex = layout::sector_rgba(n, 0.4f, 0.95f, s0.fromDeg, s0.toDeg);
  CHECK(tex.size() == static_cast<size_t>(n * n * 4));
  CHECK(alpha_polar(tex, n, 0.0f, 0.7f) == 255);    // in the middle of its span
  CHECK(alpha_polar(tex, n, 30.0f, 0.7f) == 0);     // in the next segment's direction
  CHECK(alpha_polar(tex, n, 21.0f, 0.7f) == 0);     // in the gap (span ends at 17.5)
  CHECK(alpha_polar(tex, n, 0.0f, 0.2f) == 0);      // inside the ring: the centre circle's place
  CHECK(alpha_at(tex, n, 0, 0) == 0);               // a corner
  CHECK(tex[(n / 2 + (n / 2 - 20) * n) * 4] == 255);  // white, tinted by the image's colour
  // The centre circle.
  const std::vector<uint8_t> circle = layout::circle_rgba(n, 0.9f);
  CHECK(alpha_at(circle, n, n / 2, n / 2) == 255);
  CHECK(alpha_at(circle, n, 0, 0) == 0);
}

void the_icons_sit_in_their_segments() {
  for (int i = 0; i < kSegments; ++i) {
    const layout::Vec2 c = layout::icon_centre(i, style::kRingInner, style::kDiscRadius);
    const float r = std::hypot(c.x, c.y);
    CHECK(near(r, (style::kRingInner + style::kDiscRadius) / 2.0f, 0.01f));
    CHECK(segment_for(c.x, c.y, 1.0f) == i);  // the direction it sits in picks its own segment
  }
  const layout::Vec2 top = layout::icon_centre(0, 100.0f, 300.0f);
  CHECK(near(top.x, 0.0f) && near(top.y, 200.0f));
  const layout::Vec2 right = layout::icon_centre(2, 100.0f, 300.0f);
  CHECK(near(right.x, 200.0f) && near(right.y, 0.0f));
}

void the_pick_turns_blue() {
  for (int i = 0; i < kSegments; ++i) {
    const style::Rgba c = layout::segment_colour(i, 3);
    if (i == 3)
      CHECK(c.b == style::kPickedBlue.b && c.r == style::kPickedBlue.r);
    else
      CHECK(c.r == style::kDiscGrey.r && c.b == style::kDiscGrey.b);
  }
  CHECK(layout::segment_colour(0, -1).r == style::kDiscGrey.r);  // nothing picked: all grey
}

void the_vr_pointer_finds_the_disc() {
  // A disc facing -z at (0, 1, 2): a ray from the origin straight along +z hits its centre.
  const auto hit = layout::ray_plane({0, 1, 0}, {0, 0, 1}, {0, 1, 2}, {0, 0, -1});
  CHECK(hit.has_value() && near(hit->x, 0) && near(hit->y, 1) && near(hit->z, 2));
  // Parallel to the disc, or pointing away: no hit.
  CHECK(!layout::ray_plane({0, 1, 0}, {1, 0, 0}, {0, 1, 2}, {0, 0, -1}).has_value());
  CHECK(!layout::ray_plane({0, 1, 0}, {0, 0, -1}, {0, 1, 2}, {0, 0, -1}).has_value());
  // A hit in the disc's own units picks a segment by direction, nothing in the centre or past the rim.
  CHECK(layout::segment_at({0.0f, 200.0f}, style::kRingInner, style::kDiscRadius) == 0);
  CHECK(layout::segment_at({200.0f, 0.0f}, style::kRingInner, style::kDiscRadius) == 2);
  CHECK(layout::segment_at({10.0f, 10.0f}, style::kRingInner, style::kDiscRadius) == -1);
  CHECK(layout::segment_at({500.0f, 0.0f}, style::kRingInner, style::kDiscRadius) == -1);
}

}  // namespace

int main() {
  the_wheel_shows_eight_segments_with_gaps();
  the_icons_sit_in_their_segments();
  the_pick_turns_blue();
  the_vr_pointer_finds_the_disc();
  std::printf("%s\n", g_failures ? "emotes_wheel tests FAILED" : "emotes_wheel tests passed");
  return g_failures ? 1 : 0;
}
