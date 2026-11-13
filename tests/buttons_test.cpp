// Offline tests for a mod's buttons built from the game's own UI (sdk/api/buttons.h) over the fake backend:
// a Button added to a row (or the one it already has) and the row's picture made to catch the pointer, a
// GraphicRaycaster added to the canvas, the click reaching the mod's handler through the action the host
// made and the button's own onClick, re-wiring and destroyed buttons staying silent, the EventSystem's input
// module read and switched, and the keyboard selection moving to a row or clearing.
//
// The fake keeps Unity's rules on which object each call takes, and counts a broken one: GetComponent runs on
// a component, AddComponent on a GameObject, get_gameObject on a component. The rows are made as pm::ui makes
// them, a GameObject with a RawImage whose raycastTarget is off, and handed over as that component, as a mod
// holds them (the live test of 2026-11-11: the helpers refused components and asked a GameObject for its
// components, and a fake that took any object anywhere saw neither). Run: build\buttons_test.exe
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "UnityEvents.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "buttons.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace buttons = pm::api::buttons;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x100]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

Obj g_goClass, g_transformClass, g_rawImageClass, g_graphicClass, g_buttonClass, g_raycasterClass, g_systemClass,
    g_moduleClass, g_eventClass;
std::vector<Obj*> g_made;
Obj* make(void* klass) {
  g_made.push_back(new Obj{});
  g_made.back()->klass = klass;
  return g_made.back();
}
bool isGameObject(void* o) { return o && static_cast<Obj*>(o)->klass == &g_goClass; }

// Every call that broke Unity's rule on which object it takes.
int g_misuse = 0;

// The fake's scene: each component sits on its GameObject, and each GameObject has its Transform.
std::map<void*, Obj*> g_goOf;                        // component -> its GameObject
std::map<void*, std::vector<Obj*>> g_components;     // GameObject -> its components
Obj* addTo(Obj* go, void* klass) {
  Obj* c = make(klass);
  g_components[go].push_back(c);
  g_goOf[c] = go;
  return c;
}
Obj* makeGameObject() {
  Obj* go = make(&g_goClass);
  addTo(go, &g_transformClass);
  return go;
}

// What each System.Type asks for; a RawImage is a Graphic, so the Graphic type finds it.
bool matches(void* typeObject, void* klass) {
  if (typeObject == phasmo::Button::type.type_object()) return klass == &g_buttonClass;
  if (typeObject == phasmo::GraphicRaycaster::type.type_object()) return klass == &g_raycasterClass;
  if (typeObject == phasmo::Graphic::type.type_object()) return klass == &g_rawImageClass;
  return false;
}
void* klassToAdd(void* typeObject) {
  if (typeObject == phasmo::Button::type.type_object()) return &g_buttonClass;
  if (typeObject == phasmo::GraphicRaycaster::type.type_object()) return &g_raycasterClass;
  return nullptr;
}

int g_adds = 0;
void* fake_get_component(void* component, void* typeObject, void*) {
  if (!component || isGameObject(component)) {  // Component.GetComponent: an instance method of Component
    ++g_misuse;
    return nullptr;
  }
  const auto go = g_goOf.find(component);
  if (go == g_goOf.end()) return nullptr;
  for (Obj* c : g_components[go->second])
    if (matches(typeObject, c->klass)) return c;
  return nullptr;
}
void* fake_add_component(void* gameObject, void* typeObject, void*) {
  if (!isGameObject(gameObject)) {  // GameObject.AddComponent: an instance method of GameObject
    ++g_misuse;
    return nullptr;
  }
  void* klass = klassToAdd(typeObject);
  if (!klass) return nullptr;
  ++g_adds;
  return addTo(static_cast<Obj*>(gameObject), klass);
}
void* fake_go_of(void* component, void*) {
  if (!component || isGameObject(component)) {
    ++g_misuse;
    return nullptr;
  }
  const auto it = g_goOf.find(component);
  return it == g_goOf.end() ? nullptr : it->second;
}
void* fake_transform_of(void* gameObject, void*) {
  if (!isGameObject(gameObject)) {
    ++g_misuse;
    return nullptr;
  }
  for (Obj* c : g_components[gameObject])
    if (c->klass == &g_transformClass) return c;
  return nullptr;
}

// Each Graphic's raycastTarget, as set_raycastTarget left it.
std::map<void*, bool> g_raycast;
void fake_set_raycast(void* graphic, bool on, void*) { g_raycast[graphic] = on; }

// A row as pm::ui::create_image makes it: a GameObject with a RawImage that does not catch the pointer.
// The mod holds the RawImage.
Obj* makeRow() {
  Obj* image = addTo(makeGameObject(), &g_rawImageClass);
  g_raycast[image] = false;
  return image;
}

// Each button's onClick event, made on first asking, and every AddListener the SDK asked for.
std::map<void*, Obj*> g_events;
std::vector<std::pair<void*, void*>> g_listeners;  // the event and the action added to it
void* fake_get_onClick(void* button, void*) {
  Obj*& e = g_events[button];
  if (!e) e = make(&g_eventClass);
  return e;
}
void fake_add_listener(void* event, void* action, void*) { g_listeners.push_back({event, action}); }

// The EventSystem, its input module (at kModule in the system) and its enabled state; and every
// SetSelectedGameObject asked for.
constexpr int32_t kModule = 0x08;
Obj* g_system = nullptr;
std::map<void*, bool> g_enabled;
std::vector<std::pair<void*, void*>> g_selects;
void* g_selected = nullptr;  // what the fake's EventSystem holds selected now
void* fake_get_current(void*) { return g_system; }
void fake_set_selected(void* system, void* gameObject, void*) {
  if (gameObject && !isGameObject(gameObject)) ++g_misuse;
  g_selects.push_back({system, gameObject});
  g_selected = gameObject;
}
void* fake_get_selected(void*, void*) { return g_selected; }
bool fake_get_enabled(void* module, void*) {
  const auto it = g_enabled.find(module);
  return it != g_enabled.end() && it->second;
}
void fake_set_enabled(void* module, bool on, void*) { g_enabled[module] = on; }

// The mod's click handlers, counting how each was run and with what user.
int g_oneFires = 0, g_twoFires = 0, g_goneFires = 0;
void* g_fireUser = nullptr;
void handlerOne(void* user) { ++g_oneFires; g_fireUser = user; }
void handlerTwo(void* user) { ++g_twoFires; g_fireUser = user; }
void handlerGone(void*) { ++g_goneFires; }

void* ptr(int i) { return reinterpret_cast<void*>(static_cast<uintptr_t>(i)); }

bool bindAll() {
  fake::reset();
  fake::type(map::GameObject, &g_goClass);
  fake::type(map::Button, &g_buttonClass);
  fake::type(map::GraphicRaycaster, &g_raycasterClass);
  fake::type(map::Graphic, &g_graphicClass);
  fake::type(map::EventSystem, &g_systemClass);
  fake::type(map::UnityEvent, &g_eventClass);
  fake::field(map::EventSystem_currentInputModule, kModule);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&fake_add_component));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_transform_of));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_go_of));
  fake::method(map::Graphic_set_raycastTarget, reinterpret_cast<void*>(&fake_set_raycast));
  fake::method(map::Button_get_onClick, reinterpret_cast<void*>(&fake_get_onClick));
  fake::method(map::UnityEvent_AddListener, reinterpret_cast<void*>(&fake_add_listener));
  fake::method(map::EventSystem_get_current, reinterpret_cast<void*>(&fake_get_current));
  fake::method(map::EventSystem_SetSelectedGameObject, reinterpret_cast<void*>(&fake_set_selected));
  fake::method(map::EventSystem_get_currentSelectedGameObject, reinterpret_cast<void*>(&fake_get_selected));
  fake::method(map::Behaviour_get_enabled, reinterpret_cast<void*>(&fake_get_enabled));
  fake::method(map::Behaviour_set_enabled, reinterpret_cast<void*>(&fake_set_enabled));
  std::string missing;
  return pm::api::bind(fake::host(), missing);
}

}  // namespace

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // so a crash still shows the checks it reached
  if (!bindAll()) {
    std::printf("FAIL: the game API did not bind over the fake\n");
    return 1;
  }

  // A row handed over as its picture gets exactly one Button, on the picture's GameObject, and its picture
  // now catches the pointer.
  Obj* row = makeRow();
  Obj* rowObject = g_goOf[row];
  void* button = buttons::attach(row);
  CHECK(button && static_cast<Obj*>(button)->klass == &g_buttonClass);
  CHECK(button && g_goOf[button] == rowObject);
  CHECK(g_adds == 1);
  CHECK(g_raycast[row]);
  CHECK(buttons::attach(row) == button);
  CHECK(g_adds == 1);
  // The same row by its GameObject: the Button it already has.
  CHECK(buttons::attach(rowObject) == button);
  CHECK(g_adds == 1);
  CHECK(buttons::attach(nullptr) == nullptr);

  // The canvas, handed over as its component (pm::ui::create_canvas gives the Canvas) or as its
  // GameObject, gets exactly one GraphicRaycaster.
  Obj* canvasObject = makeGameObject();
  Obj* canvas = addTo(canvasObject, &g_rawImageClass);  // a component on the canvas object stands in for the Canvas
  void* raycaster = buttons::catch_pointer(canvas);
  CHECK(raycaster && static_cast<Obj*>(raycaster)->klass == &g_raycasterClass);
  CHECK(raycaster && g_goOf[raycaster] == canvasObject);
  CHECK(buttons::catch_pointer(canvas) == raycaster);
  CHECK(buttons::catch_pointer(canvasObject) == raycaster);
  CHECK(g_adds == 2);
  CHECK(buttons::catch_pointer(nullptr) == nullptr);

  // The click: an action bound to the button, added to its onClick, reaching the handler with its user.
  CHECK(buttons::on_click(button, &handlerOne, ptr(1)));
  CHECK(fake::unity_actions().size() == 1);
  CHECK(fake::unity_actions()[0].first == button);
  CHECK(g_listeners.size() == 1 && g_listeners[0].second != nullptr);
  fake::unity_actions()[0].second(button);  // as the game's onClick invoking the action
  CHECK(g_oneFires == 1);
  CHECK(g_fireUser == ptr(1));
  // The row's picture and its GameObject both find the Button the click is wired to.
  CHECK(buttons::on_click(row, &handlerOne, ptr(1)));
  CHECK(fake::unity_actions().size() == 2 && fake::unity_actions()[1].first == button);
  CHECK(buttons::on_click(rowObject, &handlerOne, ptr(1)));
  CHECK(fake::unity_actions().size() == 3 && fake::unity_actions()[2].first == button);

  // Re-wiring replaces the handler: any action of the button, the one added before or after, runs what
  // the wiring says now.
  CHECK(buttons::on_click(button, &handlerTwo, ptr(2)));
  CHECK(fake::unity_actions().size() == 4);
  fake::unity_actions()[3].second(button);
  CHECK(g_twoFires == 1 && g_fireUser == ptr(2));
  fake::unity_actions()[0].second(button);  // the action of a replaced wiring
  CHECK(g_twoFires == 2 && g_oneFires == 1);
  // forget takes the wiring off: the action still fires and runs nothing.
  buttons::forget(row);
  fake::unity_actions()[3].second(button);
  CHECK(g_twoFires == 2);

  // A destroyed button's handler never runs again: the next wiring drops its entry.
  void* goneButton = buttons::attach(makeRow());
  CHECK(buttons::on_click(goneButton, &handlerGone, nullptr));
  const size_t goneAsk = fake::unity_actions().size() - 1;
  fake::destroy(goneButton);
  void* nextButton = buttons::attach(makeRow());
  CHECK(buttons::on_click(nextButton, &handlerOne, ptr(3)));
  fake::unity_actions()[goneAsk].second(goneButton);
  CHECK(g_goneFires == 0);
  fake::unity_actions()[fake::unity_actions().size() - 1].second(nextButton);
  CHECK(g_oneFires == 2 && g_fireUser == ptr(3));
  // No Button, no wiring: an object without one answers false and asks the host for nothing.
  const size_t asked = fake::unity_actions().size();
  CHECK(!buttons::on_click(canvas, &handlerOne, nullptr));
  CHECK(!buttons::on_click(canvasObject, &handlerOne, nullptr));
  CHECK(fake::unity_actions().size() == asked);

  // Without an EventSystem there is no input module and no selection to move.
  CHECK(buttons::input_module() == nullptr);
  CHECK(!buttons::select(button));

  // With one: the module is read from the EventSystem, and switched as the journal toggle switches it.
  g_system = make(&g_systemClass);
  Obj* module = make(&g_moduleClass);
  at<void*>(g_system, kModule) = module;
  CHECK(buttons::input_module() == module);
  CHECK(!buttons::input_on(module));
  buttons::input_set(module, true);
  CHECK(g_enabled[module]);
  buttons::input_set(module, false);  // back as it was found
  CHECK(!g_enabled[module]);
  buttons::input_set(nullptr, true);  // nothing for null

  // Selection: a component resolves to its GameObject; a GameObject goes as it is; null clears.
  CHECK(buttons::select(button));
  CHECK(g_selects.size() == 1 && g_selects[0].first == g_system && g_selects[0].second == rowObject);
  CHECK(buttons::select(row));
  CHECK(g_selects[1].second == rowObject);
  CHECK(buttons::select(canvasObject));
  CHECK(g_selects[2].second == canvasObject);
  CHECK(buttons::select(nullptr));
  CHECK(g_selects[3].second == nullptr);
  // selected() reads the selection back, which is how a scrolling list follows where the player's arrow keys
  // have moved it (the dev menu needs it: the game's navigation tells no mod).
  CHECK(buttons::selected() == nullptr);
  CHECK(buttons::select(button));
  CHECK(buttons::selected() == rowObject);
  CHECK(buttons::select(nullptr));
  CHECK(buttons::selected() == nullptr);

  // Every call above went to the kind of object Unity takes it on.
  CHECK(g_misuse == 0);

  // Take the last wirings off, so the SDK's table holds no handle when the test ends.
  buttons::forget(nextButton);

  std::printf(g_failures ? "buttons_test: %d failures\n" : "buttons_test: ok\n", g_failures);
  return g_failures ? 1 : 0;
}
