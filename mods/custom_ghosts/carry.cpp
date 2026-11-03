#include "carry.h"

#include "Photon.h"
#include "Props.h"
#include "Transform.h"
#include "UnityWorld.h"
#include "runtime.h"

namespace pm::custom::carry {

bool Carry::take(void* item, phasmo::Vector3 spot, ULONGLONG now) {
  if (busy() || !item) return false;
  void* view = phasmo::PhotonObjectInteract::view.get(item);
  if (!phasmo::PhotonView::get_IsMine(view)) phasmo::PhotonView::RequestOwnership(view);  // Takeover: granted when it comes back
  item_ = item;
  spot_ = spot;
  deadline_ = now + kOwnershipWaitMs;
  return true;
}

Carry::Step Carry::step(ULONGLONG now, Still still) {
  void* item = item_;
  if (!item) return Step::Idle;
  if (!api::alive(item) || !still(item) || now >= deadline_) {
    reset();
    return Step::Dropped;
  }
  if (!phasmo::PhotonView::get_IsMine(phasmo::PhotonObjectInteract::view.get(item))) return Step::Waiting;
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  game::set_position(game::transform_of(item), spot_);
  const phasmo::Vector3 spot{spot_.x, spot_.y, spot_.z}, still3{0, 0, 0};
  phasmo::Rigidbody::set_position(rb, &spot);
  phasmo::Rigidbody::set_velocity(rb, &still3);
  phasmo::Rigidbody::set_angularVelocity(rb, &still3);
  reset();
  return Step::Set;
}

void Carry::reset() {
  item_ = nullptr;
  spot_ = {};
  deadline_ = 0;
}

}  // namespace pm::custom::carry
