#include "yokai_ears.h"

#include <algorithm>
#include <cmath>

#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "PlayerVoice.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "perceive.h"
#include "runtime.h"

namespace pm::yokai_ears {
namespace {

phasmo::Vector3 position_of(void* transform) {
  return transform ? phasmo::Transform::get_position(transform) : phasmo::Vector3{};
}

}  // namespace

void hear(void* ghost, std::vector<api::Held>& muted) {
  void* head = phasmo::GhostAI::head.get(ghost);
  const phasmo::Vector3 at =
      position_of(head && api::alive(head) ? head : phasmo::Component::get_transform(ghost));
  void* level = phasmo::LevelController::instance.get();
  void* ghostRoom = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  const api::RefArray voices{phasmo::Object::FindObjectsOfType(phasmo::PlayerVoice::type.type_object())};
  for (size_t i = 0; i < voices.size(); ++i) {
    void* voice = voices[i];
    void* player = voice ? phasmo::PlayerVoice::player.get(voice) : nullptr;
    void* tap = voice ? phasmo::PlayerVoice::localTap.get(voice) : nullptr;
    void* source = tap ? phasmo::Component::GetComponent(tap, phasmo::AudioSource::type.type_object()) : nullptr;
    if (!source || !player || !api::alive(player)) continue;
    void* body = phasmo::Player::body.get(player);
    const phasmo::Vector3 p = position_of(body && api::alive(body) ? phasmo::GameObject::get_transform(body)
                                                                    : phasmo::Component::get_transform(player));
    void* room = phasmo::Player::currentRoom.get(player);
    const bool sameFloor = room && ghostRoom && api::alive(room) && api::alive(ghostRoom) &&
                           phasmo::LevelRoom::floor.get(room) == phasmo::LevelRoom::floor.get(ghostRoom);
    const bool outside = room && phasmo::LevelRoom::kind.get(room) == phasmo::RoomKind::outsideMap();
    const float dx = p.x - at.x, dy = p.y - at.y, dz = p.z - at.z;
    const bool heard = perceive::yokai_hears(std::sqrt(dx * dx + dy * dy + dz * dz), sameFloor, outside,
                                             phasmo::Player::wished.get(player));
    phasmo::AudioSource::set_mute(source, !heard);
    if (!heard && std::none_of(muted.begin(), muted.end(), [&](const api::Held& m) { return m.get() == source; }))
      muted.emplace_back(source);
  }
}

void unmute(std::vector<api::Held>& muted) {
  for (const api::Held& source : muted)
    if (api::alive(source.get())) phasmo::AudioSource::set_mute(source.get(), false);
  muted.clear();
}

}  // namespace pm::yokai_ears
