#include "footsteps.h"

#include "GhostAI.h"
#include "GhostSenses.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::footsteps {

void heard(void* ghost, std::vector<perceive::Mark>& out) {
  void* ears = phasmo::GhostAI::hearing.get(ghost);
  const api::List heard{ears && api::alive(ears) ? phasmo::GhostHearing::heard.get(ears) : nullptr};
  const api::ValueArray noises = heard.values();
  for (size_t i = 0, n = heard.size(); i < n; ++i) {
    void* element = noises.at(i);
    void* noise = element ? phasmo::HeardNoise::noise.get(element) : nullptr;
    void* player = noise ? phasmo::PlayerNoise::player.get(noise) : nullptr;
    if (!player || !api::alive(player) || phasmo::Player::isDead.get(player)) continue;
    void* transform = phasmo::Component::get_transform(player);
    if (!transform) continue;
    const int32_t kind = phasmo::PlayerNoise::kind.get(noise);
    const uint8_t mark = kind == phasmo::NoiseKind::SprintFootstep()   ? perceive::kStepSprint
                         : kind == phasmo::NoiseKind::CrouchFootstep() ? perceive::kStepCreep
                                                                       : perceive::kStepWalk;
    const phasmo::Vector3 p = phasmo::Transform::get_position(transform);
    out.push_back({mark, p.x, p.y, p.z});
  }
}

}  // namespace pm::footsteps
