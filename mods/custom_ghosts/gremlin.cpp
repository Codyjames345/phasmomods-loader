#include "gremlin.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "game.h"
#include "gremlin_rule.h"
#include "il2cpp.h"
#include "log.h"
#include "players.h"

namespace pg::custom::gremlin {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. REA decompile of
// Equipment.Use / UseNetworked (2026-10-20): Use sends UseNetworked(!on) to every client.
constexpr size_t kEquipmentInteract = 0x20;   // Equipment: its PhotonObjectInteract
constexpr size_t kEquipmentOn = 0x281;        // Equipment: switched on
constexpr size_t kLevelGhostRoom = 0x38;      // LevelController: the room the ghost is in
constexpr size_t kActivityInteraction = 0x28; // GhostActivity: GhostInteraction
constexpr uintptr_t kThrowEmfRva = 0x207EEF0; // GhostInteraction.CreateThrowingEMF (EMF at an object)

struct Api {
  il2cpp::Class* equipment = nullptr;
  il2cpp::Object* equipmentType = nullptr;
  const il2cpp::Method* use = nullptr;  // Equipment.Use, called through the object's own override
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  void (*throwEmf)(void*, void*, int, bool, float, const void*) = nullptr;
  bool ok = false;
} api;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

float nearest_player(Vector3 at) {
  float best = 1e9f;
  game::each_player([&](void* player) {
    if (game::living(player)) best = std::min(best, game::distance(at, game::position(game::transform_of(player))));
  });
  return best;
}

/** Running electronics nobody guards, in `room` (or within `reach` of `from` when room is null). */
std::vector<void*> targets(void* room, Vector3 from, float reach) {
  std::vector<void*> found;
  il2cpp::Array* all = api.findObjects(api.equipmentType, nullptr);
  for (uintptr_t i = 0; all && i < all->max_length; ++i) {
    void* e = all->data<void*>()[i];
    if (!e || !game::alive(e) || !gremlin_target_class(il2cpp::class_name(il2cpp::class_of(e)))) continue;
    const Vector3 at = game::position(game::transform_of(e));
    if (room ? !game::in_room(room, at, 0.2f) : game::distance(at, from) > reach) continue;
    if (gremlin_may_sabotage(il2cpp::field<bool>(e, kEquipmentOn), nearest_player(at))) found.push_back(e);
  }
  return found;
}

void switch_off(void* ghost, void* equipment) {
  const il2cpp::Method* m = il2cpp::virtual_method(equipment, api.use);
  if (!m) return;
  reinterpret_cast<void (*)(void*, const void*)>(*reinterpret_cast<void* const*>(m))(equipment, m);
  void* activity = game::ghost_activity(ghost);
  void* interaction = activity ? il2cpp::field<void*>(activity, kActivityInteraction) : nullptr;
  if (void* item = il2cpp::field<void*>(equipment, kEquipmentInteract); interaction && item)
    api.throwEmf(interaction, item, 1, false, 20.0f, nullptr);  // the EMF of a ghost's touch
  PG_LOG("gremlin: switched off a %s", il2cpp::class_name(il2cpp::class_of(equipment)));
}

}  // namespace

bool resolve() {
  api.equipment = il2cpp::cls("Assembly-CSharp.dll", "", "Equipment");
  api.equipmentType = api.equipment ? il2cpp::type_object(api.equipment) : nullptr;
  api.use = api.equipment ? il2cpp::method(api.equipment, "Use", 0) : nullptr;
  bool ok = api.equipmentType && api.use;
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1);
  api.throwEmf = reinterpret_cast<decltype(api.throwEmf)>(game::game_assembly_address(kThrowEmfRva));
  api.ok = ok && api.throwEmf;
  PG_LOG("gremlin: resolve %s", api.ok ? "ok" : "unavailable (it interacts as any ghost)");
  return true;  // optional: the Gremlin still interacts as a ghost does
}

bool interact(void* ghost) {
  if (!api.ok || !ghost || game::ghost_state(ghost) == game::kStateHunting) return false;
  if (std::uniform_real_distribution<float>(0, 1)(g_random) >= kGremlinChance) return false;
  void* level = game::level_controller();
  void* room = level ? il2cpp::field<void*>(level, kLevelGhostRoom) : nullptr;
  if (!room || !game::alive(room)) return false;
  const std::vector<void*> found = targets(room, {}, 0);
  if (found.empty()) return false;
  switch_off(ghost, found[std::uniform_int_distribution<size_t>(0, found.size() - 1)(g_random)]);
  return true;
}

bool short_out(void* ghost) {
  if (!api.ok || !ghost) return false;
  const Vector3 at = game::position(game::transform_of(ghost));
  void* best = nullptr;
  float bestDistance = kGremlinReach;
  for (void* e : targets(nullptr, at, kGremlinReach)) {
    const float d = game::distance(at, game::position(game::transform_of(e)));
    if (d <= bestDistance) {
      best = e;
      bestDistance = d;
    }
  }
  if (best) switch_off(ghost, best);
  return best != nullptr;
}

}  // namespace pg::custom::gremlin
