#include "game.h"

#include <windows.h>

#include <cmath>
#include <cstring>

#include "log.h"
#include "players.h"

namespace pg::game {
namespace {

constexpr const char* kPun = "PhotonUnityNetworking.dll";
constexpr const char* kRealtime = "PhotonRealtime.dll";
constexpr const char* kCore = "UnityEngine.CoreModule.dll";
constexpr const char* kAi = "UnityEngine.AIModule.dll";
constexpr const char* kGame = "Assembly-CSharp.dll";
constexpr const char* kInput = "Unity.InputSystem.dll";

// Obfuscated methods used by address (GameAssembly build 25811160).
constexpr uintptr_t kIdleExecuteRva = 0x215B430;  // idle state (enum 0) per-frame execute
// Physics.Raycast(Ray, out RaycastHit, float, int, QueryTriggerInteraction); another
// five-parameter overload exists, so it is taken by address.
constexpr uintptr_t kRaycastRayRva = 0x5284420;
constexpr uintptr_t kInstantiateRva = 0x51F8020;  // Object.Instantiate(Object, Transform, bool)
constexpr uintptr_t kGetComponentsRva = 0x51F48F0;  // GameObject.GetComponents(Type); a generic overload shares the arity
constexpr uintptr_t kDestroyImmediateRva = 0x51F6FD0;  // Object.DestroyImmediate(Object)
constexpr uintptr_t kDetectedComponentsRva = 0x2E871C0;  // Sensor.GetDetectedComponents<T>(List<T>), shared
constexpr uintptr_t kStartCoroutineRva = 0x51F5F00;  // MonoBehaviour.StartCoroutine(IEnumerator); a string overload shares the arity
constexpr uintptr_t kRendererEnabledRva = 0x51D3820;     // Renderer.get_enabled
constexpr uintptr_t kRendererSetEnabledRva = 0x51D3E10;  // Renderer.set_enabled
constexpr uintptr_t kColliderEnabledRva = 0x527C420;     // Collider.get_enabled
constexpr uintptr_t kColliderSetEnabledRva = 0x527C6B0;  // Collider.set_enabled
constexpr uintptr_t kSensorIsDetectedRva = 0x3D1C2E0;    // Micosmo.SensorToolkit.Sensor.IsDetected(GameObject)
constexpr size_t kGhostVisionSensor = 0xA0;              // GhostAI: its LOSSensor for players

struct Api {
  il2cpp::Class* levelController = nullptr;
  void* handlerLateUpdate = nullptr;

  bool (*inRoom)(const void*) = nullptr;
  bool (*isMaster)(const void*) = nullptr;
  void* (*localPlayer)(const void*) = nullptr;
  il2cpp::Array* (*playerList)(const void*) = nullptr;
  void* (*masterClient)(const void*) = nullptr;
  bool (*offlineMode)(const void*) = nullptr;
  void* (*currentRoom)(const void*) = nullptr;
  bool (*roomVisible)(void*, const void*) = nullptr;
  int (*actorNumber)(void*, const void*) = nullptr;

  bool (*objectAlive)(void*, const void*) = nullptr;
  void* (*getTransform)(void*, const void*) = nullptr;
  Vector3 (*getPosition)(void*, const void*) = nullptr;
  void (*setPosition)(void*, Vector3, const void*) = nullptr;
  Vector3 (*getForward)(void*, const void*) = nullptr;
  Quaternion (*getRotation)(void*, const void*) = nullptr;
  void (*setRotation)(void*, Quaternion, const void*) = nullptr;
  il2cpp::Class* gameObject = nullptr;
  void (*gameObjectCtor)(void*, il2cpp::String*, const void*) = nullptr;
  void* (*addComponent)(void*, void*, const void*) = nullptr;
  void* (*getComponent)(void*, void*, const void*) = nullptr;
  void* (*componentGameObject)(void*, const void*) = nullptr;
  void (*setEnabled)(void*, bool, const void*) = nullptr;
  bool (*getEnabled)(void*, const void*) = nullptr;
  void (*destroy)(void*, const void*) = nullptr;
  void (*setActive)(void*, bool, const void*) = nullptr;
  void* (*getParent)(void*, const void*) = nullptr;
  void (*setParent)(void*, void*, bool, const void*) = nullptr;
  Vector3 (*getLocalPosition)(void*, const void*) = nullptr;
  void (*setLocalPosition)(void*, Vector3, const void*) = nullptr;
  Quaternion (*getLocalRotation)(void*, const void*) = nullptr;
  void (*setLocalRotation)(void*, Quaternion, const void*) = nullptr;
  void (*setLocalScale)(void*, Vector3, const void*) = nullptr;
  Vector3 (*getLocalScale)(void*, const void*) = nullptr;
  Vector3 (*getLossyScale)(void*, const void*) = nullptr;
  void (*setLayer)(void*, int, const void*) = nullptr;
  bool (*rendererEnabled)(void*, const void*) = nullptr;
  void (*setRendererEnabled)(void*, bool, const void*) = nullptr;
  bool (*colliderEnabled)(void*, const void*) = nullptr;
  bool (*sensorIsDetected)(void*, void*, const void*) = nullptr;
  void (*setColliderEnabled)(void*, bool, const void*) = nullptr;
  void* (*instantiate)(void*, void*, bool, const void*) = nullptr;
  il2cpp::Array* (*getComponents)(void*, il2cpp::Object*, const void*) = nullptr;
  void (*destroyImmediate)(void*, const void*) = nullptr;
  float (*fieldOfView)(void*, const void*) = nullptr;
  float (*aspect)(void*, const void*) = nullptr;
  void* (*find)(void*, il2cpp::String*, const void*) = nullptr;
  void* (*objectTransform)(void*, const void*) = nullptr;
  int (*childCount)(void*, const void*) = nullptr;
  void* (*getChild)(void*, int, const void*) = nullptr;
  void* (*currentMouse)(const void*) = nullptr;
  void* (*pointerDelta)(void*, const void*) = nullptr;
  void* (*pointerPosition)(void*, const void*) = nullptr;
  void* (*leftButton)(void*, const void*) = nullptr;
  bool (*wasPressedThisFrame)(void*, const void*) = nullptr;
  bool (*isPressed)(void*, const void*) = nullptr;
  bool (*activeSelf)(void*, const void*) = nullptr;
  bool (*activeInHierarchy)(void*, const void*) = nullptr;
  int (*getLayer)(void*, const void*) = nullptr;
  void (*setCursorVisible)(bool, const void*) = nullptr;
  void (*setCursorLockState)(int, const void*) = nullptr;
  float (*agentSpeed)(void*, const void*) = nullptr;
  float (*agentRemaining)(void*, const void*) = nullptr;
  bool (*agentPathPending)(void*, const void*) = nullptr;
  bool (*setDestination)(void*, Vector3, const void*) = nullptr;
  Vector3 (*getDestination)(void*, const void*) = nullptr;

  void* (*networkInstance)(const void*) = nullptr;
  void (*freezeMovement)(void*, bool, const void*) = nullptr;

  void* changeState = nullptr;
  void* normalInteraction = nullptr;
  void* twinInteraction = nullptr;
  void* tryInteract = nullptr;
  void* (*startCoroutine)(void*, void*, const void*) = nullptr;
  bool (*anyPlayersInRoom)(void*, const void*) = nullptr;
  void* (*levelValues)(const void*) = nullptr;
  il2cpp::Class* setupPhase = nullptr;
  void* randomRangeInt = nullptr;
  bool (*raycast)(Ray, RaycastHit*, float, int, int, const void*) = nullptr;
  void* (*hitTransform)(RaycastHit*, const void*) = nullptr;
  bool (*isChildOf)(void*, void*, const void*) = nullptr;
  void* (*componentInParent)(void*, void*, bool, const void*) = nullptr;
  int (*cursorLockState)(const void*) = nullptr;
  il2cpp::Object* doorType = nullptr;
  il2cpp::Object* lightSwitchType = nullptr;
  il2cpp::Object* fuseBoxType = nullptr;
  il2cpp::Object* interactType = nullptr;
  il2cpp::Field* roomColliders = nullptr;           // LevelRoom.roomColliders: List<Collider>
  Bounds (*colliderBounds)(void*, const void*) = nullptr;
  char* gameAssembly = nullptr;

  void (*actionEnable)(void*, const void*) = nullptr;
  void (*actionDisable)(void*, const void*) = nullptr;
  bool (*actionEnabled)(void*, const void*) = nullptr;
} api;

using il2cpp::bind;

// A method found by other means than name and arity (overloads with the same arity).
template <class T>
bool bind(T& out, void* methodPointer, std::nullptr_t, int) {
  out = reinterpret_cast<T>(methodPointer);
  return out != nullptr;
}


}  // namespace

bool resolve() {
  il2cpp::Class* pn = il2cpp::cls(kPun, "Photon.Pun", "PhotonNetwork");
  il2cpp::Class* handler = il2cpp::cls(kPun, "Photon.Pun", "PhotonHandler");
  il2cpp::Class* photonPlayer = il2cpp::cls(kRealtime, "Photon.Realtime", "Player");
  il2cpp::Class* object = il2cpp::cls(kCore, "UnityEngine", "Object");
  il2cpp::Class* component = il2cpp::cls(kCore, "UnityEngine", "Component");
  il2cpp::Class* transform = il2cpp::cls(kCore, "UnityEngine", "Transform");
  il2cpp::Class* agent = il2cpp::cls(kAi, "UnityEngine.AI", "NavMeshAgent");
  il2cpp::Class* network = il2cpp::cls(kGame, "", "Network");
  il2cpp::Class* player = il2cpp::cls(kGame, "", "Player");
  il2cpp::Class* activity = il2cpp::cls(kGame, "", "GhostActivity");
  il2cpp::Class* ghostAi = il2cpp::cls(kGame, "", "GhostAI");
  il2cpp::Class* action = il2cpp::cls(kInput, "UnityEngine.InputSystem", "InputAction");
  il2cpp::Class* random = il2cpp::cls(kCore, "UnityEngine", "Random");
  il2cpp::Class* physics = il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Physics");
  il2cpp::Class* hit = il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "RaycastHit");
  api.levelController = il2cpp::cls(kGame, "", "LevelController");
  api.gameAssembly = reinterpret_cast<char*>(GetModuleHandleW(L"GameAssembly.dll"));

  bool ok = api.levelController != nullptr;
  ok &= bind(api.handlerLateUpdate, handler, "LateUpdate", 0);
  ok &= bind(api.inRoom, pn, "get_InRoom", 0);
  ok &= bind(api.isMaster, pn, "get_IsMasterClient", 0);
  ok &= bind(api.localPlayer, pn, "get_LocalPlayer", 0);
  ok &= bind(api.playerList, pn, "get_PlayerList", 0);
  ok &= bind(api.masterClient, pn, "get_MasterClient", 0);
  ok &= bind(api.offlineMode, pn, "get_OfflineMode", 0);
  ok &= bind(api.currentRoom, pn, "get_CurrentRoom", 0);
  ok &= bind(api.roomVisible, il2cpp::cls(kRealtime, "Photon.Realtime", "Room"), "get_IsVisible", 0);
  ok &= bind(api.actorNumber, photonPlayer, "get_ActorNumber", 0);
  ok &= bind(api.objectAlive, object, "op_Implicit", 1);
  ok &= bind(api.getTransform, component, "get_transform", 0);
  ok &= bind(api.getPosition, transform, "get_position", 0);
  ok &= bind(api.setPosition, transform, "set_position", 1);
  ok &= bind(api.getForward, transform, "get_forward", 0);
  ok &= bind(api.getRotation, transform, "get_rotation", 0);
  ok &= bind(api.setRotation, transform, "set_rotation", 1);
  il2cpp::Class* behaviour = il2cpp::cls(kCore, "UnityEngine", "Behaviour");
  api.gameObject = il2cpp::cls(kCore, "UnityEngine", "GameObject");
  ok &= bind(api.gameObjectCtor, api.gameObject, ".ctor", 1);
  ok &= bind(api.addComponent, api.gameObject, "AddComponent", 1);
  ok &= bind(api.getComponent, component, "GetComponent", 1);
  ok &= bind(api.componentGameObject, component, "get_gameObject", 0);
  ok &= bind(api.setEnabled, behaviour, "set_enabled", 1);
  ok &= bind(api.getEnabled, behaviour, "get_enabled", 0);
  ok &= bind(api.destroy, object, "Destroy", 1);
  ok &= bind(api.setActive, api.gameObject, "SetActive", 1);
  ok &= bind(api.getParent, transform, "get_parent", 0);
  ok &= bind(api.setParent, transform, "SetParent", 2);
  ok &= bind(api.getLocalPosition, transform, "get_localPosition", 0);
  ok &= bind(api.setLocalPosition, transform, "set_localPosition", 1);
  ok &= bind(api.getLocalRotation, transform, "get_localRotation", 0);
  ok &= bind(api.setLocalRotation, transform, "set_localRotation", 1);
  ok &= bind(api.setLocalScale, transform, "set_localScale", 1);
  ok &= bind(api.getLocalScale, transform, "get_localScale", 0);
  ok &= bind(api.getLossyScale, transform, "get_lossyScale", 0);
  // Object.Instantiate(Object, Transform, bool): the non-generic overload needs no inflated method.
  ok &= bind(api.instantiate, api.gameAssembly + kInstantiateRva, nullptr, 0);
  ok &= bind(api.getComponents, api.gameAssembly + kGetComponentsRva, nullptr, 0);
  ok &= bind(api.destroyImmediate, api.gameAssembly + kDestroyImmediateRva, nullptr, 0);
  ok &= bind(api.fieldOfView, il2cpp::cls(kCore, "UnityEngine", "Camera"), "get_fieldOfView", 0);
  ok &= bind(api.aspect, il2cpp::cls(kCore, "UnityEngine", "Camera"), "get_aspect", 0);
  ok &= bind(api.find, transform, "Find", 1);
  ok &= bind(api.objectTransform, api.gameObject, "get_transform", 0);
  ok &= bind(api.activeSelf, api.gameObject, "get_activeSelf", 0);
  ok &= bind(api.activeInHierarchy, api.gameObject, "get_activeInHierarchy", 0);
  ok &= bind(api.getLayer, api.gameObject, "get_layer", 0);
  ok &= bind(api.setLayer, api.gameObject, "set_layer", 1);
  ok &= bind(api.childCount, transform, "get_childCount", 0);
  ok &= bind(api.getChild, transform, "GetChild", 1);
  ok &= bind(api.currentMouse, il2cpp::cls(kInput, "UnityEngine.InputSystem", "Mouse"), "get_current", 0);
  ok &= bind(api.pointerDelta, il2cpp::cls(kInput, "UnityEngine.InputSystem", "Pointer"), "get_delta", 0);
  ok &= bind(api.pointerPosition, il2cpp::cls(kInput, "UnityEngine.InputSystem", "Pointer"), "get_position", 0);
  ok &= bind(api.leftButton, il2cpp::cls(kInput, "UnityEngine.InputSystem", "Mouse"), "get_leftButton", 0);
  ok &= bind(api.wasPressedThisFrame, il2cpp::cls(kInput, "UnityEngine.InputSystem.Controls", "ButtonControl"), "get_wasPressedThisFrame", 0);
  ok &= bind(api.isPressed, il2cpp::cls(kInput, "UnityEngine.InputSystem.Controls", "ButtonControl"), "get_isPressed", 0);
  ok &= bind(api.setCursorVisible, il2cpp::cls(kCore, "UnityEngine", "Cursor"), "set_visible", 1);
  ok &= bind(api.setCursorLockState, il2cpp::cls(kCore, "UnityEngine", "Cursor"), "set_lockState", 1);
  ok &= bind(api.agentSpeed, agent, "get_speed", 0);
  ok &= bind(api.agentRemaining, agent, "get_remainingDistance", 0);
  ok &= bind(api.agentPathPending, agent, "get_pathPending", 0);
  ok &= bind(api.setDestination, agent, "SetDestination", 1);
  ok &= bind(api.getDestination, agent, "get_destination", 0);
  ok &= bind(api.networkInstance, network, "get_Instance", 0);
  ok &= bind(api.freezeMovement, player, "ToggleFreezePlayerMovement", 1);
  ok &= bind(api.changeState, ghostAi, "ChangeState", 3);
  ok &= bind(api.normalInteraction, activity, "NormalInteraction", 1);
  ok &= bind(api.twinInteraction, activity, "TwinInteraction", 1);
  ok &= bind(api.tryInteract, activity, "TryInteract", 0);
  ok &= bind(api.startCoroutine, api.gameAssembly + kStartCoroutineRva, nullptr, 0);
  ok &= bind(api.rendererEnabled, api.gameAssembly + kRendererEnabledRva, nullptr, 0);
  ok &= bind(api.setRendererEnabled, api.gameAssembly + kRendererSetEnabledRva, nullptr, 0);
  ok &= bind(api.colliderEnabled, api.gameAssembly + kColliderEnabledRva, nullptr, 0);
  ok &= bind(api.setColliderEnabled, api.gameAssembly + kColliderSetEnabledRva, nullptr, 0);
  ok &= bind(api.sensorIsDetected, api.gameAssembly + kSensorIsDetectedRva, nullptr, 0);
  ok &= bind(api.anyPlayersInRoom, il2cpp::cls(kGame, "", "LevelRoom"), "AnyPlayersInRoom", 0);
  ok &= bind(api.levelValues, il2cpp::cls(kGame, "", "LevelValues"), "get_Instance", 0);
  api.setupPhase = il2cpp::cls(kGame, "", "SetupPhaseController");
  ok &= api.setupPhase != nullptr;
  ok &= bind(api.randomRangeInt, random, "RandomRangeInt", 2);
  ok &= physics && bind(api.raycast, api.gameAssembly + kRaycastRayRva, nullptr, 0);
  ok &= bind(api.hitTransform, hit, "get_transform", 0);
  ok &= bind(api.isChildOf, transform, "IsChildOf", 1);
  ok &= bind(api.componentInParent, component, "GetComponentInParent", 2);
  ok &= bind(api.cursorLockState, il2cpp::cls(kCore, "UnityEngine", "Cursor"), "get_lockState", 0);
  api.doorType = il2cpp::type_object(il2cpp::cls(kGame, "", "Door"));
  api.lightSwitchType = il2cpp::type_object(il2cpp::cls(kGame, "", "LightSwitch"));
  api.fuseBoxType = il2cpp::type_object(il2cpp::cls(kGame, "", "FuseBox"));
  api.interactType = il2cpp::type_object(il2cpp::cls(kGame, "", "PhotonObjectInteract"));
  ok &= api.doorType && api.lightSwitchType && api.fuseBoxType && api.interactType;
  api.roomColliders = il2cpp::field_info(il2cpp::cls(kGame, "", "LevelRoom"), "roomColliders");
  ok &= api.roomColliders && bind(api.colliderBounds, il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Collider"), "get_bounds", 0);
  ok &= bind(api.actionEnable, action, "Enable", 0);
  ok &= bind(api.actionDisable, action, "Disable", 0);
  ok &= bind(api.actionEnabled, action, "get_enabled", 0);
  PG_LOG("game: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool in_room() { return api.inRoom(nullptr); }
bool is_master() { return api.isMaster(nullptr); }
void* photon_local_player() { return api.localPlayer(nullptr); }
il2cpp::Array* photon_player_list() { return api.playerList(nullptr); }
int actor_number(void* p) { return api.actorNumber(p, nullptr); }

bool private_room() {
  if (!in_room()) return false;
  if (api.offlineMode(nullptr)) return true;  // single player
  void* room = api.currentRoom(nullptr);
  return room && !api.roomVisible(room, nullptr);  // not listed in the public lobby browser
}

int local_actor() {
  if (!in_room()) return 0;
  void* p = photon_local_player();
  return p ? actor_number(p) : 0;
}
int master_actor() {
  if (!in_room()) return 0;
  void* p = api.masterClient(nullptr);
  return p ? actor_number(p) : 0;
}
void room_actors(std::vector<int>& out) {
  out.clear();
  if (!in_room()) return;
  il2cpp::Array* list = photon_player_list();
  for (uintptr_t i = 0; list && i < list->max_length; ++i) {
    if (void* p = list->data<void*>()[i]) out.push_back(actor_number(p));
  }
}

// op_Implicit is static: (object, MethodInfo*)
bool alive(void* o) { return o && api.objectAlive(o, nullptr); }
void* transform_of(void* c) { return api.getTransform(c, nullptr); }
void* object_transform(void* go) { return go ? api.objectTransform(go, nullptr) : nullptr; }
Vector3 position(void* t) { return api.getPosition(t, nullptr); }
void set_position(void* t, Vector3 p) { api.setPosition(t, p, nullptr); }
Vector3 forward(void* t) { return api.getForward(t, nullptr); }
Quaternion rotation(void* t) { return api.getRotation(t, nullptr); }
void set_rotation(void* t, Quaternion r) { api.setRotation(t, r, nullptr); }

void* new_game_object(const char* name) {
  il2cpp::Object* go = il2cpp::object_new(api.gameObject);
  if (!go) return nullptr;
  api.gameObjectCtor(go, il2cpp::string_new(name), nullptr);
  return go;
}
void* add_component(void* go, il2cpp::Object* type) { return go && type ? api.addComponent(go, type, nullptr) : nullptr; }
void* get_component(void* c, il2cpp::Object* type) { return alive(c) && type ? api.getComponent(c, type, nullptr) : nullptr; }
void* game_object_of(void* c) { return api.componentGameObject(c, nullptr); }
void set_enabled(void* b, bool on) { api.setEnabled(b, on, nullptr); }
bool enabled(void* b) { return api.getEnabled(b, nullptr); }
void destroy(void* o) {
  if (alive(o)) api.destroy(o, nullptr);
}
void set_active(void* go, bool on) {
  if (alive(go)) api.setActive(go, on, nullptr);
}
void* parent(void* t) { return api.getParent(t, nullptr); }
void set_parent(void* t, void* p, bool worldPositionStays) { api.setParent(t, p, worldPositionStays, nullptr); }
Vector3 local_position(void* t) { return api.getLocalPosition(t, nullptr); }
void set_local_position(void* t, Vector3 p) { api.setLocalPosition(t, p, nullptr); }
Quaternion local_rotation(void* t) { return api.getLocalRotation(t, nullptr); }
void set_local_rotation(void* t, Quaternion r) { api.setLocalRotation(t, r, nullptr); }
void set_local_scale(void* t, Vector3 s) { api.setLocalScale(t, s, nullptr); }
Vector3 local_scale(void* t) { return api.getLocalScale(t, nullptr); }
Vector3 lossy_scale(void* t) { return api.getLossyScale(t, nullptr); }
void* instantiate(void* original, void* parent, bool worldPositionStays) {
  return api.instantiate(original, parent, worldPositionStays, nullptr);
}
il2cpp::Array* components(void* go, il2cpp::Object* type) { return go && type ? api.getComponents(go, type, nullptr) : nullptr; }
void destroy_immediate(void* object) { if (object) api.destroyImmediate(object, nullptr); }
float field_of_view(void* camera) { return api.fieldOfView(camera, nullptr); }
float aspect(void* camera) { return api.aspect(camera, nullptr); }
void* find_child(void* t, const char* path) { return t ? api.find(t, il2cpp::string_new(path), nullptr) : nullptr; }
int child_count(void* t) { return t ? api.childCount(t, nullptr) : 0; }
void* child(void* t, int index) { return api.getChild(t, index, nullptr); }

namespace {
// InputControl<Vector2>.ReadValue, looked up on the control's own (inflated) class.
bool read_vector2(void* control, float& x, float& y) {
  x = y = 0;
  if (!control) return false;
  static const il2cpp::Method* readValue = nullptr;
  if (!readValue) readValue = il2cpp::method(il2cpp::class_of(control), "ReadValue", 0);
  if (!readValue) return false;
  struct Vector2 {
    float x, y;
  };
  auto read = reinterpret_cast<Vector2 (*)(void*, const void*)>(*reinterpret_cast<void* const*>(readValue));
  const Vector2 v = read(control, readValue);
  x = v.x;
  y = v.y;
  return true;
}
}  // namespace

bool mouse_delta(float& dx, float& dy) {
  void* mouse = api.currentMouse(nullptr);
  return read_vector2(mouse ? api.pointerDelta(mouse, nullptr) : nullptr, dx, dy);
}

bool mouse_position(float& x, float& y) {
  void* mouse = api.currentMouse(nullptr);
  return read_vector2(mouse ? api.pointerPosition(mouse, nullptr) : nullptr, x, y);
}

bool mouse_left_pressed() {
  void* mouse = api.currentMouse(nullptr);
  void* button = mouse ? api.leftButton(mouse, nullptr) : nullptr;
  return button && api.wasPressedThisFrame(button, nullptr);
}

bool mouse_left_held() {
  void* mouse = api.currentMouse(nullptr);
  void* button = mouse ? api.leftButton(mouse, nullptr) : nullptr;
  return button && api.isPressed(button, nullptr);
}

bool active_self(void* go) { return go && api.activeSelf(go, nullptr); }
bool active_in_hierarchy(void* go) { return go && api.activeInHierarchy(go, nullptr); }
int layer(void* go) { return go ? api.getLayer(go, nullptr) : -1; }
void set_layer(void* go, int layer) { if (go) api.setLayer(go, layer, nullptr); }
bool renderer_enabled(void* r) { return api.rendererEnabled(r, nullptr); }
void set_renderer_enabled(void* r, bool on) { api.setRendererEnabled(r, on, nullptr); }
bool collider_enabled(void* c) { return api.colliderEnabled(c, nullptr); }
void set_collider_enabled(void* c, bool on) { api.setColliderEnabled(c, on, nullptr); }

void lock_cursor() { api.setCursorLockState(1, nullptr); }

void free_cursor() {
  api.setCursorLockState(0, nullptr);  // CursorLockMode.None
  api.setCursorVisible(true, nullptr);
}
bool set_destination(void* a, Vector3 target) { return api.setDestination(a, target, nullptr); }
float agent_speed(void* a) { return api.agentSpeed(a, nullptr); }
float agent_path_length(void* a) {
  if (api.agentPathPending(a, nullptr)) return -1;
  const float d = api.agentRemaining(a, nullptr);
  return d >= 0 && d < 1e6f ? d : -1;  // Infinity while the path is unknown
}

// The static instance outlives its level: after leaving a contract it still points at the destroyed
// controller, so only a live one counts as being in a contract.
void* level_controller() {
  void* lc = il2cpp::static_field_by_type(api.levelController, "LevelController");
  return alive(lc) ? lc : nullptr;
}
void* ghost_ai() {
  void* lc = level_controller();
  void* g = lc ? il2cpp::field<void*>(lc, 0x40) : nullptr;
  return alive(g) ? g : nullptr;
}
int ghost_state(void* g) { return il2cpp::field<int>(g, 0x30); }
void* ghost_agent(void* g) {
  void* a = il2cpp::field<void*>(g, 0x40);
  return alive(a) ? a : nullptr;
}

void* network() { return api.networkInstance(nullptr); }

void* local_player() {
  void* n = api.networkInstance(nullptr);
  void* p = n ? il2cpp::field<void*>(n, 0x28) : nullptr;
  return alive(p) ? p : nullptr;
}
void* player_of_actor(int actor) {
  constexpr size_t kNetworkSpots = 0x38;      // Network: List<PlayerSpot>
  constexpr size_t kSpotPhotonPlayer = 0x18;  // PlayerSpot: Photon.Realtime.Player
  constexpr size_t kSpotPlayer = 0x38;        // PlayerSpot: networked Player
  void* n = api.networkInstance(nullptr);
  auto* spots = n ? il2cpp::field<il2cpp::List*>(n, kNetworkSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    if (spot && actor_number(il2cpp::field<void*>(spot, kSpotPhotonPlayer)) == actor) return il2cpp::field<void*>(spot, kSpotPlayer);
  }
  return nullptr;
}
void* local_camera() {
  void* n = api.networkInstance(nullptr);
  void* lp = n ? il2cpp::field<void*>(n, 0x30) : nullptr;
  if (!alive(lp)) return nullptr;
  void* cam = il2cpp::field<void*>(lp, 0x30);
  return alive(cam) ? cam : nullptr;
}
void freeze_player_movement(void* p, bool frozen) { api.freezeMovement(p, frozen, nullptr); }

void* ghost_state_object(void* g) {
  void* machine = il2cpp::field<void*>(g, 0x28);
  return machine ? il2cpp::field<void*>(machine, 0x10) : nullptr;
}
bool ghost_sees(void* ghostAi, void* player) {
  if (!alive(ghostAi) || !living(player)) return false;
  void* sensor = il2cpp::field<void*>(ghostAi, kGhostVisionSensor);
  void* go = alive(sensor) ? game_object_of(player) : nullptr;
  return go && api.sensorIsDetected(sensor, go, nullptr);
}
Vector3 agent_destination(void* a) { return api.getDestination(a, nullptr); }

void* ghost_activity(void* g) {
  void* a = il2cpp::field<void*>(g, 0x58);
  return alive(a) ? a : nullptr;
}

void for_each_input_action(void* localPlayer, const std::function<void(void* action, const char* map, const char* name)>& fn) {
  if (!alive(localPlayer) || std::strcmp(il2cpp::class_name(il2cpp::class_of(localPlayer)), "LocalPCPlayer") != 0) return;
  void* controls = il2cpp::field<void*>(localPlayer, 0x118);         // LocalPCPlayer.PCControls
  void* asset = controls ? il2cpp::field<void*>(controls, 0x20) : nullptr;  // InputActionAsset
  auto* maps = asset ? il2cpp::field<il2cpp::Array*>(asset, 0x18) : nullptr;  // m_ActionMaps
  for (uintptr_t i = 0; maps && i < maps->max_length; ++i) {
    void* map = maps->data<void*>()[i];
    if (!map) continue;
    char mapName[64];
    il2cpp::to_utf8(il2cpp::field<il2cpp::String*>(map, 0x10), mapName, sizeof mapName);
    auto* actions = il2cpp::field<il2cpp::Array*>(map, 0x28);  // m_Actions
    for (uintptr_t j = 0; actions && j < actions->max_length; ++j) {
      void* a = actions->data<void*>()[j];
      if (!a) continue;
      char name[64];
      il2cpp::to_utf8(il2cpp::field<il2cpp::String*>(a, 0x10), name, sizeof name);
      fn(a, mapName, name);
    }
  }
}
void* local_player_component() {
  void* n = api.networkInstance(nullptr);
  void* lp = n ? il2cpp::field<void*>(n, 0x30) : nullptr;
  return alive(lp) ? lp : nullptr;
}
bool action_enabled(void* a) { return api.actionEnabled(a, nullptr); }
void set_action_enabled(void* a, bool on) { on ? api.actionEnable(a, nullptr) : api.actionDisable(a, nullptr); }

void* photon_handler_late_update() { return api.handlerLateUpdate; }
void* change_state() { return api.changeState; }
void* normal_interaction() { return api.normalInteraction; }
void* idle_execute() { return api.gameAssembly + kIdleExecuteRva; }
void* twin_interaction() { return api.twinInteraction; }
void* try_interact() { return api.tryInteract; }
void start_coroutine(void* behaviour, void* routine) {
  if (alive(behaviour) && routine) api.startCoroutine(behaviour, routine, nullptr);
}
bool any_players_in_room(void* levelRoom) { return alive(levelRoom) && api.anyPlayersInRoom(levelRoom, nullptr); }
void* level_values() { return api.levelValues(nullptr); }
void* setup_phase() {
  void* s = il2cpp::static_field_by_type(api.setupPhase, "SetupPhaseController");
  return alive(s) ? s : nullptr;
}
void* random_range_int() { return api.randomRangeInt; }
void* detected_components() { return api.gameAssembly + kDetectedComponentsRva; }
void* game_assembly_address(uintptr_t rva) { return api.gameAssembly ? api.gameAssembly + rva : nullptr; }

bool in_function(const void* address, uintptr_t rva, uintptr_t length) {
  const char* a = static_cast<const char*>(address);
  return a >= api.gameAssembly + rva && a < api.gameAssembly + rva + length;
}

std::vector<Bounds> room_bounds(void* levelRoom) {
  std::vector<Bounds> out;
  auto* list = levelRoom ? static_cast<il2cpp::List*>(il2cpp::reference_field(levelRoom, api.roomColliders)) : nullptr;
  for (int i = 0; list && list->items && i < list->size; ++i) {
    void* collider = list->items->data<void*>()[i];
    if (alive(collider)) out.push_back(api.colliderBounds(collider, nullptr));
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

void* raycast(Vector3 origin, Vector3 direction, float maxDistance) {
  RaycastHit hit{};
  // Physics.DefaultRaycastLayers, QueryTriggerInteraction.Ignore: room and sensor volumes are
  // triggers and must not stop the ray.
  if (!api.raycast({origin, direction}, &hit, maxDistance, -5, 1, nullptr)) return nullptr;
  return api.hitTransform(&hit, nullptr);
}
bool is_child_of(void* transform, void* parent) { return api.isChildOf(transform, parent, nullptr); }

const char* interactable_label(void* hitTransform) {
  if (!hitTransform) return nullptr;
  auto has = [&](il2cpp::Object* type) { return api.componentInParent(hitTransform, type, false, nullptr) != nullptr; };
  if (has(api.doorType)) return "Door";
  if (has(api.lightSwitchType)) return "Light switch";
  if (has(api.fuseBoxType)) return "Fuse box";
  if (has(api.interactType)) return "Object";
  return nullptr;
}

bool cursor_locked() { return api.cursorLockState(nullptr) == 1; }  // CursorLockMode.Locked

}  // namespace pg::game
