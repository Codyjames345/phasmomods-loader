#include "lithobolia.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "flame_net.h"
#include "game.h"
#include "ghosts.h"
#include "il2cpp.h"
#include "lithobolia_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"

namespace pg::custom::lithobolia {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. REA decompiles of
// GhostActivity.InteractWithAProp/GetPropToThrow, the prop throw and PlayerSanity.ChangeSanity
// (2026-10-20); the ownership calls as kikimora.cpp.
constexpr size_t kPlayerSanity = 0x90;         // Player: PlayerSanity
constexpr size_t kInteractView = 0x20;         // PhotonObjectInteract: PhotonView
constexpr size_t kInteractRigidbody = 0x38;    // PhotonObjectInteract: Rigidbody
constexpr size_t kInteractDoor = 0x58;         // PhotonObjectInteract: Door (never thrown)
constexpr size_t kPropInteract = 0x20;         // Prop: its PhotonObjectInteract
constexpr size_t kActivityInteraction = 0x28;  // GhostActivity: GhostInteraction
constexpr uintptr_t kRequestOwnershipRva = 0x4363AD0;  // PhotonView.RequestOwnership()
constexpr uintptr_t kRbVelocityRva = 0x5287710;        // Rigidbody.set_velocity
constexpr uintptr_t kThrowEmfRva = 0x207EEF0;          // GhostInteraction.CreateThrowingEMF
constexpr uintptr_t kPropToThrowRva = 0x20061F0;       // GhostActivity.GetPropToThrow() -> Prop
constexpr uintptr_t kChangeSanityRva = 0xDA5260;       // PlayerSanity.ChangeSanity(float): +x is sanity lost
constexpr size_t kHuntTarget = 0x48;           // hunt state: the player it has detected, or null
constexpr ULONGLONG kOwnershipWaitMs = 1500;

struct Api {
  void (*requestOwnership)(void*, const void*) = nullptr;
  bool (*isMine)(void*, const void*) = nullptr;
  void (*setVelocity)(void*, Vector3, const void*) = nullptr;
  Vector3 (*velocity)(void*, const void*) = nullptr;
  void (*throwEmf)(void*, void*, int, bool, float, const void*) = nullptr;
  void* (*propToThrow)(void*, const void*) = nullptr;
  void (*changeSanity)(void*, float, const void*) = nullptr;
  bool ok = false;
} api;

struct Throw {
  il2cpp::Held item;       // PhotonObjectInteract
  il2cpp::Held target;     // Player
  int actor = 0;           // the target's Photon actor
  ULONGLONG deadline = 0;  // waiting for ownership until then
  ULONGLONG landed = 0;    // launched: watched for a hit until then
} g;

Vec vec(Vector3 v) { return {v.x, v.y, v.z}; }
Vector3 vec3(Vec v) { return {v.x, v.y, v.z}; }

Vec chest_of(void* player) {
  const Vector3 p = game::position(game::transform_of(player));
  return {p.x, p.y + kLithoboliaChest, p.z};
}

/** The Photon actor of a networked Player, or 0. */
int actor_of(void* player) {
  std::vector<int> actors;
  game::room_actors(actors);
  for (int a : actors)
    if (game::player_of_actor(a) == player) return a;
  return 0;
}

/** Metres from the ghost's feet to a player's chest. */
float chest_distance(void* ghost, void* player) {
  const Vector3 at = game::position(game::transform_of(ghost));
  const Vec chest = chest_of(player);
  return length({chest.x - at.x, chest.y - at.y, chest.z - at.z});
}

/** The nearest living player the ghost's own vision sensor detects within range, and their actor; null for none. */
void* visible_target(void* ghost, int& actor) {
  std::vector<void*> seen;
  std::vector<float> distances;
  game::each_player([&](void* player) {
    if (!game::ghost_sees(ghost, player)) return;
    seen.push_back(player);
    distances.push_back(chest_distance(ghost, player));
  });
  const int best = nearest_in_range(distances, kLithoboliaRange);
  void* player = best >= 0 ? seen[best] : nullptr;
  actor = player ? actor_of(player) : 0;
  return player;
}

bool throwable(void* item) {
  if (!item || !game::alive(item) || il2cpp::field<void*>(item, kInteractDoor)) return false;
  void* rb = il2cpp::field<void*>(item, kInteractRigidbody);
  return rb && game::alive(rb) && il2cpp::field<void*>(item, kInteractView);
}

void clear() { g = Throw{}; }

/** Aim and launch the taken item, with the EMF of a ghost's throw where it left. */
void launch(void* ghost) {
  void* rb = il2cpp::field<void*>(g.item, kInteractRigidbody);
  const Vec from = vec(game::position(game::transform_of(g.item)));
  api.setVelocity(rb, vec3(aim_velocity(from, chest_of(g.target))), nullptr);
  void* activity = game::ghost_activity(ghost);
  if (void* interaction = activity ? il2cpp::field<void*>(activity, kActivityInteraction) : nullptr)
    api.throwEmf(interaction, g.item, 1, false, 20.0f, nullptr);
  g.landed = GetTickCount64() + static_cast<ULONGLONG>(kLithoboliaFlight * 1000);
  PG_LOG("lithobolia: thrown at a player");
}

/** Apply a hit to this client's own player. */
void hit_me(float sanity) {
  void* me = game::local_player();
  void* mine = me && game::alive(me) ? il2cpp::field<void*>(me, kPlayerSanity) : nullptr;
  if (mine && game::alive(mine)) api.changeSanity(mine, sanity, nullptr);
}

}  // namespace

bool resolve() {
  api.requestOwnership = reinterpret_cast<decltype(api.requestOwnership)>(game::game_assembly_address(kRequestOwnershipRva));
  api.setVelocity = reinterpret_cast<decltype(api.setVelocity)>(game::game_assembly_address(kRbVelocityRva));
  api.throwEmf = reinterpret_cast<decltype(api.throwEmf)>(game::game_assembly_address(kThrowEmfRva));
  api.propToThrow = reinterpret_cast<decltype(api.propToThrow)>(game::game_assembly_address(kPropToThrowRva));
  api.changeSanity = reinterpret_cast<decltype(api.changeSanity)>(game::game_assembly_address(kChangeSanityRva));
  bool ok = il2cpp::bind(api.isMine, il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonView"), "get_IsMine", 0);
  ok &= il2cpp::bind(api.velocity, il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Rigidbody"), "get_velocity", 0);
  api.ok = ok && api.requestOwnership && api.setVelocity && api.throwEmf && api.propToThrow && api.changeSanity;
  PG_LOG("lithobolia: resolve %s", api.ok ? "ok" : "unavailable (it throws as any ghost)");
  return true;  // optional: the Lithobolia still throws, at random
}

/** Take `prop` (a PhotonObjectInteract) over to throw it at `target` once the host owns it. */
void begin(void* prop, void* target, int actor) {
  g.item = prop;
  g.target = target;
  g.actor = actor;
  void* view = il2cpp::field<void*>(prop, kInteractView);
  if (!api.isMine(view, nullptr)) api.requestOwnership(view, nullptr);  // moved once it is the host's
  g.deadline = GetTickCount64() + kOwnershipWaitMs;
}

/** Throw `prop` at the nearest player in sight; false (nothing begun) when nobody is. */
bool throw_at_visible(void* ghost, void* prop) {
  int actor = 0;
  void* target = visible_target(ghost, actor);
  if (target) begin(prop, target, actor);
  return target != nullptr;
}

bool intercept(void* ghost, void* prop) {
  if (!api.ok || !ghost || g.item || !throwable(prop)) return false;
  throw_at_visible(ghost, prop);
  return true;  // nobody in sight: the throw is swallowed
}

bool intercept_hunt_throw(void* ghost, void* prop) {
  void* interact = prop && game::alive(prop) ? il2cpp::field<void*>(prop, kPropInteract) : nullptr;
  if (!api.ok || !ghost || !throwable(interact)) return false;
  void* state = game::ghost_state(ghost) == game::kStateHunting ? game::ghost_state_object(ghost) : nullptr;
  void* target = state ? il2cpp::field<void*>(state, kHuntTarget) : nullptr;
  // One throw at a time, and none at a target the ghost does not see within range.
  if (g.item || !game::ghost_sees(ghost, target) || chest_distance(ghost, target) > kLithoboliaRange) return true;
  begin(interact, target, actor_of(target));
  return true;
}

bool throw_now(void* ghost) {
  void* activity = ghost && api.ok ? game::ghost_activity(ghost) : nullptr;
  void* prop = activity ? api.propToThrow(activity, nullptr) : nullptr;
  void* interact = prop && game::alive(prop) ? il2cpp::field<void*>(prop, kPropInteract) : nullptr;
  return !g.item && throwable(interact) && throw_at_visible(ghost, interact);
}

void tick(void* ghost, int type) {
  if (!api.ok || !g.item) return;
  if (!ghost || type != kLithobolia || !game::alive(g.item) || !game::alive(g.target)) {
    clear();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (!g.landed) {
    if (api.isMine(il2cpp::field<void*>(g.item, kInteractView), nullptr)) launch(ghost);
    else if (now >= g.deadline) clear();  // never came back: no throw
    return;
  }
  void* rb = il2cpp::field<void*>(g.item, kInteractRigidbody);
  const Vec at = vec(game::position(game::transform_of(g.item)));
  if (lithobolia_hit(at, length(vec(api.velocity(rb, nullptr))), chest_of(g.target))) {
    PG_LOG("lithobolia: the throw hits");
    if (g.actor == game::local_actor()) {
      hit_me(kLithoboliaHit);
    } else if (g.actor) {
      const std::vector<uint8_t> message = net_msg::hit(kLithoboliaHit);
      net::send(message.data(), message.size(), net::To::Actor, true, g.actor);
    }
    clear();
  } else if (now >= g.landed) {
    clear();
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  float sanity = 0;
  if (sender != game::master_actor() || !net_msg::read_hit(data, size, sanity)) return;
  hit_me(std::clamp(sanity, 0.0f, kLithoboliaHit));
}

void reset() { clear(); }

}  // namespace pg::custom::lithobolia
