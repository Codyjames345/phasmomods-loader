#include "asterion.h"

#include <windows.h>

#include <cmath>
#include <utility>
#include <vector>

#include "asterion_rule.h"
#include "game.h"
#include "ghosts.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::custom::asterion {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. From REA decompiles
// of Door.Awake, HingeDoor.Awake/LevelControllerOpenDoor and the door calls below (2026-10-20).
constexpr size_t kDoorBody = 0x90;          // Door: Rigidbody (the leaf that swings)
constexpr size_t kDoorClosedAngle = 0xB0;   // Door: the leaf's angle when closed (degrees)
constexpr size_t kDoorKind = 0x108;         // Door: its kind; 1 the front door
constexpr size_t kHingeAxis = 0x194;        // HingeDoor: 1 swings about x, 4 about z, else y
constexpr int kFrontDoor = 1;
constexpr uintptr_t kForceDoorRva = 0x264C3C0;  // Door.GhostForceDoor(bool open, float strength, bool skipRattle)
constexpr uintptr_t kSlamNoiseRva = 0x264BC40;  // Door.CreateDoorSlamNoise(): RPC, every client hears it
constexpr uintptr_t kEnableDoorRva = 0x264C010; // Door.DisableOrEnableDoor(bool enable, bool): RPC
constexpr ULONGLONG kScanMs = 3000;
constexpr ULONGLONG kKeepJammedMs = 2000;   // how often a jammed door is forced open again

struct Api {
  il2cpp::Object* hingeType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  Vector3 (*localEuler)(void*, const void*) = nullptr;
  void (*desiredVelocity)(void*, Vector3*) = nullptr;  // icall: this build strips the managed getter
  Vector3 (*centreOfMass)(void*, const void*) = nullptr;  // Rigidbody.worldCenterOfMass: the leaf's middle
  void (*forceDoor)(void*, bool, float, bool, const void*) = nullptr;
  void (*slamNoise)(void*, const void*) = nullptr;
  void (*enableDoor)(void*, bool, bool, const void*) = nullptr;
  bool ok = false;
} api;

struct Tracked {
  il2cpp::Held door;
  il2cpp::Held leaf;  // the leaf's Transform
  il2cpp::Held body;  // the leaf's Rigidbody
  DoorTrack track;
  ULONGLONG nextForce = 0;
};

struct State {
  il2cpp::Held level;
  std::vector<Tracked> doors;
  ULONGLONG nextScan = 0, lastTick = 0;
  bool battering = false;
} g;

float leaf_angle(const Tracked& t) {
  const Vector3 e = api.localEuler(t.leaf, nullptr);
  const int axis = il2cpp::field<int>(t.door, kHingeAxis);
  return axis == 1 ? e.x : axis == 4 ? e.z : e.y;
}

void scan() {
  il2cpp::Array* found = api.findObjects(api.hingeType, nullptr);
  for (uintptr_t i = 0; found && i < found->max_length; ++i) {
    void* door = found->data<void*>()[i];
    if (!door || !game::alive(door) || il2cpp::field<int>(door, kDoorKind) == kFrontDoor) continue;
    bool known = false;
    for (const Tracked& t : g.doors) known |= t.door == door;
    void* body = il2cpp::field<void*>(door, kDoorBody);
    if (known || !body || !game::alive(body)) continue;
    Tracked t;
    t.door = door;
    t.leaf = game::transform_of(body);
    t.body = body;
    g.doors.push_back(std::move(t));
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* hinge = il2cpp::cls("Assembly-CSharp.dll", "", "HingeDoor");
  api.hingeType = hinge ? il2cpp::type_object(hinge) : nullptr;
  bool ok = api.hingeType != nullptr;
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1);
  ok &= il2cpp::bind(api.localEuler, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Transform"), "get_localEulerAngles", 0);
  api.desiredVelocity = reinterpret_cast<decltype(api.desiredVelocity)>(
      il2cpp::resolve_icall("UnityEngine.AI.NavMeshAgent::get_desiredVelocity_Injected"));
  ok &= api.desiredVelocity != nullptr;
  ok &= il2cpp::bind(api.centreOfMass, il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Rigidbody"),
                     "get_worldCenterOfMass", 0);
  api.forceDoor = reinterpret_cast<decltype(api.forceDoor)>(game::game_assembly_address(kForceDoorRva));
  api.slamNoise = reinterpret_cast<decltype(api.slamNoise)>(game::game_assembly_address(kSlamNoiseRva));
  api.enableDoor = reinterpret_cast<decltype(api.enableDoor)>(game::game_assembly_address(kEnableDoorRva));
  api.ok = ok && api.forceDoor && api.slamNoise && api.enableDoor;
  PG_LOG("asterion: resolve %s", api.ok ? "ok" : "unavailable (its doors behave as any ghost's)");
  return true;  // optional: the Asterion still hunts
}

void tick(void* ghost, int type) {
  g.battering = false;
  if (!api.ok || !ghost || type != kAsterion || !game::alive(ghost)) {
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
  const bool hunting = game::ghost_state(ghost) == game::kStateHunting;
  const Vector3 at = game::position(game::transform_of(ghost));
  Vector3 heading{};
  api.desiredVelocity(game::ghost_agent(ghost), &heading);
  for (Tracked& t : g.doors) {
    if (!game::alive(t.door) || !game::alive(t.leaf) || !game::alive(t.body)) continue;
    const Vector3 hinge = game::position(t.leaf), centre = api.centreOfMass(t.body, nullptr);
    const float dx = centre.x - at.x, dz = centre.z - at.z;
    const float distance = std::fabs(centre.y - at.y) < 2.0f ? std::sqrt(dx * dx + dz * dz) : 1e9f;
    const bool closed = door_closed(leaf_angle(t), il2cpp::field<float>(t.door, kDoorClosedAngle));
    const bool towards = door_ahead(heading.x, heading.z, dx, dz, centre.x - hinge.x, centre.z - hinge.z);
    switch (t.track.step(hunting, distance, closed, towards, dt)) {
      case DoorAction::Bang: api.slamNoise(t.door, nullptr); break;
      case DoorAction::Burst:
        api.forceDoor(t.door, true, -1, true, nullptr);
        api.enableDoor(t.door, false, false, nullptr);  // jammed: nobody closes it again
        t.nextForce = now + kKeepJammedMs;
        PG_LOG("asterion: a door is broken open");
        break;
      case DoorAction::Slam:
        api.forceDoor(t.door, false, -1, true, nullptr);
        api.slamNoise(t.door, nullptr);
        break;
      case DoorAction::None: break;
    }
    if (t.track.battering()) g.battering = true;
    // A jammed door forced shut (an event, physics) is broken open again.
    if (t.track.state == DoorTrack::State::Jammed && closed && now >= t.nextForce) {
      t.nextForce = now + kKeepJammedMs;
      api.forceDoor(t.door, true, -1, true, nullptr);
    }
  }
}

bool battering() { return g.battering; }

void reset() { g = State{}; }

}  // namespace pg::custom::asterion
