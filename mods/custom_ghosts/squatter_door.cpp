#include "squatter_door.h"

#include <algorithm>
#include <type_traits>

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

void on_attempt(Attempt callback) { g_callback = std::move(callback); }

bool create_hooks() {
  return api::hook<phasmo::Door::AttemptToUnlockDoor>(&detour_attempt, "Door.AttemptToUnlockDoor",
                                                      api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::GrabbedDoor>(&detour_grabbed, "Door.GrabbedDoor", api::Twins::CanonicalOnly);
}

void reset() {
  g_locked.clear();
  g_level = nullptr;
}

}  // namespace pm::custom::squatter::door
