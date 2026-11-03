#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "Transform.h"

// The host's own helpers over the game API (sdk/api, sdk/map). Every member they reach is a map entry, bound to the host's
// own contract in suite::start, so before that and for an entry that did not resolve each gives the answer below. core/game.h
// is the mods'. Main thread.
//
// The gates fail closed: an answer the host cannot prove stops whatever waits on it (a send, a kick, a leave, a restart).
namespace pm::hostgame {

// The keys of those of the wrappers W... that did not resolve, in their order, separated by ", "; empty when every one
// did. As api::all_resolved, it makes each entry required and is empty only after the bind. For a log line that names
// what a feature lacks.
template <const auto&... W>
std::string unresolved() {
  std::string out;
  const auto add = [&out](bool resolved, const char* key) {
    if (resolved) return;
    if (!out.empty()) out += ", ";
    out += key;
  };
  (add(std::remove_cvref_t<decltype(W)>::use::resolved(), std::remove_cvref_t<decltype(W)>::desc.key), ...);
  return out;
}

// core/room_gates.h's answers, the mods' too.
bool private_room();  // false: nothing is sent
int actor_number(void* photonPlayer);
int local_actor();   // 0: the join guard and the presence stand aside
int master_actor();  // 0: no sender is the master, and no client is
bool room_actors(std::vector<int>& out);  // false, or empty, or all 0: no gate trusts it

// Whether this client is the room's master by actor number. False when either number is 0 (unknown), so a client that
// cannot read them never acts as the host and never sends a host's RPC.
bool local_is_master();
// Whether this client is known to be in no room. False when PhotonNetwork.get_InRoom did not resolve, so a gate that may
// act only outside a room (the Mods panel's restart) stays shut.
bool out_of_room();
// Whether this client is known to be in no contract. False when LevelController.instance did not resolve or its read was
// refused (a static constructor that threw), so a gate that may act only in the lobby (the join guard's kicks and leaves)
// stays shut.
bool out_of_contract();

bool alive(void* unityObject);               // false for null and destroyed
void* transform_of(void* component);         // null when unresolved
void* object_transform(void* gameObject);    // null for null or unresolved
void* game_object_of(void* component);       // null when unresolved
int child_count(void* transform);            // 0 for null or unresolved
void* child(void* transform, int index);     // null when unresolved
void* parent(void* transform);               // null when unresolved
void set_local_scale(void* transform, phasmo::Vector3 scale);
// GameObject.GetComponents(Type), a Component[]; null for null or unresolved.
void* components(void* gameObject, void* type);
// Component.GetComponent(Type) on the component's GameObject; null for a dead component, a null type or unresolved.
void* get_component(void* component, void* type);
void set_enabled(void* behaviour, bool on);  // nothing when unresolved
bool enabled(void* behaviour);               // false for unresolved
void set_active(void* gameObject, bool on);  // nothing for a dead object or unresolved
bool active_self(void* gameObject);          // false for null or unresolved
bool active_in_hierarchy(void* gameObject);  // false for null or unresolved
void destroy(void* object);                  // Object.Destroy; nothing for a dead object or unresolved
// Object.Instantiate(original, parent, worldPositionStays): a copy under parent, or null with nothing copied for a dead
// original or parent. The map's entry is the engine's clone with no null checks of its own, so they are made here.
void* instantiate(void* original, void* parent, bool worldPositionStays);

bool cursor_locked();  // false when it cannot be read: nothing frees a cursor it cannot see locked
void free_cursor();    // CursorLockMode.None and visible; nothing when unresolved
// The host reads no mouse: its menu controls are the game's own buttons, clicked by the game's EventSystem
// (host/menu_panel.h).

}  // namespace pm::hostgame
