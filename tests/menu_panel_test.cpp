#include <windows.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "JournalController.h"
#include "Doozy.h"
#include "MainMenu.h"
#include "Transform.h"
#include "UnityEvents.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "check.h"
#include "text_fixture.h"
#include "fake_game.h"
#include "game_api.h"
#include "host_game.h"
#include "il2cpp.h"
#include "runtime.h"
#include "runtime_services.h"
#include "ui.h"
#include "ui_fixture.h"

namespace {
// The class of a script's kind, which the fake's is_instance compares with an object's first pointer.
void* class_named(const char* name) {
  static std::map<std::string, int> classes;
  return &classes[name];
}
struct Node {
  void* klassPointer = nullptr;  // an object's first pointer
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
  bool on = false, interactable = true;  // a Toggle's isOn and a Selectable's interactable
  float fontSize = 24, fontMin = 24, fontMax = 24;
  pm::ui::Align alignment = pm::ui::Align::Center;
  phasmo::Vector3 position{7, 4, 2}, scale{0.02f, 0.02f, 0.02f};
  phasmo::Quaternion rotation{0, -0.70710677f, 0, 0.70710677f};
  pm::ui::Vec2 size{0.2f, 0.04f}, anchored{0, -0.1f};
  pm::ui::Vec2 anchor{0.5f, 0.5f}, pivot{0.5f, 0.5f};
  pm::ui::Color color{0.6f, 0.5f, 0.4f, 1};
  std::string text;
  std::vector<Node*> children;
  std::vector<Node*> scripts;  // MonoBehaviours on this object (klass is the script's class)
  std::vector<int> played;     // a colour animator's Play(state) calls, oldest first
};
std::vector<std::unique_ptr<Node>> nodes;
Node *fixtureManager, *fixtureMenu, *wall, *popupSource, *boardCamera, *playerCamera;
Node* menuState;
Node *activeDecor, *inactiveDecor, *unlistedDecor;
Node *checkboxToggle, *checkboxBackground, *checkboxMark;
bool locked = true;
int freedCursor = 0;

// Where the fixture keeps what the map's fields hold, inside a Node's fields (the first pointer is the class).
constexpr int32_t kBehavioursField = 0x40;      // UISelectable.Behaviours, on a UIButton
constexpr int32_t kBehaviourListField = 0x10;   // DoozyBehaviours.Behaviours
constexpr int32_t kRunnersField = 0x18;         // ModyEventBase.Runners
constexpr int32_t kEventField = 0x20;           // ModyEvent.Event
constexpr int32_t kOnValueChangedField = 0x48;  // Toggle.onValueChanged
constexpr int32_t kGroupField = 0x50;           // Toggle.m_Group
constexpr int32_t kPointerClick = 4;            // DoozyBehaviourName.PointerClick
constexpr int32_t kDisabledAnimationField = 0x28;  // UISelectableColorAnimator.DisabledAnimation
constexpr int32_t kAnimationField = 0x30;          // ColorAnimation.Animation
constexpr int32_t kToCustomValueField = 0x38;      // ColorTargetReaction.ToCustomValue

// The game's events: the persistent listeners each has (saved with the scene), the runtime ones AddListener gave it,
// and how often the game's own action (a persistent listener) ran.
std::map<const void*, int> persistentOf;
std::map<const void*, std::vector<void*>> listenersOf;
int gameActionRuns = 0;
int eventCtors = 0;
// The loader's delegates, as runtime_services::delegate_new made them.
struct Delegate {
  std::string ctorKey;
  std::string invokeKey;
  void* target;
  pm::runtime_services::NativeListener listener;
};
std::map<const void*, Delegate> delegates;
std::unique_ptr<Node> nextAction;

Node* add_script(Node* object, const char* klass) {
  auto* script = ui_fixture::make_node(nodes, klass);
  script->klass = klass;
  script->klassPointer = class_named(klass);
  object->scripts.push_back(script);
  return script;
}
Node* script_of(void* object, const char* klass) {
  for (auto* script : object ? static_cast<Node*>(object)->scripts : std::vector<Node*>{})
    if (std::strcmp(script->klass, klass) == 0) return script;
  return nullptr;
}
// A game event, a node the fake's UnityEvent stands for, with `persistent` listeners saved with the scene.
Node* game_event(int persistent) {
  auto* event = ui_fixture::make_node(nodes, "UnityEvent");
  event->klassPointer = class_named("UnityEvent");
  persistentOf[event] = persistent;
  return event;
}
// One Doozy behaviour (a ModyEvent) named `name`, its event with `persistent` listeners and `runners` runners.
Node* behaviour(const char* name, int persistent, int runners) {
  auto* b = ui_fixture::make_node(nodes, name);
  b->klassPointer = class_named("ModyEvent");
  il2cpp::set_field<void*>(b, kEventField, game_event(persistent));
  std::vector<void*> list;
  for (int i = 0; i < runners; ++i) list.push_back(ui_fixture::make_node(nodes, "runner"));
  il2cpp::set_field<void*>(b, kRunnersField, pm::fake::list(list));
  return b;
}
// A Doozy button as the scene saves News's or the error popup's: its click behaviour runs the game's one action (a
// persistent listener) and a runner of Doozy's, and its enter behaviour has a listener of the game's too.
Node* doozy_button(Node* object) {
  Node* button = add_script(object, "UIButton");
  auto* behaviours = ui_fixture::make_node(nodes, "behaviours");
  il2cpp::set_field<void*>(behaviours, kBehaviourListField,
                           pm::fake::list({behaviour("PointerClick", 1, 1), behaviour("PointerEnter", 1, 0)}));
  il2cpp::set_field<void*>(button, kBehavioursField, behaviours);
  return button;
}
std::vector<Node*> behaviours_of(const void* button) {
  void* behaviours = button ? il2cpp::field<void*>(button, kBehavioursField) : nullptr;
  const pm::api::List list{behaviours ? il2cpp::field<void*>(behaviours, kBehaviourListField) : nullptr};
  std::vector<Node*> out;
  for (size_t i = 0; i < list.size(); ++i) out.push_back(static_cast<Node*>(list[i]));
  return out;
}
Node* click_behaviour(const void* button) {
  for (Node* b : behaviours_of(button))
    if (std::string(b->name) == "PointerClick") return b;
  return nullptr;
}
// Object.Instantiate copies a script's saved classes by value: the copy's behaviours, events and runners are its own,
// with the same persistent listeners. Runtime listeners are not saved, so the copy has none.
void copy_behaviours(Node* copy, const Node* original) {
  std::vector<void*> list;
  for (Node* b : behaviours_of(original)) {
    auto* c = ui_fixture::make_node(nodes, b->name);
    c->klassPointer = b->klassPointer;
    c->copiedFrom = b;
    il2cpp::set_field<void*>(c, kEventField, game_event(persistentOf[il2cpp::field<void*>(b, kEventField)]));
    std::vector<void*> runners;
    for (size_t i = 0, n = pm::api::List{il2cpp::field<void*>(b, kRunnersField)}.size(); i < n; ++i)
      runners.push_back(ui_fixture::make_node(nodes, "runner"));
    il2cpp::set_field<void*>(c, kRunnersField, pm::fake::list(runners));
    list.push_back(c);
  }
  auto* behaviours = ui_fixture::make_node(nodes, "behaviours");
  il2cpp::set_field<void*>(behaviours, kBehaviourListField, pm::fake::list(list));
  il2cpp::set_field<void*>(copy, kBehavioursField, behaviours);
}
// A tape's colour animator whose disabled state ends on `disabled`: its animation and reaction, saved by value.
Node* colour_animator(Node* object, phasmo::Color disabled) {
  Node* animator = add_script(object, "UISelectableColorAnimator");
  auto* animation = ui_fixture::make_node(nodes, "ColorAnimation");
  auto* reaction = ui_fixture::make_node(nodes, "ColorTargetReaction");
  il2cpp::set_field<phasmo::Color>(reaction, kToCustomValueField, disabled);
  il2cpp::set_field<void*>(animation, kAnimationField, reaction);
  il2cpp::set_field<void*>(animator, kDisabledAnimationField, animation);
  return animator;
}
Node* disabled_reaction_of(Node* object) {
  Node* animator = script_of(object, "UISelectableColorAnimator");
  auto* animation = animator ? il2cpp::field<Node*>(animator, kDisabledAnimationField) : nullptr;
  return animation ? il2cpp::field<Node*>(animation, kAnimationField) : nullptr;
}
phasmo::Color disabled_colour_of(Node* object) {
  Node* reaction = disabled_reaction_of(object);
  return reaction ? il2cpp::field<phasmo::Color>(reaction, kToCustomValueField) : phasmo::Color{};
}
// Object.Instantiate copies an animator's animations by value: the copy's animation and reaction are its own.
void copy_animation(Node* copy) {
  auto* animation = il2cpp::field<Node*>(copy, kDisabledAnimationField);
  if (!animation) return;
  auto* reaction = il2cpp::field<Node*>(animation, kAnimationField);
  auto* animationCopy = ui_fixture::make_node(nodes, "ColorAnimation");
  *animationCopy = *animation;
  if (reaction) {
    auto* reactionCopy = ui_fixture::make_node(nodes, "ColorTargetReaction");
    *reactionCopy = *reaction;
    il2cpp::set_field<void*>(animationCopy, kAnimationField, reactionCopy);
  }
  il2cpp::set_field<void*>(copy, kDisabledAnimationField, animationCopy);
}
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
    if (std::strcmp(script->klass, "UIButton") == 0) copy_behaviours(copy, script);
    if (std::strcmp(script->klass, "UISelectableColorAnimator") == 0) copy_animation(copy);
    result->scripts.push_back(copy);
  }
  return result;
}
// The game clicks a Doozy button: its click behaviour runs its event, the persistent listeners (the game's action)
// first, then the runtime ones (the loader's delegates call their listener with their target).
void game_click(const void* button) {
  Node* click = click_behaviour(button);
  void* event = click ? il2cpp::field<void*>(click, kEventField) : nullptr;
  if (!event) return;
  gameActionRuns += persistentOf[event];
  for (void* action : listenersOf[event]) {
    const Delegate& d = delegates.at(action);
    d.listener(d.target, nullptr);
  }
}
// The game clicks a Toggle: it flips isOn and raises its change event (no listener of the loader's is on it).
void game_toggle(Node* toggle) {
  toggle->on = !toggle->on;
  gameActionRuns += persistentOf[il2cpp::field<void*>(toggle, kOnValueChangedField)];
}

void play_state(void* animator, int state, const void*) { static_cast<Node*>(animator)->played.push_back(state); }
int sceneSearches = 0;  // how many times the code looked through the scene for an object
void* find_object(void* type, bool, const void*) {
  ++sceneSearches;
  return type == phasmo::ErrorManager::type.type_object() ? fixtureManager : nullptr;
}
void* get_instance(const void*) { return fixtureMenu; }
void* world_camera(void* value, const void*) { return static_cast<Node*>(value)->camera; }
bool focused(void* value, const void*) { return static_cast<Node*>(value)->focused; }
pm::ui::Vec2 size_delta(void* value, const void*) { return static_cast<Node*>(value)->size; }
pm::ui::Vec2 anchored_position(void* value, const void*) { return static_cast<Node*>(value)->anchored; }
void event_ctor(void*, const void*) { ++eventCtors; }
int32_t persistent_count(void* event, const void*) { return persistentOf[event]; }
void add_listener(void* event, void* action, const void*) { listenersOf[event].push_back(action); }
void* get_behaviour(void* button, int32_t name, const void*) {
  return name == kPointerClick ? click_behaviour(button) : nullptr;
}
bool is_on(void* toggle, const void*) { return static_cast<Node*>(toggle)->on; }
void set_on_quietly(void* toggle, bool on, const void*) { static_cast<Node*>(toggle)->on = on; }
void set_interactable(void* control, bool on, const void*) { static_cast<Node*>(control)->interactable = on; }
void stands_in(const void*) {}
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
ULONGLONG fixtureTime = 1000;
/** Advance production message time without depending on the wall clock. */
ULONGLONG WINAPI clock_ms() { return fixtureTime; }
}

#define GetTickCount64 clock_ms
#include "../host/menu_panel.cpp"
#undef GetTickCount64

namespace il2cpp {
void log_exception(const char*, const void*) {}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}

namespace pm::runtime_services {
// The runtime services' delegate: the fixture notes its constructor's entry, its target and its listener.
void* delegate_new(const char* ctorKey, const char* invokeKey, void* target, NativeListener listener) {
  if (!ctorKey || !invokeKey || !target || !listener) return nullptr;
  auto* action = ui_fixture::make_node(nodes, "UnityAction");
  delegates[action] = {ctorKey, invokeKey, target, listener};
  return action;
}
}  // namespace pm::runtime_services

namespace pm::hostgame {
bool alive(void* value) { return value && static_cast<Node*>(value)->alive; }
void* transform_of(void* value) { return value; }
void* object_transform(void* value) { return value; }
void* game_object_of(void* value) { return value; }
void* parent(void* value) { return value ? static_cast<Node*>(value)->parent : nullptr; }
void set_local_scale(void* value, phasmo::Vector3 scale) { static_cast<Node*>(value)->scale = scale; }
bool throwWhileCopying = false;  // Object.Instantiate raises a managed exception, as a real copy can
void* instantiate(void* original, void* parent, bool world) {
  if (throwWhileCopying) throw Il2CppExceptionWrapper{nullptr};
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
void* get_component(void* value, void* type) {
  if (!value) return nullptr;
  auto* item = static_cast<Node*>(value);
  if (!type) return nullptr;
  if (type == phasmo::Canvas::type.type_object()) return item->canvas;
  if (type == phasmo::CameraFocusUI::type.type_object()) return item->focus;
  if (type == phasmo::TextMeshProUGUI::type.type_object() || type == phasmo::Image::type.type_object()) return item;
  if (type == phasmo::UIButton::type.type_object()) return script_of(item, "UIButton");
  // The fixture's Toggle is its object, which a "Toggle" script marks.
  if (type == phasmo::Toggle::type.type_object()) return script_of(item, "Toggle") ? item : nullptr;
  return type == phasmo::UISelectableColorAnimator::type.type_object() ? script_of(item, "UISelectableColorAnimator") : nullptr;
}
/** GetComponents(Behaviour): the object's scripts, as a managed array. */
void* components(void* value, void* type) {
  if (!type) return nullptr;
  const auto& scripts = static_cast<Node*>(value)->scripts;
  void* array = pm::fake::array(sizeof(void*), scripts.size());
  for (size_t i = 0; i < scripts.size(); ++i) pm::api::RefArray{array}.set(i, scripts[i]);
  return array;
}
void destroy(void* value) {
  auto* item = static_cast<Node*>(value);
  item->alive = false;
  for (auto* script : item->scripts) script->alive = false;
  for (auto* child : item->children) destroy(child);
}
bool cursor_locked() { return locked; }
void free_cursor() { ++freedCursor; locked = false; }
}

namespace pm::ui {
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
}
namespace pm::log { void write(const char*, ...) {} }

namespace {
// Every entry the panel reads, each to the fixture's stand-in for it.
void give(const pm::sdk::EntryDesc* without) {
  namespace f = pm::fake;
  namespace m = pm::map;
  const auto method = [](const pm::sdk::EntryDesc& d, auto fn) { f::method(d, reinterpret_cast<void*>(fn)); };
  method(m::UnityObject_FindObjectOfType, &find_object);
  method(m::ConnectToServer_get_Instance, &get_instance);
  method(m::Canvas_get_worldCamera, &world_camera);
  method(m::CameraFocusUI_get_isFocused, &focused);
  method(m::RectTransform_get_sizeDelta, &size_delta);
  method(m::RectTransform_get_anchoredPosition, &anchored_position);
  method(m::UISelectableColorAnimator_Play, &play_state);
  method(m::UnityEvent_ctor, &event_ctor);
  method(m::UnityEvent_AddListener, &add_listener);
  method(m::UnityEventBase_GetPersistentEventCount, &persistent_count);
  method(m::UISelectable_GetBehaviour, &get_behaviour);
  method(m::ToggleEvent_ctor, &event_ctor);
  method(m::Toggle_get_isOn, &is_on);
  method(m::Toggle_SetIsOnWithoutNotify, &set_on_quietly);
  method(m::Selectable_set_interactable, &set_interactable);
  for (const pm::sdk::EntryDesc* d : {&m::Canvas, &m::CameraFocusUI, &m::ErrorManager, &m::Behaviour, &m::TextMeshProUGUI,
                                      &m::Image, &m::UIButton, &m::TextLocaliserUI, &m::Button, &m::UISelectableColorAnimator,
                                      &m::GameObject, &m::UnityEvent, &m::ModyEvent, &m::Toggle, &m::ToggleEvent})
    f::type(*d, class_named(d->key));
  // The fixture's own hostgame helpers (set_enabled, components, the child walk, instantiate, set_local_scale) and its
  // runtime services (delegate_new, behind UnityAction.ctor) stand in; these only say the entries resolved.
  for (const pm::sdk::EntryDesc* d : {&m::Behaviour_set_enabled, &m::GameObject_GetComponents, &m::UnityObject_Instantiate,
                                      &m::GameObject_get_transform, &m::Component_get_transform,
                                      &m::Component_get_gameObject, &m::Transform_get_childCount, &m::Transform_GetChild,
                                      &m::Transform_set_localScale, &m::UnityAction_ctor, &m::UnityAction_Invoke})
    method(*d, &stands_in);
  f::field(m::ConnectToServer_menuDecor, 0x60);
  f::field(m::ConnectToServer_menuObjects, 0x70);
  f::field(m::MenuObjects_enabled, 0x38);
  f::field(m::ErrorManager_errorParent, 0x20);
  f::field(m::UISelectable_Behaviours, kBehavioursField);
  f::field(m::DoozyBehaviours_Behaviours, kBehaviourListField);
  f::field(m::ModyEventBase_Runners, kRunnersField);
  f::field(m::ModyEvent_Event, kEventField);
  f::field(m::Toggle_onValueChanged, kOnValueChangedField);
  f::field(m::Toggle_m_Group, kGroupField);
  f::field(m::UISelectableColorAnimator_DisabledAnimation, kDisabledAnimationField);
  f::field(m::ColorAnimation_Animation, kAnimationField);
  f::field(m::ColorTargetReaction_ToCustomValue, kToCustomValueField);
  f::value(m::DoozyBehaviourName_PointerClick, kPointerClick);
  if (without) f::forget(*without);
  std::string missing;
  pm::api::bind(f::host(), missing);
}
void reset() {
  pm::fake::reset();
  nodes.clear();
  holds.clear();
  persistentOf.clear();
  listenersOf.clear();
  delegates.clear();
  gameActionRuns = eventCtors = 0;
  wall = ui_fixture::make_node(nodes, "Wall Corkboard");
  wall->focus = ui_fixture::make_node(nodes, "focus");
  fixtureMenu = ui_fixture::make_node(nodes, "Main Menu", ui_fixture::make_node(nodes, "Menu", wall));
  menuState = ui_fixture::make_node(nodes, "Main menu state");
  il2cpp::set_field(fixtureMenu, 0x70, menuState);
  il2cpp::set_field<bool>(menuState, 0x38, true);
  checkboxToggle = ui_fixture::make_node(nodes, "Toggle.1",
      ui_fixture::make_node(nodes, "Hardware", ui_fixture::make_node(nodes, "Create Lobby", fixtureMenu->parent)));
  add_script(checkboxToggle, "Toggle");
  il2cpp::set_field<void*>(checkboxToggle, kOnValueChangedField, game_event(1));
  checkboxBackground = ui_fixture::make_node(nodes, "Background", checkboxToggle);
  checkboxMark = ui_fixture::make_node(nodes, "Checkmark", checkboxBackground);
  activeDecor = ui_fixture::make_node(nodes, "Pin and string", fixtureMenu);
  inactiveDecor = ui_fixture::make_node(nodes, "Hidden pin", fixtureMenu);
  unlistedDecor = ui_fixture::make_node(nodes, "Background", fixtureMenu);
  activeDecor->klass = inactiveDecor->klass = unlistedDecor->klass = "GameObject";
  inactiveDecor->active = false;
  il2cpp::set_field(fixtureMenu, 0x60, pm::fake::list({activeDecor, inactiveDecor}));
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
  doozy_button(accept);
  add_script(accept, "UISelectableColorAnimator");
  il2cpp::set_field(fixtureManager, 0x20, popupSource);
  locked = true;
  freedCursor = 0;
  give(nullptr);
}
Node* button_of(const pm::menu_panel::Control& c) { return static_cast<Node*>(c.button.get()); }
bool same(phasmo::Vector3 a, phasmo::Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same(phasmo::Quaternion a, phasmo::Quaternion b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
/** A copied popup keeps the native board-plane geometry across open and subsequent ticks. */
void native_transform_survives() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Consent", "Read before playing", {"Accept", "Decline"}));
  CHECK(panel.root);
  if (!panel.root) return;
  auto* root = static_cast<Node*>(panel.root);
  CHECK(root != popupSource && root->parent == popupSource->parent);
  CHECK(same(root->position, popupSource->position));
  CHECK(same(root->rotation, popupSource->rotation));
  CHECK(same(root->scale, popupSource->scale));
  playerCamera->position = {-13, 18, 5};
  CHECK(pm::menu_panel::tick(panel) == -1);
  CHECK(same(root->position, popupSource->position));
  CHECK(same(root->rotation, popupSource->rotation));
  CHECK(same(root->scale, popupSource->scale));
  CHECK(!popupSource->active);
  pm::menu_panel::close(panel);
}
/** A copy of the News tape keeps the game's Doozy button switched on, with none of News's listeners: every behaviour
    has a new event with no persistent listener and no runners, and the click behaviour's has the loader's one
    listener, a UnityAction bound to the copy's button. News itself is untouched. */
void copied_tape_keeps_game_button_and_drops_its_listeners() {
  reset();
  auto* news = ui_fixture::make_node(nodes, "News", fixtureMenu);
  Node* newsButton = doozy_button(news);
  add_script(news, "UISelectableColorAnimator");
  add_script(news, "TextLocaliserUI")->klassPointer = class_named(pm::map::TextLocaliserUI.key);
  newsButton->klassPointer = class_named(pm::map::UIButton.key);
  const std::vector<Node*> newsBehaviours = behaviours_of(newsButton);
  std::vector<void*> newsEvents;
  for (Node* b : newsBehaviours) newsEvents.push_back(il2cpp::field<void*>(b, kEventField));

  auto* copy = static_cast<Node*>(pm::hostgame::instantiate(news, fixtureMenu, false));
  pm::menu_panel::make_inert(copy);
  pm::menu_panel::Control control;
  CHECK(pm::menu_panel::clickable(control, copy));
  Node* button = script_of(copy, "UIButton");
  CHECK(button && button != newsButton && button_of(control) == button);
  if (!button) return;
  CHECK(button->enabled);                                       // the game's button works
  CHECK(!script_of(copy, "TextLocaliserUI")->enabled);          // the copy's localiser does not
  CHECK(script_of(copy, "UISelectableColorAnimator")->enabled);  // the game colours it
  const std::vector<Node*> copied = behaviours_of(button);
  CHECK(copied.size() == 2);
  for (Node* b : copied) {
    void* event = il2cpp::field<void*>(b, kEventField);
    CHECK(event && persistentOf[event] == 0);  // a new event: the game's listeners are not on it
    CHECK(std::find(newsEvents.begin(), newsEvents.end(), event) == newsEvents.end());
    CHECK(pm::api::List{il2cpp::field<void*>(b, kRunnersField)}.size() == 0);
  }
  void* clickEvent = il2cpp::field<void*>(click_behaviour(button), kEventField);
  CHECK(listenersOf[clickEvent].size() == 1);
  if (listenersOf[clickEvent].size() == 1) {
    const Delegate& d = delegates.at(listenersOf[clickEvent][0]);
    CHECK(d.ctorKey == pm::map::UnityAction_ctor.key && d.target == button);
  }
  for (Node* b : copied)
    if (b != click_behaviour(button)) CHECK(listenersOf[il2cpp::field<void*>(b, kEventField)].empty());
  // News keeps its own: the same events, with the game's listener and runner, and nothing of the loader's.
  CHECK(behaviours_of(newsButton) == newsBehaviours);
  for (size_t i = 0; i < newsBehaviours.size(); ++i) {
    CHECK(il2cpp::field<void*>(newsBehaviours[i], kEventField) == newsEvents[i] && persistentOf[newsEvents[i]] == 1);
    CHECK(listenersOf[newsEvents[i]].empty());
  }
  CHECK(pm::api::List{il2cpp::field<void*>(click_behaviour(newsButton), kRunnersField)}.size() == 1);
  // The game clicks the copy: the training does not start, the loader's listener counts it.
  game_click(button);
  CHECK(gameActionRuns == 0);
  CHECK(pm::menu_panel::clicked(control));
  game_click(newsButton);  // the game's own News still does its own
  CHECK(gameActionRuns == 1 && !pm::menu_panel::clicked(control));
  pm::menu_panel::release(control);

  // A button with no behaviours, no click behaviour, or a behaviour that is no ModyEvent (whose listeners could not be
  // taken off) is not made clickable, and stays switched off.
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    auto* source = ui_fixture::make_node(nodes, "News", fixtureMenu);
    Node* sourceButton = doozy_button(source);
    auto* again = static_cast<Node*>(pm::hostgame::instantiate(source, fixtureMenu, false));
    Node* copiedButton = script_of(again, "UIButton");
    pm::menu_panel::make_inert(again);
    copiedButton->enabled = false;
    if (mode == 0) il2cpp::set_field<void*>(copiedButton, kBehavioursField, nullptr);
    if (mode == 1) click_behaviour(copiedButton)->name = "PointerDown";
    if (mode == 2) behaviours_of(copiedButton).back()->klassPointer = class_named("Other");
    pm::menu_panel::Control refused;
    CHECK(!pm::menu_panel::clickable(refused, again));
    CHECK(!copiedButton->enabled && !refused.button && delegates.empty());
    CHECK(sourceButton);
  }
}
/** One click of the game's runs the loader's action once: clicked() takes it, and the panel's tick answers it once. A
    click on a control the loader let go of, or on a destroyed copy, counts for nothing. */
void click_event_runs_loader_action_once() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  if (panel.buttons.size() != 2) return;
  Node* apply = button_of(panel.buttons[0]);
  game_click(apply);
  CHECK(pm::menu_panel::tick(panel) == 0);
  CHECK(pm::menu_panel::tick(panel) == -1);
  CHECK(pm::menu_panel::tick(panel) == -1);
  // Two clicks before a frame are still one action: the panel answers once.
  game_click(apply);
  game_click(apply);
  CHECK(pm::menu_panel::tick(panel) == 0);
  CHECK(pm::menu_panel::tick(panel) == -1);
  CHECK(gameActionRuns == 0);
  pm::menu_panel::close(panel);
  CHECK(!panel.buttons.size());
  game_click(apply);  // its listener's count went with the panel
  pm::menu_panel::Control stale;
  stale.button = il2cpp::Held(apply);
  CHECK(!pm::menu_panel::clicked(stale));
}
/** The game's clicks reach the panel: a button's through the loader's listener, a row's through its toggle. */
void game_clicks_reach_the_panel() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  if (panel.buttons.size() != 2) return;
  CHECK(pm::menu_panel::menu_camera() == boardCamera);
  game_click(button_of(panel.buttons[1]));
  CHECK(pm::menu_panel::tick(panel) == 1);
  CHECK(pm::menu_panel::tick(panel) == -1);  // taken
  game_click(button_of(panel.buttons[0]));
  CHECK(pm::menu_panel::tick(panel) == 0);
  CHECK(pm::menu_panel::add_row(panel, 0, 1, "Sample", "Sample description"));
  CHECK(pm::menu_panel::row_clicked(panel) == -1);
  game_toggle(static_cast<Node*>(panel.rows[0].toggle.get()));
  CHECK(pm::menu_panel::row_clicked(panel) == 0);
  pm::menu_panel::set_row_checked(panel, 0, false);  // the host sets it back to its choice
  CHECK(pm::menu_panel::row_clicked(panel) == -1);
  CHECK(gameActionRuns == 0);
  pm::menu_panel::close(panel);
}
/** A panel out of view closes, and nothing clicked on it counts. */
void hidden_controls_ignore_input() {
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    pm::menu_panel::Panel panel;
    CHECK(pm::menu_panel::open(panel, "Consent", "", {"Accept"}));
    if (!panel.root || panel.buttons.empty()) return;
    CHECK(pm::menu_panel::add_row(panel, 0, 1, "Sample", "Sample description"));
    Node* button = button_of(panel.buttons[0]);
    Node* toggle = static_cast<Node*>(panel.rows[0].toggle.get());
    if (mode == 0) fixtureMenu->active = false;
    if (mode == 1) wall->focus->focused = false;
    if (mode == 2) static_cast<Node*>(panel.root)->active = false;
    game_click(button);
    game_toggle(toggle);
    CHECK(pm::menu_panel::row_clicked(panel) == -1);
    CHECK(pm::menu_panel::tick(panel) == -1);
    CHECK(!pm::menu_panel::is_open(panel));
    CHECK(pm::menu_panel::row_clicked(panel) == -1);
    CHECK(freedCursor == 0);
    pm::menu_panel::close(panel);
  }
}
/** The copies keep the game's colour animator on: the game plays it, the host never does. */
void copies_keep_the_game_animator() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  CHECK(panel.buttons.size() == 2);
  if (panel.buttons.size() != 2) return;
  for (const auto& c : panel.buttons) {
    Node* button = button_of(c);
    CHECK(button && button->enabled);
    Node* object = nullptr;
    for (auto& n : nodes)
      for (Node* s : n->scripts)
        if (s == button) object = n.get();
    Node* animator = script_of(object, "UISelectableColorAnimator");
    CHECK(animator && animator->enabled && animator->played.empty());
  }
  game_click(button_of(panel.buttons[0]));
  pm::menu_panel::tick(panel);
  for (auto& n : nodes) CHECK(n->played.empty());
  pm::menu_panel::close(panel);
}
/** A copied tape takes the disabled colour of the game's tape it is matched to, on its own animation only: the
    original it was copied from and the tape it matched keep theirs. Without an entry, or without an animator on
    either, nothing changes. */
void copy_takes_the_disabled_look() {
  const phasmo::Color seeThrough{1, 1, 1, 0.5f}, opaque{1, 1, 1, 1};
  const auto same_colour = [](phasmo::Color a, phasmo::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };
  const auto make = [&](Node*& news, Node*& training, Node*& copy) {
    reset();
    news = ui_fixture::make_node(nodes, "News", fixtureMenu);
    news->active = false;
    colour_animator(news, seeThrough);
    training = ui_fixture::make_node(nodes, "Training button", fixtureMenu);
    colour_animator(training, opaque);
    copy = static_cast<Node*>(pm::hostgame::instantiate(news, fixtureMenu, false));
  };
  Node *news, *training, *copy;
  make(news, training, copy);
  CHECK(disabled_reaction_of(copy) && disabled_reaction_of(copy) != disabled_reaction_of(news));
  CHECK(pm::menu_panel::match_disabled_look(copy, training));
  CHECK(same_colour(disabled_colour_of(copy), opaque));
  CHECK(same_colour(disabled_colour_of(news), seeThrough));
  CHECK(same_colour(disabled_colour_of(training), opaque));
  CHECK(!copy->active);  // it does not wake the copy
  for (const pm::sdk::EntryDesc* d : {&pm::map::UISelectableColorAnimator_DisabledAnimation, &pm::map::ColorAnimation_Animation,
                                      &pm::map::ColorTargetReaction_ToCustomValue, &pm::map::UISelectableColorAnimator}) {
    make(news, training, copy);
    give(d);
    CHECK(!pm::menu_panel::match_disabled_look(copy, training));
    CHECK(same_colour(disabled_colour_of(copy), seeThrough));
  }
  make(news, training, copy);
  CHECK(!pm::menu_panel::match_disabled_look(copy, nullptr));
  CHECK(!pm::menu_panel::match_disabled_look(copy, fixtureMenu));  // no animator there
  CHECK(same_colour(disabled_colour_of(copy), seeThrough));
}
/** The entries the host does not call but a mod may: each wrapper reaches the game's method with its arguments, and
    without its entry calls nothing and gives its default. */
int g_reached = 0;
const void* g_reachedSelf = nullptr;
void* g_reachedArgument = nullptr;
int32_t g_reachedState = -1;
void reached_with_event(void* self, void* eventData, const void*) {
  ++g_reached;
  g_reachedSelf = self;
  g_reachedArgument = eventData;
}
void reached_alone(void* self, const void*) {
  ++g_reached;
  g_reachedSelf = self;
}
void* reached_with_state(void* self, int32_t state, const void*) {
  ++g_reached;
  g_reachedSelf = self;
  g_reachedState = state;
  return self;
}
void entries_for_mods_reach_the_game() {
  namespace f = pm::fake;
  namespace m = pm::map;
  const auto give_them = [&](const pm::sdk::EntryDesc* without) {
    reset();
    f::method(m::UIButton_OnPointerClick, reinterpret_cast<void*>(&reached_with_event));
    f::method(m::UIButton_OnSubmit, reinterpret_cast<void*>(&reached_with_event));
    f::method(m::UnityEvent_Invoke, reinterpret_cast<void*>(&reached_alone));
    f::method(m::UISelectable_SetState, reinterpret_cast<void*>(&reached_with_state));
    if (without) f::forget(*without);
    std::string missing;
    pm::api::bind(f::host(), missing);
    g_reached = 0;
    g_reachedSelf = g_reachedArgument = nullptr;
    g_reachedState = -1;
  };
  int self = 0, eventData = 0;
  give_them(nullptr);
  phasmo::UIButton::OnPointerClick(static_cast<void*>(&self), static_cast<void*>(&eventData));
  CHECK(g_reached == 1 && g_reachedSelf == &self && g_reachedArgument == &eventData);
  phasmo::UIButton::OnSubmit(static_cast<void*>(&self), static_cast<void*>(&eventData));
  CHECK(g_reached == 2);
  phasmo::UnityEvent::Invoke(&self);
  CHECK(g_reached == 3 && g_reachedSelf == &self);
  CHECK(phasmo::UISelectable::SetState(static_cast<void*>(&self), 2).raw() == &self && g_reached == 4 &&
        g_reachedState == 2);
  for (const pm::sdk::EntryDesc* d : {&m::UIButton_OnPointerClick, &m::UIButton_OnSubmit, &m::UnityEvent_Invoke,
                                      &m::UISelectable_SetState}) {
    give_them(d);
    if (d == &m::UIButton_OnPointerClick)
      phasmo::UIButton::OnPointerClick(static_cast<void*>(&self), static_cast<void*>(&eventData));
    if (d == &m::UIButton_OnSubmit)
      phasmo::UIButton::OnSubmit(static_cast<void*>(&self), static_cast<void*>(&eventData));
    if (d == &m::UnityEvent_Invoke) phasmo::UnityEvent::Invoke(&self);
    if (d == &m::UISelectable_SetState) CHECK(!phasmo::UISelectable::SetState(static_cast<void*>(&self), 2));
    CHECK(g_reached == 0);
  }
  // Play stays the game's, for a mod: the fixture's animator notes the state.
  reset();
  auto* animator = ui_fixture::make_node(nodes, "UISelectableColorAnimator");
  phasmo::UISelectableColorAnimator::Play(static_cast<void*>(animator), 2);
  CHECK((animator->played == std::vector<int>{2}));
}
/** Visibility requires the focused menu and its live, enabled canvas camera. */
void menu_visibility() {
  reset();
  CHECK(pm::menu_panel::main_menu_visible() && !pm::menu_panel::why_menu_hidden());
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
    CHECK(!pm::menu_panel::main_menu_visible() && pm::menu_panel::why_menu_hidden());
    pm::menu_panel::Panel panel;
    CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}));
    CHECK(!pm::menu_panel::is_open(panel));
    CHECK(freedCursor == 0);
  }
  reset();
  boardCamera->alive = false;
  CHECK(!pm::menu_panel::main_menu_visible());
}
/** The game's buttons follow its menu state, not where the camera looks (the map, the shop). */
void buttons_follow_menu_state() {
  for (int mode = 0; mode < 2; ++mode) {
    reset();
    if (mode == 0) wall->focus->focused = false;
    if (mode == 1) boardCamera->enabled = false;
    CHECK(pm::menu_panel::menu_buttons_shown() && !pm::menu_panel::main_menu_visible());
  }
  for (int mode = 0; mode < 4; ++mode) {
    reset();
    if (mode == 0) il2cpp::set_field<bool>(menuState, 0x38, false);  // Connecting
    if (mode == 1) il2cpp::set_field<void*>(fixtureMenu, 0x70, nullptr);
    if (mode == 2) fixtureMenu->active = false;
    if (mode == 3) fixtureMenu = nullptr;
    CHECK(!pm::menu_panel::menu_buttons_shown());
  }
}
/** An active focused board can be Connecting; the native aggregate menu state remains decisive. */
void aggregate_menu_visibility() {
  reset();
  menuState->alive = false;  // Managed state is not a Unity object with native lifetime.
  CHECK(pm::menu_panel::main_menu_visible());
  for (bool missingState : {false, true}) {
    reset();
    if (missingState) il2cpp::set_field<void*>(fixtureMenu, 0x70, nullptr);
    else il2cpp::set_field<bool>(menuState, 0x38, false);
    CHECK(!pm::menu_panel::main_menu_visible());
    pm::menu_panel::Panel panel;
    CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}));
    pm::menu_panel::close(panel);
  }
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
  il2cpp::set_field<bool>(menuState, 0x38, false);
  activeDecor->active = false;
  CHECK(pm::menu_panel::tick(panel) == -1);
  CHECK(!pm::menu_panel::is_open(panel));
  CHECK(!activeDecor->active && !inactiveDecor->active);
  pm::menu_panel::close(panel);
  CHECK(!activeDecor->active && !inactiveDecor->active);
}
/** Reopening replaces the owned clone; invalid input leaves no live panel or native damage. */
void reopen_and_empty_buttons() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "First", "", {"OK"}));
  auto* first = static_cast<Node*>(panel.root);
  CHECK(pm::menu_panel::open(panel, "Second", "", {"OK"}));
  CHECK(first && !first->alive);
  CHECK(panel.root && panel.root != first);
  auto* second = static_cast<Node*>(panel.root);
  CHECK(!pm::menu_panel::open(panel, "Empty", "", {}));
  CHECK(second && !second->alive);
  CHECK(!pm::menu_panel::is_open(panel));
  CHECK(pm::menu_panel::tick(panel) == -1);
  CHECK(pm::menu_panel::row_clicked(panel) == -1);
  CHECK(popupSource->alive && !popupSource->active);
  pm::menu_panel::close(panel);
}
/** The popup's entries cannot stop the menu reading as in view; without one, no popup can ever be made, and open says
    the failure lasts, so the consent notice answers Declined at once (consent_ui_test). */
void optional_popup_metadata() {
  reset();
  give(&pm::map::ErrorManager);
  CHECK(pm::menu_panel::main_menu_visible());
  CHECK(pm::menu_panel::menu_camera() == boardCamera);
  pm::menu_panel::Panel panel;
  pm::menu_panel::OpenFailure failure;
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure));
  CHECK(failure.lasting && failure.why);
  CHECK(!pm::menu_panel::is_open(panel));
  // Without any of these the menu never reads as in view, and menu_available() says so: the notice answers Declined
  // (consent_ui_test) rather than waiting for good. The menu objects' class is found by shape, so an update can make it
  // ambiguous; its two fields name it, so they are then blocked too.
  for (const pm::sdk::EntryDesc* required : {&pm::map::ConnectToServer_get_Instance, &pm::map::Canvas, &pm::map::CameraFocusUI,
                                             &pm::map::Canvas_get_worldCamera, &pm::map::CameraFocusUI_get_isFocused,
                                             &pm::map::ConnectToServer_menuObjects,
                                             &pm::map::MenuObjects_enabled}) {
    reset();
    pm::fake::forget(*required);
    std::string missing;
    pm::api::bind(pm::fake::host(), missing);
    CHECK(!pm::menu_panel::menu_available());
    CHECK(!pm::menu_panel::main_menu_visible());
    CHECK(!pm::menu_panel::menu_buttons_shown());
    CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}));
  }
  // A copy whose scripts it cannot tell would keep acting for the game, and one whose button cannot take the loader's
  // listener (or lose the game's) would run the game's action or nothing: no copy is made, ever, in this build.
  for (const pm::sdk::EntryDesc* kind : {&pm::map::UIButton, &pm::map::TextLocaliserUI, &pm::map::Button, &pm::map::Behaviour,
                                         &pm::map::Behaviour_set_enabled, &pm::map::GameObject_GetComponents,
                                         &pm::map::ErrorManager_errorParent, &pm::map::UnityObject_FindObjectOfType,
                                         &pm::map::TextMeshProUGUI, &pm::map::Image, &pm::map::RectTransform_get_sizeDelta,
                                         &pm::map::RectTransform_get_anchoredPosition, &pm::map::UnityObject_Instantiate,
                                         &pm::map::GameObject, &pm::map::GameObject_get_transform,
                                         &pm::map::Component_get_transform, &pm::map::Component_get_gameObject,
                                         &pm::map::Transform_get_childCount, &pm::map::Transform_GetChild,
                                         &pm::map::UnityEvent, &pm::map::UnityEvent_ctor, &pm::map::UnityEvent_AddListener,
                                         &pm::map::UnityEventBase_GetPersistentEventCount, &pm::map::UnityAction_ctor,
                                         &pm::map::UISelectable_Behaviours, &pm::map::UISelectable_GetBehaviour,
                                         &pm::map::DoozyBehaviours_Behaviours, &pm::map::DoozyBehaviourName_PointerClick,
                                         &pm::map::ModyEvent, &pm::map::ModyEvent_Event, &pm::map::ModyEventBase_Runners}) {
    reset();
    pm::fake::forget(*kind);
    std::string missing;
    pm::api::bind(pm::fake::host(), missing);
    CHECK(!pm::menu_panel::can_copy());
    failure = {};
    CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure));
    CHECK(failure.lasting);
    CHECK(delegates.empty());
  }
  // Without these no control of the loader's can take a click: the notice answers Declined (consent_ui_test).
  CHECK(pm::menu_panel::listeners_available());
  for (const pm::sdk::EntryDesc* d : {&pm::map::UnityEvent_AddListener, &pm::map::UnityEventBase_GetPersistentEventCount,
                                      &pm::map::UnityAction_ctor, &pm::map::UnityAction_Invoke}) {
    reset();
    give(d);
    CHECK(!pm::menu_panel::listeners_available());
  }
  // The colour animator alone is optional: the copy is made and clicked as with it.
  reset();
  pm::fake::forget(pm::map::UISelectableColorAnimator_Play);
  std::string missing;
  pm::api::bind(pm::fake::host(), missing);
  CHECK(pm::menu_panel::can_copy());
  CHECK(pm::menu_panel::open(panel, "Consent", "", {"Accept"}));
  if (!panel.buttons.empty()) {
    game_click(button_of(panel.buttons[0]));
    CHECK(pm::menu_panel::tick(panel) == 0);
  }
  pm::menu_panel::close(panel);
}
/** open says whether a failure can pass. The menu out of view, or the game's popup not found in the scene, may change
    as the game runs; a popup or button that lacks what a copy needs is the build's, and asking again makes none. */
void open_says_whether_a_failure_lasts() {
  pm::menu_panel::Panel panel;
  pm::menu_panel::OpenFailure failure;
  reset();
  wall->focus->focused = false;
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure) && !failure.lasting && failure.why);
  reset();
  fixtureManager = nullptr;  // FindObjectOfType finds no ErrorManager
  failure = {};
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure) && !failure.lasting && failure.why);
  reset();
  il2cpp::set_field<void*>(fixtureManager, 0x20, nullptr);  // the manager holds no popup yet
  failure = {};
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure) && !failure.lasting);
  reset();
  for (auto* child : popupSource->children)
    if (std::string(child->name) == "Error Accept Button") child->name = "Renamed Button";
  failure = {};
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure) && failure.lasting && failure.why);
  CHECK(!pm::menu_panel::is_open(panel));
  reset();
  for (auto* child : popupSource->children)
    if (std::string(child->name) == "Error Accept Button")
      behaviours_of(script_of(child, "UIButton")).back()->klassPointer = class_named("Other");  // clickable refuses
  failure = {};
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure) && failure.lasting && failure.why);
  CHECK(!pm::menu_panel::is_open(panel) && delegates.empty());
  failure = {};
  CHECK(!pm::menu_panel::open(panel, "Consent", "", {}, &failure) && failure.lasting);
  reset();
  CHECK(pm::menu_panel::open(panel, "Consent", "", {"Accept"}, &failure));
  pm::menu_panel::close(panel);
}
/** Consent uses the inset paper area at a readable fixed size for both short and long copy. */
void readable_popup_body() {
  for (float nativeSize : {24.0f, 32.0f}) {
    reset();
    auto* paper = static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background"));
    auto* nativeBody = static_cast<Node*>(pm::hostgame::find_child(paper, "Error Message"));
    paper->size = {0.8f, 0.5f};
    nativeBody->fontSize = nativeSize;
    nativeBody->fontMax = nativeSize;
    nativeBody->autoSize = true;
    pm::menu_panel::Panel panel;
    CHECK(pm::menu_panel::open(panel, "Consent", "Short notice", {"Accept", "Decline"}));
    auto* body = static_cast<Node*>(panel.body);
    CHECK(body);
    if (!body) return;
    CHECK(!body->autoSize);
    CHECK(body->fontSize == nativeSize * 2.0f * 0.75f);  // twice the game's: its size suits a short error on a small box
    CHECK(body->alignment == pm::ui::Align::TopLeft && body->wrap);
    const float left = body->anchor.x * paper->size.x + body->anchored.x - body->pivot.x * body->size.x;
    const float bottom = body->anchor.y * paper->size.y + body->anchored.y - body->pivot.y * body->size.y;
    CHECK(left > 0 && bottom > 0);
    CHECK(left + body->size.x < paper->size.x && bottom + body->size.y < paper->size.y);
    CHECK(body->size.x > paper->size.x * 0.8f && body->size.y > paper->size.y * 0.8f);
    const float readableSize = body->fontSize;
    const std::string longNotice = std::string(400, 'L') + "\nSecond paragraph.";
    pm::menu_panel::set_text(panel.body, longNotice);
    CHECK(body->text == longNotice);
    CHECK(!body->autoSize && body->fontSize == readableSize);
    pm::menu_panel::set_text(panel.body, "");
    CHECK(body->fontSize == readableSize);
    CHECK(nativeBody->fontSize == nativeSize && nativeBody->autoSize);
    pm::menu_panel::close(panel);
  }
}
/** Separate name and description columns remain aligned and readable regardless of content. */
void consistent_row_typography() {
  for (float nativeSize : {0.01f, 0.014f}) {
    reset();
    auto* nativeBody = static_cast<Node*>(pm::hostgame::find_child(
        popupSource, "Error_Text box/Background/Error Message"));
    nativeBody->fontSize = nativeSize;
    nativeBody->fontMax = nativeSize;
    static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background"))->size = {0.8f, 0.5f};
    pm::menu_panel::Panel panel;
    CHECK(pm::menu_panel::open(panel, "Mods", std::string(80, 's'), {"Close"}));
    if (!panel.body) return;
    CHECK(static_cast<Node*>(panel.body)->fontSize < nativeSize * 2.0f);
    CHECK(pm::menu_panel::add_row(panel, 0, 2, "Short mod", "Short description"));
    pm::menu_panel::set_text(panel.body, std::string(100, 's'));
    const std::string longDescription(120, 'L');
    CHECK(pm::menu_panel::add_row(panel, 1, 2, "Longer mod name", longDescription));
    CHECK(panel.rows.size() == 2);
    if (panel.rows.size() == 2) {
      for (size_t i = 0; i < panel.rows.size(); ++i) {
        const auto& row = panel.rows[i];
        auto* name = static_cast<Node*>(row.name);
        auto* description = static_cast<Node*>(row.description);
        auto* toggle = static_cast<Node*>(row.toggle);
        CHECK(name && description && toggle);
        if (!name || !description || !toggle) continue;
        CHECK(name->parent == toggle && description->parent == toggle);
        CHECK(name->alignment == pm::ui::Align::Left && description->alignment == pm::ui::Align::Left);
        CHECK(!name->autoSize && !description->autoSize && description->wrap);
        CHECK(name->fontSize == nativeSize * 2.0f && description->fontSize == nativeSize * 2.0f * 0.75f);
        CHECK(name->text == (i == 0 ? "Short mod" : "Longer mod name"));
        CHECK(description->text == (i == 0 ? "Short description" : longDescription));
        const float nameLeft = name->anchor.x * toggle->size.x + name->anchored.x - name->pivot.x * name->size.x;
        const float descriptionLeft = description->anchor.x * toggle->size.x + description->anchored.x -
                                      description->pivot.x * description->size.x;
        CHECK(nameLeft >= 0 && nameLeft + name->size.x <= descriptionLeft);
        CHECK(descriptionLeft + description->size.x <= toggle->size.x);
        game_toggle(toggle);
        CHECK(pm::menu_panel::row_clicked(panel) == static_cast<int>(i));
        pm::menu_panel::set_row_checked(panel, i, false);
      }
      auto* firstName = static_cast<Node*>(panel.rows[0].name);
      auto* secondName = static_cast<Node*>(panel.rows[1].name);
      auto* firstDescription = static_cast<Node*>(panel.rows[0].description);
      auto* secondDescription = static_cast<Node*>(panel.rows[1].description);
      CHECK(firstName->anchored.x == secondName->anchored.x && firstName->size.x == secondName->size.x);
      CHECK(firstDescription->anchored.x == secondDescription->anchored.x &&
            firstDescription->size.x == secondDescription->size.x);
    }
    pm::menu_panel::close(panel);
  }
}
/** A row is a copy of the lobby paper's Toggle with a new change event and no group: the game's toggle flips its tick,
    the host sets it without raising the event, and none of the game's listeners runs. */
void row_checkboxes() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
  checkboxToggle->active = checkboxBackground->active = checkboxMark->active = false;
  checkboxBackground->enabled = checkboxMark->enabled = false;
  CHECK(pm::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  CHECK(panel.rows.size() == 1);
  if (!panel.rows.empty()) {
    auto* toggle = static_cast<Node*>(panel.rows[0].toggle.get());
    CHECK(toggle && toggle != checkboxToggle && toggle->copiedFrom == checkboxToggle);
    if (toggle) {
      void* event = il2cpp::field<void*>(toggle, kOnValueChangedField);
      CHECK(event && event != il2cpp::field<void*>(checkboxToggle, kOnValueChangedField) && persistentOf[event] == 0);
      CHECK(!il2cpp::field<void*>(toggle, kGroupField) && toggle->interactable);
      auto* box = static_cast<Node*>(pm::hostgame::find_child(toggle, "Background"));
      auto* mark = static_cast<Node*>(pm::hostgame::find_child(toggle, "Background/Checkmark"));
      CHECK(box && mark);
      // Switched on whatever the lobby's own were when it was copied, which stay as they were.
      CHECK(toggle->active && box && box->active && box->enabled && mark && mark->active && mark->enabled);
      CHECK(!checkboxToggle->active && !checkboxBackground->active && !checkboxBackground->enabled);
      CHECK(!checkboxMark->active && !checkboxMark->enabled);
      pm::menu_panel::set_row_checked(panel, 0, true);
      CHECK(toggle->on && !checkboxToggle->on && panel.rows[0].shownOn);
      CHECK(pm::menu_panel::row_clicked(panel) == -1);
      game_toggle(toggle);
      CHECK(pm::menu_panel::row_clicked(panel) == 0);
      CHECK(gameActionRuns == 0);
      pm::menu_panel::set_rows_interactable(panel, false);
      CHECK(!toggle->interactable);
      pm::menu_panel::set_row_checked(panel, 1, false);  // no such row
      CHECK(!toggle->on);
    }
  }
  pm::menu_panel::close(panel);
  // Without the checkbox's parts, or with the game's toggle in a group (its copy would join it and switch the game's
  // toggles off), no row is made.
  for (int mode = 0; mode < 4; ++mode) {
    reset();
    CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
    if (mode == 0) checkboxMark->name = "Unavailable";
    if (mode == 1) checkboxBackground->name = "Unavailable";
    if (mode == 2) checkboxToggle->scripts.clear();
    if (mode == 3) il2cpp::set_field<void*>(checkboxToggle, kGroupField, ui_fixture::make_node(nodes, "ToggleGroup"));
    const auto& children = static_cast<Node*>(panel.paper)->children;
    const size_t childrenBefore = children.size();
    CHECK(!pm::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
    CHECK(panel.rows.empty());
    CHECK(children.size() == childrenBefore);  // the source is checked before anything is copied
    pm::menu_panel::close(panel);
    CHECK(activeDecor->active && !inactiveDecor->active);
  }
  // Without an entry the rows use, none is made.
  for (const pm::sdk::EntryDesc* d : {&pm::map::Toggle, &pm::map::Toggle_m_Group, &pm::map::Toggle_onValueChanged,
                                      &pm::map::Toggle_get_isOn, &pm::map::Toggle_SetIsOnWithoutNotify,
                                      &pm::map::ToggleEvent, &pm::map::ToggleEvent_ctor,
                                      &pm::map::Selectable_set_interactable, &pm::map::Transform_set_localScale}) {
    reset();
    give(d);
    CHECK(!pm::menu_panel::can_copy_rows());
    CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
    CHECK(!pm::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
    pm::menu_panel::close(panel);
  }
}
/** Everything an open panel keeps between frames stays reachable for the GC until it is closed. */
void kept_objects_are_held() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Apply", "Close"}));
  CHECK(pm::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  std::vector<void*> kept{panel.root, panel.title, panel.body, panel.paper};
  for (const auto& button : panel.buttons) kept.push_back(button.button);
  for (void* label : panel.buttonTexts) kept.push_back(label);
  for (const auto& row : panel.rows) kept.insert(kept.end(), {row.toggle, row.name, row.description});
  CHECK(kept.size() == 11);
  for (void* object : kept) CHECK(object && held(object));
  CHECK(held(activeDecor) && held(fixtureMenu));  // the hidden decoration and its menu, restored on close
  pm::menu_panel::close(panel);
  for (void* object : holds) CHECK(!object);
}
/** However many rows the panel holds, a row's text stays within its height rather than running into the next row. */
void row_text_stays_within_its_row() {
  reset();
  auto* nativeBody = static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background/Error Message"));
  nativeBody->fontSize = nativeBody->fontMax = 0.01f;
  static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background"))->size = {0.8f, 0.5f};
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
  constexpr int kMany = 40;
  CHECK(pm::menu_panel::add_row(panel, 0, kMany, "Sample", "Description"));
  if (!panel.rows.empty()) {
    auto* name = static_cast<Node*>(panel.rows[0].name);
    CHECK(name->fontSize < 0.02f && name->fontSize <= name->size.y * 0.4f + 0.0001f);
    CHECK(static_cast<Node*>(panel.rows[0].description)->fontSize == name->fontSize * 0.75f);
  }
  pm::menu_panel::close(panel);
}
/** A native error popup already shrunk by its own content still supplies its intended font size. */
void source_autosize_does_not_shrink_mod_text() {
  reset();
  auto* nativeBody = static_cast<Node*>(pm::hostgame::find_child(
      popupSource, "Error_Text box/Background/Error Message"));
  nativeBody->autoSize = true;
  nativeBody->fontSize = 0.0035f;
  nativeBody->fontMax = 0.01f;
  static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background"))->size = {0.8f, 0.5f};
  pm::menu_panel::Panel panel;
  CHECK(pm::menu_panel::open(panel, "Notice", "Readable notice", {"Close"}));
  auto* body = static_cast<Node*>(panel.body);
  CHECK(body && !body->autoSize && body->fontSize == 0.015f);
  CHECK(pm::menu_panel::add_row(panel, 0, 1, "Sample", "Description"));
  if (!panel.rows.empty()) {
    CHECK(static_cast<Node*>(panel.rows[0].name)->fontSize == 0.02f);
    CHECK(static_cast<Node*>(panel.rows[0].description)->fontSize == 0.015f);
  }
  CHECK(nativeBody->autoSize && nativeBody->fontSize == 0.0035f && nativeBody->fontMax == 0.01f);
  pm::menu_panel::close(panel);
}
/** Bounds expressed from the parent's bottom-left, as RectTransform anchors and pivots define. */
pm::ui::Rect bounds(const Node* item, pm::ui::Vec2 parentSize) {
  return {item->anchor.x * parentSize.x + item->anchored.x - item->pivot.x * item->size.x,
          item->anchor.y * parentSize.y + item->anchored.y - item->pivot.y * item->size.y,
          item->size.x, item->size.y};
}
/** Five- and six-mod menus keep every row within the paper and clear of the status footer. */
void multi_row_geometry() {
  constexpr float tolerance = 0.000001f;
  for (int count : {5, 6}) {
    reset();
    auto* nativePaper = static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error_Text box/Background"));
    nativePaper->size = {0.2f, 0.17f};
    auto* nativeBody = static_cast<Node*>(pm::hostgame::find_child(nativePaper, "Error Message"));
    nativeBody->fontSize = nativeBody->fontMax = 0.01f;
    pm::menu_panel::Panel panel;
    CHECK(pm::menu_panel::open(panel, "Mods", "Apply to save changes.", {"Apply", "Close"}));
    for (int index = 0; index < count; ++index)
      CHECK(pm::menu_panel::add_row(panel, index, count, "Mod " + std::to_string(index),
                                   "First line\nSecond line"));
    CHECK(panel.rows.size() == static_cast<size_t>(count));
    const auto footer = bounds(static_cast<Node*>(panel.body), nativePaper->size);
    CHECK(footer.x > 0 && footer.y > 0 && footer.width > 0 && footer.height > 0);
    CHECK(footer.height >= static_cast<Node*>(panel.body)->fontSize);
    float previousBottom = nativePaper->size.y;
    for (const auto& row : panel.rows) {
      auto* toggle = static_cast<Node*>(row.toggle);
      const auto area = bounds(toggle, nativePaper->size);
      CHECK(area.x > 0 && area.x + area.width < nativePaper->size.x);
      CHECK(area.width > nativePaper->size.x * 0.8f && area.height > 0);
      CHECK(area.y >= footer.y + footer.height - tolerance);
      CHECK(area.y + area.height <= previousBottom + tolerance);
      previousBottom = area.y;
      for (void* cell : {row.name.get(), pm::hostgame::find_child(toggle, "Background")}) {
        auto* item = static_cast<Node*>(cell);
        const auto cellArea = bounds(item, toggle->size);
        CHECK(cellArea.x >= -tolerance && cellArea.y >= -tolerance);
        CHECK(cellArea.width > 0 && cellArea.height > 0);
        CHECK(cellArea.x + cellArea.width <= toggle->size.x + tolerance);
        CHECK(cellArea.y + cellArea.height <= toggle->size.y + tolerance);
      }
      auto* description = static_cast<Node*>(row.description);
      const auto descriptionArea = bounds(description, toggle->size);
      CHECK(descriptionArea.x + descriptionArea.width <= toggle->size.x + tolerance);
      CHECK(static_cast<Node*>(row.name)->fontSize <= area.height);
      CHECK(description->fontSize * 2 <= area.height);
    }
    pm::menu_panel::close(panel);
  }
}
/** Invalid row indices/counts reject without creating partial content or affecting a later row. */
void invalid_row_arguments() {
  reset();
  pm::menu_panel::Panel panel;
  CHECK(!pm::menu_panel::add_row(panel, 0, 1, "Missing panel", ""));
  CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
  const auto childrenBefore = static_cast<Node*>(panel.paper)->children.size();
  CHECK(!pm::menu_panel::add_row(panel, 0, 0, "Zero count", ""));
  CHECK(!pm::menu_panel::add_row(panel, 0, -1, "Negative count", ""));
  CHECK(!pm::menu_panel::add_row(panel, -1, 1, "Negative index", ""));
  CHECK(!pm::menu_panel::add_row(panel, 1, 1, "Index past end", ""));
  CHECK(panel.rows.empty());
  CHECK(static_cast<Node*>(panel.paper)->children.size() == childrenBefore);
  CHECK(pm::menu_panel::add_row(panel, 0, 1, "Valid", ""));
  CHECK(panel.rows.size() == 1);
  pm::menu_panel::close(panel);
}
/** Every overlapping popup owns suppression until the final owner closes, in either order. */
void decor_lifetime() {
  for (bool reverse : {false, true}) {
    reset();
    pm::menu_panel::Panel first, second;
    CHECK(pm::menu_panel::open(first, "Consent", "", {"OK"}));
    CHECK(!activeDecor->active && !inactiveDecor->active && unlistedDecor->active);
    CHECK(pm::menu_panel::open(second, "Notice", "", {"OK"}));
    CHECK(!activeDecor->active && !inactiveDecor->active);
    pm::menu_panel::close(reverse ? second : first);
    CHECK(!activeDecor->active && !inactiveDecor->active);
    pm::menu_panel::close(reverse ? first : second);
    CHECK(activeDecor->active && !inactiveDecor->active && unlistedDecor->active);
    pm::menu_panel::close(first);
    pm::menu_panel::close(second);
    CHECK(activeDecor->active && !inactiveDecor->active);
  }
}
/** Failed opens never acquire decor ownership or release another successful panel's ownership. */
void decor_failed_open() {
  reset();
  pm::menu_panel::Panel first, failed;
  CHECK(!pm::menu_panel::open(failed, "Empty", "", {}));
  CHECK(activeDecor->active && !inactiveDecor->active);
  CHECK(pm::menu_panel::open(first, "Consent", "", {"OK"}));
  auto* title = static_cast<Node*>(pm::hostgame::find_child(popupSource, "Error Title tape"));
  CHECK(title);
  if (title) title->name = "Missing title";
  CHECK(!pm::menu_panel::open(failed, "Invalid", "", {"OK"}));
  CHECK(!activeDecor->active && !inactiveDecor->active);
  pm::menu_panel::close(first);
  CHECK(activeDecor->active && !inactiveDecor->active);
  CHECK(!pm::menu_panel::open(failed, "Invalid", "", {"OK"}));
  CHECK(activeDecor->active && !inactiveDecor->active);
}
/** A mod's message never takes the place of the host's: it waits behind it and shows once that is gone. A host's message in
    front of a mod's sends the mod's behind it, unlost. */
void a_mods_message_waits_behind_the_hosts() {
  namespace mp = pm::menu_panel;
  const auto tryAgain = [] {
    fixtureTime += 1100;
    mp::tick_message();
  };
  const auto title = [] {
    void* shown = mp::shown_message().title;
    return shown ? static_cast<Node*>(shown)->text : std::string();
  };
  const auto clickIt = [&] {
    CHECK(mp::shown_message().buttons.size() == 1);
    if (mp::shown_message().buttons.size() == 1) game_click(button_of(mp::shown_message().buttons[0]));
    mp::tick_message();
  };
  reset();
  wall->focus->focused = true;
  mp::message("The host's", "Removal list.");
  mp::mod_message("The mod's", "A score.");
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()));
  CHECK(title() == "The host's");
  clickIt();
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()));
  CHECK(title() == "The mod's");
  clickIt();
  tryAgain();
  CHECK(!mp::is_open(mp::shown_message()));

  mp::mod_message("The mod's", "A score.");  // the other order
  tryAgain();
  CHECK(title() == "The mod's");
  mp::message("The host's", "Removal list.");
  tryAgain();
  CHECK(title() == "The host's");
  clickIt();
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()));
  CHECK(title() == "The mod's");
  clickIt();

  mp::message("The host's", "A room since left.");  // taking the host's back leaves the mod's
  mp::mod_message("The mod's", "A score.");
  mp::drop_message();
  tryAgain();
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()));
  CHECK(title() == "The mod's");
  clickIt();
}

/** A message is the game's popup on whatever paper of the board is up: it waits for the board, comes back when the board
    does, and goes when clicked. It is never drawn any other way; one that can never be made, or throws, is dropped. */
void message_on_any_paper_of_the_board() {
  namespace mp = pm::menu_panel;
  const auto tryAgain = [] {
    fixtureTime += 1100;  // the popup is searched for at most once a second
    mp::tick_message();
  };
  reset();
  fixtureMenu->active = false;  // the lobby's paper is up
  wall->focus->focused = false;
  CHECK(!mp::main_menu_visible());
  mp::message("Your mods are off", "Because.");
  mp::tick_message();
  CHECK(!mp::is_open(mp::shown_message()));  // waits: the board is out of view
  wall->focus->focused = true;
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()) && mp::shown_message().anyPaper && mp::drawn(mp::shown_message()));
  mp::tick_message();
  CHECK(mp::is_open(mp::shown_message()));  // the main menu's paper being off closes nothing here
  // The player walks away from the board: it hides, and is there again on coming back.
  wall->focus->focused = false;
  mp::tick_message();
  CHECK(!mp::is_open(mp::shown_message()));
  wall->focus->focused = true;
  tryAgain();
  CHECK(mp::is_open(mp::shown_message()));
  CHECK(mp::shown_message().buttons.size() == 1);
  if (mp::shown_message().buttons.size() == 1) game_click(button_of(mp::shown_message().buttons[0]));
  mp::tick_message();
  CHECK(!mp::is_open(mp::shown_message()));
  tryAgain();
  CHECK(!mp::is_open(mp::shown_message()));  // clicked: gone for good

  // Taken back while it waits, or while it shows.
  mp::message("Your mods are off", "Because.");
  mp::drop_message();
  mp::tick_message();
  CHECK(!mp::is_open(mp::shown_message()));
  mp::message("Your mods are off", "Because.");
  mp::tick_message();
  CHECK(mp::is_open(mp::shown_message()));
  mp::drop_message();
  CHECK(!mp::is_open(mp::shown_message()));
  // A later one replaces it.
  mp::message("First", "Words.");
  mp::message("Second", "Words.");
  mp::tick_message();
  CHECK(mp::is_open(mp::shown_message()));
  mp::drop_message();

  // A popup the build cannot make, or that throws while it is built: dropped, nothing drawn in its place.
  reset();
  il2cpp::set_field(fixtureManager, 0x20, popupSource);
  pm::hostgame::throwWhileCopying = true;
  mp::message("Throws", "Words.");
  mp::tick_message();
  pm::hostgame::throwWhileCopying = false;
  CHECK(!mp::is_open(mp::shown_message()));
  tryAgain();
  CHECK(!mp::is_open(mp::shown_message()));  // dropped, not retried for good
}
/** A contract's scene has no popup, for as long as the contract lasts: a waiting message searches less and less often for
    one, shows soon after the lobby is back, and searches once a second again while it is only waiting for the board. */
void waiting_message_backs_off_when_the_scene_has_no_popup() {
  namespace mp = pm::menu_panel;
  reset();
  Node* const manager = fixtureManager;
  fixtureManager = nullptr;  // in a contract: no ErrorManager in the scene
  sceneSearches = 0;
  mp::message("Your mods are off", "Because.");
  // Twenty minutes, a frame's check every 100 ms.
  for (int i = 0; i < 12000; ++i) {
    fixtureTime += 100;
    mp::tick_message();
  }
  CHECK(!mp::is_open(mp::shown_message()));
  CHECK(sceneSearches > 0 && sceneSearches <= 90);  // about 1200 at one search a second
  // Back in the lobby: it shows within the longest wait.
  fixtureManager = manager;
  bool shown = false;
  for (int i = 0; i < 170 && !shown; ++i) {
    fixtureTime += 100;
    mp::tick_message();
    shown = mp::is_open(mp::shown_message());
  }
  CHECK(shown);
  // Walking away from the board is no search of a scene without a popup: it comes back within a second or so.
  wall->focus->focused = false;
  mp::tick_message();
  CHECK(!mp::is_open(mp::shown_message()));
  wall->focus->focused = true;
  for (int i = 0; i < 11; ++i) {
    fixtureTime += 100;
    mp::tick_message();
  }
  CHECK(mp::is_open(mp::shown_message()));
  mp::drop_message();
  // A new message looks at once, whatever the last one waited for.
  fixtureManager = nullptr;
  mp::message("First", "Words.");
  for (int i = 0; i < 400; ++i) {
    fixtureTime += 100;
    mp::tick_message();
  }
  fixtureManager = manager;
  mp::message("Second", "Words.");
  mp::tick_message();
  CHECK(mp::is_open(mp::shown_message()));
  mp::drop_message();
}
/** Losing focus and external root destruction both restore decor; dead decor receives no writes. */
void decor_abnormal_close() {
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    pm::menu_panel::Panel panel;
    CHECK(pm::menu_panel::open(panel, "Mods", "", {"Close"}));
    CHECK(!activeDecor->active);
    if (mode == 0) {
      wall->focus->focused = false;
      CHECK(pm::menu_panel::tick(panel) == -1);
      CHECK(activeDecor->active && !inactiveDecor->active);
    } else if (mode == 1) {
      pm::hostgame::destroy(panel.root);
      pm::menu_panel::tick_message();
      CHECK(activeDecor->active && !inactiveDecor->active);
    } else {
      activeDecor->alive = false;
      pm::menu_panel::close(panel);
      CHECK(!inactiveDecor->active);
    }
    pm::menu_panel::close(panel);
  }
}
}

namespace pm::host {
void* find_child(void* value, const char* path) { return pm::hostgame::find_child(value, path); }
}  // namespace pm::host

int main() {
  use_text({"i18n/default.json"});
  native_transform_survives();
  copied_tape_keeps_game_button_and_drops_its_listeners();
  click_event_runs_loader_action_once();
  game_clicks_reach_the_panel();
  hidden_controls_ignore_input();
  copies_keep_the_game_animator();
  copy_takes_the_disabled_look();
  entries_for_mods_reach_the_game();
  menu_visibility();
  aggregate_menu_visibility();
  buttons_follow_menu_state();
  reopen_and_empty_buttons();
  optional_popup_metadata();
  open_says_whether_a_failure_lasts();
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
  message_on_any_paper_of_the_board();
  a_mods_message_waits_behind_the_hosts();
  row_text_stays_within_its_row();
  waiting_message_backs_off_when_the_scene_has_no_popup();
  return g_failures ? 1 : 0;
}
