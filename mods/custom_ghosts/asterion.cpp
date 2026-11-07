#include "asterion.h"

#include <windows.h>

#include <cmath>
#include <utility>
#include <vector>

#include "Door.h"
#include "GhostAI.h"
#include "Photon.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "asterion_rule.h"
#include "flame_net.h"
#include "game.h"
#include "ghosts.h"
#include "held.h"
#include "log.h"
#include "net.h"
#include "runtime.h"

namespace pm::custom::asterion {
namespace {

using phasmo::Vector3;

// The doors and their lock: sdk/systems/doors.md, and the map entries in sdk/map/Door.h.
constexpr ULONGLONG kScanMs = 3000;

// A door the host tracks through the Asterion's hunts.
struct Tracked {
  api::Held door;
  api::Held leaf;  // the leaf's Transform
  api::Held body;  // the leaf's Rigidbody
  DoorTrack track;
};

// A broken leaf swinging open on this client, then locked there.
struct Swing {
  api::Held door;
  api::Held leaf;
  api::Held body;
  float from = 0, to = 0;  // its angle as it broke, and its open angle (HingeDoor.openAngle)
  ULONGLONG start = 0;
  bool locked = false;
};

struct State {
  api::Held level;
  std::vector<Tracked> doors;  // the host's
  std::vector<Swing> swings;   // every client's
  ULONGLONG nextScan = 0, lastTick = 0;
  bool battering = false;
} g;

/** The leaf's angle about its hinge: its local Euler angle on the HingeDoor's axis (x, z, else y). */
float leaf_angle(void* door, void* leaf) {
  const Vector3 e = phasmo::Transform::get_localEulerAngles(leaf);
  const int axis = phasmo::HingeDoor::hingeAxis.get(door);
  return axis == phasmo::Axis::X() ? e.x : axis == phasmo::Axis::Z() ? e.z : e.y;
}

/** Turns the leaf to `angle` on its hinge's axis, keeping its other two angles. */
void set_leaf_angle(void* door, void* leaf, float angle) {
  Vector3 e = phasmo::Transform::get_localEulerAngles(leaf);
  const int axis = phasmo::HingeDoor::hingeAxis.get(door);
  (axis == phasmo::Axis::X() ? e.x : axis == phasmo::Axis::Z() ? e.z : e.y) = angle;
  phasmo::Transform::set_localEulerAngles(leaf, &e);
}

std::string name_of(void* door) { return api::to_utf8(phasmo::Object::get_name(door)); }

/** Every hinged door but the front door, tracked once (the host's). */
void scan() {
  const api::RefArray found{phasmo::Object::FindObjectsOfType(phasmo::HingeDoor::type.type_object())};
  const int frontDoor = phasmo::DoorKind::Main();
  for (size_t i = 0; i < found.size(); ++i) {
    void* door = found[i];
    if (!api::alive(door) || phasmo::Door::kind.get(door) == frontDoor) continue;
    bool known = false;
    for (const Tracked& t : g.doors) known |= t.door == door;
    void* body = phasmo::Door::body.get(door);
    if (known || !api::alive(body)) continue;
    Tracked t;
    t.door = door;
    t.leaf = phasmo::Component::get_transform(body);
    t.body = body;
    g.doors.push_back(std::move(t));
  }
}

/**
 * This client breaks `door` open: its leaf starts its swing to the open angle, made kinematic so physics
 * does not fight it. Once it is fully open it is locked there (tick_doors). A door already broken is left.
 */
void break_open(void* door) {
  void* body = api::alive(door) ? phasmo::Door::body.get(door) : nullptr;
  if (!api::alive(body)) return;
  for (const Swing& s : g.swings)
    if (s.door == door) return;
  Swing s;
  s.door = door;
  s.body = body;
  s.leaf = phasmo::Component::get_transform(body);
  s.from = leaf_angle(door, s.leaf);
  s.to = phasmo::HingeDoor::openAngle.get(door);
  s.start = GetTickCount64();
  phasmo::Rigidbody::set_isKinematic(body, true);
  PM_LOG("asterion: %s is broken open (from %.0f to %.0f degrees)", name_of(door).c_str(), s.from, s.to);
  g.swings.push_back(std::move(s));
}

/**
 * Locks a fully open leaf where it stands, leaving the state the game's lock (Door.SyncLockState) leaves:
 * Door.locked set (the padlock), the body frozen in every position and rotation, then the occlusion portal
 * updated. The game never locks an open door, and a mod never sends that RPC, so each client does this
 * itself (the user's ruling, 2026-11-06). It also becomes a DoorKind.PermanentLock door: a player who grabs
 * it rattles it, where an ordinary interior door that is only locked is unlocked by the first grab
 * (Door.AttemptToUnlockDoor), as the user's live run showed.
 */
void lock_open(Swing& s) {
  const Vector3 still{0, 0, 0};
  phasmo::Rigidbody::set_velocity(s.body, &still);
  phasmo::Rigidbody::set_angularVelocity(s.body, &still);
  phasmo::Rigidbody::set_isKinematic(s.body, false);
  phasmo::Rigidbody::set_constraints(s.body, phasmo::Rigidbody::kFreezeAll);
  phasmo::Door::locked.set(s.door, true);
  phasmo::Door::kind.set(s.door, phasmo::DoorKind::PermanentLock());
  phasmo::Door::CheckOcclusionPortal(s.door);
  s.locked = true;
  PM_LOG("asterion: %s is locked open at %.0f degrees", name_of(s.door).c_str(), leaf_angle(s.door, s.leaf));
}

/** The host: break these leaves open here and on every other client. */
void break_everywhere(const std::vector<void*>& leaves) {
  std::vector<uint32_t> ids;
  for (void* door : leaves) {
    const int32_t id = phasmo::PhotonView::get_ViewID(phasmo::Door::view.get(door));
    if (id > 0) ids.push_back(static_cast<uint32_t>(id));
    break_open(door);
  }
  if (ids.empty()) return;
  const std::vector<uint8_t> message = net_msg::doors(ids);
  net::send(message.data(), message.size(), net::To::Others, true);
}

/** The other leaf of the double door g.doors[i] (Door.otherDoor or Door.pairedDoor, named by either leaf), or null. */
Tracked* partner_of(size_t i) {
  std::vector<const void*> doors, others, paired;
  for (const Tracked& t : g.doors) {
    const bool alive = api::alive(t.door);
    doors.push_back(alive ? t.door.get() : nullptr);
    others.push_back(alive ? phasmo::Door::otherDoor.get(t.door) : nullptr);
    paired.push_back(alive ? phasmo::Door::pairedDoor.get(t.door) : nullptr);
  }
  const int k = door_partner(doors, others, paired, i);
  return k < 0 ? nullptr : &g.doors[k];
}

}  // namespace

bool resolve() { return true; }

void tick(void* ghost, int type) {
  g.battering = false;
  if (!ghost || type != kAsterion || !game::alive(ghost)) {
    g.doors.clear();
    return;
  }
  void* level = game::level_controller();
  if (level != g.level) {
    reset();
    g.level = level;
  }
  const ULONGLONG now = GetTickCount64();
  const float dt = g.lastTick ? (now - g.lastTick) / 1000.0f : 0;
  g.lastTick = now;
  if (now >= g.nextScan) {
    g.nextScan = now + kScanMs;
    scan();
  }
  const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
  const phasmo::Vector3 at = game::position(game::transform_of(ghost));
  Vector3 heading{};
  phasmo::NavMeshAgent::get_desiredVelocity_Injected(game::ghost_agent(ghost), &heading);
  for (size_t i = 0; i < g.doors.size(); ++i) {
    Tracked& t = g.doors[i];
    if (!api::alive(t.door) || !api::alive(t.leaf) || !api::alive(t.body)) continue;
    const Vector3 hinge = phasmo::Transform::get_position(t.leaf);
    const Vector3 centre = phasmo::Rigidbody::get_worldCenterOfMass(t.body);
    const float dx = centre.x - at.x, dz = centre.z - at.z;
    const float distance = std::fabs(centre.y - at.y) < 2.0f ? std::sqrt(dx * dx + dz * dz) : 1e9f;
    const bool closed = door_closed(leaf_angle(t.door, t.leaf), phasmo::Door::closedAngle.get(t.door));
    const bool towards = door_ahead(heading.x, heading.z, dx, dz, centre.x - hinge.x, centre.z - hinge.z);
    switch (t.track.step(hunting, distance, closed, towards, dt)) {
      case DoorAction::Bang: phasmo::Door::CreateDoorSlamNoise(t.door, nullptr); break;
      case DoorAction::Burst: {
        // A double door gives way as one: its other leaf breaks open with it.
        std::vector<void*> leaves{t.door.get()};
        Tracked* other = partner_of(i);
        if (other && other->track.state != DoorTrack::State::Jammed) {
          other->track.jam();
          leaves.push_back(other->door.get());
        }
        break_everywhere(leaves);
        break;
      }
      case DoorAction::None: break;
    }
    if (t.track.battering()) g.battering = true;
  }
}

void tick_doors(int type) {
  if (type != kAsterion) {
    g.swings.clear();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  for (Swing& s : g.swings) {
    if (s.locked || !api::alive(s.door) || !api::alive(s.leaf) || !api::alive(s.body)) continue;
    const float seconds = (now - s.start) / 1000.0f;
    set_leaf_angle(s.door, s.leaf, swing_angle(s.from, s.to, seconds));
    if (seconds >= kAsterionSwing) lock_open(s);
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  std::vector<uint32_t> ids;
  if (sender != game::master_actor() || !net_msg::read_doors(data, size, ids)) return;
  const api::RefArray found{phasmo::Object::FindObjectsOfType(phasmo::HingeDoor::type.type_object())};
  for (uint32_t id : ids)
    for (size_t i = 0; i < found.size(); ++i) {
      void* door = found[i];
      if (api::alive(door) &&
          phasmo::PhotonView::get_ViewID(phasmo::Door::view.get(door)) == static_cast<int32_t>(id)) {
        break_open(door);
        break;
      }
    }
}

bool battering() { return g.battering; }

void reset() { g = State{}; }

}  // namespace pm::custom::asterion
