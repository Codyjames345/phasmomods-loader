#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include "Camera.h"
#include "Difficulty.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "held.h"
#include "runtime.h"

// Plain helpers over the game API (sdk/api, sdk/map): every member they reach is a map entry, so a game
// update moves only the map. In a mod they call through the mod's host contract, in the host through the
// host's own. All calls must be made on Unity's main thread.
namespace pm::game {

using phasmo::Quaternion;
using phasmo::Vector3;

constexpr float kGhostEyeHeight = 1.6f;  // metres above the ghost's transform the ghost player sees from

bool resolve();  // false when GameAssembly.dll is not loaded

// Photon (PUN / Realtime)
bool in_room();
bool is_master();
int actor_number(void* photonPlayer);
// Single player, or a room that is not listed publicly. The mod only acts in these. False whenever one of
// the Photon members it reads did not resolve in this game build: a room it cannot judge is no private one.
bool private_room();
int local_actor();   // 0 when not in a room, or when the actor numbers cannot be read
int master_actor();  // 0 when not in a room, or when the actor numbers cannot be read
void room_actors(std::vector<int>& out);

// Unity
bool alive(void* unityObject);  // UnityEngine.Object implicit bool (not destroyed)
// The object's own Transform, whichever kind of object this is: a GameObject answers through
// GameObject.get_transform, anything else through Component.get_transform. A GameObject is not a Component, so
// the two accessors are not interchangeable, and the helper picks by the object's own class. Null when the object
// is null or when its kind cannot be read: nothing is guessed. Main thread, any phase.
api::Ref<pm::map::Transform> transform_of(void* gameObjectOrComponent);
api::Ref<pm::map::Transform> object_transform(void* gameObject);  // GameObject.get_transform, for one known to be one
// The RectTransform of a UI object, for the entries that set anchors, pivot, position and size: those read a
// layout a plain Transform does not have. Null when the object's transform is a plain Transform, when the object
// is null, or when the class cannot be read. An object made at run time has a RectTransform once it takes a
// component whose class requires one; see sdk/systems/menus.md, "A list that scrolls".
api::Ref<pm::map::Transform> rect_transform_of(void* gameObjectOrComponent);
Vector3 position(void* transform);
inline float distance(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }
void set_position(void* transform, Vector3 p);
Vector3 forward(void* transform);
bool set_destination(void* agent, Vector3 target);
float agent_speed(void* agent);
float agent_path_length(void* agent);  // NavMeshAgent.remainingDistance, negative while unknown
Quaternion rotation(void* transform);
void set_rotation(void* transform, Quaternion r);
api::Object new_game_object(const char* name);
api::Object add_component(void* gameObject, void* type);  // type: System.Type
// The GameObject the object belongs to: a GameObject's own is itself, and a component's comes from
// Component.get_gameObject. Null when the object is null or when its kind cannot be read.
api::Object game_object_of(void* gameObjectOrComponent);
void set_enabled(void* behaviour, bool on);
bool enabled(void* behaviour);
void destroy(void* object);  // UnityEngine.Object.Destroy
void set_active(void* gameObject, bool on);
api::Ref<pm::map::Transform> parent(void* transform);
void set_parent(void* transform, void* parent, bool worldPositionStays);
Vector3 local_position(void* transform);
void set_local_position(void* transform, Vector3 p);
Quaternion local_rotation(void* transform);
void set_local_rotation(void* transform, Quaternion r);
void set_local_scale(void* transform, Vector3 s);
Vector3 local_scale(void* transform);
Vector3 lossy_scale(void* transform);  // Transform.lossyScale: its scale in the world
// UnityEngine.Object.Instantiate(original, parent, worldPositionStays): a copy of a GameObject or
// component (and its children) under parent; returns the copy of what was passed. Null, with nothing copied, for a
// null or destroyed original or parent. Unity differs: it throws for a null original and clones a null parent's copy
// at the scene root. Every caller passes a live parent.
api::Object instantiate(void* original, void* parent, bool worldPositionStays);
int child_count(void* transform);
api::Ref<pm::map::Transform> child(void* transform, int index);
// Input System mouse movement since the last frame, in pixels; false when there is no mouse.
bool mouse_delta(float& dx, float& dy);
void lock_cursor();  // CursorLockMode.Locked
void free_cursor();  // CursorLockMode.None and visible, for the mod's own menus
// Input System pointer position in screen pixels (origin bottom left), false when there is no mouse.
bool mouse_position(float& x, float& y);
bool mouse_left_pressed();  // the left button went down this frame
bool mouse_left_held();
bool active_self(void* gameObject);  // GameObject.activeSelf
bool active_in_hierarchy(void* gameObject);
int layer(void* gameObject);
void set_layer(void* gameObject, int layer);
// Renderer.enabled and Collider.enabled (components, not Behaviours).
bool renderer_enabled(void* renderer);
void set_renderer_enabled(void* renderer, bool on);
bool collider_enabled(void* collider);
void set_collider_enabled(void* collider, bool on);

// Game singletons
api::Ref<pm::map::LevelController> level_controller();  // LevelController static instance, null outside a contract
api::Ref<pm::map::GhostAI> ghost_ai();               // phasmo::LevelController::currentGhost, alive
int ghost_state(void* ghostAi);                       // phasmo::GhostAI::state, a phasmo::GhostState value
api::Ref<pm::map::NavMeshAgent> ghost_agent(void* ghostAi);  // phasmo::GhostAI::agent, a NavMeshAgent
api::Ref<pm::map::Network> network();                 // Network.Instance
api::Object local_player();                           // phasmo::Network::localPlayer, this client's Player
api::Object player_of_actor(int actor);  // the networked Player of a Photon actor (Network's spots), or null
api::Ref<pm::map::Camera> local_camera();             // phasmo::LocalPlayer::camera of local_player_component()
api::Ref<pm::map::GhostActivity> ghost_activity(void* ghostAi);  // phasmo::GhostAI::activity
api::Object ghost_state_object(void* ghostAi);  // the ghost state machine's current state object
/**
 * Whether the ghost's own vision sensor (phasmo::GhostAI::vision) detects a living player: any of the
 * player's PlayerLOSTarget objects (phasmo::Player::losTargets), as the hunt's sight test asks, without
 * that test's side effects. False for null or dead.
 */
bool ghost_sees(void* ghostAi, void* player);
/**
 * Whether a SensorToolkit sensor detects this GameObject now (Sensor.IsDetected): whether it is one of the
 * sensor's signal objects, the objects Sensor.GetDetectedComponents<T> reads its components from. False for
 * null or destroyed.
 */
bool sensor_detects(void* sensor, void* gameObject);
Vector3 agent_destination(void* agent);
void start_coroutine(void* behaviour, void* routine);  // MonoBehaviour.StartCoroutine(IEnumerator)
bool any_players_in_room(void* levelRoom);            // LevelRoom.AnyPlayersInRoom
api::Ref<pm::map::LevelValues> level_values();        // LevelValues.Instance
api::Ref<pm::map::SetupPhaseController> setup_phase();  // SetupPhaseController's instance, null outside a contract
api::Ref<pm::map::LocalPlayer> local_player_component();  // phasmo::Network::localRig, LocalPlayer (LocalPCPlayer on PC)

// A LevelRoom's colliders' world bounds (empty when it has none), as the map's Bounds (centre and extents).
using Bounds = phasmo::Bounds;
std::vector<Bounds> room_bounds(void* levelRoom);
/** Whether a world point lies inside a LevelRoom's colliders, widened by `margin` metres. */
bool in_room(void* levelRoom, Vector3 p, float margin = 0.0f);
/** The level's rooms (LevelController.rooms); empty outside a contract. */
api::RefArray level_rooms();
/** Distance in metres from `p` to the nearest of a LevelRoom's collider boxes: 0 inside one, 1e9 with none. */
float distance_to_room(void* levelRoom, Vector3 p);
/** The first of the level's rooms (LevelController.rooms, in its order) whose colliders, widened by `margin` metres,
 *  hold `p`; null outside a contract and outside every room. */
api::Object room_at(Vector3 p, float margin = 0.0f);

// The placed video cameras (CCTV) a mod checks for what they film. A scan of every CCTV is costly, so `scan` keeps the
// last one for kCameraScanMs; the caller owns it and clears it with its other state when it stops.
inline constexpr uint64_t kCameraScanMs = 2000;
struct CameraScan {
  std::vector<api::Held> cameras;
  uint64_t nextScan = 0;
};
/** Calls `visit` with each live CCTV that is switched on (Equipment.isOn), rescanning when `now` (milliseconds on any
 *  steady clock) has reached the scan's next time; stops at the first visit that returns true. Main thread. */
void each_running_camera(CameraScan& scan, uint64_t now, const std::function<bool(void* camera)>& visit);

// First non-trigger collider hit along the ray, as its Transform, or null.
api::Ref<pm::map::Transform> raycast(Vector3 origin, Vector3 direction, float maxDistance);
/** The same ray on the layers of `layerMask` only (a bit per layer), as a game script's own mask casts it. */
api::Ref<pm::map::Transform> raycast(Vector3 origin, Vector3 direction, float maxDistance, int32_t layerMask);
/** How far along the ray (`direction` of length 1) the first non-trigger collider on the layers of `layerMask` is,
 *  in metres; negative when nothing is hit within `maxDistance`. */
float raycast_distance(Vector3 origin, Vector3 direction, float maxDistance, int32_t layerMask);
bool is_child_of(void* transform, void* parent);
// What the ghost could interact with at hitTransform ("Door", "Light switch", ...), or null.
const char* interactable_label(void* hitTransform);
bool cursor_locked();  // false while a menu (pause, journal) has the cursor

}  // namespace pm::game
