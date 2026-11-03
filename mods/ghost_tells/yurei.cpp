#include "yurei.h"

#include <vector>

#include "Difficulty.h"
#include "Door.h"
#include "Equipment.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "UnityObject.h"
#include "asterion_rule.h"
#include "game_api.h"
#include "gate.h"
#include "held.h"
#include "log.h"
#include "net.h"
#include "rolls.h"
#include "runtime.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"
#include "yurei_rule.h"

namespace pm::tells::yurei {
namespace {

using phasmo::Door;
using world::Vector3;

struct State {
  api::Held ghost;
  Seal seal;
  api::Held sealedRoom;
  Hold hold;
  api::Held heldDoor;      // the door its ability slammed, locked
  bool inAbility = false;  // inside its ChangeState(GhostAbility)
  api::Held abilityDoor;
  Draw draw;
  api::Held drawRoom;
  Steer steer;             // the door its next ability is aimed at
} g;

void* doors_of(void* room) { return api::alive(room) ? phasmo::LevelRoom::doors.get(room) : nullptr; }

bool hinged(void* door) { return api::alive(door) && phasmo::HingeDoor::type.is(door); }

bool open(void* door) {
  void* body = Door::body.get(door);
  void* t = api::alive(body) ? phasmo::Component::get_transform(body) : nullptr;
  if (!t) return false;
  const Vector3 e = phasmo::Transform::get_localEulerAngles(t);
  const int axis = phasmo::HingeDoor::hingeAxis.get(door);
  // The leaf swings about the hinge's axis: x, z, or else y.
  const float angle = axis == phasmo::Axis::X() ? e.x : axis == phasmo::Axis::Z() ? e.z : e.y;
  return !custom::door_closed(angle, Door::closedAngle.get(door));
}

/** Each hinged door of a room open or not, by its index in the room's Door[] (others count as shut). */
std::vector<bool> open_doors(void* doors) {
  std::vector<bool> out;
  const api::RefArray all{doors};
  for (size_t i = 0; i < all.size(); ++i) {
    void* door = all[i];
    out.push_back(hinged(door) && open(door));
  }
  return out;
}

void shut(void* door) { Door::GhostForceDoor(door, false, -1.0f, true); }

bool in_doors(void* doors, void* door) {
  const api::RefArray all{doors};
  for (size_t i = 0; i < all.size(); ++i)
    if (all[i] == door) return true;
  return false;
}

/** A door a running hunt has locked and will unlock itself (LevelController.UnlockHuntingDoors). */
bool hunt_locks(void* door) {
  void* level = phasmo::LevelController::instance.get();
  if (!api::alive(level)) return false;
  if (in_doors(phasmo::LevelController::huntingDoors.get(level), door)) return true;
  const api::RefArray variations{phasmo::LevelController::mapVariations.get(level)};
  for (size_t i = 0; i < variations.size(); ++i)
    if (void* v = variations[i]; v && in_doors(phasmo::SmallMapVariation::huntingDoorsToLock.get(v), door))
      return true;
  return false;
}

void* difficulty() {
  void* values = phasmo::LevelValues::get_Instance();
  return api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
}

void unlock_held() {
  void* door = g.heldDoor.get();
  g.heldDoor = nullptr;
  g.hold = Hold{};
  if (!api::alive(door)) return;
  if (g.ghost.get() && world::hunting(g.ghost.get()) && hunt_locks(door)) return;  // the hunt unlocks it as it ends
  Door::ToggleDoorLock(door, true, true);
}

void detour_trap(api::Original<phasmo::WhiteSage::YureiTrap> original, void* sage, void* object, void* sensor) {
  original(sage, object, sensor);
  if (told_now() != kYurei) return;
  if (world::is_master()) {
    trapped();
  } else {
    const std::vector<uint8_t> m = net::trapped();
    pm::net::send(m.data(), m.size(), pm::net::To::Master, true);
  }
}

void detour_force_door(api::Original<Door::GhostForceDoor> original, void* door, bool openIt, float strength,
                       bool skipRattle) {
  if (g.inAbility && !g.abilityDoor.get() && door) g.abilityDoor = door;
  original(door, openIt, strength, skipRattle);
}

void around_state(void* ai, int state, bool before) {
  if (told_now() != kYurei || ai != g.ghost.get()) return;
  // The ability's entry changes the ghost to idle first; its door roll comes after that returns.
  if (state == phasmo::GhostState::idle() && !before && g.inAbility) g.steer.idle_done();
  if (state != phasmo::GhostState::GhostAbility()) return;
  if (before) {
    g.inAbility = true;
    g.abilityDoor = nullptr;
    g.steer.begin();
    return;
  }
  g.inAbility = false;
  g.steer.end();
  void* door = g.abilityDoor.get();
  g.abilityDoor = nullptr;
  void* d = difficulty();
  // Door.HuntingCloseDoor itself does nothing on a friendly ghost's difficulty.
  if (!hinged(door) || g.heldDoor.get() || (d && phasmo::Difficulty::friendlyGhost.get(d))) return;
  Door::HuntingCloseDoor(door);
  g.heldDoor = door;
  g.hold.lock();
  PM_LOG("yurei: the door its ability slammed is held locked");
}

bool steer_pick(int min, int max, int& result) { return g.steer.take(min, max, result); }

bool may_draw(void* ghost) {
  const int state = world::state(ghost);
  void* values = phasmo::LevelValues::get_Instance();
  const bool eventsBlocked = api::alive(values) && phasmo::LevelValues::eventsBlocked.get(values);
  return (state == phasmo::GhostState::idle() || state == phasmo::GhostState::wander() ||
          state == phasmo::GhostState::favouriteRoom()) &&
         !g.seal.active() && !world::hunting(ghost) && !eventsBlocked;
}

}  // namespace

void resolve() {
  gate::around_state(&around_state);
  rolls::add_roller(&steer_pick);
}

bool create_hooks() {
  return api::hook<phasmo::WhiteSage::YureiTrap>(&detour_trap, "WhiteSage.YureiTrap", api::Twins::CanonicalOnly) &&
         api::hook<Door::GhostForceDoor>(&detour_force_door, "Door.GhostForceDoor", api::Twins::CanonicalOnly);
}

void trapped() {
  if (told_now() != kYurei || !g.ghost.get() || g.seal.active()) return;
  g.seal.start();
  g.sealedRoom = world::ghost_room();
  PM_LOG("yurei: trapped by incense, it shuts itself in");
}

void tick_host(void* ghost, float dt) {
  if (ghost != g.ghost.get()) {
    reset();
    g.ghost = ghost;
  }
  if (g.seal.active()) {
    void* doors = doors_of(g.sealedRoom.get());
    const api::RefArray all{doors};
    for (int i : g.seal.step(open_doors(doors), dt)) shut(all[static_cast<size_t>(i)]);
  }
  if (g.hold.step(dt)) unlock_held();
  // The room its ability takes a door of: the game's own idea of where the ghost is.
  void* room = world::ghost_room();
  if (room != g.drawRoom.get()) {
    g.drawRoom = room;
    g.draw.reset();
  }
  void* doors = doors_of(room);
  const int door = g.draw.step(open_doors(doors), may_draw(ghost), dt);
  if (door >= 0) {
    g.steer.aim(door, static_cast<int>(api::RefArray{doors}.size()));
    PM_LOG("yurei: a door left open draws its ability");
    gate::change_state(ghost, phasmo::GhostState::GhostAbility());
    g.steer.end();  // whether or not the game ran the ability
  }
}

void reset() {
  if (g.heldDoor.get()) unlock_held();
  g = State{};
}

}  // namespace pm::tells::yurei
