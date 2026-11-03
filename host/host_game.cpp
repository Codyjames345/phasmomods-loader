#include "host_game.h"

#include "Input.h"
#include "LevelController.h"
#include "Photon.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "room_gates.h"
#include "runtime.h"

namespace pm::hostgame {

bool private_room() { return room_gates::private_room(); }
int actor_number(void* p) { return room_gates::actor_number(p); }
int local_actor() { return room_gates::local_actor(); }
int master_actor() { return room_gates::master_actor(); }
bool room_actors(std::vector<int>& out) { return room_gates::room_actors(out); }

bool local_is_master() {
  const int local = local_actor();
  return local != 0 && local == master_actor();
}

bool out_of_room() { return phasmo::PhotonNetwork::get_InRoom.available() && !phasmo::PhotonNetwork::get_InRoom(); }

// The static instance outlives its level: after leaving a contract it still points at the destroyed controller, so only a
// live one counts as being in a contract.
bool out_of_contract() {
  void* controller = nullptr;  // read, not get, which answers null for a refused read too
  return phasmo::LevelController::instance.read(controller) && !alive(controller);
}

bool alive(void* o) { return o && api::alive(o); }
void* transform_of(void* c) { return phasmo::Component::get_transform(c); }
void* object_transform(void* go) { return go ? phasmo::GameObject::get_transform(go) : nullptr; }
void* game_object_of(void* c) { return phasmo::Component::get_gameObject(c); }
int child_count(void* t) { return t ? phasmo::Transform::get_childCount(t) : 0; }
void* child(void* t, int index) { return phasmo::Transform::GetChild(t, index); }
void* parent(void* t) { return phasmo::Transform::get_parent(t); }
void set_local_scale(void* t, phasmo::Vector3 s) {
  const phasmo::Vector3 v = s;
  phasmo::Transform::set_localScale(t, &v);
}
void* components(void* go, void* type) { return go && type ? phasmo::GameObject::GetComponents(go, type) : nullptr; }
void* get_component(void* c, void* type) { return alive(c) && type ? phasmo::Component::GetComponent(c, type) : nullptr; }
void set_enabled(void* b, bool on) { phasmo::Behaviour::set_enabled(b, on); }
bool enabled(void* b) { return phasmo::Behaviour::get_enabled(b); }
void set_active(void* go, bool on) {
  if (alive(go)) phasmo::GameObject::SetActive(go, on);
}
bool active_self(void* go) { return go && phasmo::GameObject::get_activeSelf(go); }
bool active_in_hierarchy(void* go) { return go && phasmo::GameObject::get_activeInHierarchy(go); }
void destroy(void* o) {
  if (alive(o)) phasmo::Object::Destroy(o, 0.0f);  // Object.Destroy(Object) is Destroy(obj, 0)
}
void* instantiate(void* original, void* parent, bool worldPositionStays) {
  if (!alive(original) || !alive(parent)) return nullptr;
  return phasmo::Object::Instantiate(original, parent, worldPositionStays);
}

bool cursor_locked() {
  return api::all_resolved<phasmo::Cursor::get_lockState, phasmo::CursorLockMode::Locked>() &&
         phasmo::Cursor::get_lockState() == phasmo::CursorLockMode::Locked();
}
void free_cursor() {
  phasmo::Cursor::set_lockState(phasmo::CursorLockMode::None());
  phasmo::Cursor::set_visible(true);
}
}  // namespace pm::hostgame
