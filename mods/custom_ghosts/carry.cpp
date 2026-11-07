#include "carry.h"

#include <algorithm>
#include <vector>

#include "LevelRoom.h"
#include "Photon.h"
#include "Props.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "runtime.h"

namespace pm::custom::carry {

namespace {

drag::Point point(phasmo::Vector3 v) { return {v.x, v.y, v.z}; }
phasmo::Vector3 vec(drag::Point p) { return {p.x, p.y, p.z}; }

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
 * The held corner and its route. The corner is the top corner of the thing's collider box nearest its way
 * home; the route runs from it over the navigation mesh (NavMesh.CalculatePath, the corners of a complete
 * path, from the floor below the thing to the floor below its spot) at the corner's height above the floor,
 * at least kMinHold, to above its spot. With no floor in reach or no complete path it goes straight.
 */
void Carry::start_drag(void* item, ULONGLONG now) {
  void* tr = game::transform_of(item);
  const phasmo::Vector3 from = game::position(tr);
  std::vector<drag::Point> corners;
  phasmo::Vector3 floorFrom{}, floorTo{};
  const bool floors = floor_below(from, floorFrom) && floor_below(spot_, floorTo);
  if (floors) {
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
  // Which way it goes first: to the route's first corner past where it lies, else straight to its spot.
  drag::Point ahead = point(spot_);
  for (const drag::Point& c : corners)
    if (std::hypot(c.x - from.x, c.z - from.z) > 0.3f) {
      ahead = c;
      break;
    }
  void* collider = phasmo::Component::GetComponentInChildren(item, phasmo::Collider::type.type_object(), false);
  drag::Point grab = point(from);
  if (api::alive(collider)) {
    const phasmo::Bounds b = phasmo::Collider::get_bounds(collider);
    grab = drag::grab_corner(point(b.center), point(b.extents), {ahead.x - from.x, 0, ahead.z - from.z});
  }
  const float hold = std::max(drag::kMinHold, floors ? grab.y - floorFrom.y : grab.y - from.y);
  const drag::Point end{spot_.x, floors ? floorTo.y + hold : spot_.y + hold, spot_.z};
  route_ = drag::route(grab, corners, end, hold);
  const phasmo::Vector3 grabWorld = vec(grab);
  grabLocal_ = phasmo::Transform::InverseTransformPoint(tr, &grabWorld);
  travelled_ = 0;
  last_ = now;
  const float walk = route_.length / speed_ * 1000.0f;
  deadline_ = now + static_cast<ULONGLONG>(walk) + kDragSpareMs;
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  wasKinematic_ = phasmo::Rigidbody::get_isKinematic(rb);
  phasmo::Rigidbody::set_isKinematic(rb, false);  // under physics on the way, so it swings and hits the floor
}

Carry::Step Carry::step(ULONGLONG now, Still still) {
  void* item = item_;
  if (!item) return Step::Idle;
  if (!api::alive(item) || !still(item) || now >= deadline_) {
    reset();  // left where it is: a dragged thing stays under physics and falls
    return Step::Dropped;
  }
  if (!owned_) {
    if (!phasmo::PhotonView::get_IsMine(phasmo::PhotonObjectInteract::view.get(item))) return Step::Waiting;
    owned_ = true;
    if (speed_ > 0) start_drag(item, now);
  }
  void* rb = phasmo::PhotonObjectInteract::body.get(item);
  const phasmo::Vector3 still3{0, 0, 0};
  if (speed_ > 0) {
    const float dt = std::min(0.1f, (now - last_) / 1000.0f);
    last_ = now;
    void* tr = game::transform_of(item);
    const phasmo::Vector3 grab = phasmo::Transform::TransformPoint(tr, &grabLocal_);
    const drag::Point hand = drag::along(route_, travelled_);
    travelled_ = drag::advance(travelled_, route_.length, drag::distance(point(grab), hand), speed_, dt);
    if (!drag::arrived(travelled_, route_.length, point(game::position(tr)), point(spot_))) {
      // The pull, given as this frame's change of velocity at the held corner (Rigidbody.AddForceAtPosition).
      const phasmo::Vector3 v = phasmo::Rigidbody::GetPointVelocity(rb, &grab);
      const drag::Point a = drag::pull(point(grab), drag::along(route_, travelled_), point(v));
      const phasmo::Vector3 change{a.x * dt, a.y * dt, a.z * dt};
      phasmo::Rigidbody::AddForceAtPosition(rb, &change, &grab, phasmo::Rigidbody::kVelocityChange);
      return Step::Moving;
    }
  }
  place(item, spot_);
  phasmo::Rigidbody::set_velocity(rb, &still3);
  phasmo::Rigidbody::set_angularVelocity(rb, &still3);
  if (speed_ > 0) phasmo::Rigidbody::set_isKinematic(rb, wasKinematic_);  // on its spot as it lay before
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
  grabLocal_ = {};
  travelled_ = 0;
  last_ = 0;
}

}  // namespace pm::custom::carry
