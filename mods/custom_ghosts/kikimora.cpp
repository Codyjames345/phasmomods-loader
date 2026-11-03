#include "kikimora.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "Equipment.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Props.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "carry.h"
#include "game.h"
#include "game_api.h"
#include "log.h"
#include "runtime.h"

namespace pm::custom::kikimora {
namespace {

using phasmo::Vector3;

constexpr float kReach = 8.0f;           // metres from the ghost
constexpr float kAboveFloor = 0.3f;      // metres: where a hidden item is set down

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};
ULONGLONG g_next = 0;
carry::Carry g_carry;  // the item it is hiding, taken over and waiting to be moved

phasmo::Vector3 vec(Vector3 v) { return {v.x, v.y, v.z}; }

// The level's rooms, and the one holding a point, from core (game::level_rooms, game::room_at), widened 0.2 m.
api::RefArray rooms() { return game::level_rooms(); }
void* room_at(Vector3 p) { return game::room_at(p, 0.2f); }

bool dark(void* room) {
  bool flickering = false;
  return room && !phasmo::LevelRoom::AreRoomLightsOn(room, &flickering);
}

/** Loose equipment: a PhotonObjectInteract whose Prop is Equipment and which lies free, not held or placed. */
bool loose_equipment(void* item) {
  if (!api::alive(item)) return false;
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  void* view = phasmo::PhotonObjectInteract::view.get(item);
  return phasmo::Equipment::type.is(phasmo::PhotonObjectInteract::prop.get(item)) && api::alive(rb) &&
         !phasmo::Rigidbody::get_isKinematic(rb) && view &&
         phasmo::PhotonView::OwnershipTransfer.get(view) == phasmo::OwnershipOption::Takeover();
}

/** A dark room with nobody in it, other than `except`, and a spot on its floor. */
bool hiding_place(void* except, Vector3& spot) {
  const api::RefArray all = rooms();
  std::vector<void*> candidates;
  for (size_t i = 0; i < all.size(); ++i) {
    void* room = all[i];
    if (room && room != except && api::alive(room) && dark(room) && !game::any_players_in_room(room))
      candidates.push_back(room);
  }
  std::shuffle(candidates.begin(), candidates.end(), g_random);
  for (void* room : candidates) {
    const std::vector<game::Bounds> boxes = game::room_bounds(room);
    if (boxes.empty()) continue;
    phasmo::NavMeshHit hit{};
    const phasmo::Vector3 center = vec(boxes[0].center);
    if (phasmo::NavMesh::SamplePosition(&center, &hit, 3.0f, -1) &&
        game::in_room(room, {hit.position.x, hit.position.y, hit.position.z}, 0.2f)) {
      spot = {hit.position.x, hit.position.y + kAboveFloor, hit.position.z};
      return true;
    }
  }
  return false;
}

}  // namespace

bool resolve() { return true; }

void* nearest(void* ghost) {
  if (!ghost) return nullptr;
  const Vector3 at = game::position(game::transform_of(ghost));
  const api::RefArray items{phasmo::Object::FindObjectsOfType(phasmo::PhotonObjectInteract::type.type_object())};
  void* best = nullptr;
  float bestDistance = kReach;
  for (size_t i = 0; i < items.size(); ++i) {
    void* item = items[i];
    if (!loose_equipment(item)) continue;
    const Vector3 p = game::position(game::transform_of(item));
    const float d = game::distance(p, at);
    if (d < bestDistance && dark(room_at(p))) {
      best = item;
      bestDistance = d;
    }
  }
  return best;
}

bool hide(void* ghost, void* item) {
  if (g_carry.busy()) return false;  // still carrying the last one
  if (!ghost || !loose_equipment(item)) return false;
  void* transform = game::transform_of(item);
  const Vector3 from = game::position(transform);
  void* room = room_at(from);
  Vector3 spot{};
  if (game::distance(from, game::position(game::transform_of(ghost))) > kReach || !dark(room) || !hiding_place(room, spot)) return false;
  // EMF where it was taken from, as a ghost's throw leaves.
  if (void* interaction = phasmo::GhostAI::interaction.get(ghost))
    phasmo::GhostInteraction::CreateThrowingEMF(interaction, item, phasmo::EMFType::objectMoved(), false, 20.0f);
  g_carry.take(item, spot, GetTickCount64());
  PM_LOG("kikimora: taking a piece of equipment %.1f m away", game::distance(from, spot));
  return true;
}

void move_pending(ULONGLONG now) {
  switch (g_carry.step(now, &loose_equipment)) {
    case carry::Carry::Step::Dropped: PM_LOG("kikimora: the equipment was not handed over in time; left where it was"); break;
    case carry::Carry::Step::Set: PM_LOG("kikimora: hid a piece of equipment"); break;
    default: break;
  }
}

void tick(void* ghost, bool steered) {
  const ULONGLONG now = GetTickCount64();
  move_pending(now);
  if (!g_next) g_next = now + carry::kMinGapMs;
  if (steered || now < g_next) return;
  g_next = now + carry::kMinGapMs +
           std::uniform_int_distribution<ULONGLONG>(0, carry::kMaxGapMs - carry::kMinGapMs)(g_random);
  if (void* item = nearest(ghost)) hide(ghost, item);
}

bool in_lit_room(void* ghost) {
  if (!ghost || !api::alive(ghost)) return false;
  void* room = room_at(game::position(game::transform_of(ghost)));
  return room && !dark(room);
}

void reset() {
  g_next = 0;
  g_carry.reset();
}

}  // namespace pm::custom::kikimora
