// Offline tests for the Squatter's way out (mods/custom_ghosts/squatter_door.cpp) over the fake backend: the map's
// exits read from LevelController's lists, the living players still within them, the lock through the game's own hunt
// lock (Door.HuntingCloseDoor, on the master only), and a player's try at a locked exit (Door.AttemptToUnlockDoor and
// Door.GrabbedDoor). The facts the fakes stand for are in sdk/systems/doors.md. Run: build\squatter_door_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Difficulty.h"
#include "Door.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"
#include "squatter_door.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};  // the count of hooked bodies running, which hooks.cpp keeps in the game
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace door = pm::custom::squatter::door;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};

// Where the fields lie in the fake objects.
constexpr int32_t kLevelHuntingDoors = 0x08, kLevelVariations = 0x10;
constexpr int32_t kVariationDoors = 0x08;
constexpr int32_t kNetworkSpots = 0x08, kSpotPlayer = 0x08;
constexpr int32_t kPlayerDead = 0x08, kPlayerRoom = 0x10;
constexpr int32_t kRoomKind = 0x08;
constexpr int32_t kValuesDifficulty = 0x08, kDifficultyFriendly = 0x08;
// RoomKind's numbers in build 25811160: insideRoom 0, outsideMap 1, exterior 2, exteriorRoom 3.
constexpr int32_t kInside = 0, kOutsideMap = 1, kExterior = 2, kExteriorRoom = 3;

template <class T>
T& at(Obj& o, int32_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&o) + offset);
}

Obj g_level, g_net, g_values, g_difficulty, g_variation, g_front, g_back, g_gateLeft, g_gateRight, g_bedroom;
Obj g_hall, g_garden, g_campfire, g_outside;
Obj g_spots[4], g_players[4];
void* g_levelRef = nullptr;   // LevelController.instance
void* g_valuesRef = nullptr;  // LevelValues.instance
void* g_netRef = nullptr;     // what Network.get_Instance answers
bool g_master = true;

// What the game's door methods were asked, in order.
std::vector<std::string> g_calls;
std::vector<void*> g_closed;  // the doors HuntingCloseDoor was called for
int g_attemptBodies = 0, g_grabBodies = 0;
std::vector<void*> g_reported;  // the doors on_attempt reported

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

// The game's bodies.
void fake_hunting_close(void* d, const void*) {
  g_calls.push_back("HuntingCloseDoor");
  g_closed.push_back(d);
}
void fake_toggle_lock(void*, bool, bool, const void*) { g_calls.push_back("ToggleDoorLock"); }
void fake_lock(void*, bool, const void*) { g_calls.push_back("LockDoor"); }
void fake_unlock(void*, bool, const void*) { g_calls.push_back("UnlockDoor"); }
void fake_disable(void*, bool, bool, const void*) { g_calls.push_back("DisableOrEnableDoor"); }
void fake_attempt(void*, const void*) { ++g_attemptBodies; }
void fake_grabbed(void*, const void*) { ++g_grabBodies; }
bool fake_is_master(const void*) { return g_master; }
void* fake_network(const void*) { return g_netRef; }

// A player's try at `d` through the hooked game methods.
void attempt(void* d) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("Door.AttemptToUnlockDoor"))(d, nullptr);
}
void grab(void* d) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("Door.GrabbedDoor"))(d, nullptr);
}

void* refs(const std::vector<void*>& items) {
  void* arr = fake::array(sizeof(void*), items.size());
  for (size_t i = 0; i < items.size(); ++i) pm::api::RefArray{arr}.set(i, items[i]);
  return arr;
}

// The map's exits: `doors` in LevelController.huntingDoors, `variationDoors` in its one variation's list.
void exits_are(const std::vector<void*>& doors, const std::vector<void*>& variationDoors = {}) {
  at<void*>(g_level, kLevelHuntingDoors) = refs(doors);
  at<void*>(g_variation, kVariationDoors) = refs(variationDoors);
  at<void*>(g_level, kLevelVariations) = refs({&g_variation});
}

// The room's players: one per room given, `dead` the indexes of the dead; a null room is a player in none.
void players_in(const std::vector<Obj*>& rooms, const std::vector<int>& dead = {}) {
  std::vector<void*> spots;
  for (size_t i = 0; i < rooms.size(); ++i) {
    g_players[i] = g_spots[i] = Obj{};
    at<void*>(g_players[i], kPlayerRoom) = rooms[i];
    at<void*>(g_spots[i], kSpotPlayer) = &g_players[i];
    spots.push_back(&g_spots[i]);
  }
  for (const int i : dead) at<bool>(g_players[i], kPlayerDead) = true;
  at<void*>(g_net, kNetworkSpots) = fake::list(spots);
}

// One contract on a house with a front door, on the master unless `master` is false, the hooks made.
void start(bool master = true, bool friendly = false) {
  door::reset();  // what it holds goes back before the fake's own storage does
  fake::reset();
  g_levelRef = &g_level;
  g_valuesRef = &g_values;
  g_netRef = &g_net;
  g_level = g_net = g_values = g_difficulty = g_variation = Obj{};
  g_bedroom = g_hall = g_garden = g_campfire = g_outside = Obj{};
  at<int32_t>(g_bedroom, kRoomKind) = kInside;
  at<int32_t>(g_hall, kRoomKind) = kInside;
  at<int32_t>(g_garden, kRoomKind) = kExteriorRoom;
  at<int32_t>(g_campfire, kRoomKind) = kExterior;
  at<int32_t>(g_outside, kRoomKind) = kOutsideMap;
  at<void*>(g_values, kValuesDifficulty) = &g_difficulty;
  at<bool>(g_difficulty, kDifficultyFriendly) = friendly;
  fake::static_field(map::LevelController_instance, &g_levelRef, sizeof g_levelRef, true);
  fake::field(map::LevelController_huntingDoors, kLevelHuntingDoors);
  fake::field(map::LevelController_mapVariations, kLevelVariations);
  fake::field(map::SmallMapVariation_huntingDoorsToLock, kVariationDoors);
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_playerSpots, kNetworkSpots);
  fake::field(map::PlayerSpot_player, kSpotPlayer);
  fake::field(map::Player_isDead, kPlayerDead);
  fake::field(map::Player_currentRoom, kPlayerRoom);
  fake::field(map::LevelRoom_kind, kRoomKind);
  fake::value(map::RoomKind_outsideMap, kOutsideMap);
  fake::static_field(map::LevelValues_instance, &g_valuesRef, sizeof g_valuesRef, true);
  fake::field(map::LevelValues_difficulty, kValuesDifficulty);
  fake::field(map::Difficulty_friendlyGhost, kDifficultyFriendly);
  fake::method(map::PhotonNetwork_get_IsMasterClient, reinterpret_cast<void*>(&fake_is_master));
  fake::method(map::Door_HuntingCloseDoor, reinterpret_cast<void*>(&fake_hunting_close));
  fake::method(map::Door_ToggleDoorLock, reinterpret_cast<void*>(&fake_toggle_lock));
  fake::method(map::Door_LockDoor, reinterpret_cast<void*>(&fake_lock));
  fake::method(map::Door_UnlockDoor, reinterpret_cast<void*>(&fake_unlock));
  fake::method(map::Door_DisableOrEnableDoor, reinterpret_cast<void*>(&fake_disable));
  fake::method(map::Door_AttemptToUnlockDoor, reinterpret_cast<void*>(&fake_attempt));
  fake::method(map::Door_GrabbedDoor, reinterpret_cast<void*>(&fake_grabbed));
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  CHECK(pm::api::bind(&g_host, missing));
  CHECK(door::create_hooks());
  g_master = master;
  g_calls.clear();
  g_closed.clear();
  g_attemptBodies = g_grabBodies = 0;
  g_reported.clear();
  door::on_attempt([](void* d) { g_reported.push_back(d); });
  exits_are({&g_front});
  players_in({&g_bedroom});
}

int count(const std::vector<void*>& list, void* item) {
  int n = 0;
  for (void* each : list) n += each == item;
  return n;
}

// Every exit of the map, from LevelController.huntingDoors and its variations' lists, locks once: a door in both
// lists once, and a second call locks nothing more.
void locks_every_exit_once() {
  start();
  exits_are({&g_front, &g_back}, {&g_back});
  const std::vector<void*> exits = door::exits();
  CHECK(exits.size() == 2 && count(exits, &g_front) == 1 && count(exits, &g_back) == 1);
  CHECK(door::lock_all());
  CHECK(g_closed.size() == 2 && count(g_closed, &g_front) == 1 && count(g_closed, &g_back) == 1);
  CHECK(door::locked());
  CHECK(door::lock_all());
  CHECK(g_closed.size() == 2);
  // A new contract's level forgets the last one's lock.
  static Obj nextLevel;
  nextLevel = Obj{};
  at<void*>(nextLevel, kLevelHuntingDoors) = refs({&g_front});
  at<void*>(nextLevel, kLevelVariations) = refs({});
  g_levelRef = &nextLevel;
  CHECK(!door::locked());
  CHECK(door::lock_all() && g_closed.size() == 3);
}

// Only living players within the exits count: a dead one inside and a living one in the outside room do not, and
// a room within the exits counts whatever its kind.
void counts_only_living_inside() {
  start();
  players_in({&g_bedroom, &g_outside, &g_hall}, {2});
  CHECK(door::living_inside() == 1);
  players_in({&g_garden, &g_campfire, &g_outside});
  CHECK(door::living_inside() == 2);
  players_in({&g_outside, &g_outside});
  CHECK(door::living_inside() == 0);
  players_in({&g_outside, &g_bedroom}, {1});
  CHECK(door::living_inside() == 0);
}

// A count it cannot read reliably is -1, never 0: no network, no spots, a spot with no player, a living player with no
// room, or a field the build lacks. A dead player's room is never read.
void a_failed_count_reads_as_unknown() {
  start();
  g_netRef = nullptr;
  CHECK(door::living_inside() == -1);
  start();
  at<void*>(g_net, kNetworkSpots) = nullptr;
  CHECK(door::living_inside() == -1);
  start();
  at<void*>(g_net, kNetworkSpots) = fake::list({});
  CHECK(door::living_inside() == -1);
  start();
  players_in({&g_outside, &g_outside});
  at<void*>(g_spots[1], kSpotPlayer) = nullptr;
  CHECK(door::living_inside() == -1);
  start();
  players_in({&g_outside, nullptr});
  CHECK(door::living_inside() == -1);
  players_in({&g_outside, nullptr}, {1});
  CHECK(door::living_inside() == 0);
  for (const pm::sdk::EntryDesc* lacking : {&map::Player_currentRoom, &map::Player_isDead, &map::LevelRoom_kind,
                                            &map::RoomKind_outsideMap}) {
    start();
    players_in({&g_outside});
    fake::forget(*lacking);
    std::string missing;
    pm::api::bind(&g_host, missing);
    CHECK(door::living_inside() == -1);
  }
}

// A map without a house (a campsite) still locks its way out: both leaves of its gate are exits, and a player at a
// campfire or in a tent is within them.
void a_map_with_no_house_still_locks_its_door() {
  start();
  exits_are({&g_gateLeft, &g_gateRight});
  players_in({&g_campfire, &g_garden});
  CHECK(door::living_inside() == 2);
  players_in({&g_outside, &g_outside});
  CHECK(door::living_inside() == 0);
  CHECK(door::lock_all());
  CHECK(g_closed.size() == 2 && count(g_closed, &g_gateLeft) == 1 && count(g_closed, &g_gateRight) == 1);
}

// A player's try at a locked exit, through either of the game's routes, is reported with its door on the trying
// player's client, master or not, and the game's own try still runs.
void an_attempt_on_a_locked_door_reports_it() {
  for (const bool master : {true, false}) {
    start(master);
    exits_are({&g_front, &g_back});
    CHECK(door::lock_all());
    attempt(&g_front);
    CHECK(g_reported.size() == 1 && g_reported[0] == &g_front && g_attemptBodies == 1);
    grab(&g_back);
    CHECK(g_reported.size() == 2 && g_reported[1] == &g_back && g_grabBodies == 1);
  }
}

// A try at a door it has not locked reports nothing: any door before the lock, and a door that is no exit after it.
// The game's own try runs either way.
void an_attempt_on_an_unlocked_door_reports_nothing() {
  start();
  attempt(&g_front);
  grab(&g_front);
  CHECK(g_reported.empty() && g_attemptBodies == 1 && g_grabBodies == 1);
  CHECK(door::lock_all());
  attempt(&g_bedroom);
  grab(&g_bedroom);
  CHECK(g_reported.empty() && g_attemptBodies == 2 && g_grabBodies == 2);
  // After the level is gone, its exits are no longer locked.
  door::reset();
  attempt(&g_front);
  CHECK(g_reported.empty());
}

// The lock is the game's own: Door.HuntingCloseDoor on the master and nothing else, which sends what the game sends.
// Another client calls nothing in the game but knows its exits are locked. Nothing locks on a friendly ghost's
// difficulty, or when the difficulty cannot be read, or with no exits.
void locking_takes_the_games_own_path() {
  start();
  CHECK(door::lock_all());
  CHECK(g_calls == std::vector<std::string>{"HuntingCloseDoor"});
  start(false);
  CHECK(door::lock_all() && g_calls.empty() && door::locked());
  start(true, true);
  CHECK(!door::lock_all() && g_calls.empty() && !door::locked());
  start();
  g_valuesRef = nullptr;
  CHECK(!door::lock_all() && g_calls.empty() && !door::locked());
  start();
  exits_are({});
  CHECK(!door::lock_all() && g_calls.empty() && !door::locked());
  start();
  g_levelRef = nullptr;
  CHECK(door::exits().empty() && !door::lock_all() && g_calls.empty());
}

// A build that lacks an entry the doors need fails the mod's binding, naming it, as Custom Ghosts' start does.
void a_missing_entry_fails_the_start() {
  for (const pm::sdk::EntryDesc* lacking : {&map::Door_HuntingCloseDoor, &map::Door_AttemptToUnlockDoor,
                                            &map::Door_GrabbedDoor, &map::LevelController_huntingDoors,
                                            &map::SmallMapVariation_huntingDoorsToLock, &map::RoomKind_outsideMap}) {
    start();
    fake::forget(*lacking);
    std::string missing;
    CHECK(!pm::api::bind(&g_host, missing) && missing.find(lacking->key) != std::string::npos);
  }
}

}  // namespace

int main() {
  locks_every_exit_once();
  counts_only_living_inside();
  a_failed_count_reads_as_unknown();
  a_map_with_no_house_still_locks_its_door();
  an_attempt_on_a_locked_door_reports_it();
  an_attempt_on_an_unlocked_door_reports_nothing();
  locking_takes_the_games_own_path();
  a_missing_entry_fails_the_start();
  door::reset();  // what it holds goes back before the fake's own storage does
  std::printf(g_failures ? "%d check(s) failed\n" : "all squatter door tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
