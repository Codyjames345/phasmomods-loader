#include "ui.h"

#include "game.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::ui {
namespace {

constexpr int kRenderModeScreenSpaceOverlay = 0;
constexpr int kScaleWithScreenSize = 1;

struct Api {
  il2cpp::Class* gameObject = nullptr;
  il2cpp::Object* canvasType = nullptr;
  il2cpp::Object* scalerType = nullptr;
  il2cpp::Object* textType = nullptr;
  il2cpp::Object* rawImageType = nullptr;
  il2cpp::Object* raycasterType = nullptr;

  void* (*findObjectOfType)(void*, const void*) = nullptr;
  void* (*getTransform)(void*, const void*) = nullptr;
  void (*setParent)(void*, void*, bool, const void*) = nullptr;

  void (*setRenderMode)(void*, int, const void*) = nullptr;
  void (*setSortingOrder)(void*, int, const void*) = nullptr;
  void (*setScaleMode)(void*, int, const void*) = nullptr;
  void (*setReferenceResolution)(void*, Vec2, const void*) = nullptr;
  void (*setMatch)(void*, float, const void*) = nullptr;

  void (*setAnchorMin)(void*, Vec2, const void*) = nullptr;
  void (*setAnchorMax)(void*, Vec2, const void*) = nullptr;
  void (*setPivot)(void*, Vec2, const void*) = nullptr;
  void (*setAnchoredPosition)(void*, Vec2, const void*) = nullptr;
  void (*setSizeDelta)(void*, Vec2, const void*) = nullptr;

  void (*setText)(void*, il2cpp::String*, const void*) = nullptr;
  void (*setFontSize)(void*, float, const void*) = nullptr;
  float (*getFontSize)(void*, const void*) = nullptr;
  float (*getFontSizeMax)(void*, const void*) = nullptr;
  void (*setAutoSizing)(void*, bool, const void*) = nullptr;
  void (*setFontSizeMin)(void*, float, const void*) = nullptr;
  void (*setFontSizeMax)(void*, float, const void*) = nullptr;
  void (*setAlignment)(void*, int, const void*) = nullptr;
  void (*setWordWrap)(void*, bool, const void*) = nullptr;
  void* (*getFont)(void*, const void*) = nullptr;
  void (*setFont)(void*, void*, const void*) = nullptr;
  void* (*getSharedMaterial)(void*, const void*) = nullptr;
  void (*setSharedMaterial)(void*, void*, const void*) = nullptr;
  const il2cpp::Method* setGraphicColor = nullptr;  // virtual: TextMeshPro overrides it
  const il2cpp::Method* getGraphicColor = nullptr;
  void (*setRaycastTarget)(void*, bool, const void*) = nullptr;
  void (*setTexture)(void*, void*, const void*) = nullptr;
  void (*setUvRect)(void*, Rect, const void*) = nullptr;
  bool (*containsScreenPoint)(void*, Vec2, void*, const void*) = nullptr;
  bool (*screenToLocal)(void*, Vec2, void*, Vec2*, const void*) = nullptr;
  Rect (*rect)(void*, const void*) = nullptr;
} api;

using il2cpp::bind;

// Copy the game's own UI font and material from a text the game already shows.
void style_like_game(void* text) {
  void* sample = api.findObjectOfType(api.textType, nullptr);
  if (!sample || sample == text || !game::alive(sample)) return;
  if (void* font = api.getFont(sample, nullptr)) api.setFont(text, font, nullptr);
  if (void* material = api.getSharedMaterial(sample, nullptr)) api.setSharedMaterial(text, material, nullptr);
}

void* child(void* parent, const char* name) {
  void* go = game::new_game_object(name);
  if (go && parent) api.setParent(api.getTransform(go, nullptr), game::transform_of(parent), false, nullptr);
  return go;
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* object = il2cpp::cls(kCore, "UnityEngine", "Object");
  il2cpp::Class* transform = il2cpp::cls(kCore, "UnityEngine", "Transform");
  il2cpp::Class* rect = il2cpp::cls(kCore, "UnityEngine", "RectTransform");
  il2cpp::Class* canvas = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
  il2cpp::Class* scaler = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "CanvasScaler");
  il2cpp::Class* rawImage = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "RawImage");
  il2cpp::Class* raycaster = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "GraphicRaycaster");
  il2cpp::Class* rectUtility = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "RectTransformUtility");
  il2cpp::Class* graphic = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Graphic");
  il2cpp::Class* tmpText = il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TMP_Text");
  il2cpp::Class* tmpUgui = il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI");
  api.gameObject = il2cpp::cls(kCore, "UnityEngine", "GameObject");
  api.canvasType = il2cpp::type_object(canvas);
  api.scalerType = il2cpp::type_object(scaler);
  api.textType = il2cpp::type_object(tmpUgui);
  api.rawImageType = il2cpp::type_object(rawImage);
  api.raycasterType = il2cpp::type_object(raycaster);

  bool ok = api.gameObject && api.canvasType && api.scalerType && api.textType && api.rawImageType && api.raycasterType;
  ok &= bind(api.setTexture, rawImage, "set_texture", 1);
  ok &= bind(api.setUvRect, rawImage, "set_uvRect", 1);
  ok &= bind(api.containsScreenPoint, rectUtility, "RectangleContainsScreenPoint", 3);
  ok &= bind(api.screenToLocal, rectUtility, "ScreenPointToLocalPointInRectangle", 4);
  ok &= bind(api.rect, rect, "get_rect", 0);
  ok &= bind(api.findObjectOfType, object, "FindObjectOfType", 1);
  ok &= bind(api.getTransform, api.gameObject, "get_transform", 0);
  ok &= bind(api.setParent, transform, "SetParent", 2);
  ok &= bind(api.setRenderMode, canvas, "set_renderMode", 1);
  ok &= bind(api.setSortingOrder, canvas, "set_sortingOrder", 1);
  ok &= bind(api.setScaleMode, scaler, "set_uiScaleMode", 1);
  ok &= bind(api.setReferenceResolution, scaler, "set_referenceResolution", 1);
  ok &= bind(api.setMatch, scaler, "set_matchWidthOrHeight", 1);
  ok &= bind(api.setAnchorMin, rect, "set_anchorMin", 1);
  ok &= bind(api.setAnchorMax, rect, "set_anchorMax", 1);
  ok &= bind(api.setPivot, rect, "set_pivot", 1);
  ok &= bind(api.setAnchoredPosition, rect, "set_anchoredPosition", 1);
  ok &= bind(api.setSizeDelta, rect, "set_sizeDelta", 1);
  ok &= bind(api.setText, tmpText, "set_text", 1);
  ok &= bind(api.setFontSize, tmpText, "set_fontSize", 1);
  ok &= bind(api.getFontSize, tmpText, "get_fontSize", 0);
  ok &= bind(api.getFontSizeMax, tmpText, "get_fontSizeMax", 0);
  ok &= bind(api.setAutoSizing, tmpText, "set_enableAutoSizing", 1);
  ok &= bind(api.setFontSizeMin, tmpText, "set_fontSizeMin", 1);
  ok &= bind(api.setFontSizeMax, tmpText, "set_fontSizeMax", 1);
  ok &= bind(api.setAlignment, tmpText, "set_alignment", 1);
  ok &= bind(api.setWordWrap, tmpText, "set_enableWordWrapping", 1);
  ok &= bind(api.getFont, tmpText, "get_font", 0);
  ok &= bind(api.setFont, tmpText, "set_font", 1);
  ok &= bind(api.getSharedMaterial, tmpText, "get_fontSharedMaterial", 0);
  ok &= bind(api.setSharedMaterial, tmpText, "set_fontSharedMaterial", 1);
  api.setGraphicColor = il2cpp::method(graphic, "set_color", 1);
  api.getGraphicColor = il2cpp::method(graphic, "get_color", 0);
  ok &= api.setGraphicColor && api.getGraphicColor;
  ok &= bind(api.setRaycastTarget, graphic, "set_raycastTarget", 1);
  PG_LOG("ui: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

void* create_canvas(const char* name, int sortingOrder) {
  void* go = game::new_game_object(name);
  if (!go) return nullptr;
  void* canvas = game::add_component(go, api.canvasType);
  api.setRenderMode(canvas, kRenderModeScreenSpaceOverlay, nullptr);
  api.setSortingOrder(canvas, sortingOrder, nullptr);
  void* scaler = game::add_component(go, api.scalerType);
  api.setScaleMode(scaler, kScaleWithScreenSize, nullptr);
  api.setReferenceResolution(scaler, {1920, 1080}, nullptr);
  api.setMatch(scaler, 0.5f, nullptr);
  return canvas;
}

void* create_text(void* parent, const char* name, float fontSize) {
  void* go = child(parent, name);
  void* text = game::add_component(go, api.textType);
  if (!text) return nullptr;
  style_like_game(text);
  api.setFontSize(text, fontSize, nullptr);
  api.setAlignment(text, static_cast<int>(Align::Center), nullptr);
  api.setWordWrap(text, false, nullptr);
  api.setRaycastTarget(text, false, nullptr);
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
  void* image = game::add_component(go, api.rawImageType);
  if (!image) return nullptr;
  api.setTexture(image, texture, nullptr);
  api.setRaycastTarget(image, false, nullptr);
  return image;
}

void set_texture(void* rawImage, void* texture) { api.setTexture(rawImage, texture, nullptr); }
void set_uv_rect(void* rawImage, Rect uv) { api.setUvRect(rawImage, uv, nullptr); }

bool contains(void* component, Vec2 screenPoint, void* camera) {
  return component && game::alive(component) &&
         api.containsScreenPoint(game::transform_of(component), screenPoint, camera, nullptr);
}

bool normalized_point(void* component, Vec2 screenPoint, Vec2& out, void* camera) {
  if (!component || !game::alive(component)) return false;
  void* rt = game::transform_of(component);
  Vec2 local{};
  if (!api.screenToLocal(rt, screenPoint, camera, &local, nullptr)) return false;
  const Rect r = api.rect(rt, nullptr);
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
  api.setAnchorMin(rt, anchor, nullptr);
  api.setAnchorMax(rt, anchor, nullptr);
  api.setPivot(rt, pivot, nullptr);
  api.setAnchoredPosition(rt, position, nullptr);
  api.setSizeDelta(rt, size, nullptr);
}

void set_alignment(void* text, Align align) { api.setAlignment(text, static_cast<int>(align), nullptr); }
void set_word_wrap(void* text, bool on) { api.setWordWrap(text, on, nullptr); }
float font_size(void* text) { return api.getFontSize(text, nullptr); }
float font_size_max(void* text) { return api.getFontSizeMax(text, nullptr); }
void set_fixed_font_size(void* text, float size) {
  api.setAutoSizing(text, false, nullptr);
  api.setFontSize(text, size, nullptr);
}
void set_auto_size(void* text, float min, float max) {
  api.setFontSizeMax(text, max, nullptr);
  api.setFontSizeMin(text, min, nullptr);
  api.setAutoSizing(text, true, nullptr);
}
void set_raycast_target(void* graphic, bool on) { api.setRaycastTarget(graphic, on, nullptr); }
void block_pointer(void* canvas) {
  if (canvas && game::alive(canvas)) game::add_component(game::game_object_of(canvas), api.raycasterType);
}

void set_text(void* text, const char* utf8) { api.setText(text, il2cpp::string_new(utf8), nullptr); }
/**
 * Graphic.color is virtual and TMP_Text keeps its own colour (m_fontColor), so call the override the
 * object's class runs, as the game does; the base setter would leave a text's colour unchanged.
 */
void set_color(void* graphic, Color color) {
  const il2cpp::Method* m = il2cpp::virtual_method(graphic, api.setGraphicColor);
  if (!m) return;
  reinterpret_cast<void (*)(void*, const Color*, const void*)>(*reinterpret_cast<void* const*>(m))(graphic, &color, m);
}

Color color(void* graphic) {
  const il2cpp::Method* m = il2cpp::virtual_method(graphic, api.getGraphicColor);
  if (!m) return {0, 0, 0, 0};
  return reinterpret_cast<Color (*)(void*, const void*)>(*reinterpret_cast<void* const*>(m))(graphic, m);
}

}  // namespace pg::ui
