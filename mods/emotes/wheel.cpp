#include "wheel.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "UnityRendering.h"
#include "Transform.h"
#include "bundles.h"
#include "emote_assets.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "screen_effects.h"
#include "text_table.h"
#include "ui.h"
#include "wheel_layout.h"
#include "wheel_style.h"

namespace pm::emotes::wheel {

namespace {

struct State {
  std::array<api::Held, kSegments> icons;     // the bundle's icon textures
  std::array<api::Held, kSegments> segments;  // the drawn segment textures
  api::Held centre;                           // the drawn centre circle
  api::Held canvas;                           // the open wheel's Canvas; its children go with it
  std::array<api::Held, kSegments> images;    // the open wheel's segment images
  api::Held name;                             // its name text
  bool vr = false;
  bool loaded = false;
  int picked = -1;
};
State g;

ui::Color to_ui(style::Rgba c) { return {c.r, c.g, c.b, c.a}; }

void* drawn(const std::vector<uint8_t>& rgba) {
  return ui::texture_from_rgba(style::kTextureSize, style::kTextureSize, rgba.data(), rgba.size());
}

// Lays the wheel out on `canvas`, centred on its middle.
bool build(void* canvas) {
  const float d = style::kDiscRadius * 2.0f;
  const ui::Vec2 mid{0.5f, 0.5f};
  if (void* c = ui::create_raw_image(canvas, "pm emote centre", g.centre)) {
    ui::set_color(c, to_ui(style::kDiscGrey));
    const float cd = style::kCentreRadius * 2.0f;
    ui::place(c, mid, mid, {0, 0}, {cd, cd});
  }
  for (int i = 0; i < kSegments; ++i) {
    void* img = ui::create_raw_image(canvas, "pm emote segment", g.segments[i]);
    if (!img) return false;
    ui::set_color(img, to_ui(style::kDiscGrey));
    ui::place(img, mid, mid, {0, 0}, {d, d});
    g.images[i] = img;
    if (g.icons[i]) {
      if (void* icon = ui::create_raw_image(canvas, "pm emote icon", g.icons[i])) {
        ui::set_color(icon, to_ui(style::kIconWhite));
        const layout::Vec2 at = layout::icon_centre(i, style::kRingInner, style::kDiscRadius);
        ui::place(icon, mid, mid, {at.x, at.y}, {style::kIconSize, style::kIconSize});
      }
    }
  }
  g.name = ui::create_text(canvas, "pm emote name", style::kNameFontSize);
  if (g.name) {
    ui::set_color(g.name, to_ui(style::kNameWhite));
    ui::place(g.name, mid, {0.5f, 1.0f}, {0, -style::kDiscRadius - style::kNameBelow}, {d * 1.5f, style::kNameFontSize * 1.6f});
    ui::set_text(g.name, "");
  }
  return true;
}

void* transform_of_canvas() { return g.canvas ? game::transform_of(g.canvas) : nullptr; }

}  // namespace

bool load(void* bundle) {
  if (g.loaded) return true;
  for (int i = 0; i < kSegments; ++i) {
    void* tex = bundle ? api::load_bundle_asset(bundle, icon_path(kEmotes[i]).c_str(),
                                                phasmo::Texture2D::type.type_object())
                       : nullptr;
    if (!tex) PM_LOG("Emotes: no icon %s in the bundle; its segment shows bare", icon_path(kEmotes[i]).c_str());
    g.icons[i] = tex;
  }
  const float inner = style::kRingInner / style::kDiscRadius;
  for (int i = 0; i < kSegments; ++i) {
    const layout::Span s = layout::segment_span(i);
    g.segments[i] = drawn(layout::sector_rgba(style::kTextureSize, inner, 1.0f, s.fromDeg, s.toDeg,
                                              style::kGapWidth / style::kDiscRadius));
    if (!g.segments[i]) return false;
  }
  g.centre = drawn(layout::circle_rgba(style::kTextureSize, 1.0f));
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
  g.picked = segment >= 0 && segment < kSegments ? segment : -1;
  for (int i = 0; i < kSegments; ++i)
    if (g.images[i]) ui::set_color(g.images[i], to_ui(layout::segment_colour(i, g.picked)));
  if (g.name)
    ui::set_text(g.name, g.picked >= 0 ? i18n::text(kEmotes[g.picked].nameKey).c_str() : "");
}

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
  for (api::Held& h : g.images) h = nullptr;
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
  g = State{};
}

}  // namespace pm::emotes::wheel
