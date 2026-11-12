#include "ui.h"

#include <algorithm>

#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityUI.h"
#include "game.h"
#include "log.h"
#include "runtime.h"

// Every Unity UI member here is a map entry (sdk/map/UnityUI.h, Transform.h), reached through the game
// API, so a game update moves only the map.
namespace pm::ui {
namespace {

phasmo::Vector2 to_game(Vec2 v) { return {v.x, v.y}; }

// A text the game itself made, to copy its font from. One under this client's own player rig comes first: the journal,
// the HUD and the interact prompts are built from the game's UI font, and they are there whether or not they are on
// screen. Which text the rest of the scene happens to hold is not something to depend on: an emote's name came out in
// another font entirely (the user's report, 2026-11-10).
void* game_text_sample() {
  void* type = phasmo::TextMeshProUGUI::type.type_object();
  if (!type) return nullptr;
  void* rig = game::local_player_component();
  if (void* own = rig ? phasmo::Component::GetComponentInChildren(rig, type, true) : nullptr; own && game::alive(own))
    return own;
  return phasmo::Object::FindObjectOfType(type, false);
}

// Copy the game's own UI font and material from a text the game already shows.
void style_like_game(void* text) {
  void* sample = game_text_sample();
  if (!sample || sample == text || !game::alive(sample)) return;
  if (void* font = phasmo::TMP_Text::get_font(sample)) phasmo::TMP_Text::set_font(text, font);
  if (void* material = phasmo::TMP_Text::get_fontSharedMaterial(sample))
    phasmo::TMP_Text::set_fontSharedMaterial(text, material);
}

void* child(void* parent, const char* name) {
  void* go = game::new_game_object(name);
  if (go && parent) phasmo::Transform::SetParent(game::object_transform(go), game::transform_of(parent), false);
  return go;
}

int32_t alignment(Align align) {
  switch (align) {
    case Align::TopLeft: return phasmo::TextAlignmentOptions::TopLeft();
    case Align::Left: return phasmo::TextAlignmentOptions::Left();
    case Align::Top: return phasmo::TextAlignmentOptions::Top();
    case Align::Center: break;
  }
  return phasmo::TextAlignmentOptions::Center();
}

}  // namespace

// Every member is reached through the map, so there is nothing to bind: a missing one fails closed where it is used.
bool resolve() { return true; }

void* create_canvas(const char* name, int sortingOrder) {
  void* go = game::new_game_object(name);
  if (!go) return nullptr;
  void* canvas = game::add_component(go, phasmo::Canvas::type.type_object());
  phasmo::Canvas::set_renderMode(canvas, phasmo::RenderMode::ScreenSpaceOverlay());
  phasmo::Canvas::set_sortingOrder(canvas, sortingOrder);
  void* scaler = game::add_component(go, phasmo::CanvasScaler::type.type_object());
  phasmo::CanvasScaler::set_uiScaleMode(scaler, phasmo::CanvasScaler::ScaleWithScreenSize());
  phasmo::CanvasScaler::set_referenceResolution(scaler, {1920, 1080});
  phasmo::CanvasScaler::set_matchWidthOrHeight(scaler, 0.5f);
  return canvas;
}

void* create_text(void* parent, const char* name, float fontSize) {
  void* go = child(parent, name);
  void* text = game::add_component(go, phasmo::TextMeshProUGUI::type.type_object());
  if (!text) return nullptr;
  style_like_game(text);
  phasmo::TMP_Text::set_fontSize(text, fontSize);
  phasmo::TMP_Text::set_alignment(text, alignment(Align::Center));
  phasmo::TMP_Text::set_enableWordWrapping(text, false);
  phasmo::Graphic::set_raycastTarget(text, false);
  return text;
}

// A RawImage without a texture is a solid quad of its colour. (Plain Images created on the mod's
// canvases set their colour but did not draw in game; RawImages do.)
void* create_image(void* parent, const char* name, Color color) {
  void* image = create_raw_image(parent, name, nullptr);
  if (image) set_color(image, color);
  return image;
}

void* create_raw_image(void* parent, const char* name, void* texture) {
  void* go = child(parent, name);
  void* image = game::add_component(go, phasmo::RawImage::type.type_object());
  if (!image) return nullptr;
  phasmo::RawImage::set_texture(image, texture);
  phasmo::Graphic::set_raycastTarget(image, false);
  return image;
}

void set_texture(void* rawImage, void* texture) { phasmo::RawImage::set_texture(rawImage, texture); }
void set_uv_rect(void* rawImage, Rect uv) {
  const phasmo::Rect r{uv.x, uv.y, uv.width, uv.height};
  phasmo::RawImage::set_uvRect(rawImage, &r);
}

bool contains(void* component, Vec2 screenPoint, void* camera) {
  return component && game::alive(component) &&
         phasmo::RectTransformUtility::RectangleContainsScreenPoint(game::transform_of(component), to_game(screenPoint),
                                                                    camera);
}

bool normalized_point(void* component, Vec2 screenPoint, Vec2& out, void* camera) {
  if (!component || !game::alive(component)) return false;
  void* rt = game::transform_of(component);
  phasmo::Vector2 local{};
  if (!phasmo::RectTransformUtility::ScreenPointToLocalPointInRectangle(rt, to_game(screenPoint), camera, &local))
    return false;
  const phasmo::Rect r = phasmo::RectTransform::get_rect(rt);
  if (r.width <= 0 || r.height <= 0) return false;
  out = {(local.x - r.x) / r.width, (local.y - r.y) / r.height};
  return out.x >= 0 && out.x <= 1 && out.y >= 0 && out.y <= 1;
}

void destroy(void* component) {
  if (component && game::alive(component)) game::destroy(game::game_object_of(component));
}

void set_active(void* component, bool on) {
  if (component && game::alive(component)) game::set_active(game::game_object_of(component), on);
}

void place(void* component, Vec2 anchor, Vec2 pivot, Vec2 position, Vec2 size) {
  void* rt = game::transform_of(component);  // a UI component's transform is its RectTransform
  phasmo::RectTransform::set_anchorMin(rt, to_game(anchor));
  phasmo::RectTransform::set_anchorMax(rt, to_game(anchor));
  phasmo::RectTransform::set_pivot(rt, to_game(pivot));
  phasmo::RectTransform::set_anchoredPosition(rt, to_game(position));
  phasmo::RectTransform::set_sizeDelta(rt, to_game(size));
}

void set_alignment(void* text, Align align) { phasmo::TMP_Text::set_alignment(text, alignment(align)); }
void set_word_wrap(void* text, bool on) { phasmo::TMP_Text::set_enableWordWrapping(text, on); }
float font_size(void* text) { return phasmo::TMP_Text::get_fontSize(text); }
float font_size_max(void* text) { return phasmo::TMP_Text::get_fontSizeMax(text); }
void set_fixed_font_size(void* text, float size) {
  phasmo::TMP_Text::set_enableAutoSizing(text, false);
  phasmo::TMP_Text::set_fontSize(text, size);
}
void set_auto_size(void* text, float min, float max) {
  phasmo::TMP_Text::set_fontSizeMax(text, max);
  phasmo::TMP_Text::set_fontSizeMin(text, min);
  phasmo::TMP_Text::set_enableAutoSizing(text, true);
}

void set_text(void* text, const char* utf8) { phasmo::TMP_Text::set_text(text, api::make_string(utf8)); }
/**
 * Graphic.color is virtual and TMP_Text keeps its own colour (m_fontColor), so call the override the
 * object's class runs, as the game does; the base setter would leave a text's colour unchanged.
 */
void set_color(void* graphic, Color color) {
  const phasmo::Color c{color.r, color.g, color.b, color.a};
  phasmo::Graphic::set_color.virtual_call(graphic, &c);
}

Color color(void* graphic) {
  const phasmo::Color c = phasmo::Graphic::get_color.virtual_call(graphic);
  return {c.r, c.g, c.b, c.a};
}

// A UI object pinned between two corners of its parent, with a size delta on top of that. place() keeps both
// anchors equal, which is what a laid-out row wants; a stretch needs two different ones, which no helper in
// the map sets for us.
void stretch(void* component, Vec2 anchorMin, Vec2 anchorMax, Vec2 pivot, Vec2 position, Vec2 sizeDelta) {
  void* rt = game::transform_of(component);
  phasmo::RectTransform::set_anchorMin(rt, to_game(anchorMin));
  phasmo::RectTransform::set_anchorMax(rt, to_game(anchorMax));
  phasmo::RectTransform::set_pivot(rt, to_game(pivot));
  phasmo::RectTransform::set_anchoredPosition(rt, to_game(position));
  phasmo::RectTransform::set_sizeDelta(rt, to_game(sizeDelta));
}

// Built the way Unity's own editor builds one: a viewport carrying the game's clip and its ScrollRect, a
// background inside it that takes the pointer (the ScrollRect scrolls on the pointer, and sees it only
// through a graphic whose raycastTarget is on), and a content object pinned to the viewport's top edge as
// tall as the whole list. A row laid out at anchor {0.5, 1} and y = -(index + 0.5) * pitch therefore sits
// where the list says, whatever the list's height.
ScrollView create_scroll_view(void* parent, const char* name, Vec2 position, Vec2 size, float contentHeight) {
  ScrollView view;
  const std::string base = name && *name ? name : "pm scroll view";
  view.viewport = child(parent, base.c_str());
  if (!view.viewport) return view;
  place(view.viewport, {0.5f, 0.5f}, {0.5f, 0.5f}, position, size);
  view.height = size.y;
  view.contentHeight = contentHeight;
  if (void* back = create_raw_image(view.viewport, (base + " back").c_str(), nullptr)) {
    stretch(back, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f});
    phasmo::Graphic::set_raycastTarget(back, true);
  }
  view.content = child(view.viewport, (base + " content").c_str());
  if (view.content)
    stretch(view.content, {0.0f, 1.0f}, {1.0f, 1.0f}, {0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, contentHeight});
  view.scroll = game::add_component(view.viewport, phasmo::ScrollRect::type.type_object());
  if (!view.scroll) return view;  // no ScrollRect: the rows still show, they just cannot move
  game::add_component(view.viewport, phasmo::RectMask2D::type.type_object());
  if (view.content) phasmo::ScrollRect::content.set(view.scroll, game::transform_of(view.content));
  phasmo::ScrollRect::viewport.set(view.scroll, game::transform_of(view.viewport));
  phasmo::ScrollRect::vertical.set(view.scroll, true);
  phasmo::ScrollRect::horizontal.set(view.scroll, false);
  // A ScrollRect made at run time holds the free-running mode (Unity's editor-only Reset never runs), which
  // would let a drag throw the list out of the menu for good.
  phasmo::ScrollRect::movementType.set(view.scroll, phasmo::ScrollMovementType::Clamped());
  phasmo::ScrollRect::set_verticalNormalizedPosition(view.scroll, 1.0f);  // lists start at the top
  return view;
}

void set_scroll_height(ScrollView& view, float contentHeight) {
  const float fraction = view.scroll && view.contentHeight > view.height
                             ? phasmo::ScrollRect::get_verticalNormalizedPosition(view.scroll)
                             : 1.0f;
  view.contentHeight = contentHeight;
  if (!view.content) return;
  void* rt = game::transform_of(view.content);
  const phasmo::Vector2 size = phasmo::RectTransform::get_sizeDelta(rt);
  phasmo::RectTransform::set_sizeDelta(rt, {size.x, contentHeight});
  // After the new height, so the game clamps the position it is given against the bounds the list now has.
  if (view.scroll) phasmo::ScrollRect::set_verticalNormalizedPosition(view.scroll, fraction);
}

void scroll_reveal(const ScrollView& view, float fromTop, float band) {
  if (!view.scroll || !(view.contentHeight > view.height)) return;
  const float span = view.contentHeight - view.height;
  const float current = (1.0f - phasmo::ScrollRect::get_verticalNormalizedPosition(view.scroll)) * span;
  const float top = scroll_top_needed(view.contentHeight, view.height, current, fromTop, band);
  if (top == current) return;  // the band is showing already: leave the list where the player put it
  phasmo::ScrollRect::set_verticalNormalizedPosition(view.scroll, 1.0f - top / span);
  phasmo::ScrollRect::StopMovement(view.scroll);  // a key press wins over a drag's inertia
}

}  // namespace pm::ui

namespace pm::ui {

void* create_world_canvas(const char* name, void* follow, float metresPerUnit) {
  if (!follow || !game::alive(follow)) return nullptr;
  void* go = game::new_game_object(name);
  if (!go) return nullptr;
  void* canvas = game::add_component(go, phasmo::Canvas::type.type_object());
  if (!canvas) {
    game::destroy(go);
    return nullptr;
  }
  phasmo::Canvas::set_renderMode(canvas, phasmo::RenderMode::WorldSpace());
  void* t = game::object_transform(go);
  phasmo::Transform::SetParent(t, follow, false);
  game::set_local_position(t, {0.0f, 0.0f, 0.0f});
  game::set_local_rotation(t, {0.0f, 0.0f, 0.0f, 1.0f});
  game::set_local_scale(t, {metresPerUnit, metresPerUnit, metresPerUnit});
  return canvas;
}

void* texture_from_rgba(int width, int height, const uint8_t* rgba, size_t bytes) {
  if (width <= 0 || height <= 0 || !rgba || bytes != static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
    return nullptr;
  void* texture = phasmo::Texture2D::type.make();
  if (!texture) return nullptr;
  // One level, no mip chain: the player's texture quality setting drops a texture's top mip levels, which made the
  // Emotes wheel's shapes blocky; and a texture with levels left unwritten shows the engine's grey fill.
  if (phasmo::Texture2D::ctor_format.available() && phasmo::TextureFormat::RGBA32() != 0)
    phasmo::Texture2D::ctor_format(texture, width, height, static_cast<int32_t>(phasmo::TextureFormat::RGBA32()), false);
  else
    phasmo::Texture2D::ctor(texture, width, height);
  phasmo::Object::set_hideFlags(texture, phasmo::HideFlags::DontUnloadUnusedAsset());
  void* pixels = phasmo::Texture2D::GetPixels32(texture, 0);
  const api::Array<phasmo::Color32> texels{pixels};
  if (texels.size() * sizeof(phasmo::Color32) != bytes) {
    game::destroy(texture);
    return nullptr;
  }
  std::copy_n(rgba, bytes, reinterpret_cast<uint8_t*>(&texels[0]));
  phasmo::Texture2D::SetPixels32(texture, pixels, 0);
  // Its levels written from level 0, should the fallback constructor have given it a chain.
  phasmo::Texture2D::Apply(texture, true, true);
  return texture;
}

}  // namespace pm::ui
