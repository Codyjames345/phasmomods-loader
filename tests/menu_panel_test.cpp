#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "game.h"
#include "ui.h"
#include "ui_fixture.h"

namespace {
struct Node {
  char fields[0x80]{};
  const char* name = "";
  const char* klass = "RectTransform";
  Node* parent = nullptr;
  Node* canvas = nullptr;
  Node* focus = nullptr;
  Node* camera = nullptr;
  const Node* copiedFrom = nullptr;
  bool alive = true, active = true, enabled = true, focused = true;
  bool wrap = false, autoSize = false;
  float fontSize = 24, fontMin = 24, fontMax = 24;
  pg::ui::Color rendererColor{0.3f, 0.4f, 0.5f, 1};  // CanvasRenderer colour (Toggle fades its alpha)
  pg::ui::Align alignment = pg::ui::Align::Center;
  il2cpp::Vector3 position{7, 4, 2}, scale{0.02f, 0.02f, 0.02f};
  il2cpp::Quaternion rotation{0, -0.70710677f, 0, 0.70710677f};
  pg::ui::Vec2 size{0.2f, 0.04f}, anchored{0, -0.1f};
  pg::ui::Vec2 anchor{0.5f, 0.5f}, pivot{0.5f, 0.5f};
  pg::ui::Color color{0.6f, 0.5f, 0.4f, 1};
  std::string text;
  std::vector<Node*> children;
  std::vector<Node*> scripts;  // MonoBehaviours on this object (klass is the script's class)
  std::vector<int> played;     // a colour animator's Play(state) calls, oldest first
};
std::vector<std::unique_ptr<Node>> nodes;
Node *fixtureManager, *fixtureMenu, *wall, *popupSource, *boardCamera, *playerCamera;
Node* menuState;
Node *activeDecor, *inactiveDecor, *unlistedDecor;
Node *checkboxBackground, *checkboxMark;
bool mouseAvailable = true, mouseClicked = true, locked = true;
void* hitCamera = nullptr;
void* hitTarget = nullptr;
int freedCursor = 0;
const char* unavailableClass = "";

Node* clone(Node* original, Node* parent) {
  auto* result = ui_fixture::make_node(nodes, original->name, parent);
  auto children = original->children;
  *result = *original;
  result->copiedFrom = original;
  result->parent = parent;
  result->children.clear();
  for (auto* child : children) clone(child, result);
  result->scripts.clear();
  for (auto* script : original->scripts) {
    auto* copy = ui_fixture::make_node(nodes, script->name);
    *copy = *script;
    copy->copiedFrom = script;
    result->scripts.push_back(copy);
  }
  return result;
}
Node* add_script(Node* object, const char* klass) {
  auto* script = ui_fixture::make_node(nodes, klass);
  script->klass = klass;
  object->scripts.push_back(script);
  return script;
}
Node* script_of(void* object, const char* klass) {
  for (auto* script : object ? static_cast<Node*>(object)->scripts : std::vector<Node*>{})
    if (std::strcmp(script->klass, klass) == 0) return script;
  return nullptr;
}
void play_state(void* animator, int state, const void*) { static_cast<Node*>(animator)->played.push_back(state); }
void* find_object(il2cpp::Object* type, const void*) {
  const char* name = reinterpret_cast<const char*>(type);
  if (std::strcmp(name, "ErrorManager") == 0) return fixtureManager;
  if (std::strcmp(name, "ConnectToServer") == 0) return fixtureMenu;
  return nullptr;
}
void* get_main(const void*) { return playerCamera; }
void* get_instance(const void*) { return fixtureMenu; }
void* world_camera(void* value, const void*) { return static_cast<Node*>(value)->camera; }
bool focused(void* value, const void*) { return static_cast<Node*>(value)->focused; }
pg::ui::Vec2 size_delta(void* value, const void*) { return static_cast<Node*>(value)->size; }
pg::ui::Vec2 anchored_position(void* value, const void*) { return static_cast<Node*>(value)->anchored; }
void* canvas_renderer(void* value, const void*) { return value; }
pg::ui::Color renderer_color(void* value, const void*) { return static_cast<Node*>(value)->rendererColor; }
void set_renderer_color(void* value, const pg::ui::Color* color, const void*) { static_cast<Node*>(value)->rendererColor = *color; }
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
}

#include "../host/menu_panel.cpp"

namespace il2cpp {
Class* cls(const char*, const char*, const char* name) {
  return std::strcmp(name, unavailableClass) == 0 ? nullptr : const_cast<char*>(name);
}
Object* type_object(Class* klass) { return reinterpret_cast<Object*>(klass); }
void* method_ptr(Class* klass, const char* name, int) {
  if (!klass) return nullptr;
#define BIND(text, fn) if (std::strcmp(name, text) == 0) return reinterpret_cast<void*>(&fn)
  BIND("FindObjectOfType", find_object);
  BIND("get_main", get_main);
  BIND("get_Instance", get_instance);
  BIND("get_worldCamera", world_camera);
  BIND("get_isFocused", focused);
  BIND("get_sizeDelta", size_delta);
  BIND("get_anchoredPosition", anchored_position);
  BIND("get_canvasRenderer", canvas_renderer);
  BIND("GetColor", renderer_color);
  BIND("SetColor", set_renderer_color);
  if (std::strcmp(static_cast<const char*>(klass), "UISelectableColorAnimator") == 0) BIND("Play", play_state);
#undef BIND
  return nullptr;
}
Class* class_of(const void* value) { return value ? const_cast<char*>(static_cast<const Node*>(value)->klass) : nullptr; }
const char* class_name(Class* klass) { return static_cast<const char*>(klass); }
bool is_class(const void* value, const char* name) {
  return value && std::strcmp(static_cast<const Node*>(value)->klass, name) == 0;
}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}

namespace pg::game {
bool alive(void* value) { return value && static_cast<Node*>(value)->alive; }
void* transform_of(void* value) { return value; }
void* object_transform(void* value) { return value; }
void* game_object_of(void* value) { return value; }
void* parent(void* value) { return value ? static_cast<Node*>(value)->parent : nullptr; }
Vector3 position(void* value) { return static_cast<Node*>(value)->position; }
void set_position(void* value, Vector3 at) { static_cast<Node*>(value)->position = at; }
Quaternion rotation(void* value) { return static_cast<Node*>(value)->rotation; }
void set_rotation(void* value, Quaternion at) { static_cast<Node*>(value)->rotation = at; }
Vector3 local_scale(void* value) { return static_cast<Node*>(value)->scale; }
void set_local_scale(void* value, Vector3 scale) { static_cast<Node*>(value)->scale = scale; }
void* instantiate(void* original, void* parent, bool world) {
  CHECK(!world);
  return clone(static_cast<Node*>(original), static_cast<Node*>(parent));
}
void set_active(void* value, bool on) {
  CHECK(value && static_cast<Node*>(value)->alive);
  if (value && static_cast<Node*>(value)->alive) static_cast<Node*>(value)->active = on;
}
void set_enabled(void* value, bool on) { static_cast<Node*>(value)->enabled = on; }
bool enabled(void* value) { return value && static_cast<Node*>(value)->enabled; }
bool active_self(void* value) { return value && static_cast<Node*>(value)->active; }
bool active_in_hierarchy(void* value) { return value && ui_fixture::active_in_hierarchy(static_cast<Node*>(value)); }
int child_count(void* value) { return static_cast<int>(static_cast<Node*>(value)->children.size()); }
void* child(void* value, int index) { return static_cast<Node*>(value)->children[index]; }
void* find_child(void* value, const char* path) {
  if (!value) return nullptr;
  const std::string full(path);
  const size_t slash = full.find('/');
  const std::string part = full.substr(0, slash);
  for (auto* child : static_cast<Node*>(value)->children)
    if (part == child->name)
      return slash == std::string::npos ? child : find_child(child, full.c_str() + slash + 1);
  return nullptr;
}
void* get_component(void* value, il2cpp::Object* type) {
  if (!value) return nullptr;
  auto* item = static_cast<Node*>(value);
  const auto* name = reinterpret_cast<const char*>(type);
  if (std::strcmp(name, "Canvas") == 0) return item->canvas;
  if (std::strcmp(name, "CameraFocusUI") == 0) return item->focus;
  if (std::strcmp(name, "TextMeshProUGUI") == 0 || std::strcmp(name, "Image") == 0) return item;
  return script_of(item, name);
}
/** GetComponents(Behaviour): the object's scripts, as a managed array laid out in a Node's fields. */
il2cpp::Array* components(void* value, il2cpp::Object*) {
  auto* array = ui_fixture::make_node(nodes, "Behaviour[]");
  const auto& scripts = static_cast<Node*>(value)->scripts;
  il2cpp::set_field<uintptr_t>(array, 0x18, scripts.size());
  for (size_t i = 0; i < scripts.size(); ++i) il2cpp::set_field(array, 0x20 + 8 * i, scripts[i]);
  return reinterpret_cast<il2cpp::Array*>(array);
}
void destroy(void* value) {
  auto* item = static_cast<Node*>(value);
  item->alive = false;
  for (auto* child : item->children) destroy(child);
}
bool mouse_position(float& x, float& y) { x = 120; y = 160; return mouseAvailable; }
bool mouse_left_pressed() { return mouseClicked; }
bool cursor_locked() { return locked; }
void free_cursor() { ++freedCursor; locked = false; }
}

namespace pg::ui {
/** Model a TMP layout pass choosing within the requested autosize range. */
void set_text(void* value, const char* text) {
  auto* item = static_cast<Node*>(value);
  item->text = text;
  if (item->autoSize) item->fontSize = item->text.size() > 30 ? item->fontMin : item->fontMax;
}
void set_word_wrap(void* value, bool on) { static_cast<Node*>(value)->wrap = on; }
void set_alignment(void* value, Align alignment) { static_cast<Node*>(value)->alignment = alignment; }
float font_size(void* value) { return static_cast<Node*>(value)->fontSize; }
float font_size_max(void* value) { return static_cast<Node*>(value)->fontMax; }
void set_auto_size(void* value, float min, float max) {
  auto* item = static_cast<Node*>(value);
  item->autoSize = true;
  item->fontMin = min;
  item->fontMax = max;
}
void set_fixed_font_size(void* value, float size) {
  auto* item = static_cast<Node*>(value);
  item->autoSize = false;
  item->fontSize = size;
}
void place(void* value, Vec2 anchor, Vec2 pivot, Vec2 at, Vec2 size) {
  static_cast<Node*>(value)->anchor = anchor;
  static_cast<Node*>(value)->pivot = pivot;
  static_cast<Node*>(value)->anchored = at;
  static_cast<Node*>(value)->size = size;
}
Color color(void* value) { return static_cast<Node*>(value)->color; }
void set_color(void* value, Color color) { static_cast<Node*>(value)->color = color; }
bool contains(void* value, Vec2, void* camera) {
  hitCamera = camera;
  return camera == boardCamera && (!hitTarget || value == hitTarget);
}
}
namespace pg::log { void write(const char*, ...) {} }
namespace pg::notice { void show(const std::string&, const std::string&, unsigned) {} }

namespace {
void reset() {
  nodes.clear();
  holds.clear();
  wall = ui_fixture::make_node(nodes, "Wall Corkboard");
  wall->focus = ui_fixture::make_node(nodes, "focus");
  fixtureMenu = ui_fixture::make_node(nodes, "Main Menu", ui_fixture::make_node(nodes, "Menu", wall));
  menuState = ui_fixture::make_node(nodes, "Main menu state");
  il2cpp::set_field(fixtureMenu, 0x70, menuState);
  il2cpp::set_field<bool>(menuState, 0x38, true);
  checkboxBackground = ui_fixture::make_node(nodes, "Background", ui_fixture::make_node(nodes, "Toggle.1",
      ui_fixture::make_node(nodes, "Hardware", ui_fixture::make_node(nodes, "Create Lobby", fixtureMenu->parent))));
  checkboxMark = ui_fixture::make_node(nodes, "Checkmark", checkboxBackground);
  checkboxMark->active = true;
  checkboxMark->rendererColor.a = 0;
  activeDecor = ui_fixture::make_node(nodes, "Pin and string", fixtureMenu);
  inactiveDecor = ui_fixture::make_node(nodes, "Hidden pin", fixtureMenu);
  unlistedDecor = ui_fixture::make_node(nodes, "Background", fixtureMenu);
  activeDecor->klass = inactiveDecor->klass = unlistedDecor->klass = "GameObject";
  inactiveDecor->active = false;
  auto* decorList = ui_fixture::make_node(nodes, "Decor list");
  decorList->klass = "List`1";
  auto* decorArray = ui_fixture::make_node(nodes, "Decor array");
  decorArray->klass = "GameObject[]";
  il2cpp::set_field(fixtureMenu, 0x60, decorList);
  il2cpp::set_field(decorList, 0x10, decorArray);
  il2cpp::set_field<int32_t>(decorList, 0x18, 2);
  il2cpp::set_field<uintptr_t>(decorArray, 0x18, 2);
  il2cpp::set_field(decorArray, 0x20, activeDecor);
  il2cpp::set_field(decorArray, 0x28, inactiveDecor);
  fixtureMenu->canvas = ui_fixture::make_node(nodes, "canvas");
  boardCamera = ui_fixture::make_node(nodes, "Board Camera");
  playerCamera = ui_fixture::make_node(nodes, "Player Camera");
  playerCamera->position = {12, 8, -4};
  fixtureMenu->canvas->camera = boardCamera;
  fixtureManager = ui_fixture::make_node(nodes, "ErrorManager");
  popupSource = ui_fixture::make_node(nodes, "ErrorParent", fixtureMenu->parent);
  popupSource->active = false;
  ui_fixture::make_node(nodes, "Error Title Text (TMP)", ui_fixture::make_node(nodes, "Error Title tape", popupSource));
  ui_fixture::make_node(nodes, "Error Message", ui_fixture::make_node(nodes, "Background", ui_fixture::make_node(nodes, "Error_Text box", popupSource)));
  auto* accept = ui_fixture::make_node(nodes, "Error Accept Button", popupSource);
  ui_fixture::make_node(nodes, "Accept Text (TMP)", accept);
  add_script(accept, "UIButton");
  add_script(accept, "UISelectableColorAnimator");
  il2cpp::set_field(fixtureManager, 0x20, popupSource);
  mouseAvailable = mouseClicked = locked = true;
  freedCursor = 0;
  hitCamera = nullptr;
  hitTarget = nullptr;
  unavailableClass = "";
  CHECK(pg::menu_panel::resolve());
}
bool same(il2cpp::Vector3 a, il2cpp::Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same(il2cpp::Quaternion a, il2cpp::Quaternion b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
/** A copied popup keeps the native board-plane geometry across open and subsequent ticks. */
void native_transform_survives() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Consent", "Read before playing", {"Accept", "Decline"}));
  CHECK(panel.root);
  if (!panel.root) return;
  auto* root = static_cast<Node*>(panel.root);
  CHECK(root != popupSource && root->parent == popupSource->parent);
  CHECK(same(root->position, popupSource->position));
  CHECK(same(root->rotation, popupSource->rotation));
  CHECK(same(root->scale, popupSource->scale));
  playerCamera->position = {-13, 18, 5};
  mouseClicked = false;
  CHECK(pg::menu_panel::tick(panel) == -1);
  CHECK(same(root->position, popupSource->position));
  CHECK(same(root->rotation, popupSource->rotation));
  CHECK(same(root->scale, popupSource->scale));
  CHECK(!popupSource->active);
  pg::menu_panel::close(panel);
}
/** Main-menu clicks and rows use the canvas camera even while Camera.main is the player camera. */
void board_camera_hits() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
  if (panel.buttons.empty()) return;
  CHECK(pg::menu_panel::menu_camera() == boardCamera);
  CHECK(pg::menu_panel::press(panel.buttons[0]));
  CHECK(hitCamera == boardCamera);
  CHECK(pg::menu_panel::add_row(panel, 0, 1, "Sample", "Sample description"));
  CHECK(pg::menu_panel::row_clicked(panel) == 0);
  CHECK(hitCamera == boardCamera);
  mouseClicked = false;
  CHECK(!pg::menu_panel::press(panel.buttons[0]));
  CHECK(pg::menu_panel::row_clicked(panel) == -1);
  mouseAvailable = false;
  mouseClicked = true;
  CHECK(!pg::menu_panel::press(panel.buttons[0]));
  CHECK(pg::menu_panel::row_clicked(panel) == -1);
  pg::menu_panel::close(panel);
}
/** Inactive controls cannot consume clicks, including descendants of an inactive menu. */
void hidden_controls_ignore_input() {
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    pg::menu_panel::Panel panel;
    CHECK(pg::menu_panel::open(panel, "Consent", "", {"Accept"}));
    if (!panel.root || panel.buttons.empty()) return;
    CHECK(pg::menu_panel::add_row(panel, 0, 1, "Sample", "Sample description"));
    if (mode == 0) fixtureMenu->active = false;
    if (mode == 1) wall->focus->focused = false;
    if (mode == 2) static_cast<Node*>(panel.root)->active = false;
    CHECK(!pg::menu_panel::press(panel.buttons[0]));
    CHECK(pg::menu_panel::row_clicked(panel) == -1);
    CHECK(pg::menu_panel::tick(panel) == -1);
    CHECK(pg::menu_panel::row_clicked(panel) == -1);
    CHECK(freedCursor == 0);
    pg::menu_panel::close(panel);
  }
  reset();
  auto* image = ui_fixture::make_node(nodes, "Mods", fixtureMenu);
  image->active = false;
  auto control = pg::menu_panel::control_of(image);
  CHECK(!pg::menu_panel::press(control));
}
/** Copied tapes keep the game's own colour animator and play its states as the pointer enters and
    leaves, once per change, while their buttons stay switched off. */
void hover_plays_native_animation() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  CHECK(panel.buttons.size() == 2);
  if (panel.buttons.size() != 2) return;
  auto* apply = static_cast<Node*>(panel.buttons[0].image.get());
  auto* close = static_cast<Node*>(panel.buttons[1].image.get());
  Node* applyAnimator = script_of(apply, "UISelectableColorAnimator");
  Node* closeAnimator = script_of(close, "UISelectableColorAnimator");
  CHECK(applyAnimator && closeAnimator && applyAnimator != closeAnimator);
  if (!applyAnimator || !closeAnimator) return;
  CHECK(panel.buttons[0].animator.get() == applyAnimator && applyAnimator->enabled && closeAnimator->enabled);
  CHECK(!script_of(apply, "UIButton")->enabled && !script_of(close, "UIButton")->enabled);
  const pg::ui::Color drawn = apply->color;
  mouseClicked = false;
  hitTarget = apply;
  CHECK(pg::menu_panel::tick(panel) == -1);
  CHECK(pg::menu_panel::tick(panel) == -1);
  CHECK((applyAnimator->played == std::vector<int>{1}));  // highlighted once, not restarted each frame
  CHECK(closeAnimator->played.empty());
  hitTarget = close;
  pg::menu_panel::tick(panel);
  CHECK((applyAnimator->played == std::vector<int>{1, 0}));
  CHECK((closeAnimator->played == std::vector<int>{1}));
  hitTarget = wall;  // over neither
  pg::menu_panel::tick(panel);
  CHECK((closeAnimator->played == std::vector<int>{1, 0}));
  CHECK(apply->color.r == drawn.r && apply->color.g == drawn.g && apply->color.b == drawn.b);
  pg::menu_panel::close(panel);
}
/** A tape hidden while the pointer is on it lights up again when it shows under the pointer. */
void hover_restarts_after_hiding() {
  reset();
  auto* tape = ui_fixture::make_node(nodes, "Mods", fixtureMenu);
  Node* animator = add_script(tape, "UISelectableColorAnimator");
  auto control = pg::menu_panel::control_of(tape);
  mouseClicked = false;
  hitTarget = tape;
  CHECK(!pg::menu_panel::press(control));
  tape->active = false;
  CHECK(!pg::menu_panel::press(control));
  tape->active = true;
  CHECK(!pg::menu_panel::press(control));
  CHECK((animator->played == std::vector<int>{1, 1}));
  mouseClicked = true;
  CHECK(pg::menu_panel::press(control));
  CHECK((animator->played == std::vector<int>{1, 1}));
}
/** Visibility requires the focused menu and its live, enabled canvas camera. */
void menu_visibility() {
  reset();
  CHECK(pg::menu_panel::main_menu_visible() && !pg::menu_panel::why_menu_hidden());
  for (int mode = 0; mode < 10; ++mode) {
    reset();
    if (mode == 0) fixtureMenu = nullptr;
    if (mode == 1) fixtureMenu->alive = false;
    if (mode == 2) fixtureMenu->active = false;
    if (mode == 3) fixtureMenu->parent->active = false;
    if (mode == 4) wall->focus->focused = false;
    if (mode == 5) wall->focus = nullptr;
    if (mode == 6) fixtureMenu->canvas = nullptr;
    if (mode == 7) fixtureMenu->canvas->camera = nullptr;
    if (mode == 8) boardCamera->enabled = false;
    if (mode == 9) boardCamera->active = false;
    CHECK(!pg::menu_panel::main_menu_visible() && pg::menu_panel::why_menu_hidden());
    pg::menu_panel::Panel panel;
    CHECK(!pg::menu_panel::open(panel, "Consent", "", {"Accept"}));
    CHECK(!pg::menu_panel::is_open(panel));
    CHECK(freedCursor == 0);
  }
  reset();
  boardCamera->alive = false;
  CHECK(!pg::menu_panel::main_menu_visible());
}
/** The game's buttons follow its menu state, not where the camera looks (the map, the shop). */
void buttons_follow_menu_state() {
  for (int mode = 0; mode < 2; ++mode) {
    reset();
    if (mode == 0) wall->focus->focused = false;
    if (mode == 1) boardCamera->enabled = false;
    CHECK(pg::menu_panel::menu_buttons_shown() && !pg::menu_panel::main_menu_visible());
  }
  for (int mode = 0; mode < 4; ++mode) {
    reset();
    if (mode == 0) il2cpp::set_field<bool>(menuState, 0x38, false);  // Connecting
    if (mode == 1) il2cpp::set_field<void*>(fixtureMenu, 0x70, nullptr);
    if (mode == 2) fixtureMenu->active = false;
    if (mode == 3) fixtureMenu = nullptr;
    CHECK(!pg::menu_panel::menu_buttons_shown());
  }
}
/** An active focused board can be Connecting; the native aggregate menu state remains decisive. */
void aggregate_menu_visibility() {
  reset();
  menuState->alive = false;  // Managed state is not a Unity object with native lifetime.
  CHECK(pg::menu_panel::main_menu_visible());
  for (bool missingState : {false, true}) {
    reset();
    if (missingState) il2cpp::set_field<void*>(fixtureMenu, 0x70, nullptr);
    else il2cpp::set_field<bool>(menuState, 0x38, false);
    CHECK(!pg::menu_panel::main_menu_visible());
    pg::menu_panel::Panel panel;
    CHECK(!pg::menu_panel::open(panel, "Consent", "", {"Accept"}));
    pg::menu_panel::close(panel);
  }
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
  il2cpp::set_field<bool>(menuState, 0x38, false);
  activeDecor->active = false;
  CHECK(pg::menu_panel::tick(panel) == -1);
  CHECK(!pg::menu_panel::is_open(panel));
  CHECK(!activeDecor->active && !inactiveDecor->active);
  pg::menu_panel::close(panel);
  CHECK(!activeDecor->active && !inactiveDecor->active);
}
/** Reopening replaces the owned clone; invalid input leaves no live panel or native damage. */
void reopen_and_empty_buttons() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "First", "", {"OK"}));
  auto* first = static_cast<Node*>(panel.root);
  CHECK(pg::menu_panel::open(panel, "Second", "", {"OK"}));
  CHECK(first && !first->alive);
  CHECK(panel.root && panel.root != first);
  auto* second = static_cast<Node*>(panel.root);
  CHECK(!pg::menu_panel::open(panel, "Empty", "", {}));
  CHECK(second && !second->alive);
  CHECK(!pg::menu_panel::is_open(panel));
  CHECK(pg::menu_panel::tick(panel) == -1);
  CHECK(pg::menu_panel::row_clicked(panel) == -1);
  CHECK(popupSource->alive && !popupSource->active);
  pg::menu_panel::close(panel);
}
/** Optional popup metadata cannot prevent the consent overlay from seeing the focused menu. */
void optional_popup_metadata() {
  reset();
  unavailableClass = "ErrorManager";
  CHECK(pg::menu_panel::resolve());
  CHECK(pg::menu_panel::main_menu_visible());
  CHECK(pg::menu_panel::menu_camera() == boardCamera);
  pg::menu_panel::Panel panel;
  CHECK(!pg::menu_panel::open(panel, "Consent", "", {"Accept"}));
  CHECK(!pg::menu_panel::is_open(panel));
  for (const char* required : {"ConnectToServer", "Canvas", "CameraFocusUI"}) {
    reset();
    unavailableClass = required;
    CHECK(pg::menu_panel::resolve());
    CHECK(!pg::menu_panel::main_menu_visible());
    CHECK(!pg::menu_panel::open(panel, "Consent", "", {"Accept"}));
  }
}
/** Consent uses the inset paper area at a readable fixed size for both short and long copy. */
void readable_popup_body() {
  for (float nativeSize : {24.0f, 32.0f}) {
    reset();
    auto* paper = static_cast<Node*>(pg::game::find_child(popupSource, "Error_Text box/Background"));
    auto* nativeBody = static_cast<Node*>(pg::game::find_child(paper, "Error Message"));
    paper->size = {0.8f, 0.5f};
    nativeBody->fontSize = nativeSize;
    nativeBody->fontMax = nativeSize;
    nativeBody->autoSize = true;
    pg::menu_panel::Panel panel;
    CHECK(pg::menu_panel::open(panel, "Consent", "Short notice", {"Accept", "Decline"}));
    auto* body = static_cast<Node*>(panel.body);
    CHECK(body);
    if (!body) return;
    CHECK(!body->autoSize);
    CHECK(body->fontSize == nativeSize * 0.75f);
    CHECK(body->alignment == pg::ui::Align::TopLeft && body->wrap);
    const float left = body->anchor.x * paper->size.x + body->anchored.x - body->pivot.x * body->size.x;
    const float bottom = body->anchor.y * paper->size.y + body->anchored.y - body->pivot.y * body->size.y;
    CHECK(left > 0 && bottom > 0);
    CHECK(left + body->size.x < paper->size.x && bottom + body->size.y < paper->size.y);
    CHECK(body->size.x > paper->size.x * 0.8f && body->size.y > paper->size.y * 0.8f);
    const float readableSize = body->fontSize;
    const std::string longNotice = std::string(400, 'L') + "\nSecond paragraph.";
    pg::menu_panel::set_text(panel.body, longNotice);
    CHECK(body->text == longNotice);
    CHECK(!body->autoSize && body->fontSize == readableSize);
    pg::menu_panel::set_text(panel.body, "");
    CHECK(body->fontSize == readableSize);
    CHECK(nativeBody->fontSize == nativeSize && nativeBody->autoSize);
    pg::menu_panel::close(panel);
  }
}
/** Separate name and description columns remain aligned and readable regardless of content. */
void consistent_row_typography() {
  for (float nativeSize : {0.01f, 0.014f}) {
    reset();
    auto* nativeBody = static_cast<Node*>(pg::game::find_child(
        popupSource, "Error_Text box/Background/Error Message"));
    nativeBody->fontSize = nativeSize;
    nativeBody->fontMax = nativeSize;
    pg::menu_panel::Panel panel;
    CHECK(pg::menu_panel::open(panel, "Mods", std::string(80, 's'), {"Close"}));
    if (!panel.body) return;
    CHECK(static_cast<Node*>(panel.body)->fontSize < nativeSize);
    CHECK(pg::menu_panel::add_row(panel, 0, 2, "Short mod", "Short description"));
    pg::menu_panel::set_text(panel.body, std::string(100, 's'));
    const std::string longDescription(120, 'L');
    CHECK(pg::menu_panel::add_row(panel, 1, 2, "Longer mod name", longDescription));
    CHECK(panel.rows.size() == 2);
    if (panel.rows.size() == 2) {
      for (size_t i = 0; i < panel.rows.size(); ++i) {
        const auto& row = panel.rows[i];
        auto* name = static_cast<Node*>(row.name);
        auto* description = static_cast<Node*>(row.description);
        auto* hit = static_cast<Node*>(row.hit);
        CHECK(name && description && hit);
        if (!name || !description || !hit) continue;
        CHECK(name->parent == hit && description->parent == hit);
        CHECK(name->alignment == pg::ui::Align::Left && description->alignment == pg::ui::Align::Left);
        CHECK(!name->autoSize && !description->autoSize && description->wrap);
        CHECK(name->fontSize == nativeSize && description->fontSize == nativeSize * 0.75f);
        CHECK(name->text == (i == 0 ? "Short mod" : "Longer mod name"));
        CHECK(description->text == (i == 0 ? "Short description" : longDescription));
        const float nameLeft = name->anchor.x * hit->size.x + name->anchored.x - name->pivot.x * name->size.x;
        const float descriptionLeft = description->anchor.x * hit->size.x + description->anchored.x -
                                      description->pivot.x * description->size.x;
        CHECK(nameLeft >= 0 && nameLeft + name->size.x <= descriptionLeft);
        CHECK(descriptionLeft + description->size.x <= hit->size.x);
        hitTarget = hit;
        CHECK(pg::menu_panel::row_clicked(panel) == static_cast<int>(i));
      }
      auto* firstName = static_cast<Node*>(panel.rows[0].name);
      auto* secondName = static_cast<Node*>(panel.rows[1].name);
      auto* firstDescription = static_cast<Node*>(panel.rows[0].description);
      auto* secondDescription = static_cast<Node*>(panel.rows[1].description);
      CHECK(firstName->anchored.x == secondName->anchored.x && firstName->size.x == secondName->size.x);
      CHECK(firstDescription->anchored.x == secondDescription->anchored.x &&
            firstDescription->size.x == secondDescription->size.x);
    }
    pg::menu_panel::close(panel);
  }
}
/** Checkbox graphics are copied without the native toggle and only the copied mark is switched. */
void row_checkboxes() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
  CHECK(pg::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  CHECK(panel.rows.size() == 1);
  if (!panel.rows.empty()) {
    const auto& row = panel.rows[0];
    auto* mark = static_cast<Node*>(row.checkmark);
    CHECK(mark && mark != checkboxMark && mark->copiedFrom == checkboxMark);
    if (mark) {
      CHECK(mark->parent && mark->parent->copiedFrom == checkboxBackground);
      CHECK(mark->parent && mark->parent->parent == row.hit);
      pg::menu_panel::set_row_checked(panel, 0, false);
      CHECK(!mark->active && checkboxMark->active);
      pg::menu_panel::set_row_checked(panel, 0, true);
      CHECK(mark->active && checkboxMark->active);
      const pg::ui::Color shown = mark->rendererColor;
      CHECK(shown.r == 0.3f && shown.g == 0.4f && shown.b == 0.5f && shown.a == 1);
      CHECK(checkboxMark->rendererColor.a == 0);
      pg::menu_panel::set_row_checked(panel, 1, false);
      CHECK(mark->active);
    }
  }
  pg::menu_panel::close(panel);
  for (bool missingMark : {false, true}) {
    reset();
    CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
    (missingMark ? checkboxMark : checkboxBackground)->name = "Unavailable";
    CHECK(!pg::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
    CHECK(panel.rows.empty());
    pg::menu_panel::close(panel);
    CHECK(activeDecor->active && !inactiveDecor->active);
  }
}
/** Everything an open panel keeps between frames stays reachable for the GC until it is closed. */
void kept_objects_are_held() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  CHECK(pg::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  std::vector<void*> kept{panel.root, panel.title, panel.body, panel.paper};
  for (const auto& button : panel.buttons) kept.insert(kept.end(), {button.image, button.animator});
  for (void* label : panel.buttonTexts) kept.push_back(label);
  for (const auto& row : panel.rows) kept.insert(kept.end(), {row.hit, row.name, row.description, row.checkmark});
  CHECK(kept.size() == 14);
  for (void* object : kept) CHECK(object && held(object));
  CHECK(held(activeDecor) && held(fixtureMenu));  // the hidden decoration and its menu, restored on close
  pg::menu_panel::close(panel);
  for (void* object : holds) CHECK(!object);
}
/** A native error popup already shrunk by its own content still supplies its intended font size. */
void source_autosize_does_not_shrink_mod_text() {
  reset();
  auto* nativeBody = static_cast<Node*>(pg::game::find_child(
      popupSource, "Error_Text box/Background/Error Message"));
  nativeBody->autoSize = true;
  nativeBody->fontSize = 0.0035f;
  nativeBody->fontMax = 0.01f;
  pg::menu_panel::Panel panel;
  CHECK(pg::menu_panel::open(panel, "Notice", "Readable notice", {"Close"}));
  auto* body = static_cast<Node*>(panel.body);
  CHECK(body && !body->autoSize && body->fontSize == 0.0075f);
  CHECK(pg::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  if (!panel.rows.empty()) {
    CHECK(static_cast<Node*>(panel.rows[0].name)->fontSize == 0.01f);
    CHECK(static_cast<Node*>(panel.rows[0].description)->fontSize == 0.0075f);
  }
  CHECK(nativeBody->autoSize && nativeBody->fontSize == 0.0035f && nativeBody->fontMax == 0.01f);
  pg::menu_panel::close(panel);
}
/** Bounds expressed from the parent's bottom-left, as RectTransform anchors and pivots define. */
pg::ui::Rect bounds(const Node* item, pg::ui::Vec2 parentSize) {
  return {item->anchor.x * parentSize.x + item->anchored.x - item->pivot.x * item->size.x,
          item->anchor.y * parentSize.y + item->anchored.y - item->pivot.y * item->size.y,
          item->size.x, item->size.y};
}
/** Five- and six-mod menus keep every row within the paper and clear of the status footer. */
void multi_row_geometry() {
  constexpr float tolerance = 0.000001f;
  for (int count : {5, 6}) {
    reset();
    auto* nativePaper = static_cast<Node*>(pg::game::find_child(popupSource, "Error_Text box/Background"));
    nativePaper->size = {0.2f, 0.17f};
    auto* nativeBody = static_cast<Node*>(pg::game::find_child(nativePaper, "Error Message"));
    nativeBody->fontSize = nativeBody->fontMax = 0.01f;
    pg::menu_panel::Panel panel;
    CHECK(pg::menu_panel::open(panel, "Mods", "Apply to save changes.", {"Apply", "Close"}));
    for (int index = 0; index < count; ++index)
      CHECK(pg::menu_panel::add_row(panel, index, count, "Mod " + std::to_string(index),
                                   "First line\nSecond line"));
    CHECK(panel.rows.size() == static_cast<size_t>(count));
    const auto footer = bounds(static_cast<Node*>(panel.body), nativePaper->size);
    CHECK(footer.x > 0 && footer.y > 0 && footer.width > 0 && footer.height > 0);
    CHECK(footer.height >= static_cast<Node*>(panel.body)->fontSize);
    float previousBottom = nativePaper->size.y;
    for (const auto& row : panel.rows) {
      auto* hit = static_cast<Node*>(row.hit);
      const auto area = bounds(hit, nativePaper->size);
      CHECK(area.x > 0 && area.x + area.width < nativePaper->size.x);
      CHECK(area.width > nativePaper->size.x * 0.8f && area.height > 0);
      CHECK(area.y >= footer.y + footer.height - tolerance);
      CHECK(area.y + area.height <= previousBottom + tolerance);
      previousBottom = area.y;
      for (void* cell : {row.name.get(), row.description.get(), static_cast<void*>(static_cast<Node*>(row.checkmark)->parent)}) {
        auto* item = static_cast<Node*>(cell);
        const auto cellArea = bounds(item, hit->size);
        CHECK(cellArea.x >= -tolerance && cellArea.y >= -tolerance);
        CHECK(cellArea.width > 0 && cellArea.height > 0);
        CHECK(cellArea.x + cellArea.width <= hit->size.x + tolerance);
        CHECK(cellArea.y + cellArea.height <= hit->size.y + tolerance);
      }
      CHECK(static_cast<Node*>(row.name)->fontSize <= area.height);
      CHECK(static_cast<Node*>(row.description)->fontSize * 2 <= area.height);
    }
    pg::menu_panel::close(panel);
  }
}
/** Invalid row indices/counts reject without creating partial content or affecting a later row. */
void invalid_row_arguments() {
  reset();
  pg::menu_panel::Panel panel;
  CHECK(!pg::menu_panel::add_row(panel, 0, 1, "Missing panel", ""));
  CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
  const auto childrenBefore = static_cast<Node*>(panel.paper)->children.size();
  CHECK(!pg::menu_panel::add_row(panel, 0, 0, "Zero count", ""));
  CHECK(!pg::menu_panel::add_row(panel, 0, -1, "Negative count", ""));
  CHECK(!pg::menu_panel::add_row(panel, -1, 1, "Negative index", ""));
  CHECK(!pg::menu_panel::add_row(panel, 1, 1, "Index past end", ""));
  CHECK(panel.rows.empty());
  CHECK(static_cast<Node*>(panel.paper)->children.size() == childrenBefore);
  CHECK(pg::menu_panel::add_row(panel, 0, 1, "Valid", ""));
  CHECK(panel.rows.size() == 1);
  pg::menu_panel::close(panel);
}
/** Every overlapping popup owns suppression until the final owner closes, in either order. */
void decor_lifetime() {
  for (bool reverse : {false, true}) {
    reset();
    pg::menu_panel::Panel first, second;
    CHECK(pg::menu_panel::open(first, "Consent", "", {"OK"}));
    CHECK(!activeDecor->active && !inactiveDecor->active && unlistedDecor->active);
    CHECK(pg::menu_panel::open(second, "Notice", "", {"OK"}));
    CHECK(!activeDecor->active && !inactiveDecor->active);
    pg::menu_panel::close(reverse ? second : first);
    CHECK(!activeDecor->active && !inactiveDecor->active);
    pg::menu_panel::close(reverse ? first : second);
    CHECK(activeDecor->active && !inactiveDecor->active && unlistedDecor->active);
    pg::menu_panel::close(first);
    pg::menu_panel::close(second);
    CHECK(activeDecor->active && !inactiveDecor->active);
  }
}
/** Failed opens never acquire decor ownership or release another successful panel's ownership. */
void decor_failed_open() {
  reset();
  pg::menu_panel::Panel first, failed;
  CHECK(!pg::menu_panel::open(failed, "Empty", "", {}));
  CHECK(activeDecor->active && !inactiveDecor->active);
  CHECK(pg::menu_panel::open(first, "Consent", "", {"OK"}));
  auto* title = static_cast<Node*>(pg::game::find_child(popupSource, "Error Title tape"));
  CHECK(title);
  if (title) title->name = "Missing title";
  CHECK(!pg::menu_panel::open(failed, "Invalid", "", {"OK"}));
  CHECK(!activeDecor->active && !inactiveDecor->active);
  pg::menu_panel::close(first);
  CHECK(activeDecor->active && !inactiveDecor->active);
  CHECK(!pg::menu_panel::open(failed, "Invalid", "", {"OK"}));
  CHECK(activeDecor->active && !inactiveDecor->active);
}
/** Losing focus and external root destruction both restore decor; dead decor receives no writes. */
void decor_abnormal_close() {
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    pg::menu_panel::Panel panel;
    CHECK(pg::menu_panel::open(panel, "Mods", "", {"Close"}));
    CHECK(!activeDecor->active);
    if (mode == 0) {
      wall->focus->focused = false;
      CHECK(pg::menu_panel::tick(panel) == -1);
      CHECK(activeDecor->active && !inactiveDecor->active);
    } else if (mode == 1) {
      pg::game::destroy(panel.root);
      pg::menu_panel::tick_message();
      CHECK(activeDecor->active && !inactiveDecor->active);
    } else {
      activeDecor->alive = false;
      pg::menu_panel::close(panel);
      CHECK(!inactiveDecor->active);
    }
    pg::menu_panel::close(panel);
  }
}
}

int main() {
  native_transform_survives();
  board_camera_hits();
  hidden_controls_ignore_input();
  hover_plays_native_animation();
  hover_restarts_after_hiding();
  menu_visibility();
  aggregate_menu_visibility();
  buttons_follow_menu_state();
  reopen_and_empty_buttons();
  optional_popup_metadata();
  readable_popup_body();
  consistent_row_typography();
  row_checkboxes();
  kept_objects_are_held();
  source_autosize_does_not_shrink_mod_text();
  multi_row_geometry();
  invalid_row_arguments();
  decor_lifetime();
  decor_failed_open();
  decor_abnormal_close();
  return g_failures ? 1 : 0;
}
