#include "bogeyman.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "ClosetZone.h"
#include "Door.h"
#include "GhostAI.h"
#include "GhostHuntState.h"
#include "LevelController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "behaviour.h"
#include "bogeyman_rule.h"
#include "game.h"
#include "ghosts.h"
#include "held.h"
#include "log.h"
#include "actors.h"
#include "players.h"
#include "runtime.h"

namespace pm::custom::bogeyman {
namespace {

using phasmo::Vector3;

// The class dump and REA, 2026-10-20: ClosetZone.CheckCloset is what the game's hunt calls on a closet.
// The game's own ghost warps (GhostController.WarpGhostToFavouriteRoom, GhostAI's) sample their point
// first (NavMesh.SamplePosition), in area mask 1; its hunt's chase samples a closet's nav spot within 1 m
// (a constant in the game's code, build 25811160).
constexpr float kNavSpotSample = 1.0f;
constexpr int kWalkableArea = 1;
constexpr float kCreakStrength = 0.6f;       // a slow creak, not a slam
constexpr ULONGLONG kScanMs = 5000;

struct Hiding {
  api::Held object;  // ClosetZone or HidingSpot
  bool closet = false;
  Place at{};
};

struct State {
  api::Held level;
  std::vector<Hiding> places;
  std::vector<bool> searched;
  int goal = -1;
  ULONGLONG goalSince = 0;  // when it set out for the goal
  bool open = false;
  ULONGLONG nextScan = 0;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

/** A closet's two doors (ClosetZone.leftClosetDoor, .rightClosetDoor), either of them null. */
std::pair<void*, void*> doors_of(void* closet) {
  return {phasmo::ClosetZone::leftClosetDoor.get(closet), phasmo::ClosetZone::rightClosetDoor.get(closet)};
}

Place place_of(void* object, bool closet) {
  void* spot = closet ? phasmo::ClosetZone::navSpot.get(object) : nullptr;
  const Vector3 p = game::position(api::alive(spot) ? spot : game::transform_of(object));
  return {p.x, p.y, p.z};
}

void scan() {
  g.places.clear();
  void* closetType = phasmo::ClosetZone::type.type_object();
  for (void* type : {closetType, phasmo::HidingSpot::type.type_object()}) {
    const api::RefArray found{type ? phasmo::Object::FindObjectsOfType(type) : nullptr};
    for (size_t i = 0; i < found.size(); ++i) {
      void* o = found[i];
      if (!api::alive(o)) continue;
      Hiding h;
      h.closet = type == closetType;
      h.at = place_of(o, h.closet);
      h.object = o;
      g.places.push_back(std::move(h));
    }
  }
  g.searched.assign(g.places.size(), false);
  g.goal = -1;
}

float floor_area(void* room) {
  float area = 0;
  for (const game::Bounds& b : game::room_bounds(room)) area += 4 * b.extents.x * b.extents.z;
  return area;
}

std::vector<Place> places_only() {
  std::vector<Place> out;
  for (const Hiding& h : g.places) out.push_back(h.at);
  return out;
}

/** The lobby spot (Network.playerSpots) that holds a networked Player, or null. */
void* spot_of(void* player) {
  void* net = player ? phasmo::Network::get_Instance() : nullptr;
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i)
    if (void* spot = spots[i]; spot && phasmo::PlayerSpot::player.get(spot).raw() == player) return spot;
  return nullptr;
}

/** A player's level within their prestige, as the lobby board shows it; kUnknownProgress without a spot. */
int player_level(void* player) {
  void* spot = spot_of(player);
  return spot ? phasmo::PlayerSpot::level.get(spot) : kUnknownProgress;
}

/** A player's prestige, as the lobby board shows it; kUnknownProgress without a spot. */
int player_prestige(void* player) {
  void* spot = spot_of(player);
  return spot ? phasmo::PlayerSpot::prestige.get(spot) : kUnknownProgress;
}

/** The youngest of these players (bogeyman_rule.h youngest) among those its hunt may go for; null for none. */
void* youngest_of(void* ghost, const std::vector<void*>& players) {
  std::vector<Candidate> candidates;
  for (void* p : players)
    candidates.push_back({player_prestige(p), player_level(p), actor_of_player(p), behaviour::may_go_for(ghost, p)});
  const int chosen = youngest(candidates);
  return chosen < 0 ? nullptr : players[static_cast<size_t>(chosen)];
}

/** It has reached this hiding place: a closet is checked as the game's own hunt does, and it is done with it. */
void search(int index) {
  const Hiding& h = g.places[static_cast<size_t>(index)];
  if (h.closet && api::alive(h.object.get())) phasmo::ClosetZone::CheckCloset(h.object.get());
  g.searched[static_cast<size_t>(index)] = true;
}

/** Open one of a closet's doors slowly with the game's own ghost door push; false when it has none. */
bool open_door(void* closet) {
  const auto [left, right] = doors_of(closet);
  for (void* door : {left, right})
    if (api::alive(door)) {
      phasmo::Door::GhostForceDoor(door, true, kCreakStrength, false);
      return true;
    }
  return false;
}

}  // namespace

void* target(void* ghost, std::span<const void* const> excluded) {
  if (!ghost || !api::alive(ghost)) return nullptr;
  std::vector<void*> players;
  std::vector<Candidate> known;
  std::vector<int32_t> excludedActors;
  game::each_player([&](void* p) {
    const int32_t actor = actor_of_player(p);
    if (std::find(excluded.begin(), excluded.end(), p) != excluded.end()) excludedActors.push_back(actor);
    players.push_back(p);
    known.push_back({player_prestige(p), player_level(p), actor, game::living(p)});
  });
  const int chosen = bogeyman_target(known, excludedActors);
  return chosen < 0 ? nullptr : players[static_cast<size_t>(chosen)];
}

bool resolve() { return true; }

void tick(void* ghost, int type, bool steered) {
  g.open = false;
  if (!ghost || type != kBogeyman || !game::alive(ghost)) {
    if (g.level) reset();
    return;
  }
  void* level = game::level_controller();
  const ULONGLONG now = GetTickCount64();
  if (level != g.level || now >= g.nextScan) {
    if (level != g.level || g.places.empty()) scan();
    g.level = level;
    g.nextScan = now + kScanMs;
  }
  void* room = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  g.open = api::alive(room) && open_room(floor_area(room));
  if (game::ghost_state(ghost) != phasmo::GhostState::hunting()) {
    std::fill(g.searched.begin(), g.searched.end(), false);  // every hiding place again next hunt
    g.goal = -1;
    return;
  }
  void* state = game::ghost_state_object(ghost);
  if (!state) return;
  if (!behaviour::hunt_begun(ghost)) {  // until the start delay ends the game's own hunt moves it nowhere: nor does this
    g.goalSince = now;
    return;
  }
  const Vector3 at = game::position(game::transform_of(ghost));
  if (void* prey = phasmo::GhostHuntState::target.get(state)) {
    // In an open room its sight is short: prey beyond it is lost.
    if (api::alive(prey) && open_sight_lost(g.open, game::distance(at, game::position(game::transform_of(prey))))) {
      phasmo::GhostHuntState::target.set(state, nullptr);
    } else {
      g.goalSince = now;  // a chase is not time spent failing to reach the hiding place
      return;
    }
  }
  if (steered) {  // the route is the player's, and it searches what that route reaches
    g.goal = -1;
    g.goalSince = now;
    if (const int reached = reached_hiding_place(places_only(), g.searched, {at.x, at.y, at.z}); reached >= 0)
      search(reached);
    return;
  }
  if (g.goal >= 0 && g.goal < static_cast<int>(g.places.size())) {
    const Place p = g.places[g.goal].at;
    const bool arrived = game::distance(at, {p.x, p.y, p.z}) < kBogeymanArrived;
    // A spot off the NavMesh (in a locker, under a bed) is never reached: it gives up and moves on.
    if (arrived || (now - g.goalSince) / 1000.0f > kBogeymanGiveUp) {
      if (arrived) search(g.goal);
      else g.searched[g.goal] = true;
      g.goal = -1;
    }
  }
  if (g.goal < 0) {
    g.goal = next_hiding_place(places_only(), g.searched, {at.x, at.y, at.z});
    g.goalSince = now;
  }
  if (g.goal >= 0) {
    const Place p = g.places[g.goal].at;
    game::set_destination(game::ghost_agent(ghost), {p.x, p.y, p.z});
  }
}

bool in_open_room() { return g.open; }

void* closet_door(void* ghost) {
  void* activity = ghost ? game::ghost_activity(ghost) : nullptr;
  void* sensor = activity ? phasmo::GhostActivity::interactionSensor.get(activity) : nullptr;
  if (!sensor) return nullptr;
  const int mainDoor = phasmo::DoorKind::Main();  // GhostActivity.GetDoorToOpen never picks the main door
  std::vector<void*> doors;  // the closet doors the game's own pick could have returned
  for (const Hiding& h : g.places) {
    if (!h.closet || !api::alive(h.object.get())) continue;
    const auto [left, right] = doors_of(h.object.get());
    for (void* door : {left, right})
      if (api::alive(door) && phasmo::Door::kind.get(door) != mainDoor &&
          game::sensor_detects(sensor, game::game_object_of(door)))
        doors.push_back(door);
  }
  if (doors.empty()) return nullptr;
  const float share = std::uniform_real_distribution<float>(0, 1)(g_random);
  const size_t index = std::uniform_int_distribution<size_t>(0, doors.size() - 1)(g_random);
  const int chosen = bogeyman_closet_door(doors.size(), share, index);
  return chosen < 0 ? nullptr : doors[static_cast<size_t>(chosen)];
}

void hunt_begins(void* ghost) {
  void* agent = ghost && game::alive(ghost) ? game::ghost_agent(ghost) : nullptr;
  if (!api::alive(agent)) return;
  // A Mimic's copy made at the hunt's own change has had no frame of these rules yet.
  if (void* level = game::level_controller(); level != g.level || g.places.empty()) {
    scan();
    g.level = level;
  }
  std::vector<void*> players;
  game::each_player([&](void* p) { players.push_back(p); });
  void* chosen = youngest_of(ghost, players);
  if (!chosen) return;
  void* theirs = phasmo::Player::closet.get(chosen);  // it does not step out of the closet they hide in
  std::vector<Place> closets;
  std::vector<size_t> placeOf;  // closets[i] is g.places[placeOf[i]]
  for (size_t i = 0; i < g.places.size(); ++i)
    if (g.places[i].closet && g.places[i].object.get() != theirs && api::alive(g.places[i].object.get())) {
      closets.push_back(g.places[i].at);
      placeOf.push_back(i);
    }
  const Vector3 them = game::position(game::transform_of(chosen));
  const int lurk = lurk_place(closets, {them.x, them.y, them.z});
  if (lurk < 0) return;  // no closet near them: it starts where it is
  const size_t index = placeOf[lurk];
  const Place spot = g.places[index].at;  // the closet's nav spot, where a searcher stands
  const phasmo::Vector3 from{spot.x, spot.y, spot.z};
  phasmo::NavMeshHit hit{};
  if (!phasmo::NavMesh::SamplePosition(&from, &hit, kNavSpotSample, kWalkableArea)) {
    PM_LOG("bogeyman: no NavMesh at the closet nearest the youngest player; the hunt begins where it is");
    return;
  }
  // NavMeshAgent.Warp: the game's own ghost teleport (GhostController.WarpGhostToFavouriteRoom, the Twins).
  if (!phasmo::NavMeshAgent::Warp(agent, &hit.position)) {
    PM_LOG("bogeyman: the teleport to a closet failed; the hunt begins where it is");
    return;
  }
  open_door(g.places[index].object.get());
  if (index < g.searched.size()) g.searched[index] = true;  // it has just come out of this one
  PM_LOG("bogeyman: the hunt begins from the closet nearest the youngest player (prestige %d, level %d)",
         player_prestige(chosen), player_level(chosen));
}

void* choose_target(void* ghost, void* proposed) {
  if (!proposed) return nullptr;
  const float distance = game::distance(game::position(game::transform_of(ghost)), game::position(game::transform_of(proposed)));
  return open_sight_lost(g.open, distance) ? nullptr : proposed;  // beyond its short sight in an open room: lost
}

void reset() { g = State{}; }

}  // namespace pm::custom::bogeyman
