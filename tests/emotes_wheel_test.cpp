// Offline tests for the Emotes wheel's layout (mods/emotes/wheel_layout.h): the shapes drawn as textures, each segment's
// span with its gap, where each icon sits, the picked segment's colour, the VR pointer's hit on the disc, and the page
// row's place above the disc with the two page glyphs either side of its text. The wheel's
// Unity objects themselves are checked in game (the smoke list). The design is
// docs/superpowers/specs/2026-11-10-emotes-design.md, "The wheel". Run: build\emotes_wheel_test.exe
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "check.h"
#include "emote_rule.h"
#include "scroll_glyph.h"
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

// The alpha at a point given from the texture's centre in texels, x right and y up.
uint8_t alpha_point(const std::vector<uint8_t>& rgba, int size, float x, float y) {
  const int tx = static_cast<int>(std::floor(size / 2.0f + x)), ty = static_cast<int>(std::floor(size / 2.0f + y));
  return alpha_at(rgba, size, tx, ty);
}

// The user's ruling (2026-11-10): the gaps between the eight segments are even, by circles and angles. Each segment is
// the ring between two circles less a band of the same width along each boundary ray, so a gap is as wide at the rim as
// at the centre circle, and the same between every pair.
void the_wheel_shows_eight_segments_with_even_gaps() {
  const int n = 512;
  const float half = n / 2.0f, inner = 0.35f, gap = 0.05f;  // gap: its full width, a fraction of the half-size
  const float gapTexels = gap * half;
  for (int i = 0; i < kSegments; ++i) {
    const layout::Span s = layout::segment_span(i);
    CHECK(near(s.toDeg - s.fromDeg, 45.0f) && near((s.fromDeg + s.toDeg) / 2.0f, i * 45.0f));
    const std::vector<uint8_t> tex = layout::sector_rgba(n, inner, 1.0f, s.fromDeg, s.toDeg, gap);
    CHECK(tex.size() == static_cast<size_t>(n * n * 4));
    for (const float edgeDeg : {s.fromDeg, s.toDeg}) {
      const float a = edgeDeg / 57.29577951f;
      const float inward = edgeDeg == s.fromDeg ? 1.0f : -1.0f;  // toward the segment from this boundary
      // Along the boundary ray (0 up, clockwise) and across it, toward the segment.
      const float ux = std::sin(a), uy = std::cos(a), nx = std::cos(a) * inward, ny = -std::sin(a) * inward;
      for (const float rFrac : {inner + 0.08f, (inner + 1.0f) / 2.0f, 0.92f}) {
        const float r = rFrac * half;
        const auto at = [&](float across) { return alpha_point(tex, n, ux * r + nx * across, uy * r + ny * across); };
        CHECK(at(0.0f) == 0);                          // on the boundary: the gap
        CHECK(at(gapTexels / 2.0f - 1.5f) == 0);       // still in the gap
        CHECK(at(gapTexels / 2.0f + 1.5f) == 255);     // just past it, solid: the same width at every radius
      }
    }
    // Inside the ring only: the centre circle's place and past the rim are clear; the middle of its span solid.
    const float mid = i * 45.0f / 57.29577951f;
    CHECK(alpha_point(tex, n, std::sin(mid) * half * (inner - 0.03f), std::cos(mid) * half * (inner - 0.03f)) == 0);
    CHECK(alpha_point(tex, n, std::sin(mid) * half * 0.7f, std::cos(mid) * half * 0.7f) == 255);
    CHECK(alpha_at(tex, n, 0, 0) == 0);
  }
  // White, tinted by the image's colour.
  const std::vector<uint8_t> tex = layout::sector_rgba(n, inner, 1.0f, -22.5f, 22.5f, gap);
  CHECK(tex[(static_cast<size_t>(n / 2 + n / 3) * n + n / 2) * 4] == 255);
  // The centre circle: a disc.
  const std::vector<uint8_t> circle = layout::circle_rgba(n, 0.9f);
  CHECK(alpha_at(circle, n, n / 2, n / 2) == 255);
  CHECK(alpha_point(circle, n, 0.85f * half * 0.7071f, 0.85f * half * 0.7071f) == 255);
  CHECK(alpha_point(circle, n, 0.95f * half * 0.7071f, 0.95f * half * 0.7071f) == 0);
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
      CHECK(c.r == style::kDiscBlack.r && c.b == style::kDiscBlack.b);
  }
  CHECK(layout::segment_colour(0, -1).r == style::kDiscBlack.r);  // nothing picked: all the disc's black
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

void page_row_sits_above_the_disc() {
  const layout::PageRow row = layout::page_row(style::kDiscRadius, style::kPageAbove, style::kPageTextWidth,
                                               style::kGlyphSize, style::kGlyphGap);
  CHECK(near(row.text.y, style::kDiscRadius + style::kPageAbove));
  CHECK(near(row.text.x, 0.0f));
}

void glyphs_sit_either_side_of_the_page_text() {
  const layout::PageRow row = layout::page_row(style::kDiscRadius, style::kPageAbove, style::kPageTextWidth,
                                               style::kGlyphSize, style::kGlyphGap);
  CHECK(row.previous.x < -style::kPageTextWidth / 2.0f);  // clear of the text's box on the left
  CHECK(near(row.next.x, -row.previous.x));               // and mirrored on the right
  CHECK(near(row.previous.y, row.text.y) && near(row.next.y, row.text.y));  // level with the text
}

void glyph_at_hits_each_glyph_and_misses_between() {
  const float half = style::kGlyphSize / 2.0f;
  const layout::PageRow row = layout::page_row(style::kDiscRadius, style::kPageAbove, style::kPageTextWidth,
                                               style::kGlyphSize, style::kGlyphGap);
  CHECK(layout::glyph_at(row.previous, row, style::kGlyphSize) == -1);
  CHECK(layout::glyph_at(row.next, row, style::kGlyphSize) == 1);
  CHECK(layout::glyph_at(row.text, row, style::kGlyphSize) == 0);  // the text's centre hits neither glyph
  CHECK(layout::glyph_at({row.previous.x - half - 0.5f, row.previous.y}, row, style::kGlyphSize) == 0);  // just past
  CHECK(layout::glyph_at({row.previous.x - half, row.previous.y}, row, style::kGlyphSize) == -1);        // the edge
  CHECK(layout::glyph_at({row.next.x + half, row.next.y}, row, style::kGlyphSize) == 1);                 // counts
}

}  // namespace

// The keyboard and mouse page glyphs (the user's reference, traced as shapes, 2026-11-13): a mouse with its wheel, an
// arrow above it for scrolling up and below it for scrolling down, white lines on clear, drawn at 100 texels so a
// texel is one unit of the drawing. Rows run from the bottom, as Unity's textures do (texture_from_rgba), so the
// drawing's row y (from the top) is texture row 99 - y: a glyph drawn top row first showed upside down in game.
uint8_t alpha_at(const std::vector<uint8_t>& rgba, int x, int y) {
  return rgba[(static_cast<size_t>(99 - y) * 100 + x) * 4 + 3];
}

void the_scroll_glyphs_are_white_lines_on_clear() {
  for (ScrollArrow which : {ScrollArrow::Up, ScrollArrow::Down}) {
    const std::vector<uint8_t> g = scroll_glyph_rgba(100, which);
    CHECK(g.size() == 100u * 100u * 4u);
    if (g.size() != 100u * 100u * 4u) return;
    bool white = true;
    for (size_t i = 0; i < g.size(); i += 4) white = white && g[i] == 255 && g[i + 1] == 255 && g[i + 2] == 255;
    CHECK(white);
    CHECK(alpha_at(g, 0, 0) == 0 && alpha_at(g, 99, 99) == 0 && alpha_at(g, 99, 0) == 0 && alpha_at(g, 0, 99) == 0);
    // Mirrored left to right, as the reference is.
    bool mirrored = true;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 50; ++x) mirrored = mirrored && alpha_at(g, x, y) == alpha_at(g, 99 - x, y);
    CHECK(mirrored);
  }
}

void the_up_glyph_has_its_arrow_above_the_mouse() {
  const std::vector<uint8_t> g = scroll_glyph_rgba(100, ScrollArrow::Up);
  CHECK(g.size() == 100u * 100u * 4u);
  if (g.size() != 100u * 100u * 4u) return;
  CHECK(alpha_at(g, 50, 5) == 255);   // the arrow's tip
  CHECK(alpha_at(g, 46, 8) == 255);   // its head
  CHECK(alpha_at(g, 28, 61) == 255);  // the mouse's left side, halfway down
  CHECK(alpha_at(g, 50, 75) == 0);    // inside the mouse, under its wheel: an outline only
  CHECK(alpha_at(g, 46, 91) == 0);    // nothing below
}

void the_down_glyph_has_its_arrow_below_the_mouse() {
  const std::vector<uint8_t> g = scroll_glyph_rgba(100, ScrollArrow::Down);
  CHECK(g.size() == 100u * 100u * 4u);
  if (g.size() != 100u * 100u * 4u) return;
  CHECK(alpha_at(g, 50, 94) == 255);  // the arrow's tip
  CHECK(alpha_at(g, 46, 91) == 255);  // its head
  CHECK(alpha_at(g, 28, 39) == 255);  // the mouse's left side, halfway down
  CHECK(alpha_at(g, 50, 55) == 0);    // inside the mouse, under its wheel
  CHECK(alpha_at(g, 46, 8) == 0);     // nothing above
}

int main() {
  the_wheel_shows_eight_segments_with_even_gaps();
  the_icons_sit_in_their_segments();
  the_pick_turns_blue();
  the_vr_pointer_finds_the_disc();
  page_row_sits_above_the_disc();
  glyphs_sit_either_side_of_the_page_text();
  glyph_at_hits_each_glyph_and_misses_between();
  the_scroll_glyphs_are_white_lines_on_clear();
  the_up_glyph_has_its_arrow_above_the_mouse();
  the_down_glyph_has_its_arrow_below_the_mouse();
  std::printf("%s\n", g_failures ? "emotes_wheel tests FAILED" : "emotes_wheel tests passed");
  return g_failures ? 1 : 0;
}
