// Offline tests for a mod's buttons built from the game's own UI (sdk/api/buttons.h) over the fake backend:
// a Button added to a row (or the one it already has), a GraphicRaycaster added to the canvas, the click
// reaching the mod's handler through the action the host made and the button's own onClick, re-wiring and
// destroyed buttons staying silent, the EventSystem's input module read and switched, and the keyboard
// selection moving to a row or clearing. Run: build\buttons_test.exe
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

Obj g_goClass, g_buttonClass, g_raycasterClass, g_systemClass, g_moduleClass, g_eventClass;
std::vector<Obj*> g_made;
Obj* make(void* klass) {
  g_made.push_back(new Obj{});
  g_made.back()->klass = klass;
  return g_made.back();
}

// The fake's UI: components sit on their GameObject; one Button or Raycaster per object at most.
std::map<void*, std::vector<Obj*>> g_components;
int g_adds = 0;
void* klassFor(void* typeObject) {
  if (typeObject == phasmo::Button::type.type_object()) return &g_buttonClass;
  if (typeObject == phasmo::GraphicRaycaster::type.type_object()) return &g_raycasterClass;
  return nullptr;
}
void* fake_get_component(void* object, void* typeObject, void*) {
  void* klass = klassFor(typeObject);
  if (!klass) return nullptr;
  for (Obj* c : g_components[object])
    if (c->klass == klass) return c;
  return nullptr;
}
void* fake_add_component(void* object, void* typeObject, void*) {
  void* klass = klassFor(typeObject);
  if (!klass) return nullptr;
  ++g_adds;
  Obj* c = make(klass);
  g_components[object].push_back(c);
  return c;
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
void* fake_get_current(void*) { return g_system; }
void fake_set_selected(void* system, void* gameObject, void*) { g_selects.push_back({system, gameObject}); }
bool fake_get_enabled(void* module, void*) {
  const auto it = g_enabled.find(module);
  return it != g_enabled.end() && it->second;
}
void fake_set_enabled(void* module, bool on, void*) { g_enabled[module] = on; }

// Each component's GameObject, as Component.get_gameObject answers it.
std::map<void*, Obj*> g_goOf;
void* fake_go_of(void* component, void*) {
  const auto it = g_goOf.find(component);
  return it == g_goOf.end() ? nullptr : it->second;
}

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
  fake::type(map::EventSystem, &g_systemClass);
  fake::type(map::UnityEvent, &g_eventClass);
  fake::field(map::EventSystem_currentInputModule, kModule);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&fake_add_component));
  fake::method(map::Button_get_onClick, reinterpret_cast<void*>(&fake_get_onClick));
  fake::method(map::UnityEvent_AddListener, reinterpret_cast<void*>(&fake_add_listener));
  fake::method(map::EventSystem_get_current, reinterpret_cast<void*>(&fake_get_current));
  fake::method(map::EventSystem_SetSelectedGameObject, reinterpret_cast<void*>(&fake_set_selected));
  fake::method(map::Behaviour_get_enabled, reinterpret_cast<void*>(&fake_get_enabled));
  fake::method(map::Behaviour_set_enabled, reinterpret_cast<void*>(&fake_set_enabled));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_go_of));
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

  // A row gets exactly one Button, added once; the canvas exactly one GraphicRaycaster.
  Obj* row = make(&g_goClass);
  void* button = buttons::attach(row);
  CHECK(button && static_cast<Obj*>(button)->klass == &g_buttonClass);
  CHECK(g_adds == 1);
  CHECK(buttons::attach(row) == button);
  CHECK(g_adds == 1);
  CHECK(buttons::attach(nullptr) == nullptr);
  Obj* canvas = make(&g_goClass);
  void* raycaster = buttons::catch_pointer(canvas);
  CHECK(raycaster && static_cast<Obj*>(raycaster)->klass == &g_raycasterClass);
  CHECK(buttons::catch_pointer(canvas) == raycaster);

  // The click: an action bound to the button, added to its onClick, reaching the handler with its user.
  CHECK(buttons::on_click(button, &handlerOne, ptr(1)));
  CHECK(fake::unity_actions().size() == 1);
  CHECK(fake::unity_actions()[0].first == button);
  CHECK(g_listeners.size() == 1 && g_listeners[0].second != nullptr);
  fake::unity_actions()[0].second(button);  // as the game's onClick invoking the action
  CHECK(g_oneFires == 1);
  CHECK(g_fireUser == ptr(1));
  // The Button on its object can be handed over in place of the component, and the component's own too.
  CHECK(buttons::on_click(button, &handlerOne, ptr(1)));
  CHECK(fake::unity_actions().size() == 2);

  // Re-wiring replaces the handler: any action of the button, the one added before or after, runs what
  // the wiring says now.
  CHECK(buttons::on_click(button, &handlerTwo, ptr(2)));
  CHECK(fake::unity_actions().size() == 3);
  fake::unity_actions()[2].second(button);
  CHECK(g_twoFires == 1 && g_fireUser == ptr(2));
  fake::unity_actions()[0].second(button);  // the action of the replaced wiring
  CHECK(g_twoFires == 2 && g_oneFires == 1);
  // forget takes the wiring off: the action still fires and runs nothing.
  buttons::forget(button);
  fake::unity_actions()[2].second(button);
  CHECK(g_twoFires == 2);

  // A destroyed button's handler never runs again: the next wiring drops its entry.
  Obj* goneRow = make(&g_goClass);
  void* goneButton = buttons::attach(goneRow);
  CHECK(buttons::on_click(goneButton, &handlerGone, nullptr));
  const size_t goneAsk = fake::unity_actions().size() - 1;
  fake::destroy(goneButton);
  Obj* nextRow = make(&g_goClass);
  void* nextButton = buttons::attach(nextRow);
  CHECK(buttons::on_click(nextButton, &handlerOne, ptr(3)));
  fake::unity_actions()[goneAsk].second(goneButton);
  CHECK(g_goneFires == 0);
  fake::unity_actions()[fake::unity_actions().size() - 1].second(nextButton);
  CHECK(g_oneFires == 2 && g_fireUser == ptr(3));
  // No Button, no wiring: a plain object answers false and asks the host for nothing.
  CHECK(!buttons::on_click(canvas, &handlerOne, nullptr));

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
  g_goOf[button] = row;
  CHECK(buttons::select(button));
  CHECK(g_selects.size() == 1 && g_selects[0].first == g_system && g_selects[0].second == row);
  CHECK(buttons::select(canvas));
  CHECK(g_selects[1].second == canvas);
  CHECK(buttons::select(nullptr));
  CHECK(g_selects[2].second == nullptr);

  // Take the last wirings off, so the SDK's table holds no handle when the test ends.
  buttons::forget(nextButton);

  std::printf(g_failures ? "buttons_test: %d failures\n" : "buttons_test: ok\n", g_failures);
  return g_failures ? 1 : 0;
}
