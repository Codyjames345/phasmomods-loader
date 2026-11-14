// Offline tests for the Asterion's broken doors (mods/custom_ghosts/asterion.cpp) over the fake backend: what a
// break and a lock change in a door, and what the Dev Menu's re-type mid-contract gives back (the rule for lasting
// effects, 2026-11-14). The source is included so its clock can be the test's. The facts the fakes stand for are in
// sdk/systems/doors.md. Run: build\asterion_test.exe
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "fake_game.h"

namespace {
ULONGLONG g_now = 100000;
ULONGLONG test_clock() { return g_now; }
}  // namespace
#define GetTickCount64 test_clock
#include "../mods/custom_ghosts/asterion.cpp"
#undef GetTickCount64

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace asterion = pm::custom::asterion;
using phasmo::Vector3;
using namespace pm::custom;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};

template <class T>
T& at(Obj& o, int32_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&o) + offset);
}

// The fake's field offsets.
constexpr int32_t kDoorLocked = 0x08, kDoorKind = 0x0C;  // Door.locked and Door.kind
constexpr int32_t kDoorBody = 0x10;                      // Door.body
constexpr int32_t kDoorCreak = 0x18;                     // Door.loopSource (none here)
constexpr int32_t kDoorAxis = 0x1C, kDoorOpen = 0x20;    // HingeDoor.hingeAxis and openAngle
constexpr int32_t kBodyKinematic = 0x08, kBodyConstraints = 0x0C;
// Made-up numbers for DoorKind's None, Main and PermanentLock, and Axis's X and Z: the code reads them from the
// map, so none is the game's.
constexpr int32_t kKindNone = 20, kKindMain = 21, kKindPermanent = 22;
constexpr int32_t kAxisX = 1, kAxisZ = 4;

char g_hingeKlass, g_rigidbodyKlass;
Obj g_door, g_body;  // one broken door: its leaf's Transform is the body itself here
std::map<void*, Vector3> g_euler;  // each leaf's Transform.localEulerAngles
int g_portals = 0;                 // Door.CheckOcclusionPortal's calls

// The game's bodies.
void* fake_own_transform(void* component, const void*) { return component; }
Vector3 fake_euler(void* t, const void*) { return g_euler[t]; }
void fake_set_euler(void* t, const Vector3* v, const void*) { g_euler[t] = *v; }
bool fake_get_kinematic(void* b, const void*) { return at<bool>(*static_cast<Obj*>(b), kBodyKinematic); }
void fake_set_kinematic(void* b, bool on, const void*) { at<bool>(*static_cast<Obj*>(b), kBodyKinematic) = on; }
int32_t fake_get_constraints(void* b, const void*) { return at<int32_t>(*static_cast<Obj*>(b), kBodyConstraints); }
void fake_set_constraints(void* b, int32_t flags, const void*) { at<int32_t>(*static_cast<Obj*>(b), kBodyConstraints) = flags; }
void fake_still(void* b, const Vector3*, const void*) {}  // Rigidbody.set_velocity / set_angularVelocity
void fake_portal(void* d, const void*) { ++g_portals; }
void* fake_name(void* d, const void*) { return pm::api::make_string("bedroom door"); }

// One contract with one ordinary hinged door, unbroken, its body free.
void start() {
  asterion::reset();  // what it holds goes back before the fake's own storage does
  fake::reset();
  g_door = g_body = Obj{};
  g_door.klass = &g_hingeKlass;
  g_body.klass = &g_rigidbodyKlass;
  at<bool>(g_door, kDoorLocked) = false;
  at<int32_t>(g_door, kDoorKind) = kKindNone;
  at<void*>(g_door, kDoorBody) = &g_body;
  at<void*>(g_door, kDoorCreak) = nullptr;
  at<int32_t>(g_door, kDoorAxis) = kAxisX;
  at<float>(g_door, kDoorOpen) = 90.0f;
  at<bool>(g_body, kBodyKinematic) = false;
  at<int32_t>(g_body, kBodyConstraints) = 0;
  g_euler.clear();
  g_euler[&g_body] = Vector3{0, 0, 0};
  g_now = 100000;
  g_portals = 0;
  fake::field(map::Door_locked, kDoorLocked);
  fake::field(map::Door_kind, kDoorKind);
  fake::field(map::Door_body, kDoorBody);
  fake::field(map::Door_loopSource, kDoorCreak);
  fake::field(map::HingeDoor_hingeAxis, kDoorAxis);
  fake::field(map::HingeDoor_openAngle, kDoorOpen);
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_own_transform));
  fake::method(map::Transform_get_localEulerAngles, reinterpret_cast<void*>(&fake_euler));
  fake::method(map::Transform_set_localEulerAngles, reinterpret_cast<void*>(&fake_set_euler));
  fake::method(map::Rigidbody_get_isKinematic, reinterpret_cast<void*>(&fake_get_kinematic));
  fake::method(map::Rigidbody_set_isKinematic, reinterpret_cast<void*>(&fake_set_kinematic));
  fake::method(map::Rigidbody_get_constraints, reinterpret_cast<void*>(&fake_get_constraints));
  fake::method(map::Rigidbody_set_constraints, reinterpret_cast<void*>(&fake_set_constraints));
  fake::method(map::Rigidbody_set_velocity, reinterpret_cast<void*>(&fake_still));
  fake::method(map::Rigidbody_set_angularVelocity, reinterpret_cast<void*>(&fake_still));
  fake::method(map::Door_CheckOcclusionPortal, reinterpret_cast<void*>(&fake_portal));
  fake::method(map::UnityObject_get_name, reinterpret_cast<void*>(&fake_name));
  fake::type(map::HingeDoor, &g_hingeKlass);
  fake::type(map::Rigidbody, &g_rigidbodyKlass);
  fake::value(map::DoorKind_None, kKindNone);
  fake::value(map::DoorKind_Main, kKindMain);
  fake::value(map::DoorKind_PermanentLock, kKindPermanent);
  fake::value(map::Axis_X, kAxisX);
  fake::value(map::Axis_Z, kAxisZ);
  std::string missing;
  pm::api::bind(fake::host(), missing);  // entries this code never calls stay unbound, as in the wiring tests
}

// The door the Asterion broke, then locked open as a whole contract's break: what a re-type must give back.
void a_retype_mends_a_door_locked_open() {
  start();
  asterion::break_open(&g_door);
  CHECK(at<bool>(g_body, kBodyKinematic));  // the swing runs under code, not physics
  g_now += 400;                             // past kAsterionSwing: fully open, so locked
  asterion::tick_doors(kAsterion);
  CHECK(at<bool>(g_door, kDoorLocked));
  CHECK(at<int32_t>(g_door, kDoorKind) == kKindPermanent);
  CHECK(at<bool>(g_body, kBodyKinematic) == false);
  CHECK(at<int32_t>(g_body, kBodyConstraints) == phasmo::Rigidbody::kFreezeAll);
  CHECK(g_euler[&g_body].x == 90.0f);  // swung to its open angle
  CHECK(g_portals == 1);               // lock_open updated the portal
  asterion::tick_doors(kTempest);      // the Dev Menu re-types: another type haunts now
  CHECK(at<bool>(g_door, kDoorLocked) == false);
  CHECK(at<int32_t>(g_door, kDoorKind) == kKindNone);
  CHECK(at<bool>(g_body, kBodyKinematic) == false);
  CHECK(at<int32_t>(g_body, kBodyConstraints) == 0);
  CHECK(g_portals == 2);  // the mending updated the portal too
  CHECK(g_euler[&g_body].x == 90.0f);  // left open where it stands
  asterion::tick_doors(kTempest);      // let go once: nothing left to mend
  CHECK(g_portals == 2);
  asterion::reset();
}

// A re-type mid-swing: the leaf never locked, but the body went kinematic for the swing and must go free.
void a_retype_mends_a_door_still_swinging() {
  start();
  asterion::break_open(&g_door);
  g_now += 150;  // half of kAsterionSwing
  asterion::tick_doors(kTempest);
  CHECK(at<bool>(g_body, kBodyKinematic) == false);
  CHECK(at<int32_t>(g_body, kBodyConstraints) == 0);
  CHECK(at<bool>(g_door, kDoorLocked) == false);
  CHECK(at<int32_t>(g_door, kDoorKind) == kKindNone);
  CHECK(g_portals == 1);  // the leaf moved under code, so the portal needs its update
  asterion::reset();
}

// A door that was already locked when it broke (a PermanentLock door, like a shed) goes back locked, not open to
// the world: the record is the door's own state, not "unlock everything".
void a_door_broken_locked_goes_back_locked() {
  start();
  at<bool>(g_door, kDoorLocked) = true;
  at<int32_t>(g_door, kDoorKind) = kKindPermanent;
  asterion::break_open(&g_door);
  g_now += 400;
  asterion::tick_doors(kAsterion);
  asterion::tick_doors(kTempest);
  CHECK(at<bool>(g_door, kDoorLocked));
  CHECK(at<int32_t>(g_door, kDoorKind) == kKindPermanent);
  CHECK(at<bool>(g_body, kBodyKinematic) == false);
  CHECK(at<int32_t>(g_body, kBodyConstraints) == 0);
  asterion::reset();
}

// The next contract: the level's doors are gone, so a re-type mends nothing and needs nothing.
void a_new_level_needs_no_mending() {
  start();
  asterion::break_open(&g_door);
  g_now += 400;
  asterion::tick_doors(kAsterion);
  fake::destroy(&g_door);  // the level teardown, as between contracts
  asterion::tick_doors(kTempest);
  CHECK(g_portals == 1);  // only lock_open's update; the gone door was skipped
  asterion::reset();
}

}  // namespace

int main() {
  a_retype_mends_a_door_locked_open();
  a_retype_mends_a_door_still_swinging();
  a_door_broken_locked_goes_back_locked();
  a_new_level_needs_no_mending();
  pm::fake::reset();
  return g_failures ? 1 : 0;
}
