#include "world.h"

#include <algorithm>
#include <cmath>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "Photon.h"
#include "Player.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "game.h"
#include "game_api.h"
#include "held.h"
#include "runtime.h"

namespace pm::tells::world {
namespace {

using phasmo::GhostAI;
constexpr float kChest = 1.2f, kEye = 1.6f;  // metres above the feet
constexpr float kFloor = 2.5f;               // metres up or down still counted as the same floor
constexpr float kLongestFrame = 0.25f;       // seconds: a longer frame (a hitch) counts as this much
// Unity's Physics.DefaultRaycastLayers (every layer but Ignore Raycast), a documented engine constant
// the compiler folds into callers, so no game data holds it.
constexpr int32_t kDefaultRaycastLayers = -5;

api::Held g_hunt;  // the hunt state whose speed update ran last on this client (the master)

// GhostHuntState.UpdateSpeed runs every frame of a hunt on the master, with the hunt state: the one
// place the hunt's own object is handed to code, since GhostAI keeps its states in a machine whose
// type the map cannot single out.
void detour_update_speed(api::Original<phasmo::GhostHuntState::UpdateSpeed> original, void* hunt) {
  if (hunt != g_hunt.get()) g_hunt = hunt;
  original(hunt);
}

void* level() {
  void* level = phasmo::LevelController::instance.get();
  return api::alive(level) ? level : nullptr;
}

void* alive_or_null(void* object) { return api::alive(object) ? object : nullptr; }

}  // namespace

bool create_hooks() {
  return api::hook<phasmo::GhostHuntState::UpdateSpeed>(&detour_update_speed, "GhostHuntState.UpdateSpeed",
                                                        api::Twins::CanonicalOnly);
}

void reset() { g_hunt = nullptr; }

void* ghost() {
  void* l = level();
  return l ? alive_or_null(phasmo::LevelController::currentGhost.get(l)) : nullptr;
}

bool is_master() { return phasmo::PhotonNetwork::get_IsMasterClient(); }

int master_actor() {
  void* master = phasmo::PhotonNetwork::get_MasterClient();
  return master ? phasmo::PhotonPlayer::get_ActorNumber(master) : 0;
}

int local_actor() {
  void* local = phasmo::PhotonNetwork::get_LocalPlayer();
  return local ? phasmo::PhotonPlayer::get_ActorNumber(local) : 0;
}

void each_player(const std::function<void(void*)>& visit) {
  void* network = phasmo::Network::get_Instance();
  const api::List spots{network ? phasmo::Network::playerSpots.get(network) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    void* player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (api::alive(player)) visit(player);
  }
}

bool living(void* player) { return api::alive(player) && !phasmo::Player::isDead.get(player); }

void* player_of_actor(int actor) {
  void* network = phasmo::Network::get_Instance();
  const api::List spots{network ? phasmo::Network::playerSpots.get(network) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    void* photon = spot ? phasmo::PlayerSpot::photonPlayer.get(spot) : nullptr;
    if (photon && phasmo::PhotonPlayer::get_ActorNumber(photon) == actor)
      return alive_or_null(phasmo::PlayerSpot::player.get(spot));
  }
  return nullptr;
}

void* local_player() {
  const int actor = local_actor();
  return actor ? player_of_actor(actor) : nullptr;
}

int state(void* ai) { return api::alive(ai) ? GhostAI::state.get(ai) : -1; }
bool hunting(void* ai) { return api::alive(ai) && GhostAI::isHunting.get(ai); }
bool appeared(void* ai) { return api::alive(ai) && GhostAI::isAppeared.get(ai); }

void* favourite_room(void* ai) {
  void* info = api::alive(ai) ? GhostAI::info.get(ai) : nullptr;
  return info ? alive_or_null(phasmo::GhostInfo::favouriteRoom.get(info)) : nullptr;
}

void* hunt_state(void* ai) {
  void* hunt = g_hunt.get();
  if (!hunt || !hunting(ai) || state(ai) != phasmo::GhostState::hunting()) return nullptr;
  return phasmo::GhostHuntState::ghost.get(hunt) == ai ? hunt : nullptr;
}

void* hunt_target(void* ai) {
  void* hunt = hunt_state(ai);
  return hunt ? alive_or_null(phasmo::GhostHuntState::target.get(hunt)) : nullptr;
}

void* hunt_search(void* ai) {
  void* hunt = hunt_state(ai);
  return hunt ? alive_or_null(phasmo::GhostHuntState::searchTarget.get(hunt)) : nullptr;
}

void* ghost_room() {
  void* l = level();
  return l ? alive_or_null(phasmo::LevelController::currentGhostRoom.get(l)) : nullptr;
}

// The level's rooms and a room's colliders come from core (core/game.h), as Custom Ghosts reads them.
void* room_at(Vector3 p) { return game::room_at(p); }

int room_index(void* room) {
  const api::RefArray rooms = game::level_rooms();
  for (size_t i = 0; room && i < rooms.size(); ++i)
    if (rooms[i] == room) return static_cast<int>(i);
  return -1;
}

void* room_by_index(int index) {
  const api::RefArray rooms = game::level_rooms();
  if (index < 0 || static_cast<size_t>(index) >= rooms.size()) return nullptr;
  return alive_or_null(rooms[static_cast<size_t>(index)]);
}

std::vector<Bounds> room_bounds(void* room) { return game::room_bounds(room); }
bool in_room(void* room, Vector3 p) { return game::in_room(room, p); }
float distance_to_room(void* room, Vector3 p) { return game::distance_to_room(room, p); }

bool clear_line(Vector3 from, Vector3 to) {
  const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
  const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (d < 0.5f) return true;
  const Vector3 dir{dx / d, dy / d, dz / d};
  // Triggers (room and sensor volumes) never stop the ray; it stops short of what it looks at.
  return !phasmo::Physics::Raycast(&from, &dir, d - 0.4f, kDefaultRaycastLayers,
                                   phasmo::QueryTriggerInteraction::Ignore());
}

float distance(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }

Vector3 feet(void* component) {
  void* t = api::alive(component) ? phasmo::Component::get_transform(component) : nullptr;
  return t ? phasmo::Transform::get_position(t) : Vector3{};
}

Vector3 forward(void* component) {
  void* t = api::alive(component) ? phasmo::Component::get_transform(component) : nullptr;
  return t ? phasmo::Transform::get_forward(t) : Vector3{};
}

Vector3 chest(void* ai) {
  const Vector3 f = feet(ai);
  return {f.x, f.y + kChest, f.z};
}

Vector3 eye(void* player) {
  const Vector3 f = feet(player);
  return {f.x, f.y + kEye, f.z};
}

bool same_floor(Vector3 a, Vector3 b) { return std::fabs(a.y - b.y) < kFloor; }

int living_within(Vector3 p, float range) {
  int n = 0;
  each_player([&](void* player) {
    if (!living(player)) return;
    const Vector3 at = feet(player);
    if (same_floor(at, p) && std::hypot(at.x - p.x, at.z - p.z) <= range) ++n;
  });
  return n;
}

// Time.get_deltaTime alone, as pm::api::frame_time reads it: the one Time entry the tells require.
float frame_seconds() { return std::clamp(phasmo::Time::get_deltaTime(), 0.0f, kLongestFrame); }

}  // namespace pm::tells::world
