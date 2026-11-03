#include "ui.h"

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

// Copy the game's own UI font and material from a text the game already shows.
void style_like_game(void* text) {
  void* sample = phasmo::Object::FindObjectOfType(phasmo::TextMeshProUGUI::type.type_object(), false);
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

}  // namespace pm::ui
