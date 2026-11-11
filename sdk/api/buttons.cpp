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

void* button_of(void* object) {
  if (!object) return nullptr;
  if (phasmo::Button::type.is(object)) return object;
  if (!phasmo::GameObject::type.is(object)) return nullptr;
  void* type = phasmo::Button::type.type_object();
  return type ? phasmo::Component::GetComponent(object, type) : nullptr;
}

}  // namespace

void* attach(void* gameObject) {
  if (!gameObject || !phasmo::GameObject::type.is(gameObject)) return nullptr;
  void* type = phasmo::Button::type.type_object();
  if (!type) return nullptr;
  if (void* existing = phasmo::Component::GetComponent(gameObject, type)) return existing;
  return phasmo::GameObject::AddComponent(gameObject, type);
}

void* catch_pointer(void* canvasGameObject) {
  if (!canvasGameObject || !phasmo::GameObject::type.is(canvasGameObject)) return nullptr;
  void* type = phasmo::GraphicRaycaster::type.type_object();
  if (!type) return nullptr;
  if (void* existing = phasmo::Component::GetComponent(canvasGameObject, type)) return existing;
  return phasmo::GameObject::AddComponent(canvasGameObject, type);
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

void* input_module() {
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

}  // namespace pm::api::buttons
