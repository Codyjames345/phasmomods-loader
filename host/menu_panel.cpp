#include "menu_panel.h"

#include <algorithm>
#include <cstring>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "notice.h"

namespace pg::menu_panel {
namespace {

constexpr size_t kErrorParent = 0x20;  // ErrorManager: ErrorParent, the popup it shows and hides
constexpr size_t kMenuDecor = 0x60;    // ConnectToServer: List<GameObject> used by EnableMenuDecor
constexpr size_t kMenuState = 0x70;    // ConnectToServer: state updated by TurnMenuObjects
constexpr size_t kMenuEnabled = 0x38;  // native menu state: all enable reasons are satisfied
constexpr float kMargin = 0.006f;      // inside the paper, in the menu canvas's units
constexpr float kRowsShare = 0.76f;    // leave a separate footer for status and dependency messages
constexpr float kBodyTextShare = 0.75f;  // readable paragraphs in the native paper's units
constexpr float kCheckboxShare = 0.09f;
constexpr float kNameShare = 0.26f;
constexpr float kSmallestText = 0.35f;  // auto-sizing goes down to this share of the game's size
constexpr unsigned kFallbackMs = 15000;
// The copy's scripts that would act on the game's behalf: its buttons (whose listeners drive the
// error queue), the localisers that put the game's words back, and the error manager itself. The
// buttons' colour animators stay on: they only colour the copy, and press() plays them.
constexpr const char* kSwitchedOff[] = {"UIButton", "TextLocaliserUI", "Button", "ErrorManager"};
// UISelectableColorAnimator.Play's states (Doozy's UISelectionState, as its GetAnimation reads them).
constexpr int kNormal = 0;
constexpr int kHighlighted = 1;

struct Api {
  il2cpp::Object* errorManagerType = nullptr;
  il2cpp::Object* behaviourType = nullptr;
  il2cpp::Object* textType = nullptr;
  il2cpp::Object* imageType = nullptr;
  il2cpp::Object* canvasType = nullptr;
  il2cpp::Object* focusType = nullptr;
  il2cpp::Object* colorAnimatorType = nullptr;
  void (*playState)(void*, int, const void*) = nullptr;  // UISelectableColorAnimator.Play(state)
  void* (*findObjectOfType)(il2cpp::Object*, const void*) = nullptr;
  void* (*mainMenu)(const void*) = nullptr;
  void* (*worldCamera)(void*, const void*) = nullptr;
  bool (*isFocused)(void*, const void*) = nullptr;
  ui::Vec2 (*sizeDelta)(void*, const void*) = nullptr;
  ui::Vec2 (*anchoredPosition)(void*, const void*) = nullptr;
  void* (*canvasRenderer)(void*, const void*) = nullptr;
  ui::Color (*rendererColor)(void*, const void*) = nullptr;  // CanvasRenderer (SetAlpha is stripped)
  void (*setRendererColor)(void*, const ui::Color*, const void*) = nullptr;
  bool menuOk = false;
  bool ok = false;
} api;

Panel g_message;
struct Decoration {
  il2cpp::Held object;
  bool active;
};
std::vector<Decoration> g_decor;
std::vector<il2cpp::Held> g_decorPanels;
il2cpp::Held g_decorMenu;

/** Read the same aggregate that drives the native menu's explicit control list. */
bool menu_enabled(void* menu) {
  void* state = game::alive(menu) ? il2cpp::field<void*>(menu, kMenuState) : nullptr;
  return state && il2cpp::field<bool>(state, kMenuEnabled);
}

/** Restore the board only after every owning popup has closed or been destroyed by the scene. */
void release_decor(void* closing = nullptr) {
  g_decorPanels.erase(std::remove_if(g_decorPanels.begin(), g_decorPanels.end(), [closing](const il2cpp::Held& root) {
    return root.get() == closing || !game::alive(root.get());
  }), g_decorPanels.end());
  if (!g_decorPanels.empty()) return;
  if (menu_enabled(g_decorMenu.get()))
    for (const Decoration& decor : g_decor)
      if (game::alive(decor.object.get())) game::set_active(decor.object.get(), decor.active);
  g_decor.clear();
  g_decorMenu = nullptr;
}

void* error_popup() {
  void* manager = api.ok ? api.findObjectOfType(api.errorManagerType, nullptr) : nullptr;
  void* popup = game::alive(manager) ? il2cpp::field<void*>(manager, kErrorParent) : nullptr;
  if (!game::alive(popup)) return nullptr;
  return il2cpp::is_class(popup, "GameObject") ? game::object_transform(popup) : game::transform_of(popup);
}

template <typename F>
void each_object(void* transform, const F& fn) {
  if (!transform) return;
  fn(game::game_object_of(transform));
  for (int i = 0; i < game::child_count(transform); ++i) each_object(game::child(transform, i), fn);
}

}  // namespace

void make_inert(void* root) {
  each_object(root, [](void* go) {
    il2cpp::Array* scripts = game::components(go, api.behaviourType);
    for (uintptr_t i = 0; scripts && i < scripts->max_length; ++i) {
      void* s = scripts->data<void*>()[i];
      if (!s || !game::alive(s)) continue;
      const char* name = il2cpp::class_name(il2cpp::class_of(s));
      for (const char* off : kSwitchedOff)
        if (name && std::strcmp(name, off) == 0) game::set_enabled(s, false);
    }
  });
}

namespace {

void* text_at(void* root, const char* path) {
  void* t = game::find_child(root, path);
  return t ? game::get_component(t, api.textType) : nullptr;
}

void fit(void* text) {
  const float size = ui::font_size(text);
  ui::set_word_wrap(text, true);
  ui::set_auto_size(text, size * kSmallestText, size);
}


/** Resolve the active main-menu component without searching the scene each frame. */
void* main_menu() {
  void* menu = api.menuOk ? api.mainMenu(nullptr) : nullptr;
  return game::alive(menu) && game::active_in_hierarchy(game::game_object_of(menu)) ? menu : nullptr;
}

/** Hide the game's curated decoration list without losing its pre-popup visibility states. */
void hide_decor(void* root) {
  release_decor();
  if (g_decorPanels.empty()) {
    void* menu = main_menu();
    g_decorMenu = il2cpp::Held(menu);
    auto* list = menu ? il2cpp::field<il2cpp::List*>(menu, kMenuDecor) : nullptr;
    if (list && list->items && list->size >= 0 &&
        static_cast<uintptr_t>(list->size) <= list->items->max_length) {
      for (int i = 0; i < list->size; ++i) {
        void* object = list->items->data<void*>()[i];
        if (game::alive(object)) g_decor.push_back({il2cpp::Held(object), game::active_self(object)});
      }
      for (const Decoration& decor : g_decor) game::set_active(decor.object.get(), false);
    }
  }
  g_decorPanels.emplace_back(root);
}

/** Copy only the paper's text style; each column has its own fixed size and rectangle. */
void* text_cell(Panel& p, void* parent, float x, float width, float height, float size) {
  void* copy = game::instantiate(game::transform_of(p.body), parent, false);
  void* text = copy ? game::get_component(copy, api.textType) : nullptr;
  if (!text) return nullptr;
  ui::place(text, {0, 0.5f}, {0, 0.5f}, {x, 0}, {width, height});
  ui::set_alignment(text, ui::Align::Left);
  ui::set_word_wrap(text, true);
  ui::set_fixed_font_size(text, size);
  return text;
}

/** Clone the native lobby paper's box and tick, without its Toggle or listeners. */
void* checkbox(void* parent, float size) {
  void* menu = main_menu();
  void* board = menu ? game::parent(game::transform_of(menu)) : nullptr;
  void* source = game::find_child(board, "Create Lobby/Hardware/Toggle.1/Background");
  if (!source || !game::find_child(source, "Checkmark") || !api.canvasRenderer || !api.rendererColor ||
      !api.setRendererColor) return nullptr;
  void* box = game::instantiate(source, parent, false);
  void* mark = box ? game::find_child(box, "Checkmark") : nullptr;
  if (!mark) { if (box) game::destroy(game::game_object_of(box)); return nullptr; }
  make_inert(box);
  game::set_local_scale(box, {1, 1, 1});
  game::set_local_scale(mark, {1, 1, 1});
  ui::place(box, {0, 0.5f}, {0, 0.5f}, {0, 0}, {size, size});
  ui::place(mark, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {size, size});
  for (void* t : {box, mark}) {
    void* image = game::get_component(t, api.imageType);
    if (!image) { game::destroy(game::game_object_of(box)); return nullptr; }
    game::set_enabled(image, true);
    void* renderer = api.canvasRenderer(image, nullptr);
    if (!renderer) { game::destroy(game::game_object_of(box)); return nullptr; }
    ui::Color shown = api.rendererColor(renderer, nullptr);  // the source Toggle may have faded it out
    shown.a = 1;
    api.setRendererColor(renderer, &shown, nullptr);
  }
  game::set_active(game::game_object_of(box), true);
  game::set_active(game::game_object_of(mark), false);
  return mark;
}

void play(const Control& c, int state) {
  if (api.playState && game::alive(c.animator.get())) api.playState(c.animator.get(), state, nullptr);
}

bool pointer(ui::Vec2& at) {
  float x = 0, y = 0;
  if (!game::mouse_position(x, y)) return false;
  at = {x, y};
  return true;
}

}  // namespace

void* menu_camera() {
  void* menu = main_menu();
  void* canvas = menu ? game::get_component(menu, api.canvasType) : nullptr;
  void* camera = game::alive(canvas) ? api.worldCamera(canvas, nullptr) : nullptr;
  return game::alive(camera) && game::enabled(camera) &&
         game::active_in_hierarchy(game::game_object_of(camera)) ? camera : nullptr;
}

bool menu_buttons_shown() {
  void* menu = main_menu();
  return menu && menu_enabled(menu);
}

const char* why_menu_hidden() {
  void* menu = main_menu();
  if (!menu) return "no main menu is active";
  if (!menu_enabled(menu)) return "the menu's buttons are off";
  if (!menu_camera()) return "the menu's camera is off";
  for (void* transform = game::transform_of(menu); transform; transform = game::parent(transform)) {
    void* focus = game::get_component(transform, api.focusType);
    if (game::alive(focus)) return api.isFocused(focus, nullptr) ? nullptr : "the camera is on another board";
  }
  return "the menu's board has no camera focus";
}

bool main_menu_visible() { return !why_menu_hidden(); }

Control control_of(void* root) {
  Control c;
  c.image = il2cpp::Held(game::get_component(root, api.imageType));
  if (api.colorAnimatorType) c.animator = il2cpp::Held(game::get_component(root, api.colorAnimatorType));
  return c;
}

bool press(Control& c) {
  void* image = c.image.get();
  const bool shown = game::alive(image) && game::active_in_hierarchy(game::game_object_of(image));
  ui::Vec2 at{};
  const bool over = shown && main_menu_visible() && pointer(at) && ui::contains(image, at, menu_camera());
  // A hidden copy's animator starts again from its button's state when it shows, so only a copy
  // still shown is told that the pointer left.
  if (over != c.over && (over || shown)) play(c, over ? kHighlighted : kNormal);
  c.over = over;
  return over && game::mouse_left_pressed();
}

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* rect = il2cpp::cls(kCore, "UnityEngine", "RectTransform");
  api.errorManagerType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "ErrorManager"));
  api.behaviourType = il2cpp::type_object(il2cpp::cls(kCore, "UnityEngine", "Behaviour"));
  api.textType = il2cpp::type_object(il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI"));
  api.imageType = il2cpp::type_object(il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Image"));
  il2cpp::Class* canvas = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
  il2cpp::Class* focus = il2cpp::cls("Assembly-CSharp.dll", "", "CameraFocusUI");
  api.canvasType = il2cpp::type_object(canvas);
  api.focusType = il2cpp::type_object(focus);
  api.findObjectOfType = reinterpret_cast<decltype(api.findObjectOfType)>(
      il2cpp::method_ptr(il2cpp::cls(kCore, "UnityEngine", "Object"), "FindObjectOfType", 1));
  api.mainMenu = reinterpret_cast<decltype(api.mainMenu)>(
      il2cpp::method_ptr(il2cpp::cls("Assembly-CSharp.dll", "", "ConnectToServer"), "get_Instance", 0));
  api.worldCamera = reinterpret_cast<decltype(api.worldCamera)>(il2cpp::method_ptr(canvas, "get_worldCamera", 0));
  api.isFocused = reinterpret_cast<decltype(api.isFocused)>(il2cpp::method_ptr(focus, "get_isFocused", 0));
  api.sizeDelta = reinterpret_cast<decltype(api.sizeDelta)>(il2cpp::method_ptr(rect, "get_sizeDelta", 0));
  api.anchoredPosition = reinterpret_cast<decltype(api.anchoredPosition)>(il2cpp::method_ptr(rect, "get_anchoredPosition", 0));
  api.canvasRenderer = reinterpret_cast<decltype(api.canvasRenderer)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Graphic"), "get_canvasRenderer", 0));
  il2cpp::Class* renderer = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "CanvasRenderer");
  api.rendererColor = reinterpret_cast<decltype(api.rendererColor)>(il2cpp::method_ptr(renderer, "GetColor", 0));
  api.setRendererColor = reinterpret_cast<decltype(api.setRendererColor)>(il2cpp::method_ptr(renderer, "SetColor", 1));
  il2cpp::Class* colorAnimator = il2cpp::cls("DoozyRuntime.dll", "Doozy.Runtime.UIManager.Animators", "UISelectableColorAnimator");
  api.colorAnimatorType = il2cpp::type_object(colorAnimator);
  if (!il2cpp::bind(api.playState, colorAnimator, "Play", 1))
    PG_LOG("menu: the game's colour animator is unavailable; the mod's tapes do not light up under the pointer");
  api.menuOk = api.canvasType && api.focusType && api.mainMenu && api.worldCamera && api.isFocused;
  api.ok = api.menuOk && api.errorManagerType && api.behaviourType && api.textType && api.imageType &&
           api.findObjectOfType && api.sizeDelta && api.anchoredPosition;
  if (!api.menuOk) PG_LOG("menu: main-menu focus unavailable; consent waits without capturing input");
  PG_LOG("menu: resolve %s", api.ok ? "ok" : "unavailable (popups fall back to the overlay)");
  return true;  // optional: without it the mod's popups use its own overlay
}

bool open(Panel& p, const std::string& title, const std::string& body, const std::vector<std::string>& buttons) {
  close(p);
  if (!main_menu_visible()) return false;
  void* source = error_popup();
  if (!source || buttons.empty()) return false;
  void* copy = game::instantiate(source, game::parent(source), false);
  void* root = copy && il2cpp::is_class(copy, "GameObject") ? game::object_transform(copy) : copy;
  if (!root) return false;
  make_inert(root);
  game::set_active(game::game_object_of(root), true);
  p.root = root;
  p.title = text_at(root, "Error Title tape/Error Title Text (TMP)");
  p.body = text_at(root, "Error_Text box/Background/Error Message");
  p.paper = game::find_child(root, "Error_Text box/Background");
  void* button = game::find_child(root, "Error Accept Button");
  if (!p.title || !p.body || !p.paper || !button) {
    PG_LOG("menu: the game's popup no longer has its title, paper or button; the overlay is used instead");
    close(p);
    return false;
  }
  set_text(p.title, title);
  p.rowFontSize = ui::font_size_max(p.body);
  const ui::Vec2 paper = api.sizeDelta(p.paper, nullptr);
  ui::place(p.body, {0, 1}, {0, 1}, {kMargin, -kMargin}, {paper.x - 2 * kMargin, paper.y - 2 * kMargin});
  ui::set_alignment(p.body, ui::Align::TopLeft);
  ui::set_word_wrap(p.body, true);
  ui::set_fixed_font_size(p.body, p.rowFontSize * kBodyTextShare);
  set_text(p.body, body);
  const ui::Vec2 size = api.sizeDelta(button, nullptr);
  const ui::Vec2 at = api.anchoredPosition(button, nullptr);
  const float width = size.x * (buttons.size() > 1 ? 1.6f : 1.0f);
  for (size_t i = 0; i < buttons.size(); ++i) {
    void* b = i == 0 ? button : game::instantiate(button, root, false);
    const float x = (static_cast<float>(i) - (static_cast<float>(buttons.size()) - 1) / 2) * width * 1.15f;
    ui::place(b, {0.5f, 0.5f}, {0.5f, 0.5f}, {x, at.y}, {width, size.y});
    void* image = game::get_component(b, api.imageType);
    void* label = text_at(b, "Accept Text (TMP)");
    if (!image || !label) continue;
    fit(label);
    set_text(label, buttons[i]);
    p.buttons.push_back(control_of(b));
    p.buttonTexts.emplace_back(label);
  }
  if (p.buttons.size() != buttons.size()) {
    PG_LOG("menu: the game's button has no image or label; the overlay is used instead");
    close(p);
    return false;
  }
  hide_decor(root);
  return true;
}

void close(Panel& p) {
  release_decor(p.root);
  if (p.root && game::alive(p.root)) game::destroy(game::game_object_of(p.root));
  p = Panel{};
}

bool is_open(const Panel& p) { return p.root && game::alive(p.root); }

bool drawn(const Panel& p) {
  void* root = p.root.get();
  if (!game::alive(root) || !game::active_in_hierarchy(game::game_object_of(root))) return false;
  void* canvas = game::get_component(game::parent(root), api.canvasType);  // the error popup's own canvas
  return !game::alive(canvas) || game::enabled(canvas);
}

int tick(Panel& p) {
  if (!is_open(p)) return -1;
  if (!main_menu_visible() || !game::active_in_hierarchy(game::game_object_of(p.root))) {
    close(p);
    return -1;
  }
  if (game::cursor_locked()) game::free_cursor();
  int clicked = -1;
  for (size_t i = 0; i < p.buttons.size(); ++i)
    if (press(p.buttons[i])) clicked = static_cast<int>(i);
  return clicked;
}

bool add_row(Panel& p, int index, int count, const std::string& name, const std::string& description) {
  if (!is_open(p) || count <= 0 || index < 0 || index >= count) return false;
  const ui::Vec2 paper = api.sizeDelta(p.paper, nullptr);
  const float inner = paper.x - 2 * kMargin;
  const float height = (paper.y * kRowsShare - kMargin) / static_cast<float>(count);
  if (p.rows.empty())
    ui::place(p.body, {0, 0}, {0, 0}, {kMargin, kMargin}, {inner, paper.y * (1 - kRowsShare) - kMargin});
  void* root = game::instantiate(game::transform_of(p.body), p.paper, false);
  if (!root) return false;
  void* hit = game::get_component(root, api.textType);
  ui::place(root, {0, 1}, {0, 1}, {kMargin, -kMargin - height * static_cast<float>(index)}, {inner, height});
  set_text(hit, "");
  const float nameX = inner * kCheckboxShare;
  const float descriptionX = nameX + inner * kNameShare;
  Row row{hit,
          text_cell(p, root, nameX, inner * kNameShare - kMargin, height, p.rowFontSize),
          text_cell(p, root, descriptionX, inner - descriptionX, height, p.rowFontSize * kBodyTextShare),
          checkbox(root, std::min(p.rowFontSize * 0.9f, height * 0.6f))};
  if (!row.hit || !row.name || !row.description || !row.checkmark) {
    game::destroy(game::game_object_of(root));
    return false;
  }
  set_text(row.name, name);
  set_text(row.description, description);
  p.rows.push_back(std::move(row));
  return true;
}

void set_row_checked(Panel& p, size_t index, bool checked) {
  if (index < p.rows.size() && game::alive(p.rows[index].checkmark))
    game::set_active(game::game_object_of(p.rows[index].checkmark), checked);
}

int row_clicked(const Panel& p) {
  ui::Vec2 at{};
  if (!is_open(p) || !main_menu_visible() ||
      !game::active_in_hierarchy(game::game_object_of(p.root)) ||
      !pointer(at) || !game::mouse_left_pressed()) return -1;
  for (size_t i = 0; i < p.rows.size(); ++i)
    if (ui::contains(p.rows[i].hit, at, menu_camera())) return static_cast<int>(i);
  return -1;
}

void set_text(void* text, const std::string& value) {
  if (text && game::alive(text)) ui::set_text(text, value.c_str());
}

void message(const std::string& title, const std::string& body) {
  if (open(g_message, title, body, {"OK"})) return;
  notice::show(title, body, kFallbackMs);  // no menu popup to copy here: the mod's own
}

void tick_message() {
  release_decor();
  if (tick(g_message) == 0) close(g_message);
}

}  // namespace pg::menu_panel
