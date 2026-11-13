#include "join_prompt.h"

#include <cmath>
#include <vector>

#include "Transform.h"
#include "UnityRendering.h"
#include "game.h"
#include "held.h"
#include "keycap_glyph.h"
#include "log.h"
#include "text_table.h"
#include "ui.h"

namespace pm::emotes::join {

namespace {

namespace api = pm::api;

// The prompt's size, in the canvas's units, and how big a unit is in the world. Starting values, judged in game.
constexpr float kMetresPerUnit = 0.002f;    // 100 units = 20 cm
constexpr float kGlyphSize = 64.0f;
constexpr float kGap = 12.0f;
constexpr float kTextWidth = 140.0f;
constexpr float kFontSize = 44.0f;
constexpr float kKeyFontSize = 34.0f;
constexpr float kAboveHead = 0.35f;         // metres over the head bone
constexpr int kKeycapTexels = 128;

struct State {
  api::Held keycap;                    // the drawn keycap texture
  api::Held canvas, keycapImage, keyText, padImage, joinText;
  api::Held head;                      // the head the prompt hangs over
};
State g;

ui::Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }

void* transform_of_canvas() { return g.canvas ? game::transform_of(g.canvas) : nullptr; }

bool build(void* head) {
  void* canvas = ui::create_world_canvas("pm emote join", head, kMetresPerUnit);
  if (!canvas) return false;
  g.canvas = canvas;
  g.head = head;
  void* t = transform_of_canvas();
  const float width = kGlyphSize + kGap + kTextWidth;
  phasmo::RectTransform::set_sizeDelta(t, phasmo::Vector2{width, kGlyphSize});
  const ui::Vec2 mid{0.5f, 0.5f};
  const float glyphX = -width / 2.0f + kGlyphSize / 2.0f;
  const float textX = width / 2.0f - kTextWidth / 2.0f;
  g.keycapImage = ui::create_raw_image(canvas, "pm emote join keycap", g.keycap);
  if (g.keycapImage) {
    ui::set_color(g.keycapImage, white());
    ui::place(g.keycapImage, mid, mid, {glyphX, 0.0f}, {kGlyphSize, kGlyphSize});
  }
  g.keyText = ui::create_text(canvas, "pm emote join key", kKeyFontSize);
  if (g.keyText) {
    ui::set_color(g.keyText, white());
    ui::set_alignment(g.keyText, ui::Align::Center);
    ui::place(g.keyText, mid, mid, {glyphX, 0.0f}, {kGlyphSize, kGlyphSize});
  }
  g.padImage = ui::create_sprite_image(canvas, "pm emote join button", nullptr);
  if (g.padImage) ui::place(g.padImage, mid, mid, {glyphX, 0.0f}, {kGlyphSize, kGlyphSize});
  g.joinText = ui::create_text(canvas, "pm emote join text", kFontSize);
  if (g.joinText) {
    ui::set_color(g.joinText, white());
    ui::set_alignment(g.joinText, ui::Align::Left);
    ui::place(g.joinText, mid, mid, {textX, 0.0f}, {kTextWidth, kGlyphSize});
    ui::set_text(g.joinText, i18n::text("emotes.join").c_str());
  }
  return true;
}

}  // namespace

bool load() {
  if (g.keycap && game::alive(g.keycap)) return true;
  const std::vector<uint8_t> rgba = keycap_glyph_rgba(kKeycapTexels);
  g.keycap = ui::texture_from_rgba(kKeycapTexels, kKeycapTexels, rgba.data(), rgba.size());
  if (!g.keycap) PM_LOG("Emotes: the join prompt's keycap could not be drawn");
  return static_cast<bool>(g.keycap);
}

void hide() {
  if (g.canvas && game::alive(g.canvas)) ui::destroy(g.canvas);
  for (api::Held* h : {&g.canvas, &g.keycapImage, &g.keyText, &g.padImage, &g.joinText, &g.head}) *h = nullptr;
}

void unload() {
  hide();
  if (g.keycap && game::alive(g.keycap)) game::destroy(g.keycap);
  g = State{};
}

void show(void* head, void* viewer, void* padSprite, const std::string& keyName) {
  if (!head || !game::alive(head)) return hide();
  if (!(g.canvas && game::alive(g.canvas)) || static_cast<void*>(g.head) != head) {
    hide();
    if (!build(head)) return;
  }
  const bool pad = padSprite != nullptr;
  if (g.padImage) {
    ui::set_sprite(g.padImage, padSprite);
    ui::set_active(g.padImage, pad);
  }
  if (g.keycapImage) ui::set_active(g.keycapImage, !pad);
  if (g.keyText) {
    ui::set_active(g.keyText, !pad);
    if (!pad) ui::set_text(g.keyText, keyName.c_str());
  }
  // Held a set height over the head in the world, since the bone's own axes turn with the dance, and turned about the
  // vertical so its face looks back at the viewer (a canvas is read from behind its forward), as the VR wheel does.
  void* t = transform_of_canvas();
  if (!t) return;
  phasmo::Vector3 at = game::position(head);
  at.y += kAboveHead;
  game::set_position(t, at);
  if (viewer && game::alive(viewer)) {
    const phasmo::Vector3 from = game::position(viewer);
    const float yaw = std::atan2(at.x - from.x, at.z - from.z) / 2.0f;
    game::set_rotation(t, {0.0f, std::sin(yaw), 0.0f, std::cos(yaw)});
  }
}

}  // namespace pm::emotes::join
