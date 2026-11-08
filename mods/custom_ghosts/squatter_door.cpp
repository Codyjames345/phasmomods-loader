#include "squatter_door.h"

#include <algorithm>
#include <optional>
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
#include "room_gates.h"
#include "runtime.h"
#include "squatter_difficulty.h"

namespace pm::custom::squatter::door {
namespace {

// An exit this client holds, and what the master's re-lock has read of it (relock).
struct HeldExit {
  api::Held door;
  bool seenLocked = false;             // the master read it locked since the hold or since its last re-lock
  std::optional<uint64_t> relockedAt;  // when the master last locked it again
};

api::Held g_level;               // the LevelController whose exits are held, or null
std::vector<HeldExit> g_locked;  // its exits, as they were when the hold began
Attempt g_callback;
int g_openExitsWish = 0;  // MonkeyPaw.OpenExitsWish calls running on this thread
bool g_haunting = false;  // set_haunting: the master's lock is the Squatter's

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
  return d && std::any_of(g_locked.begin(), g_locked.end(), [d](const HeldExit& e) { return e.door.get() == d; });
}

/** Holds `doors`, the exits of the map whose LevelController is `l`, on this client from now on. */
void hold(void* l, const std::vector<void*>& doors) {
  g_level = l;
  g_locked.clear();
  for (void* d : doors) {
    HeldExit e;
    e.door = d;
    g_locked.push_back(std::move(e));
  }
  PM_LOG("squatter: %zu exit(s) held", doors.size());
}

/**
 * Whether the game's own check of Door.HuntingCloseDoorNetworked's sender lets the call in `info` (a
 * PhotonMessageInfo) through: outside a room or offline every call, in a room only one from the room's first actor, the
 * lowest actor number of PhotonNetwork.get_PlayerList (sdk/systems/photon.md, "The game's checks on an RPC"). A list
 * the game would throw on, or no sender, lets nothing through.
 */
bool sent_by_first_actor(void* info) {
  if (!phasmo::PhotonNetwork::get_InRoom() || phasmo::PhotonNetwork::get_OfflineMode()) return true;
  std::vector<int> actors;
  if (!room_gates::room_actors(actors) || actors.empty()) return false;
  void* sender = info ? phasmo::PhotonMessageInfo::Sender.get(info) : nullptr;
  return sender && room_gates::actor_number(sender) == *std::min_element(actors.begin(), actors.end());
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

// The master's lock reaching this client: the game's own RPC body runs first, as it would, checks and all. Once it has
// taken a lock of an exit from the room's first actor while the Squatter haunts, this client holds every exit of the
// map, in the same call that brought the lock, so no Monkey Paw call on this client falls between the two. Nothing here
// changes what the body does or what it refuses.
void detour_hunting_close_networked(api::Original<phasmo::Door::HuntingCloseDoorNetworked> original, void* d,
                                    void* info) {
  original(d, info);
  forget_old_level();
  if (g_level || !g_haunting || friendly() || !sent_by_first_actor(info)) return;
  const std::vector<void*> doors = exits();
  if (std::find(doors.begin(), doors.end(), d) != doors.end()) hold(level(), doors);
}

// The Monkey Paw never opens an exit the Squatter holds (the user's rule, 2026-11-08). Each detour refuses only the
// call that would undo the lock for a held exit, on every client that runs it, and lets every other call through; the
// rest of each wish stays the game's (sdk/systems/doors.md, "The Monkey Paw").

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
  // Another client holds the exits from the master's word alone (detour_hunting_close_networked), never its own count.
  if (!phasmo::PhotonNetwork::get_IsMasterClient()) return false;
  void* l = level();
  if (!l || friendly()) return false;  // an unreadable difficulty counts as friendly: never lock
  const std::vector<void*> doors = exits();
  if (doors.empty()) return false;
  // The game's own hunt lock, from the client a hunt calls it on: its RPCs refuse any sender but the room's first
  // actor (sdk/systems/doors.md, "Who may send what").
  for (void* d : doors) phasmo::Door::HuntingCloseDoor(d);
  if (!g_level) hold(l, doors);  // the master's own RPC may have held them already
  return true;
}

bool locked() {
  forget_old_level();
  return static_cast<bool>(g_level.get());
}

bool holds(void* door) { return locked_exit(door); }

int relock(uint64_t now) {
  forget_old_level();
  if (!g_level || !phasmo::PhotonNetwork::get_IsMasterClient() || friendly()) return 0;
  int relocked = 0;
  for (HeldExit& e : g_locked) {
    void* d = e.door.get();
    if (!d || !api::alive(d)) continue;
    const bool lockedHere = phasmo::Door::locked.get(d);
    e.seenLocked = e.seenLocked || lockedHere;
    // Grabbable again, or unlocked after this client read it locked: a wish's unlock that raced the lock. An exit
    // another player's client owns never reads locked here, as the game's lock stays on its owner (doors.md).
    const bool reopened = phasmo::Door::grabbable.get(d) || (e.seenLocked && !lockedHere);
    if (!reopened || (e.relockedAt && now - *e.relockedAt < kRelockMs)) continue;
    phasmo::Door::HuntingCloseDoor(d);
    e.seenLocked = false;
    e.relockedAt = now;
    ++relocked;
  }
  if (relocked) PM_LOG("squatter: %d exit(s) a wish reopened locked again", relocked);
  return relocked;
}

void on_attempt(Attempt callback) { g_callback = std::move(callback); }

void set_haunting(bool on) { g_haunting = on; }

bool create_hooks() {
  constexpr api::Twins only = api::Twins::CanonicalOnly;
  return api::hook<phasmo::Door::AttemptToUnlockDoor>(&detour_attempt, "Door.AttemptToUnlockDoor", only) &&
         api::hook<phasmo::Door::GrabbedDoor>(&detour_grabbed, "Door.GrabbedDoor", only) &&
         api::hook<phasmo::Door::LockDoorForTime>(&detour_lock_for_time, "Door.LockDoorForTime", only) &&
         api::hook<phasmo::Door::ToggleDoorLock>(&detour_toggle_lock, "Door.ToggleDoorLock", only) &&
         api::hook<phasmo::Door::UnlockDoor>(&detour_unlock, "Door.UnlockDoor", only) &&
         api::hook<phasmo::Door::ForceToggleHuntCollider>(&detour_hunt_collider, "Door.ForceToggleHuntCollider",
                                                          only) &&
         api::hook<phasmo::MonkeyPaw::OpenExitsWish>(&detour_open_exits_wish, "MonkeyPaw.OpenExitsWish", only) &&
         api::hook<phasmo::Door::HuntingCloseDoorNetworked>(&detour_hunting_close_networked,
                                                            "Door.HuntingCloseDoorNetworked", only);
}

void reset() {
  g_locked.clear();
  g_level = nullptr;
}

}  // namespace pm::custom::squatter::door
