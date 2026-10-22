#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include "il2cpp.h"

// Resolved game and engine entry points. Managed methods are called through their
// IL2CPP method pointers with the trailing hidden MethodInfo* argument (null).
// All calls must be made on Unity's main thread.
namespace pg::game {

using il2cpp::Quaternion;
using il2cpp::Vector3;

struct Ray {
  Vector3 origin, direction;
};
struct RaycastHit {  // UnityEngine.RaycastHit, 0x2C bytes
  unsigned char bytes[0x2C];
};

constexpr float kGhostEyeHeight = 1.6f;  // metres above the ghost's transform the ghost player sees from

bool resolve();  // false if anything required is missing

// Photon (PUN / Realtime)
bool in_room();
bool is_master();
void* photon_local_player();          // Photon.Realtime.Player
il2cpp::Array* photon_player_list();  // Photon.Realtime.Player[]
int actor_number(void* photonPlayer);
// Single player, or a room that is not listed publicly. The mod only acts in these.
bool private_room();
int local_actor();   // 0 when not in a room
int master_actor();  // 0 when not in a room
void room_actors(std::vector<int>& out);

// Unity
bool alive(void* unityObject);  // UnityEngine.Object implicit bool (not destroyed)
void* transform_of(void* component);
void* object_transform(void* gameObject);  // GameObject.transform
Vector3 position(void* transform);
inline float distance(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }
void set_position(void* transform, Vector3 p);
Vector3 forward(void* transform);
bool set_destination(void* agent, Vector3 target);
float agent_speed(void* agent);
float agent_path_length(void* agent);  // NavMeshAgent.remainingDistance, negative while unknown
Quaternion rotation(void* transform);
void set_rotation(void* transform, Quaternion r);
void* new_game_object(const char* name);
void* add_component(void* gameObject, il2cpp::Object* type);  // type: System.Type
void* get_component(void* component, il2cpp::Object* type);   // on the component's GameObject
void* game_object_of(void* component);
void set_enabled(void* behaviour, bool on);
bool enabled(void* behaviour);
void destroy(void* object);  // UnityEngine.Object.Destroy
void set_active(void* gameObject, bool on);
void* parent(void* transform);
void set_parent(void* transform, void* parent, bool worldPositionStays);
Vector3 local_position(void* transform);
void set_local_position(void* transform, Vector3 p);
Quaternion local_rotation(void* transform);
void set_local_rotation(void* transform, Quaternion r);
void set_local_scale(void* transform, Vector3 s);
Vector3 local_scale(void* transform);
Vector3 lossy_scale(void* transform);  // Transform.lossyScale: its scale in the world
// UnityEngine.Object.Instantiate(original, parent, worldPositionStays): a copy of a GameObject or
// component (and its children) under parent; returns the copy of what was passed.
void* instantiate(void* original, void* parent, bool worldPositionStays);
void* find_child(void* transform, const char* path);  // Transform.Find, "a/b" paths, or null
il2cpp::Array* components(void* gameObject, il2cpp::Object* type);  // GameObject.GetComponents(Type)
void destroy_immediate(void* object);  // Object.DestroyImmediate: gone now, before anything can run
float field_of_view(void* camera);     // vertical, degrees
float aspect(void* camera);
int child_count(void* transform);
void* child(void* transform, int index);
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

// Game singletons and offsets (build 25811160)
void* level_controller();          // LevelController static instance, null outside a contract
void* ghost_ai();                  // LevelController+0x40
int ghost_state(void* ghostAi);    // GhostAI+0x30, GhostAI.State enum (2 = hunting)
void* ghost_agent(void* ghostAi);  // GhostAI+0x40, NavMeshAgent
void* network();                   // Network.Instance
void* local_player();              // Network.Instance+0x28, this client's Player
void* player_of_actor(int actor);  // the networked Player of a Photon actor (Network's spots), or null
void* local_camera();              // Network.Instance+0x30 (LocalPlayer) +0x30 (Camera)
void freeze_player_movement(void* player, bool frozen);
void* ghost_activity(void* ghostAi);  // GhostAI+0x58, GhostActivity
void* ghost_state_object(void* ghostAi);  // current IState (GhostAI+0x28 machine, +0x10)
/**
 * Whether the ghost's own vision sensor (GhostAI+0xA0, "Ghost player vision sensor") detects a living
 * player: the call vanilla Footstep.OnEnable makes for the local player. False for null or dead.
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
void* level_values();                                 // LevelValues.Instance
void* setup_phase();                                  // SetupPhaseController's instance, null outside a contract
void* local_player_component();       // Network.Instance+0x30, LocalPlayer (LocalPCPlayer on PC)

// Hunter input: the InputActions of LocalPCPlayer.PCControls (+0x118) asset (+0x20).
void for_each_input_action(void* localPlayer, const std::function<void(void* action, const char* map, const char* name)>& fn);
bool action_enabled(void* action);
void set_action_enabled(void* action, bool on);

// Hook targets
void* photon_handler_late_update();  // Photon.Pun.PhotonHandler.LateUpdate
void* change_state();                // GhostAI.ChangeState(State, PhotonObjectInteract, bool)
void* normal_interaction();          // GhostActivity.NormalInteraction(bool)
void* idle_execute();                // idle state Execute (obfuscated, by address)
void* twin_interaction();            // GhostActivity.TwinInteraction(bool) -> IEnumerator
void* try_interact();                // GhostActivity.TryInteract(): the idle roll's interaction (inlines the normal one)
void* random_range_int();            // UnityEngine.Random.RandomRangeInt(int, int)
void* detected_components();         // Sensor.GetDetectedComponents<T>(List<T>) shared code
bool in_function(const void* address, uintptr_t rva, uintptr_t length);  // GameAssembly range test
void* game_assembly_address(uintptr_t rva);  // build 25811160 only (verified at load)

// A LevelRoom's colliders' world bounds (empty when it has none).
struct Bounds {
  Vector3 center, extents;
};
std::vector<Bounds> room_bounds(void* levelRoom);
/** Whether a world point lies inside a LevelRoom's colliders, widened by `margin` metres. */
bool in_room(void* levelRoom, Vector3 p, float margin = 0.0f);

// First non-trigger collider hit along the ray, as its Transform, or null.
void* raycast(Vector3 origin, Vector3 direction, float maxDistance);
bool is_child_of(void* transform, void* parent);
// What the ghost could interact with at hitTransform ("Door", "Light switch", ...), or null.
const char* interactable_label(void* hitTransform);
bool cursor_locked();  // false while a menu (pause, journal) has the cursor

constexpr int kStateHunting = 2;

}  // namespace pg::game
