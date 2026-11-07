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

/** Unity's default raycast layers: every layer but Ignore Raycast (Physics.DefaultRaycastLayers). */
constexpr int32_t kDefaultRaycastLayers = -5;

/** Whether nothing solid is overhead from the floor at `floor` up to the height `over` (Physics.Raycast,
 * triggers such as room volumes ignored). */
bool open_above(drag::Point floor, float over) {
  const float reach = over - floor.y - 0.05f;
  if (reach <= 0) return true;
  const phasmo::Vector3 from{floor.x, floor.y + 0.05f, floor.z}, upward{0, 1, 0};
  return !phasmo::Physics::Raycast(&from, &upward, reach, kDefaultRaycastLayers,
                                   phasmo::QueryTriggerInteraction::Ignore());
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
 * at least kMinHold, to where the corner lies with the thing on its spot. A thing that lay up on a table or
 * shelf is never lifted: its route ends on the floor beside it, where nothing is overhead up to its surface
 * (Physics.Raycast upward), and it is left there. With no floor in reach or no complete path it goes straight.
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
  float drop = 0;  // the corner's height above the thing's bottom
  if (api::alive(collider)) {
    const phasmo::Bounds b = phasmo::Collider::get_bounds(collider);
    grab = drag::grab_corner(point(b.center), point(b.extents), {ahead.x - from.x, 0, ahead.z - from.z});
    drop = 2 * b.extents.y;
  }
  const float hold = std::max(drag::kMinHold, floors ? grab.y - floorFrom.y : grab.y - from.y);
  // Where the corner lies with the thing on its spot, as high above the thing's middle as it is now.
  const drag::Point rest{spot_.x, spot_.y + (grab.y - from.y), spot_.z};
  const float surface = rest.y - drop;  // the top of what it lay on
  beside_ = floors && !corners.empty() && surface - floorTo.y > drag::kRaised;
  if (beside_) {
    const float above = surface + 0.1f;  // up through the top of the table or shelf, to tell what is under it
    route_ = drag::route_beside(grab, corners, hold, [above](drag::Point f) { return open_above(f, above); });
  } else {
    route_ = drag::route(grab, corners, rest, hold);
  }
  const phasmo::Vector3 grabWorld = vec(grab);
  grabLocal_ = phasmo::Transform::InverseTransformPoint(tr, &grabWorld);
  travelled_ = 0;
  last_ = now;
  headway_ = {};
  tugged_ = 0;
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
    let_go(item);  // left where it is: a dragged thing stays under physics and falls
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
    const float lag = drag::distance(point(grab), hand);
    travelled_ = drag::advance(travelled_, route_.length, lag, speed_, dt);
    tugged_ = drag::tug(tugged_, lag > drag::kLead, dt);
    // Beside a table or shelf the held corner is to reach the route's end; else the thing its spot.
    const bool there = beside_ ? travelled_ >= route_.length &&
                                     drag::distance(point(grab), route_.points.back()) <= drag::kArrived
                               : drag::arrived(travelled_, route_.length, point(game::position(tr)), point(spot_));
    if (!there) {
      if (headway_.stuck(travelled_, now)) {  // snagged for good: left where it lies
        let_go(item);
        return Step::Dropped;
      }
      // The pull, given as this frame's change of velocity at the held corner (Rigidbody.AddForceAtPosition),
      // toward the hand, raised by any tug.
      drag::Point to = drag::along(route_, travelled_);
      to.y += tugged_;
      const phasmo::Vector3 v = phasmo::Rigidbody::GetPointVelocity(rb, &grab);
      const drag::Point a = drag::pull(point(grab), to, point(v));
      const phasmo::Vector3 change{a.x * dt, a.y * dt, a.z * dt};
      phasmo::Rigidbody::AddForceAtPosition(rb, &change, &grab, phasmo::Rigidbody::kVelocityChange);
      return Step::Moving;
    }
  }
  if (speed_ > 0 && beside_) {
    // Beside the table or shelf it lay on: left on the floor there, under physics, never lifted onto it.
    gap_ = drag::distance(point(game::position(game::transform_of(item))), point(spot_));
    reset();
    return Step::Set;
  }
  place(item, spot_);
  phasmo::Rigidbody::set_velocity(rb, &still3);
  phasmo::Rigidbody::set_angularVelocity(rb, &still3);
  if (speed_ > 0) phasmo::Rigidbody::set_isKinematic(rb, wasKinematic_);  // on its spot as it lay before
  gap_ = 0;
  reset();
  return Step::Set;
}

void Carry::let_go(void* item) {
  gap_ = -1;
  if (api::alive(item)) {
    const phasmo::Vector3 at = game::position(game::transform_of(item));
    gap_ = drag::distance(point(at), point(spot_));
  }
  reset();  // left where it is: a dragged thing stays under physics and falls
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
  headway_ = {};
  tugged_ = 0;
  beside_ = false;
}

}  // namespace pm::custom::carry
