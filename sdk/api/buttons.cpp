#include "buttons.h"

#include <vector>

#include "UnityEvents.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "game_api.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::buttons {
namespace {

// The buttons this SDK is listening to: the action is bound to the Button component, and the game invokes
// it with that same object, so the target it hands the listener names the button clicked.
struct Wiring {
  Held target;
  void (*fn)(void*);
  void* user;
};
std::vector<Wiring> g_wiring;

// The contract's listener. It runs inside the game's managed call: nothing may leave it.
void clicked(void* target) {
  for (Wiring& w : g_wiring) {
    if (w.target != target) continue;
    try {
      w.fn(w.user);
    } catch (...) {
      PM_LOG("button: the click handler threw; the game's click went no further");
    }
    return;
  }
}

// A helper's object argument may be the GameObject or any component on it: pm::ui's creators hand back
// components (a Canvas, a RawImage). Each half of that pair goes where it belongs. The map's
// Component.GetComponent is Component's own method, so its object has to be a component (any one of them
// finds the object's siblings); a GameObject has a same-named method of its own, which is not what we call,
// so for a GameObject we go through its Transform, which every GameObject has. GameObject.AddComponent
// stays on the GameObject. The live test of 2026-11-11 found both mixed up: the helpers took only
// GameObjects, so the menu's components were refused and no row got a Button, and their GetComponent was
// Component's, run with a GameObject as its object. The offline fake took any object anywhere, so it saw
// neither.

// The GameObject the object is or sits on; null when it is neither.
void* game_object_of(void* object) {
  if (!object) return nullptr;
  if (phasmo::GameObject::type.is(object)) return object;
  return phasmo::Component::get_gameObject(object);
}

// A component on the object, to ask for its siblings: the object itself when it is a component, else
// the GameObject's Transform, which every GameObject has.
void* component_on(void* object) {
  if (!object) return nullptr;
  if (phasmo::GameObject::type.is(object)) return phasmo::GameObject::get_transform(object);
  return object;
}

// The object's component of `type` (a System.Type), or null.
void* sibling(void* object, void* type) {
  void* on = component_on(object);
  return on && type ? phasmo::Component::GetComponent(on, type) : nullptr;
}

// The object's component of `type`: the one it has, or a new one on its GameObject.
void* sibling_or_new(void* object, void* type) {
  if (!type) return nullptr;
  if (void* existing = sibling(object, type)) return existing;
  void* gameObject = game_object_of(object);
  return gameObject ? phasmo::GameObject::AddComponent(gameObject, type) : nullptr;
}

void* button_of(void* object) {
  if (!object) return nullptr;
  if (phasmo::Button::type.is(object)) return object;
  return sibling(object, phasmo::Button::type.type_object());
}

}  // namespace

Object attach(void* gameObjectOrComponent) {
  void* button = sibling_or_new(gameObjectOrComponent, phasmo::Button::type.type_object());
  if (!button) return nullptr;
  // The Button reacts only to what the raycaster hits, and the raycaster hits only a Graphic whose
  // raycastTarget is on. pm::ui::create_image makes its pictures with it off, so a plain picture never
  // takes a click meant for what is behind it; a picture that becomes a button must take them.
  if (void* graphic = sibling(gameObjectOrComponent, phasmo::Graphic::type.type_object()))
    phasmo::Graphic::set_raycastTarget(graphic, true);
  return button;
}

Object catch_pointer(void* canvasGameObjectOrComponent) {
  return sibling_or_new(canvasGameObjectOrComponent, phasmo::GraphicRaycaster::type.type_object());
}

bool on_click(void* buttonOrGameObject, void (*fn)(void* user), void* user) {
  void* button = button_of(buttonOrGameObject);
  if (!button || !fn) return false;
  const PmHostApi* h = detail::host();
  if (!h || !h->unity_action_new) return false;
  void* event = phasmo::Button::get_onClick(button);
  if (!event) return false;
  void* action = h->unity_action_new(button, &clicked);
  if (!action) return false;
  // Drop this button's old wiring and any whose object the game has destroyed, so a pointer cannot be
  // read after its object went, and a re-wired button calls its handler once.
  std::erase_if(g_wiring, [button](const Wiring& w) {
    return w.target == button || !alive(w.target);
  });
  phasmo::UnityEvent::AddListener(event, action);
  g_wiring.push_back({Held(button), fn, user});
  return true;
}

void forget(void* buttonOrGameObject) {
  void* button = button_of(buttonOrGameObject);
  if (!button) return;
  std::erase_if(g_wiring, [button](const Wiring& w) { return w.target == button; });
}

Object input_module() {
  void* system = phasmo::EventSystem::get_current();
  return system ? phasmo::EventSystem::currentInputModule.get(system) : nullptr;
}

bool input_on(void* module) { return phasmo::Behaviour::get_enabled(module); }

void input_set(void* module, bool on) {
  if (module) phasmo::Behaviour::set_enabled(module, on);
}

bool select(void* gameObjectOrComponent) {
  if (!phasmo::EventSystem::get_current.available() ||
      !phasmo::EventSystem::SetSelectedGameObject.available())
    return false;
  void* system = phasmo::EventSystem::get_current();
  if (!system) return false;
  void* gameObject = gameObjectOrComponent;
  if (gameObject && !phasmo::GameObject::type.is(gameObject))
    gameObject = phasmo::Component::get_gameObject(gameObject);
  phasmo::EventSystem::SetSelectedGameObject(system, gameObject);
  return true;
}

// The EventSystem's own selection. The game's arrow-key and gamepad navigation moves it without telling any
// mod, which is why a scrolling list has to read it back to follow the player.
Ref<pm::map::GameObject> selected() {
  if (!phasmo::EventSystem::get_current.available() ||
      !phasmo::EventSystem::get_currentSelectedGameObject.available())
    return nullptr;
  void* system = phasmo::EventSystem::get_current();
  return system ? phasmo::EventSystem::get_currentSelectedGameObject(system) : nullptr;
}

}  // namespace pm::api::buttons
