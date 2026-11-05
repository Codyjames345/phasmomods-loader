#include "asterion.h"

#include <windows.h>

#include <cmath>
#include <utility>
#include <vector>

#include "Door.h"
#include "GhostAI.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "asterion_rule.h"
#include "game.h"
#include "ghosts.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::custom::asterion {
namespace {

using phasmo::Vector3;

// From REA decompiles of Door.Awake, HingeDoor.Awake/LevelControllerOpenDoor and the door calls below
// (2026-10-20); the map entries name each one (sdk/map/Door.h).
constexpr ULONGLONG kScanMs = 3000;
constexpr ULONGLONG kKeepJammedMs = 2000;   // how often a jammed door is forced open again

constexpr ULONGLONG kReportMs[] = {1000, 3000};  // after a burst, when the log says how far the leaf has turned

struct Tracked {
  api::Held door;
  api::Held leaf;  // the leaf's Transform
  api::Held body;  // the leaf's Rigidbody
  DoorTrack track;
  DoorJam jam;
  ULONGLONG nextForce = 0;
  ULONGLONG burstAt = 0;  // when it burst, for the reports below
  float burstAngle = 0;   // the leaf's angle as it burst
  int reported = 0;       // how many of kReportMs have been logged
};

struct State {
  api::Held level;
  std::vector<Tracked> doors;
  ULONGLONG nextScan = 0, lastTick = 0;
  bool battering = false;
} g;

/** The leaf's angle about its hinge: its local Euler angle on the HingeDoor's axis (x, z, else y). */
float leaf_angle(const Tracked& t) {
  const Vector3 e = phasmo::Transform::get_localEulerAngles(t.leaf);
  const int axis = phasmo::HingeDoor::hingeAxis.get(t.door);
  return axis == phasmo::Axis::X() ? e.x : axis == phasmo::Axis::Z() ? e.z : e.y;
}

/** Every hinged door but the front door, tracked once. */
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

const char* yes_no(bool b) { return b ? "yes" : "no"; }

/** Whether the door's push is still running: Door.GhostForceDoor does nothing while it is. */
bool push_running(const Tracked& t) { return phasmo::Door::forceRoutine.get(t.door) != nullptr; }

/**
 * Force the door open at the game's own strength (-1), skipping its rattle. The push switches the door
 * back on for players as it ends, so the jam is sent again after it (DoorJam).
 */
void force_open(Tracked& t) {
  phasmo::Door::GhostForceDoor(t.door, true, -1, true);
  t.jam.pushed();
}

/** What the log says of a door the Asterion bursts, and how far its leaf has turned since. */
void report(Tracked& t, ULONGLONG now) {
  if (!t.burstAt || t.reported >= static_cast<int>(std::size(kReportMs)) || now < t.burstAt + kReportMs[t.reported])
    return;
  PM_LOG("asterion: %llu ms after the burst the leaf has turned %.0f degrees (closed %s, push running %s, kinematic %s)",
         kReportMs[t.reported], std::fabs(delta_angle(t.burstAngle, leaf_angle(t))),
         yes_no(door_closed(leaf_angle(t), phasmo::Door::closedAngle.get(t.door))), yes_no(push_running(t)),
         yes_no(phasmo::Rigidbody::get_isKinematic(t.body)));
  ++t.reported;
}

}  // namespace

bool resolve() { return true; }

void tick(void* ghost, int type) {
  g.battering = false;
  if (!ghost || type != kAsterion || !game::alive(ghost)) {
    if (!g.doors.empty()) reset();
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
  for (Tracked& t : g.doors) {
    if (!api::alive(t.door) || !api::alive(t.leaf) || !api::alive(t.body)) continue;
    const Vector3 hinge = phasmo::Transform::get_position(t.leaf);
    const Vector3 centre = phasmo::Rigidbody::get_worldCenterOfMass(t.body);
    const float dx = centre.x - at.x, dz = centre.z - at.z;
    const float distance = std::fabs(centre.y - at.y) < 2.0f ? std::sqrt(dx * dx + dz * dz) : 1e9f;
    const bool closed = door_closed(leaf_angle(t), phasmo::Door::closedAngle.get(t.door));
    const bool towards = door_ahead(heading.x, heading.z, dx, dz, centre.x - hinge.x, centre.z - hinge.z);
    switch (t.track.step(hunting, distance, closed, towards, dt)) {
      case DoorAction::Bang: phasmo::Door::CreateDoorSlamNoise(t.door, nullptr); break;
      case DoorAction::Burst: {
        const bool busy = push_running(t);
        PM_LOG("asterion: a door is broken open (push already running %s, moved with physics %s, kinematic %s)",
               yes_no(busy), yes_no(phasmo::Door::useForceInsteadOfAnimations.get(t.door)),
               yes_no(phasmo::Rigidbody::get_isKinematic(t.body)));
        t.burstAt = now;
        t.burstAngle = leaf_angle(t);
        t.reported = 0;
        force_open(t);
        t.nextForce = now + kKeepJammedMs;
        break;
      }
      case DoorAction::None: break;
    }
    if (t.track.battering()) g.battering = true;
    const bool jammed = t.track.state == DoorTrack::State::Jammed;
    // Jammed: nobody closes it again. Sent once each push is over, as the push switches it back on.
    if (t.jam.due(jammed, push_running(t))) phasmo::Door::DisableOrEnableDoor(t.door, false, false);
    // A jammed door forced shut (an event, physics) is broken open again.
    if (jammed && closed && now >= t.nextForce) {
      t.nextForce = now + kKeepJammedMs;
      force_open(t);
    }
    report(t, now);
  }
}

bool battering() { return g.battering; }

void reset() { g = State{}; }

}  // namespace pm::custom::asterion
