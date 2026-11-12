#include "game.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <utility>

#include "Camera.h"
#include "Door.h"
#include "Equipment.h"
#include "GhostAI.h"
#include "GhostIdleState.h"
#include "Input.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "LightSwitch.h"
#include "Photon.h"
#include "Player.h"
#include "Props.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "Difficulty.h"
#include "log.h"
#include "players.h"
#include "room_gates.h"
#include "runtime.h"
#include "sight_rule.h"

// Every game member here is reached through the game map (sdk/map) and the game API (sdk/api), so a
// game update moves only the map. In a mod the wrappers bind to the mod's host contract, in the host to
// the host's own.
namespace pm::game {
namespace {

}  // namespace

bool resolve() {
  const bool ok = GetModuleHandleW(L"GameAssembly.dll") != nullptr;
  PM_LOG("game: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool in_room() { return phasmo::PhotonNetwork::get_InRoom(); }
bool is_master() { return phasmo::PhotonNetwork::get_IsMasterClient(); }
int actor_number(void* p) { return room_gates::actor_number(p); }
bool private_room() { return room_gates::private_room(); }
int local_actor() { return room_gates::local_actor(); }
int master_actor() { return room_gates::master_actor(); }
void room_actors(std::vector<int>& out) { room_gates::room_actors(out); }

bool alive(void* o) { return o && api::alive(o); }

namespace {
/// What the object is, as its own class answers: 1 for a GameObject, 0 for a component or anything else, and
/// -1 when the class cannot be read. A GameObject is not a Component, so the two accessors below are not
/// interchangeable: passing one through the other's reads a layout the object does not have, and the game
/// faults (what the dev menu's first scroll list did on 2026-11-12). No answer is guessed at -1.
int kind_of(void* object) {
  if (!object || !phasmo::GameObject::type.klass()) return -1;
  return phasmo::GameObject::type.is(object) ? 1 : 0;
}
}  // namespace

void* transform_of(void* object) {
  const int kind = kind_of(object);
  if (kind < 0) return nullptr;
  return kind == 1 ? phasmo::GameObject::get_transform(object) : phasmo::Component::get_transform(object);
}
void* object_transform(void* go) { return go ? phasmo::GameObject::get_transform(go) : nullptr; }
// The layout entries (anchors, pivot, position, size) belong to RectTransform, a Transform subclass the game
// only gives an object when something asks for it: every UI class in the game's own UnityEngine.UI carries
// [RequireComponent(typeof(RectTransform))] (Graphic, RawImage's base, RectMask2D, ScrollRect, LayoutElement).
// So an object made at run time can be a plain Transform, and running the layout entries on one reads a layout
// it does not have. Asked and answered, never guessed: the type check answers false for a plain Transform and for a
// class that cannot be read, and both mean the same thing here, that the layout entries are not for this object.
void* rect_transform_of(void* object) {
  void* t = transform_of(object);
  return t && phasmo::RectTransform::type.is(t) ? t : nullptr;
}
Vector3 position(void* t) { return phasmo::Transform::get_position(t); }
void set_position(void* t, Vector3 p) {
  const Vector3 v = p;
  phasmo::Transform::set_position(t, &v);
}
Vector3 forward(void* t) { return phasmo::Transform::get_forward(t); }
Quaternion rotation(void* t) { return phasmo::Transform::get_rotation(t); }
void set_rotation(void* t, Quaternion r) {
  const Quaternion q = r;
  phasmo::Transform::set_rotation(t, &q);
}

void* new_game_object(const char* name) {
  void* go = phasmo::GameObject::type.make();
  if (!go) return nullptr;
  phasmo::GameObject::ctor(go, api::make_string(name));
  return go;
}
void* add_component(void* go, void* type) { return go && type ? phasmo::GameObject::AddComponent(go, type) : nullptr; }
void* game_object_of(void* object) {
  const int kind = kind_of(object);
  if (kind < 0) return nullptr;
  // A GameObject's own GameObject is itself: only a component has one to be asked for.
  return kind == 1 ? object : phasmo::Component::get_gameObject(object);
}
void set_enabled(void* b, bool on) { phasmo::Behaviour::set_enabled(b, on); }
bool enabled(void* b) { return phasmo::Behaviour::get_enabled(b); }
void destroy(void* o) {
  if (alive(o)) phasmo::Object::Destroy(o, 0.0f);  // Object.Destroy(Object) is Destroy(obj, 0)
}
void set_active(void* go, bool on) {
  if (alive(go)) phasmo::GameObject::SetActive(go, on);
}
void* parent(void* t) { return phasmo::Transform::get_parent(t); }
void set_parent(void* t, void* p, bool worldPositionStays) { phasmo::Transform::SetParent(t, p, worldPositionStays); }
Vector3 local_position(void* t) { return phasmo::Transform::get_localPosition(t); }
void set_local_position(void* t, Vector3 p) {
  const Vector3 v = p;
  phasmo::Transform::set_localPosition(t, &v);
}
Quaternion local_rotation(void* t) { return phasmo::Transform::get_localRotation(t); }
void set_local_rotation(void* t, Quaternion r) {
  const Quaternion q = r;
  phasmo::Transform::set_localRotation(t, &q);
}
void set_local_scale(void* t, Vector3 s) {
  const Vector3 v = s;
  phasmo::Transform::set_localScale(t, &v);
}
Vector3 local_scale(void* t) { return phasmo::Transform::get_localScale(t); }
Vector3 lossy_scale(void* t) { return phasmo::Transform::get_lossyScale(t); }
// Object.Instantiate(original, parent, worldPositionStays) is the engine's clone after its null checks
// (phasmo::Object::Instantiate), so they are made here: a missing original or parent copies nothing.
void* instantiate(void* original, void* parent, bool worldPositionStays) {
  if (!alive(original) || !alive(parent)) return nullptr;
  return phasmo::Object::Instantiate(original, parent, worldPositionStays);
}
int child_count(void* t) { return t ? phasmo::Transform::get_childCount(t) : 0; }
void* child(void* t, int index) { return phasmo::Transform::GetChild(t, index); }

namespace {
bool read_vector2(void* control, float& x, float& y) {
  x = y = 0;
  if (!control || !phasmo::Vector2Control::ReadValue.available()) return false;
  const phasmo::Vector2 v = phasmo::Vector2Control::ReadValue(control);
  x = v.x;
  y = v.y;
  return true;
}

void* left_button() {
  void* mouse = phasmo::Mouse::get_current();
  return mouse ? phasmo::Mouse::get_leftButton(mouse) : nullptr;
}
}  // namespace

bool mouse_delta(float& dx, float& dy) {
  void* mouse = phasmo::Mouse::get_current();
  return read_vector2(mouse ? phasmo::Pointer::get_delta(mouse) : nullptr, dx, dy);
}

bool mouse_position(float& x, float& y) {
  void* mouse = phasmo::Mouse::get_current();
  return read_vector2(mouse ? phasmo::Pointer::get_position(mouse) : nullptr, x, y);
}

bool mouse_left_pressed() {
  void* button = left_button();
  return button && phasmo::ButtonControl::get_wasPressedThisFrame(button);
}

bool mouse_left_held() {
  void* button = left_button();
  return button && phasmo::ButtonControl::get_isPressed(button);
}

bool active_self(void* go) { return go && phasmo::GameObject::get_activeSelf(go); }
bool active_in_hierarchy(void* go) { return go && phasmo::GameObject::get_activeInHierarchy(go); }
int layer(void* go) { return go ? phasmo::GameObject::get_layer(go) : -1; }
void set_layer(void* go, int layer) {
  if (go) phasmo::GameObject::set_layer(go, layer);
}
bool renderer_enabled(void* r) { return phasmo::Renderer::get_enabled(r); }
void set_renderer_enabled(void* r, bool on) { phasmo::Renderer::set_enabled(r, on); }
bool collider_enabled(void* c) { return phasmo::Collider::get_enabled(c); }
void set_collider_enabled(void* c, bool on) { phasmo::Collider::set_enabled(c, on); }

void lock_cursor() { phasmo::Cursor::set_lockState(phasmo::CursorLockMode::Locked()); }

void free_cursor() {
  phasmo::Cursor::set_lockState(phasmo::CursorLockMode::None());
  phasmo::Cursor::set_visible(true);
}
bool set_destination(void* a, Vector3 target) {
  const Vector3 v = target;
  return phasmo::NavMeshAgent::SetDestination(a, &v);
}
float agent_speed(void* a) { return phasmo::NavMeshAgent::get_speed(a); }
float agent_path_length(void* a) {
  if (phasmo::NavMeshAgent::get_pathPending(a)) return -1;
  const float d = phasmo::NavMeshAgent::get_remainingDistance(a);
  return d >= 0 && d < 1e6f ? d : -1;  // Infinity while the path is unknown
}

// The static instance outlives its level: after leaving a contract it still points at the destroyed
// controller, so only a live one counts as being in a contract.
void* level_controller() {
  void* lc = phasmo::LevelController::instance.get();
  return alive(lc) ? lc : nullptr;
}
void* ghost_ai() {
  void* lc = level_controller();
  void* g = lc ? phasmo::LevelController::currentGhost.get(lc) : nullptr;
  return alive(g) ? g : nullptr;
}
int ghost_state(void* g) { return phasmo::GhostAI::state.get(g); }
void* ghost_agent(void* g) {
  void* a = phasmo::GhostAI::agent.get(g);
  return alive(a) ? a : nullptr;
}

void* network() { return phasmo::Network::get_Instance(); }

void* local_player() {
  void* n = network();
  void* p = n ? phasmo::Network::localPlayer.get(n) : nullptr;
  return alive(p) ? p : nullptr;
}
void* player_of_actor(int actor) {
  void* n = network();
  const api::List spots{n ? phasmo::Network::playerSpots.get(n) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    if (spot && actor_number(phasmo::PlayerSpot::photonPlayer.get(spot)) == actor) return phasmo::PlayerSpot::player.get(spot);
  }
  return nullptr;
}
void* local_camera() {
  void* lp = local_player_component();
  void* cam = lp ? phasmo::LocalPlayer::camera.get(lp) : nullptr;
  return alive(cam) ? cam : nullptr;
}

void* ghost_state_object(void* g) {
  void* machine = phasmo::GhostAI::stateMachine.get(g);
  return machine ? phasmo::GhostStateMachine::current.get(machine) : nullptr;
}
bool sensor_detects(void* sensor, void* gameObject) {
  return sensor && gameObject && alive(sensor) && alive(gameObject) && phasmo::Sensor::IsDetected(sensor, gameObject);
}
// As the hunt's sight test decides, without its side effects: the sensor (phasmo::GhostAI::vision)
// detects the player's PlayerLOSTarget objects (phasmo::Player::losTargets), not the Player's own
// GameObject (sight_rule.h).
bool ghost_sees(void* ghostAi, void* player) {
  if (!alive(ghostAi) || !living(player)) return false;
  void* sensor = phasmo::GhostAI::vision.get(ghostAi);
  const api::RefArray parts{phasmo::Player::losTargets.get(player)};
  if (!parts.raw) return false;
  std::vector<void*> list(parts.size());
  for (size_t i = 0; i < list.size(); ++i) list[i] = parts[i];
  return sees_a_part(list.data(), list.size(),
                     [&](void* part) { return alive(part) && sensor_detects(sensor, game_object_of(part)); });
}
Vector3 agent_destination(void* a) { return phasmo::NavMeshAgent::get_destination(a); }

void* ghost_activity(void* g) {
  void* a = phasmo::GhostAI::activity.get(g);
  return alive(a) ? a : nullptr;
}

void* local_player_component() {
  void* n = network();
  void* lp = n ? phasmo::Network::localRig.get(n) : nullptr;
  return alive(lp) ? lp : nullptr;
}

void start_coroutine(void* behaviour, void* routine) {
  if (alive(behaviour) && routine) phasmo::MonoBehaviour::StartCoroutine(behaviour, routine);
}
bool any_players_in_room(void* levelRoom) { return alive(levelRoom) && phasmo::LevelRoom::AnyPlayersInRoom(levelRoom); }
void* level_values() { return phasmo::LevelValues::get_Instance(); }
void* setup_phase() {
  void* s = phasmo::SetupPhaseController::instance.get();
  return alive(s) ? s : nullptr;
}

std::vector<Bounds> room_bounds(void* levelRoom) {
  std::vector<Bounds> out;
  const api::List list{levelRoom ? phasmo::LevelRoom::roomColliders.get(levelRoom) : nullptr};
  for (size_t i = 0; i < list.size(); ++i) {
    void* collider = list[i];
    if (!alive(collider)) continue;
    out.push_back(phasmo::Collider::get_bounds(collider));
  }
  return out;
}

bool in_room(void* levelRoom, Vector3 p, float margin) {
  for (const Bounds& b : room_bounds(levelRoom))
    if (std::fabs(p.x - b.center.x) <= b.extents.x + margin && std::fabs(p.y - b.center.y) <= b.extents.y + margin &&
        std::fabs(p.z - b.center.z) <= b.extents.z + margin)
      return true;
  return false;
}

float distance_to_room(void* levelRoom, Vector3 p) {
  float best = 1e9f;
  for (const Bounds& b : room_bounds(levelRoom)) {
    const float dx = std::max(0.0f, std::fabs(p.x - b.center.x) - b.extents.x);
    const float dy = std::max(0.0f, std::fabs(p.y - b.center.y) - b.extents.y);
    const float dz = std::max(0.0f, std::fabs(p.z - b.center.z) - b.extents.z);
    best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return best;
}

api::RefArray level_rooms() {
  void* level = level_controller();
  return api::RefArray{level ? phasmo::LevelController::rooms.get(level) : nullptr};
}

void* room_at(Vector3 p, float margin) {
  const api::RefArray rooms = level_rooms();
  for (size_t i = 0; i < rooms.size(); ++i)
    if (void* room = rooms[i]; alive(room) && in_room(room, p, margin)) return room;
  return nullptr;
}

void each_running_camera(CameraScan& scan, uint64_t now, const std::function<bool(void* camera)>& visit) {
  if (now >= scan.nextScan) {
    scan.nextScan = now + kCameraScanMs;
    scan.cameras.clear();
    const api::RefArray all{phasmo::Object::FindObjectsOfType(phasmo::CCTV::type.type_object())};
    for (size_t i = 0; i < all.size(); ++i)
      if (void* c = all[i]; alive(c)) scan.cameras.emplace_back(c);
  }
  for (const api::Held& c : scan.cameras) {
    void* camera = c.get();
    if (alive(camera) && phasmo::Equipment::isOn.get(camera) && visit(camera)) return;
  }
}

void* raycast(Vector3 origin, Vector3 direction, float maxDistance) {
  // Physics.DefaultRaycastLayers: Unity's constant -5, every layer but Ignore Raycast.
  return raycast(origin, direction, maxDistance, -5);
}

void* raycast(Vector3 origin, Vector3 direction, float maxDistance, int32_t layerMask) {
  // The RaycastHit the query fills in, sized from the game: an array's stride is the struct's size.
  // Read once: a size that cannot be read or does not fit is remembered too, so no array is made per call.
  static size_t hitSize = 0;
  static bool sized = false;
  if (!std::exchange(sized, true)) hitSize = api::ValueArray{phasmo::RaycastHit::type.make_array(1)}.stride();
  alignas(16) unsigned char hit[128] = {};
  if (!hitSize || hitSize > sizeof hit) {
    static bool logged = false;
    if (!std::exchange(logged, true)) PM_LOG("game: a RaycastHit of %zu bytes does not fit; no ray is cast", hitSize);
    return nullptr;
  }
  const phasmo::Ray ray{origin, direction};
  // QueryTriggerInteraction.Ignore: room and sensor volumes are triggers and must not stop the ray.
  if (!phasmo::Physics::RaycastHit(&ray, hit, maxDistance, layerMask, phasmo::QueryTriggerInteraction::Ignore()))
    return nullptr;
  return phasmo::RaycastHit::get_transform(hit);
}
bool is_child_of(void* transform, void* parent) { return phasmo::Transform::IsChildOf(transform, parent); }

const char* interactable_label(void* hitTransform) {
  if (!hitTransform) return nullptr;
  auto has = [&](void* type) {
    return type && phasmo::Component::GetComponentInParent(hitTransform, type, false) != nullptr;
  };
  if (has(phasmo::Door::type.type_object())) return "Door";
  if (has(phasmo::LightSwitch::type.type_object())) return "Light switch";
  if (has(phasmo::FuseBox::type.type_object())) return "Fuse box";
  if (has(phasmo::PhotonObjectInteract::type.type_object())) return "Object";
  return nullptr;
}

bool cursor_locked() { return phasmo::Cursor::get_lockState() == phasmo::CursorLockMode::Locked(); }

}  // namespace pm::game
