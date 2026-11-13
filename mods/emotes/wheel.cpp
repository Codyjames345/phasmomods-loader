#include "wheel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "UnityRendering.h"
#include "Transform.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "screen_effects.h"
#include "text_table.h"
#include "ui.h"
#include "wheel_layout.h"
#include "scroll_glyph.h"
#include "wheel_style.h"

namespace pm::emotes::wheel {

namespace {

struct State {
  std::vector<api::Held> icons;               // every emote's icon texture, in wheel order (null: none)
  std::vector<std::string> names;             // every emote's name, in wheel order
  std::array<api::Held, kSegments> segments;  // the drawn segment textures
  api::Held centre;                           // the drawn centre circle
  api::Held scrollUp, scrollDown;             // the drawn keyboard and mouse page glyphs (scroll_glyph.h)
  api::Held canvas;                           // the open wheel's Canvas; its children go with it
  std::array<api::Held, kSegments> images;    // the open wheel's segment images
  std::array<api::Held, kSegments> iconImages;  // the open wheel's icon images, showing the page's icons
  api::Held name;                             // its name text
  api::Held pageText;                         // the page row: the page number, and a glyph for each page control
  api::Held previousGlyph, nextGlyph;
  api::Held scrollUpImage, scrollDownImage;   // the open wheel's keyboard and mouse page glyphs
  int pages = 1;
  int page = 0;                               // the page shown: emotes page * kSegments onwards
  bool vr = false;
  bool loaded = false;
  int picked = -1;
};
State g;

ui::Color to_ui(style::Rgba c) { return {c.r, c.g, c.b, c.a}; }
void show_icons();
void show_page_row(void* previousSprite, void* nextSprite, bool scroll);

void* drawn(const std::vector<uint8_t>& rgba) {
  return ui::texture_from_rgba(style::kTextureSize, style::kTextureSize, rgba.data(), rgba.size());
}

// Lays the wheel out on `canvas`, centred on its middle.
bool build(void* canvas) {
  const float d = style::kDiscRadius * 2.0f;
  const ui::Vec2 mid{0.5f, 0.5f};
  if (void* c = ui::create_raw_image(canvas, "pm emote centre", g.centre)) {
    ui::set_color(c, to_ui(style::kDiscBlack));
    const float cd = style::kCentreRadius * 2.0f;
    ui::place(c, mid, mid, {0, 0}, {cd, cd});
  }
  for (int i = 0; i < kSegments; ++i) {
    void* img = ui::create_raw_image(canvas, "pm emote segment", g.segments[i]);
    if (!img) return false;
    ui::set_color(img, to_ui(style::kDiscBlack));
    ui::place(img, mid, mid, {0, 0}, {d, d});
    g.images[i] = img;
    if (void* icon = ui::create_raw_image(canvas, "pm emote icon", nullptr)) {
      const layout::Vec2 at = layout::icon_centre(i, style::kRingInner, style::kDiscRadius);
      ui::place(icon, mid, mid, {at.x, at.y}, {style::kIconSize, style::kIconSize});
      g.iconImages[i] = icon;
    }
  }
  show_icons();
  // The page row over the disc (the user's design, 2026-11-12): the page number, the previous page's control to its left
  // and the next page's to its right.
  const layout::PageRow row =
      layout::page_row(style::kDiscRadius, style::kPageAbove, style::kPageTextWidth, style::kGlyphSize, style::kGlyphGap);
  g.pageText = ui::create_text(canvas, "pm emote page", style::kPageFontSize);
  if (g.pageText) {
    ui::set_color(g.pageText, to_ui(style::kNameWhite));
    ui::place(g.pageText, mid, mid, {row.text.x, row.text.y}, {style::kPageTextWidth, style::kPageFontSize * 1.6f});
  }
  g.previousGlyph = ui::create_sprite_image(canvas, "pm emote previous page", nullptr);
  if (g.previousGlyph) ui::place(g.previousGlyph, mid, mid, {row.previous.x, row.previous.y}, {style::kGlyphSize, style::kGlyphSize});
  g.nextGlyph = ui::create_sprite_image(canvas, "pm emote next page", nullptr);
  if (g.nextGlyph) ui::place(g.nextGlyph, mid, mid, {row.next.x, row.next.y}, {style::kGlyphSize, style::kGlyphSize});
  // The keyboard and mouse glyphs at the same places: scroll up for the previous page, scroll down for the next.
  g.scrollUpImage = ui::create_raw_image(canvas, "pm emote scroll up", g.scrollUp);
  if (g.scrollUpImage) {
    ui::set_color(g.scrollUpImage, to_ui(style::kNameWhite));
    ui::place(g.scrollUpImage, mid, mid, {row.previous.x, row.previous.y}, {style::kGlyphSize, style::kGlyphSize});
  }
  g.scrollDownImage = ui::create_raw_image(canvas, "pm emote scroll down", g.scrollDown);
  if (g.scrollDownImage) {
    ui::set_color(g.scrollDownImage, to_ui(style::kNameWhite));
    ui::place(g.scrollDownImage, mid, mid, {row.next.x, row.next.y}, {style::kGlyphSize, style::kGlyphSize});
  }
  show_page_row(nullptr, nullptr, false);
  g.name = ui::create_text(canvas, "pm emote name", style::kNameFontSize);
  if (g.name) {
    ui::set_color(g.name, to_ui(style::kNameWhite));
    ui::place(g.name, mid, {0.5f, 1.0f}, {0, -style::kDiscRadius - style::kNameBelow}, {d * 1.5f, style::kNameFontSize * 1.6f});
    ui::set_text(g.name, "");
  }
  return true;
}

// The emote under segment `i` of the page shown, or -1 for an empty segment.
int emote_of(int i) {
  const int e = g.page * kSegments + i;
  return i >= 0 && i < kSegments && e < static_cast<int>(g.names.size()) ? e : -1;
}

// Puts the page's icons on the open wheel; a segment past the last emote, or one whose icon is missing, shows bare.
void show_icons() {
  for (int i = 0; i < kSegments; ++i) {
    void* image = g.iconImages[i];
    if (!image) continue;
    const int e = emote_of(i);
    void* tex = e >= 0 ? static_cast<void*>(g.icons[e]) : nullptr;
    ui::set_texture(image, tex);
    ui::set_color(image, tex ? to_ui(style::kIconWhite) : ui::Color{0, 0, 0, 0});
  }
}

// The page row: hidden with one page; otherwise the page number, and each control's glyph: the scroll glyphs for
// keyboard and mouse (`scroll`), else the game's sprite where the caller has one.
void show_page_row(void* previousSprite, void* nextSprite, bool scroll) {
  const bool many = g.pages > 1;
  if (g.pageText) {
    ui::set_active(g.pageText, many);
    if (many)
      ui::set_text(g.pageText, i18n::text("emotes.page", {{"page", std::to_string(g.page + 1)},
                                                           {"pages", std::to_string(g.pages)}}).c_str());
  }
  const auto glyph = [&](void* image, void* sprite) {
    if (!image) return;
    ui::set_sprite(image, sprite);
    ui::set_active(image, many && sprite);
  };
  glyph(g.previousGlyph, scroll ? nullptr : previousSprite);
  glyph(g.nextGlyph, scroll ? nullptr : nextSprite);
  for (void* image : {g.scrollUpImage.get(), g.scrollDownImage.get()})
    if (image) ui::set_active(image, many && scroll);
}

void* transform_of_canvas() { return g.canvas ? game::transform_of(g.canvas) : nullptr; }

}  // namespace

bool load(const std::vector<void*>& icons, const std::vector<std::string>& names) {
  if (g.loaded) return true;
  g.icons.assign(icons.begin(), icons.end());
  g.names = names;
  g.icons.resize(g.names.size());
  const float inner = style::kRingInner / style::kDiscRadius;
  for (int i = 0; i < kSegments; ++i) {
    const layout::Span s = layout::segment_span(i);
    g.segments[i] = drawn(layout::sector_rgba(style::kTextureSize, inner, 1.0f, s.fromDeg, s.toDeg,
                                              style::kGapWidth / style::kDiscRadius));
    if (!g.segments[i]) return false;
  }
  g.centre = drawn(layout::circle_rgba(style::kTextureSize, 1.0f));
  const std::vector<uint8_t> up = scroll_glyph_rgba(style::kGlyphTexels, ScrollArrow::Up);
  const std::vector<uint8_t> down = scroll_glyph_rgba(style::kGlyphTexels, ScrollArrow::Down);
  g.scrollUp = ui::texture_from_rgba(style::kGlyphTexels, style::kGlyphTexels, up.data(), up.size());
  g.scrollDown = ui::texture_from_rgba(style::kGlyphTexels, style::kGlyphTexels, down.data(), down.size());
  g.loaded = g.centre;
  return g.loaded;
}

bool open_screen() {
  if (!g.loaded || is_open()) return false;
  void* canvas = ui::create_canvas("pm emote wheel", style::kSortingOrder);
  if (!canvas) return false;
  g.canvas = canvas;
  g.vr = false;
  if (!build(canvas)) {
    close();
    return false;
  }
  api::fx::set_screen_effects(style::kBlur, style::kVignette);
  show_pick(-1);
  return true;
}

bool open_vr(void* leftHand) {
  if (!g.loaded || is_open() || !leftHand) return false;
  const float perUnit = style::kVrDiscMetres / (style::kDiscRadius * 2.0f);
  void* canvas = ui::create_world_canvas("pm emote wheel", leftHand, perUnit);
  if (!canvas) return false;
  g.canvas = canvas;
  g.vr = true;
  void* t = transform_of_canvas();
  const float size = style::kDiscRadius * 2.0f;
  phasmo::RectTransform::set_sizeDelta(t, phasmo::Vector2{size, size});
  game::set_local_position(t, {0.0f, style::kVrAboveHand, 0.0f});
  if (!build(canvas)) {
    close();
    return false;
  }
  show_pick(-1);
  return true;
}

void face(void* head) {
  void* t = transform_of_canvas();
  if (!g.vr || !t || !head || !game::alive(head)) return;
  const phasmo::Vector3 at = game::position(t), from = game::position(head);
  // Turned about the vertical so its face looks back at the head (a canvas is read from behind its forward).
  const float yaw = std::atan2(at.x - from.x, at.z - from.z) / 2.0f;
  game::set_rotation(t, {0.0f, std::sin(yaw), 0.0f, std::cos(yaw)});
}

void show_pick(int segment) {
  if (!is_open()) return;
  g.picked = emote_of(segment) >= 0 ? segment : -1;
  for (int i = 0; i < kSegments; ++i)
    if (g.images[i]) ui::set_color(g.images[i], to_ui(layout::segment_colour(i, g.picked)));
  if (g.name) ui::set_text(g.name, g.picked >= 0 ? g.names[static_cast<size_t>(emote_of(g.picked))].c_str() : "");
}

void set_page(int page) {
  g.page = page;
  g.pages = std::max(1, (static_cast<int>(g.names.size()) + kSegments - 1) / kSegments);
  if (!is_open()) return;
  show_icons();
  show_pick(-1);
}

void show_glyphs(void* previousSprite, void* nextSprite, bool scroll) {
  if (is_open()) show_page_row(previousSprite, nextSprite, scroll);
}

int pointed_glyph(void* hand) {
  void* t = transform_of_canvas();
  if (!g.vr || !t || !hand || !game::alive(hand) || g.pages < 2) return 0;
  const phasmo::Vector3 o = game::position(hand), f = game::forward(hand);
  const phasmo::Vector3 c = game::position(t), n = game::forward(t);
  const auto hit = layout::ray_plane({o.x, o.y, o.z}, {f.x, f.y, f.z}, {c.x, c.y, c.z}, {n.x, n.y, n.z});
  if (!hit) return 0;
  const phasmo::Vector3 world{hit->x, hit->y, hit->z};
  const phasmo::Vector3 local = phasmo::Transform::InverseTransformPoint(t, &world);
  const layout::PageRow row =
      layout::page_row(style::kDiscRadius, style::kPageAbove, style::kPageTextWidth, style::kGlyphSize, style::kGlyphGap);
  return layout::glyph_at({local.x, local.y}, row, style::kGlyphSize);
}

int emote_at(int segment) { return emote_of(segment); }

int pointed(void* hand) {
  void* t = transform_of_canvas();
  if (!g.vr || !t || !hand || !game::alive(hand)) return -1;
  const phasmo::Vector3 o = game::position(hand), f = game::forward(hand);
  const phasmo::Vector3 c = game::position(t), n = game::forward(t);
  const auto hit = layout::ray_plane({o.x, o.y, o.z}, {f.x, f.y, f.z}, {c.x, c.y, c.z}, {n.x, n.y, n.z});
  if (!hit) return -1;
  const phasmo::Vector3 world{hit->x, hit->y, hit->z};
  const phasmo::Vector3 local = phasmo::Transform::InverseTransformPoint(t, &world);
  return layout::segment_at({local.x, local.y}, style::kRingInner, style::kDiscRadius);
}

void close() {
  if (g.canvas) ui::destroy(g.canvas);
  if (!g.vr) api::fx::clear_screen_effects();
  g.canvas = nullptr;
  g.name = nullptr;
  g.pageText = nullptr;
  g.previousGlyph = nullptr;
  g.nextGlyph = nullptr;
  for (api::Held& h : g.images) h = nullptr;
  for (api::Held& h : g.iconImages) h = nullptr;
  g.picked = -1;
  g.vr = false;
}

bool is_open() { return g.canvas && game::alive(g.canvas); }

int icons_alive() {
  int n = 0;
  for (const api::Held& t : g.icons) n += t && game::alive(t) ? 1 : 0;
  return n;
}

int segments_alive() {
  int n = 0;
  for (const api::Held& t : g.segments) n += t && game::alive(t) ? 1 : 0;
  return n;
}

void unload() {
  close();
  for (api::Held& t : g.segments)
    if (t && game::alive(t)) game::destroy(t);
  if (g.centre && game::alive(g.centre)) game::destroy(g.centre);
  for (const api::Held* t : {&g.scrollUp, &g.scrollDown})
    if (*t && game::alive(*t)) game::destroy(*t);
  g = State{};
}

}  // namespace pm::emotes::wheel
