#include "gremlin.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "Equipment.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "Props.h"
#include "Transform.h"
#include "UnityObject.h"
#include "game.h"
#include "gremlin_rule.h"
#include "log.h"
#include "players.h"
#include "runtime.h"

namespace pm::custom::gremlin {
namespace {

using phasmo::Vector3;

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
  const api::RefArray all{phasmo::Object::FindObjectsOfType(phasmo::Equipment::type.type_object())};
  for (size_t i = 0; i < all.size(); ++i) {
    void* e = all[i];
    if (!api::alive(e) || !electronic(e)) continue;
    const Vector3 at = game::position(game::transform_of(e));
    if (room ? !game::in_room(room, at, 0.2f) : game::distance(at, from) > reach) continue;
    if (gremlin_may_sabotage(phasmo::Equipment::isOn.get(e), nearest_player(at))) found.push_back(e);
  }
  return found;
}

/**
 * Switch it off through its own Use (which the game sends to every client), and leave the EMF of a
 * thrown or moved object at it, as GhostActivity.InteractWithAProp does with the ghost's GhostInteraction.
 */
void switch_off(void* ghost, void* equipment) {
  phasmo::Equipment::Use.virtual_call(equipment);
  void* interaction = phasmo::GhostAI::interaction.get(ghost);
  if (void* item = phasmo::Prop::interact.get(equipment); interaction && item)
    phasmo::GhostInteraction::CreateThrowingEMF(interaction, item, phasmo::EMFType::objectMoved(), false, 20.0f);
  PM_LOG("gremlin: switched off %s", api::to_utf8(phasmo::Object::get_name(equipment)).c_str());
}

}  // namespace

bool resolve() { return true; }

bool electronic(void* equipment) {
  return equipment &&
         (phasmo::CCTV::type.is(equipment) || phasmo::MotionSensor::type.is(equipment) ||
          phasmo::SoundSensor::type.is(equipment) || phasmo::EMFReader::type.is(equipment) ||
          phasmo::ParabolicMicrophone::type.is(equipment) || phasmo::SoundRecorder::type.is(equipment) ||
          phasmo::Thermometer::type.is(equipment) || phasmo::EVPRecorder::type.is(equipment) ||
          phasmo::IRLightSensor::type.is(equipment) || phasmo::Torch::type.is(equipment));
}

bool interact(void* ghost) {
  if (!ghost || game::ghost_state(ghost) == phasmo::GhostState::hunting()) return false;
  if (std::uniform_real_distribution<float>(0, 1)(g_random) >= kGremlinChance) return false;
  void* level = game::level_controller();
  void* room = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  if (!room || !api::alive(room)) return false;
  const std::vector<void*> found = targets(room, {}, 0);
  if (found.empty()) return false;
  switch_off(ghost, found[std::uniform_int_distribution<size_t>(0, found.size() - 1)(g_random)]);
  return true;
}

bool short_out(void* ghost) {
  if (!ghost) return false;
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

}  // namespace pm::custom::gremlin
