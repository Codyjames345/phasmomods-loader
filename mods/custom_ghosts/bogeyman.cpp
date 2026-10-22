#include "bogeyman.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "behaviour.h"
#include "bogeyman_rule.h"
#include "game.h"
#include "ghosts.h"
#include "il2cpp.h"
#include "log.h"
#include "players.h"

namespace pg::custom::bogeyman {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (class dump and REA,
// 2026-10-20: ClosetZone.CheckCloset is what the game's hunt calls on a closet).
constexpr size_t kClosetLeftDoor = 0x28;     // ClosetZone: Door
constexpr size_t kClosetRightDoor = 0x30;    // ClosetZone: Door
constexpr size_t kClosetNavSpot = 0x48;      // ClosetZone: Transform where a searcher stands
constexpr size_t kLevelGhostRoom = 0x38;     // LevelController: the room the ghost is in
constexpr size_t kHuntTarget = 0x48;         // hunt state: the player it has detected, or null
constexpr size_t kPlayerCloset = 0x30;       // Player: the ClosetZone they hide in, or null
constexpr size_t kDoorKind = 0x108;          // Door: None 0, Main 1, Secondary 2, PermanentLock 3
constexpr int kMainDoor = 1;                 // GhostActivity.GetDoorToOpen never picks the main door
constexpr uintptr_t kCheckClosetRva = 0x262DE80;  // ClosetZone.CheckCloset()
constexpr uintptr_t kForceDoorRva = 0x264C3C0;    // Door.GhostForceDoor(bool open, float strength, bool skipRattle)
// NavMesh.SamplePosition(Vector3 source, out NavMeshHit hit, float maxDistance, int areaMask): the game's own
// ghost warps (GhostController.WarpGhostToFavouriteRoom, GhostAI's) sample their point first, in area mask 1.
// Its hunt's chase samples a closet's nav spot within 1 m (the constant at 0x5582B24).
constexpr uintptr_t kSamplePositionRva = 0x518F100;
constexpr float kNavSpotSample = 1.0f;
constexpr int kWalkableArea = 1;
// Each player's progress as the lobby board shows it, on every client: ServerManager.UpdatePlayerLevelInformation
// (0xB64E70), the buffered RPC each player sends with their saved experience, level and prestige, writes them
// into their Network.PlayerSpot (+0x28, +0x2C, +0x30); ProfileCard.UpdateUI (0x13BE4B0) shows the level,
// after the prestige when it is not 0.
constexpr size_t kSpotLevel = 0x2C;          // Network.PlayerSpot: level within the prestige (1 for a new player)
constexpr size_t kSpotPrestige = 0x30;       // Network.PlayerSpot: prestige (0 before the first; the RPC clamps it to 0..3999)
constexpr float kCreakStrength = 0.6f;       // a slow creak, not a slam
constexpr ULONGLONG kScanMs = 5000;

struct NavMeshHit {  // UnityEngine.AI.NavMeshHit (class dump), 0x24 bytes
  Vector3 position;  // +0x0
  Vector3 normal;    // +0xC
  float distance;    // +0x18
  int mask;          // +0x1C
  int hit;           // +0x20
};
static_assert(sizeof(NavMeshHit) == 0x24);

struct Api {
  il2cpp::Object* closetType = nullptr;
  il2cpp::Object* spotType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  void (*checkCloset)(void*, const void*) = nullptr;
  void (*forceDoor)(void*, bool, float, bool, const void*) = nullptr;
  // NavMeshAgent.Warp (0x518C510): the game's own ghost teleport (GhostController.WarpGhostToFavouriteRoom, the Twins).
  bool (*warp)(void*, Vector3, const void*) = nullptr;
  bool (*samplePosition)(Vector3, NavMeshHit*, float, int, const void*) = nullptr;  // kSamplePositionRva
  bool ok = false;
} api;

struct Hiding {
  il2cpp::Held object;  // ClosetZone or HidingSpot
  bool closet = false;
  Place at{};
};

struct State {
  il2cpp::Held level;
  std::vector<Hiding> places;
  std::vector<bool> searched;
  int goal = -1;
  ULONGLONG goalSince = 0;  // when it set out for the goal
  bool open = false;
  ULONGLONG nextScan = 0;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

Place place_of(void* object, bool closet) {
  void* spot = closet ? il2cpp::field<void*>(object, kClosetNavSpot) : nullptr;
  const Vector3 p = game::position(spot && game::alive(spot) ? spot : game::transform_of(object));
  return {p.x, p.y, p.z};
}

void scan() {
  g.places.clear();
  for (il2cpp::Object* type : {api.closetType, api.spotType}) {
    il2cpp::Array* found = type ? api.findObjects(type, nullptr) : nullptr;
    for (uintptr_t i = 0; found && i < found->max_length; ++i) {
      void* o = found->data<void*>()[i];
      if (!o || !game::alive(o)) continue;
      Hiding h;
      h.closet = type == api.closetType;
      h.at = place_of(o, h.closet);
      h.object = il2cpp::Held(o);
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

/** The lobby spot (Network +0x38) that holds a networked Player, or null. */
void* spot_of(void* player) {
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, game::kNetworkPlayerSpots) : nullptr;
  for (int i = 0; player && spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    if (spot && il2cpp::field<void*>(spot, game::kPlayerSpotPlayer) == player) return spot;
  }
  return nullptr;
}

/** A player's level within their prestige, as the lobby board shows it (kSpotLevel); kUnknownProgress without a spot. */
int player_level(void* player) {
  void* spot = spot_of(player);
  return spot ? il2cpp::field<int>(spot, kSpotLevel) : kUnknownProgress;
}

/** A player's prestige, as the lobby board shows it (kSpotPrestige); kUnknownProgress without a spot. */
int player_prestige(void* player) {
  void* spot = spot_of(player);
  return spot ? il2cpp::field<int>(spot, kSpotPrestige) : kUnknownProgress;
}

/** The youngest of these players (bogeyman_rule.h youngest) among those its hunt may go for; null for none. */
void* youngest_of(void* ghost, const std::vector<void*>& players) {
  const Vector3 at = game::position(game::transform_of(ghost));
  std::vector<Candidate> candidates;
  for (void* p : players)
    candidates.push_back({player_prestige(p), player_level(p), game::distance(at, game::position(game::transform_of(p))),
                          behaviour::may_go_for(ghost, p)});
  const int chosen = youngest(candidates);
  return chosen < 0 ? nullptr : players[chosen];
}

/** Open one of a closet's doors slowly with the game's own ghost door push; false when it has none. */
bool open_door(void* closet) {
  for (size_t side : {kClosetLeftDoor, kClosetRightDoor})
    if (void* door = il2cpp::field<void*>(closet, side); door && game::alive(door)) {
      api.forceDoor(door, true, kCreakStrength, false, nullptr);
      return true;
    }
  return false;
}

}  // namespace

bool resolve() {
  il2cpp::Class* closet = il2cpp::cls("Assembly-CSharp.dll", "", "ClosetZone");
  il2cpp::Class* spot = il2cpp::cls("Assembly-CSharp.dll", "", "HidingSpot");
  api.closetType = closet ? il2cpp::type_object(closet) : nullptr;
  api.spotType = spot ? il2cpp::type_object(spot) : nullptr;
  bool ok = api.closetType != nullptr;
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1);
  ok &= il2cpp::bind(api.warp, il2cpp::cls("UnityEngine.AIModule.dll", "UnityEngine.AI", "NavMeshAgent"), "Warp", 1);
  api.checkCloset = reinterpret_cast<decltype(api.checkCloset)>(game::game_assembly_address(kCheckClosetRva));
  api.forceDoor = reinterpret_cast<decltype(api.forceDoor)>(game::game_assembly_address(kForceDoorRva));
  api.samplePosition = reinterpret_cast<decltype(api.samplePosition)>(game::game_assembly_address(kSamplePositionRva));
  api.ok = ok && api.checkCloset && api.forceDoor && api.samplePosition;
  PG_LOG("bogeyman: resolve %s", api.ok ? "ok" : "unavailable (it hunts as any ghost)");
  return true;  // optional: the Bogeyman still hunts
}

void tick(void* ghost, int type, bool steered) {
  g.open = false;
  if (!api.ok || !ghost || type != kBogeyman || !game::alive(ghost)) {
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
  void* room = level ? il2cpp::field<void*>(level, kLevelGhostRoom) : nullptr;
  g.open = room && game::alive(room) && open_room(floor_area(room));
  if (game::ghost_state(ghost) != game::kStateHunting) {
    std::fill(g.searched.begin(), g.searched.end(), false);  // every hiding place again next hunt
    g.goal = -1;
    return;
  }
  void* state = game::ghost_state_object(ghost);
  if (!state) return;
  const Vector3 at = game::position(game::transform_of(ghost));
  if (void* prey = il2cpp::field<void*>(state, kHuntTarget)) {
    // In an open room its sight is short: prey beyond it is lost.
    if (game::alive(prey) && open_sight_lost(g.open, game::distance(at, game::position(game::transform_of(prey))))) {
      il2cpp::set_ref_field(state, kHuntTarget, nullptr);
    } else {
      g.goalSince = now;  // a chase is not time spent failing to reach the hiding place
      return;
    }
  }
  if (steered) {
    g.goalSince = now;
    return;
  }
  if (g.goal >= 0 && g.goal < static_cast<int>(g.places.size())) {
    const Place p = g.places[g.goal].at;
    const bool arrived = game::distance(at, {p.x, p.y, p.z}) < kBogeymanArrived;
    // A spot off the NavMesh (in a locker, under a bed) is never reached: it gives up and moves on.
    if (arrived || (now - g.goalSince) / 1000.0f > kBogeymanGiveUp) {
      Hiding& h = g.places[g.goal];
      if (arrived && h.closet && game::alive(h.object.get())) api.checkCloset(h.object.get(), nullptr);
      g.searched[g.goal] = true;
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
  if (!api.ok || !ghost) return nullptr;
  const Vector3 at = game::position(game::transform_of(ghost));
  std::vector<void*> doors;
  for (const Hiding& h : g.places) {
    if (!h.closet || !game::alive(h.object.get()) || game::distance(at, {h.at.x, h.at.y, h.at.z}) > kBogeymanCreakReach) continue;
    for (size_t side : {kClosetLeftDoor, kClosetRightDoor})
      if (void* door = il2cpp::field<void*>(h.object.get(), side);
          door && game::alive(door) && il2cpp::field<int>(door, kDoorKind) != kMainDoor)
        doors.push_back(door);
  }
  if (doors.empty() || std::uniform_real_distribution<float>(0, 1)(g_random) >= kBogeymanClosetDoorShare) return nullptr;
  return doors[std::uniform_int_distribution<size_t>(0, doors.size() - 1)(g_random)];
}

void hunt_begins(void* ghost) {
  void* agent = api.ok && ghost && game::alive(ghost) ? game::ghost_agent(ghost) : nullptr;
  if (!agent || !game::alive(agent)) return;
  std::vector<void*> players;
  game::each_player([&](void* p) { players.push_back(p); });
  void* chosen = youngest_of(ghost, players);
  if (!chosen) return;
  void* theirs = il2cpp::field<void*>(chosen, kPlayerCloset);  // it does not step out of the closet they hide in
  std::vector<Place> closets;
  std::vector<size_t> placeOf;  // closets[i] is g.places[placeOf[i]]
  for (size_t i = 0; i < g.places.size(); ++i)
    if (g.places[i].closet && g.places[i].object.get() != theirs && game::alive(g.places[i].object.get())) {
      closets.push_back(g.places[i].at);
      placeOf.push_back(i);
    }
  const Vector3 them = game::position(game::transform_of(chosen));
  const int lurk = lurk_place(closets, {them.x, them.y, them.z});
  if (lurk < 0) return;  // no closet near them: it starts where it is
  const size_t index = placeOf[lurk];
  const Place spot = g.places[index].at;  // the closet's nav spot, where a searcher stands
  NavMeshHit hit{};
  if (!api.samplePosition({spot.x, spot.y, spot.z}, &hit, kNavSpotSample, kWalkableArea, nullptr)) {
    PG_LOG("bogeyman: no NavMesh at the closet nearest the youngest player; the hunt begins where it is");
    return;
  }
  if (!api.warp(agent, hit.position, nullptr)) {
    PG_LOG("bogeyman: the teleport to a closet failed; the hunt begins where it is");
    return;
  }
  open_door(g.places[index].object.get());
  if (index < g.searched.size()) g.searched[index] = true;  // it has just come out of this one
  PG_LOG("bogeyman: the hunt begins from the closet nearest the youngest player (prestige %d, level %d)",
         player_prestige(chosen), player_level(chosen));
}

void* choose_target(void* ghost, void* proposed) {
  const Vector3 at = game::position(game::transform_of(ghost));
  std::vector<void*> detected;
  game::each_player([&](void* p) {
    if (behaviour::hunt_sees(ghost, p) && !open_sight_lost(g.open, game::distance(at, game::position(game::transform_of(p)))))
      detected.push_back(p);
  });
  if (detected.size() < 2) return proposed;
  return youngest_of(ghost, detected) == proposed ? proposed : nullptr;
}

void reset() { g = State{}; }

}  // namespace pg::custom::bogeyman
