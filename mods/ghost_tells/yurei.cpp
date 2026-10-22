#include "yurei.h"

#include <vector>

#include "asterion_rule.h"
#include "game.h"
#include "gate.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"
#include "rolls.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"
#include "yurei_rule.h"

namespace pg::tells::yurei {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (class dump, REA of the
// ability state, LevelController.Lock/UnlockHuntingDoors and Door.HuntingCloseDoor, 2026-10-21).
constexpr size_t kRoomDoors = 0x28;             // LevelRoom: Door[] (the ability's random pick)
constexpr size_t kDoorBody = 0x90;              // Door: the leaf's Rigidbody
constexpr size_t kDoorClosedAngle = 0xB0;       // Door: the leaf's angle when closed
constexpr size_t kHingeAxis = 0x194;            // HingeDoor: 1 swings about x, 4 about z, else y
constexpr size_t kLevelHuntDoors = 0xA8;        // LevelController: Door[] the hunts lock
constexpr size_t kLevelVariations = 0x28;       // LevelController: SmallMapVariation[]
constexpr size_t kVariationHuntDoors = 0x58;    // SmallMapVariation: Door[] huntingDoorsToLock
constexpr size_t kValuesDifficulty = 0x38;      // LevelValues: the contract's Difficulty
constexpr size_t kValuesEventsBlocked = 0x23;   // LevelValues: events (and abilities) blocked
constexpr size_t kDifficultyDoorsStayOpen = 0x6C;  // Difficulty: hunts lock no doors (HuntingCloseDoor checks it)
constexpr uintptr_t kTrapRva = 0x8256B0;        // WhiteSage: the incense's Yurei trap handler (TemporarilyStopWander(90))
constexpr uintptr_t kForceDoorRva = 0x264C3C0;  // Door.GhostForceDoor(bool open, float strength, bool skipRattle)
constexpr uintptr_t kHuntCloseRva = 0x264CA70;  // Door.HuntingCloseDoor(): shut and locked, RPC
constexpr uintptr_t kToggleLockRva = 0x264E530; // Door.ToggleDoorLock(bool unlock, bool): RPC
constexpr int kStateIdle = 0, kStateWander = 1, kStateFavouriteRoom = 3, kStateAbility = 15;

struct Api {
  Vector3 (*localEuler)(void*, const void*) = nullptr;
  void (*forceDoor)(void*, bool, float, bool, const void*) = nullptr;
  void (*huntClose)(void*, const void*) = nullptr;
  void (*toggleLock)(void*, bool, bool, const void*) = nullptr;
  bool ok = false;
} api;

using Trap_t = void (*)(void*, void*, void*, const void*);
using ForceDoor_t = void (*)(void*, bool, float, bool, const void*);
Trap_t g_trap = nullptr;
ForceDoor_t g_forceDoor = nullptr;

struct State {
  il2cpp::Held ghost;
  bool yurei = false;
  Seal seal;
  il2cpp::Held sealedRoom;
  Hold hold;
  il2cpp::Held heldDoor;   // the door its ability slammed, locked
  bool inAbility = false;  // inside its ChangeState(15)
  il2cpp::Held abilityDoor;
  Draw draw;
  il2cpp::Held drawRoom;
  Steer steer;             // the door its next ability is aimed at
} g;

il2cpp::Array* doors_of(void* room) { return room && game::alive(room) ? il2cpp::field<il2cpp::Array*>(room, kRoomDoors) : nullptr; }

bool hinged(void* door) { return door && game::alive(door) && il2cpp::is_kind(door, "HingeDoor"); }

bool open(void* door) {
  void* body = il2cpp::field<void*>(door, kDoorBody);
  if (!body || !game::alive(body)) return false;
  const Vector3 e = api.localEuler(game::transform_of(body), nullptr);
  const int axis = il2cpp::field<int>(door, kHingeAxis);
  const float angle = axis == 1 ? e.x : axis == 4 ? e.z : e.y;
  return !custom::door_closed(angle, il2cpp::field<float>(door, kDoorClosedAngle));
}

/** Each hinged door of a room open or not, by its index in the room's Door[] (others count as shut). */
std::vector<bool> open_doors(il2cpp::Array* doors) {
  std::vector<bool> out;
  for (uintptr_t i = 0; doors && i < doors->max_length; ++i) {
    void* door = doors->data<void*>()[i];
    out.push_back(hinged(door) && open(door));
  }
  return out;
}

void shut(void* door) { api.forceDoor(door, false, -1.0f, true, nullptr); }

bool in_doors(il2cpp::Array* doors, void* door) {
  for (uintptr_t i = 0; doors && i < doors->max_length; ++i)
    if (doors->data<void*>()[i] == door) return true;
  return false;
}

/** A door a running hunt has locked and will unlock itself (LevelController.UnlockHuntingDoors). */
bool hunt_locks(void* door) {
  void* level = game::level_controller();
  if (!level || !game::alive(level)) return false;
  if (in_doors(il2cpp::field<il2cpp::Array*>(level, kLevelHuntDoors), door)) return true;
  auto* variations = il2cpp::field<il2cpp::Array*>(level, kLevelVariations);
  for (uintptr_t i = 0; variations && i < variations->max_length; ++i)
    if (void* v = variations->data<void*>()[i]; v && in_doors(il2cpp::field<il2cpp::Array*>(v, kVariationHuntDoors), door))
      return true;
  return false;
}

void* difficulty() {
  void* values = game::level_values();
  return values && game::alive(values) ? il2cpp::field<void*>(values, kValuesDifficulty) : nullptr;
}

void unlock_held() {
  void* door = g.heldDoor;
  g.heldDoor = nullptr;
  g.hold = Hold{};
  if (!door || !game::alive(door)) return;
  if (g.ghost && world::hunting(g.ghost) && hunt_locks(door)) return;  // the hunt unlocks it as it ends
  api.toggleLock(door, true, true, nullptr);
}

void detour_trap(void* sage, void* object, void* sensor, const void* method) {
  hooks::Guard guard;
  g_trap(sage, object, sensor, method);
  if (!tells_on() || world::behaviour_type(game::ghost_ai()) != kYurei) return;
  if (game::is_master()) {
    trapped();
  } else {
    const std::vector<uint8_t> m = net::trapped();
    pg::net::send(m.data(), m.size(), pg::net::To::Master, true);
  }
}

void detour_force_door(void* door, bool openIt, float strength, bool skipRattle, const void* method) {
  hooks::Guard guard;
  if (g.inAbility && !g.abilityDoor && door) g.abilityDoor = door;
  g_forceDoor(door, openIt, strength, skipRattle, method);
}

void around_state(void* ai, int state, bool before) {
  if (!g.yurei || ai != g.ghost) return;
  // The ability's entry changes the ghost to idle first; its door roll comes after that returns.
  if (state == kStateIdle && !before && g.inAbility) g.steer.idle_done();
  if (state != kStateAbility) return;
  if (before) {
    g.inAbility = true;
    g.abilityDoor = nullptr;
    g.steer.begin();
    return;
  }
  g.inAbility = false;
  g.steer.end();
  void* door = g.abilityDoor;
  g.abilityDoor = nullptr;
  void* d = difficulty();
  if (!hinged(door) || g.heldDoor || (d && il2cpp::field<bool>(d, kDifficultyDoorsStayOpen))) return;
  api.huntClose(door, nullptr);
  g.heldDoor = door;
  g.hold.lock();
  PG_LOG("yurei: the door its ability slammed is held locked");
}

bool steer_pick(int min, int max, int& result) { return g.steer.take(min, max, result); }

bool may_draw(void* ghost) {
  const int state = game::ghost_state(ghost);
  void* values = game::level_values();
  const bool eventsBlocked = values && game::alive(values) && il2cpp::field<bool>(values, kValuesEventsBlocked);
  return (state == kStateIdle || state == kStateWander || state == kStateFavouriteRoom) && !g.seal.active() &&
         !world::hunting(ghost) && !eventsBlocked;
}

}  // namespace

bool resolve() {
  api.forceDoor = reinterpret_cast<decltype(api.forceDoor)>(game::game_assembly_address(kForceDoorRva));
  api.huntClose = reinterpret_cast<decltype(api.huntClose)>(game::game_assembly_address(kHuntCloseRva));
  api.toggleLock = reinterpret_cast<decltype(api.toggleLock)>(game::game_assembly_address(kToggleLockRva));
  api.ok = il2cpp::bind(api.localEuler, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Transform"),
                        "get_localEulerAngles", 0) &&
           api.forceDoor && api.huntClose && api.toggleLock;
  gate::around_state(&around_state);
  rolls::add_roller(&steer_pick);
  PG_LOG("yurei: resolve %s", api.ok ? "ok" : "unavailable (it plays as in the game)");
  return true;  // optional: without it the Yurei is vanilla
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kTrapRva), reinterpret_cast<void*>(&detour_trap),
                       reinterpret_cast<void**>(&g_trap), "WhiteSage.YureiTrap") &&
         hooks::create(game::game_assembly_address(kForceDoorRva), reinterpret_cast<void*>(&detour_force_door),
                       reinterpret_cast<void**>(&g_forceDoor), "Door.GhostForceDoor");
}

void trapped() {
  if (!g.yurei || !g.ghost || !api.ok || g.seal.active()) return;
  g.seal.start();
  g.sealedRoom = world::ghost_room();
  PG_LOG("yurei: trapped by incense, it shuts itself in");
}

void tick_host(void* ghost, int type, float dt) {
  if (ghost != g.ghost || (type == kYurei) != g.yurei) {
    reset();
    g.ghost = ghost;
    g.yurei = type == kYurei;
  }
  if (!g.yurei || !api.ok) return;
  if (g.seal.active()) {
    il2cpp::Array* doors = doors_of(g.sealedRoom);
    for (int i : g.seal.step(open_doors(doors), dt)) shut(doors->data<void*>()[i]);
  }
  if (g.hold.step(dt)) unlock_held();
  // The room its ability takes a door of: the game's own idea of where the ghost is.
  void* room = world::ghost_room();
  if (room != g.drawRoom) {
    g.drawRoom = room;
    g.draw.reset();
  }
  il2cpp::Array* doors = doors_of(room);
  const int door = g.draw.step(open_doors(doors), may_draw(ghost), dt);
  if (door >= 0) {
    g.steer.aim(door, static_cast<int>(doors->max_length));
    PG_LOG("yurei: a door left open draws its ability");
    gate::change_state(ghost, kStateAbility);
    g.steer.end();  // whether or not the game ran the ability
  }
}

void reset() {
  if (g.heldDoor) unlock_held();
  g = State{};
}

}  // namespace pg::tells::yurei
