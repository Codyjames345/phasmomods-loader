#include "carry.h"

#include <vector>

#include "Photon.h"
#include "Props.h"
#include "Transform.h"
#include "UnityWorld.h"
#include "runtime.h"

namespace pm::custom::carry {

namespace {

drag::Point point(phasmo::Vector3 v) { return {v.x, v.y, v.z}; }

/** The navigation mesh's floor below `at` (NavMesh.SamplePosition), or false when there is none in reach. */
bool floor_below(phasmo::Vector3 at, phasmo::Vector3& floor) {
  phasmo::NavMeshHit hit{};
  if (!phasmo::NavMesh::SamplePosition(&at, &hit, kFloorReach, -1)) return false;
  floor = hit.position;
  return true;
}

void place(void* item, phasmo::Vector3 at) {
  game::set_position(game::transform_of(item), at);
  phasmo::Rigidbody::set_position(phasmo::PhotonObjectInteract::body.get(item), &at);
}

}  // namespace

bool Carry::take(void* item, phasmo::Vector3 spot, ULONGLONG now) { return take_dragging(item, spot, now, 0); }

bool Carry::take_dragging(void* item, phasmo::Vector3 spot, ULONGLONG now, float speed) {
  if (busy() || !item) return false;
  void* view = phasmo::PhotonObjectInteract::view.get(item);
  if (!phasmo::PhotonView::get_IsMine(view)) phasmo::PhotonView::RequestOwnership(view);  // Takeover: granted when it comes back
  item_ = item;
  spot_ = spot;
  deadline_ = now + kOwnershipWaitMs;
  speed_ = speed > 0 ? speed : 0;
  owned_ = false;
  return true;
}

/**
 * The route along the floor: from below the thing to below its spot over the navigation mesh
 * (NavMesh.CalculatePath, the corners of a complete path), at the thing's height above the floor. With no
 * floor in reach or no complete path it goes straight.
 */
void Carry::start_drag(void* item) {
  const phasmo::Vector3 from = game::position(game::transform_of(item));
  std::vector<drag::Point> corners;
  phasmo::Vector3 floorFrom{}, floorTo{};
  float lift = 0;
  if (floor_below(from, floorFrom) && floor_below(spot_, floorTo)) {
    lift = from.y - floorFrom.y;
    if (!path_) {
      void* path = phasmo::NavMeshPath::type.make();
      if (path) phasmo::NavMeshPath::ctor(path);
      path_ = path;
    }
    if (path_ && phasmo::NavMesh::CalculatePath(&floorFrom, &floorTo, -1, path_) &&
        phasmo::NavMeshPath::get_status(path_) == phasmo::NavMeshPathStatus::PathComplete()) {
      const api::Array<phasmo::Vector3> found{phasmo::NavMeshPath::get_corners(path_)};
      for (size_t i = 0; i < found.size(); ++i) corners.push_back(point(found[i]));
    }
  }
  route_ = drag::route(point(from), corners, point(spot_), lift);
  travelled_ = 0;
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  wasKinematic_ = phasmo::Rigidbody::get_isKinematic(rb);
  phasmo::Rigidbody::set_isKinematic(rb, true);  // carried, not thrown about by physics on the way
}

/** Lets a dragged thing go where it is, as it was before: physics takes it again. */
void Carry::release(void* item) {
  if (owned_ && speed_ > 0 && api::alive(item))
    phasmo::Rigidbody::set_isKinematic(phasmo::PhotonObjectInteract::body.get(item), wasKinematic_);
}

Carry::Step Carry::step(ULONGLONG now, Still still) {
  void* item = item_;
  if (!item) return Step::Idle;
  if (!api::alive(item) || !still(item) || (!owned_ && now >= deadline_)) {
    release(item);
    reset();
    return Step::Dropped;
  }
  if (!owned_) {
    if (!phasmo::PhotonView::get_IsMine(phasmo::PhotonObjectInteract::view.get(item))) return Step::Waiting;
    owned_ = true;
    last_ = now;
    if (speed_ > 0) start_drag(item);
  }
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  const phasmo::Vector3 still3{0, 0, 0};
  if (speed_ > 0) {
    travelled_ += speed_ * (now - last_) / 1000.0f;
    last_ = now;
    if (travelled_ < route_.length) {
      const drag::Point p = drag::along(route_, travelled_);
      place(item, {p.x, p.y, p.z});
      return Step::Moving;
    }
  }
  place(item, spot_);
  phasmo::Rigidbody::set_velocity(rb, &still3);
  phasmo::Rigidbody::set_angularVelocity(rb, &still3);
  release(item);
  reset();
  return Step::Set;
}

void Carry::reset() {
  item_ = nullptr;
  spot_ = {};
  deadline_ = 0;
  speed_ = 0;
  owned_ = false;
  route_ = {};
  travelled_ = 0;
  last_ = 0;
}

}  // namespace pm::custom::carry
