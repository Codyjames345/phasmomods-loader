#include "squatter_door.h"

#include <algorithm>
#include <type_traits>

#include "CursedItems.h"
#include "Door.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Player.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "squatter_difficulty.h"

namespace pm::custom::squatter::door {
namespace {

api::Held g_level;                // the LevelController whose exits are locked, or null
std::vector<api::Held> g_locked;  // its exits, as lock_all locked them
Attempt g_callback;
int g_openExitsWish = 0;  // MonkeyPaw.OpenExitsWish calls running on this thread

/** The current map's live LevelController, or null. */
void* level() {
  void* l = phasmo::LevelController::instance.get();
  return l && api::alive(l) ? l : nullptr;
}

/** Forgets a lock whose map is gone, so the next contract starts unlocked. */
void forget_old_level() {
  if (g_level && g_level.get() != level()) reset();
}

/** Adds the live doors of the Door[] `doors` to `out`, each once. */
void add_doors(void* doors, std::vector<void*>& out) {
  if (!doors) return;
  const api::RefArray list{doors};
  for (size_t i = 0; i < list.size(); ++i)
    if (void* d = list[i]; d && api::alive(d) && std::find(out.begin(), out.end(), d) == out.end()) out.push_back(d);
}

/** Whether `d` is one of the exits lock_all locked for the current map. */
bool locked_exit(void* d) {
  forget_old_level();
  return d && std::any_of(g_locked.begin(), g_locked.end(), [d](const api::Held& h) { return h.get() == d; });
}

/** Reports a try at a locked exit to the callback; nothing it throws reaches the game. */
void report(void* d) {
  if (!g_callback || !locked_exit(d)) return;
  try {
    g_callback(d);
  } catch (...) {
    PM_LOG("squatter: a door attempt's callback threw");
  }
}

void detour_attempt(api::Original<phasmo::Door::AttemptToUnlockDoor> original, void* d) {
  report(d);
  original(d);
}

void detour_grabbed(api::Original<phasmo::Door::GrabbedDoor> original, void* d) {
  report(d);
  original(d);
}

// The Monkey Paw never opens an exit the Squatter holds (the user's rule, 2026-11-08). Each detour refuses only the call
// that would undo the lock for a held exit, on every client that runs it, and lets every other call through; the rest of
// each wish stays the game's (sdk/systems/doors.md, "The Monkey Paw").

// The paw's timed lock, whose only callers are the wishes: it would clear the lock at once and reopen the door after.
void detour_lock_for_time(api::Original<phasmo::Door::LockDoorForTime> original, void* d, float seconds,
                          bool takeOwnership) {
  if (holds(d)) {
    PM_LOG("squatter: a Monkey Paw wish left a locked exit shut");
    return;
  }
  original(d, seconds, takeOwnership);
}

// Either way it clears the lock, and `true` switches the door back on: the timed lock's start and end, the wish that
// opens every exit, and a hunt's end, which the Squatter never has.
void detour_toggle_lock(api::Original<phasmo::Door::ToggleDoorLock> original, void* d, bool on, bool b) {
  if (holds(d)) return;
  original(d, on, b);
}

// Inside the wish that opens every exit only: the house key's unlock (Door.AttemptToUnlockDoor) stays the game's.
void detour_unlock(api::Original<phasmo::Door::UnlockDoor> original, void* d, bool sound) {
  if (g_openExitsWish > 0 && holds(d)) return;
  original(d, sound);
}

void detour_hunt_collider(api::Original<phasmo::Door::ForceToggleHuntCollider> original, void* d, bool on) {
  if (g_openExitsWish > 0 && holds(d)) return;
  original(d, on);
}

void detour_open_exits_wish(api::Original<phasmo::MonkeyPaw::OpenExitsWish> original, void* paw, void* wisher) {
  struct Running {
    Running() { ++g_openExitsWish; }
    ~Running() { --g_openExitsWish; }
  } running;
  if (locked()) PM_LOG("squatter: a Monkey Paw wish to open the exits leaves the locked exits shut");
  original(paw, wisher);
}

}  // namespace

std::vector<void*> exits() {
  std::vector<void*> out;
  void* l = level();
  if (!l) return out;
  add_doors(phasmo::LevelController::huntingDoors.get(l), out);
  if (void* variations = phasmo::LevelController::mapVariations.get(l)) {
    const api::RefArray list{variations};
    for (size_t i = 0; i < list.size(); ++i)
      if (void* v = list[i]) add_doors(phasmo::SmallMapVariation::huntingDoorsToLock.get(v), out);
  }
  return out;
}

int living_inside() {
  using Room = std::remove_cvref_t<decltype(phasmo::Player::currentRoom)>;
  using Dead = std::remove_cvref_t<decltype(phasmo::Player::isDead)>;
  using Kind = std::remove_cvref_t<decltype(phasmo::LevelRoom::kind)>;
  using Outside = std::remove_cvref_t<decltype(phasmo::RoomKind::outsideMap)>;
  // An unresolved field reads 0 or false and outsideMap's number 0, which would count a player wrongly: unknown.
  if (!Room::use::resolved() || !Dead::use::resolved() || !Kind::use::resolved() || !Outside::use::resolved())
    return -1;
  void* net = phasmo::Network::get_Instance();
  void* spots = net ? phasmo::Network::playerSpots.get(net) : nullptr;
  if (!spots) return -1;
  const api::List list{spots};
  if (list.size() == 0) return -1;
  const int32_t outside = phasmo::RoomKind::outsideMap();
  int inside = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    void* spot = list[i];
    void* player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (!player) return -1;
    if (phasmo::Player::isDead.get(player)) continue;
    void* room = phasmo::Player::currentRoom.get(player);
    if (!room) return -1;
    if (phasmo::LevelRoom::kind.get(room) != outside) ++inside;
  }
  return inside;
}

bool lock_all() {
  forget_old_level();
  if (g_level) return true;
  void* l = level();
  if (!l || friendly()) return false;  // an unreadable difficulty counts as friendly: never lock
  const std::vector<void*> doors = exits();
  if (doors.empty()) return false;
  // The game's own hunt lock, from the client a hunt calls it on: its RPCs refuse any sender but the room's first
  // actor (sdk/systems/doors.md, "Who may send what").
  if (phasmo::PhotonNetwork::get_IsMasterClient())
    for (void* d : doors) phasmo::Door::HuntingCloseDoor(d);
  g_level = l;
  g_locked.clear();
  for (void* d : doors) g_locked.emplace_back(d);
  PM_LOG("squatter: %zu exit(s) locked", doors.size());
  return true;
}

bool locked() {
  forget_old_level();
  return static_cast<bool>(g_level.get());
}

bool holds(void* door) { return locked_exit(door); }

void on_attempt(Attempt callback) { g_callback = std::move(callback); }

bool create_hooks() {
  return api::hook<phasmo::Door::AttemptToUnlockDoor>(&detour_attempt, "Door.AttemptToUnlockDoor",
                                                      api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::GrabbedDoor>(&detour_grabbed, "Door.GrabbedDoor", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::LockDoorForTime>(&detour_lock_for_time, "Door.LockDoorForTime",
                                                  api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::ToggleDoorLock>(&detour_toggle_lock, "Door.ToggleDoorLock", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::UnlockDoor>(&detour_unlock, "Door.UnlockDoor", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::ForceToggleHuntCollider>(&detour_hunt_collider, "Door.ForceToggleHuntCollider",
                                                          api::Twins::CanonicalOnly) &&
         api::hook<phasmo::MonkeyPaw::OpenExitsWish>(&detour_open_exits_wish, "MonkeyPaw.OpenExitsWish",
                                                     api::Twins::CanonicalOnly);
}

void reset() {
  g_locked.clear();
  g_level = nullptr;
}

}  // namespace pm::custom::squatter::door
