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
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "CursedItems.h"
#include "Photon.h"
#include "Player.h"
#include "Props.h"
#include "check.h"
#include "UnityObject.h"
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
constexpr int32_t kLevelHuntingDoors = 0x08, kLevelVariations = 0x10, kLevelGhost = 0x18;
constexpr int32_t kGhostHunting = 0x08;  // GhostAI.isHunting
constexpr int32_t kVariationDoors = 0x08;
constexpr int32_t kNetworkSpots = 0x08, kSpotPlayer = 0x08;
constexpr int32_t kPlayerDead = 0x08, kPlayerRoom = 0x10;
constexpr int32_t kRoomKind = 0x08;
constexpr int32_t kValuesDifficulty = 0x08, kDifficultyFriendly = 0x08;
constexpr int32_t kInfoSender = 0x08, kActorNumber = 0x08;
constexpr int32_t kDoorGrabbable = 0x08, kDoorLocked = 0x09;  // Door.grabbable and Door.locked
constexpr int32_t kDoorKind = 0x0C, kDoorInteract = 0x10, kDoorView = 0x18;  // the quiet opener's
constexpr int32_t kDoorAxis = 0x20, kDoorShutAngle = 0x24;  // HingeDoor.hingeAxis and Door.closedAngle
constexpr int32_t kDoorBody = 0x28, kDoorOpenAngle = 0x30;  // Door.body (none here) and HingeDoor.openAngle
constexpr int32_t kDoorForce = 0x34, kDoorReversed = 0x35;  // Door.useForceInsteadOfAnimations, HingeDoor.openAngleReversed
constexpr int32_t kDoorClosed = 0x36;  // Door.closed
constexpr int32_t kAxisX = 1, kAxisY = 2, kAxisZ = 4;  // made-up as the game's Axis flags read: the code reads them
std::map<void*, phasmo::Vector3> g_euler;  // each door leaf's Transform.localEulerAngles
constexpr int32_t kInteractInUse = 0x08, kViewMine = 0x08;  // PhotonObjectInteract.inUse, PhotonView.IsMine
// Made-up numbers for DoorKind's None, Main and Secondary: the code reads None's from the map.
constexpr int32_t kKindNone = 20, kKindMain = 21, kKindSecondary = 22;
// Made-up numbers for RoomKind's insideRoom, outsideMap, exterior and exteriorRoom: the code reads outsideMap's from
// the map, so none is the game's.
constexpr int32_t kInside = 10, kOutsideMap = 11, kExterior = 12, kExteriorRoom = 13;

template <class T>
T& at(Obj& o, int32_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&o) + offset);
}

Obj g_level, g_net, g_values, g_difficulty, g_variation, g_front, g_back, g_gateLeft, g_gateRight, g_bedroom;
Obj g_hall, g_garden, g_campfire, g_outside;
Obj g_ghost;  // the contract's ghost, LevelController.currentGhost
Obj g_spots[4], g_players[4];
Obj g_photon[3];  // the room's Photon players, actors 1 (the first, the master's), 2 and 3
void* g_levelRef = nullptr;   // LevelController.instance
void* g_valuesRef = nullptr;  // LevelValues.instance
void* g_netRef = nullptr;     // what Network.get_Instance answers
bool g_master = true;
bool g_inRoom = true, g_offline = false;
void* g_playerList = nullptr;  // what PhotonNetwork.get_PlayerList answers

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
bool g_closeSwitchesOff = true;  // the hunt lock's switch-off reaches this client at once
bool g_lockRunsHere = false;     // Photon runs the lock's RPC on its sender within the call
void lock_arrives(void* d, int sender = 1);
void fake_hunting_close(void* d, const void*) {
  g_calls.push_back("HuntingCloseDoor");
  g_closed.push_back(d);
  // The game's switch-off (Door.DisableOrEnableDoor's RPC and the lock's own), which Photon runs on its sender too.
  if (g_closeSwitchesOff) at<bool>(*static_cast<Obj*>(d), kDoorGrabbable) = false;
  if (g_lockRunsHere) lock_arrives(d);
}
void fake_toggle_lock(void*, bool, bool, const void*) { g_calls.push_back("ToggleDoorLock"); }
void fake_unlock_hunting_doors(void*, const void*) { g_calls.push_back("UnlockHuntingDoors"); }
void fake_lock(void*, bool, const void*) { g_calls.push_back("LockDoor"); }
void fake_unlock(void*, bool, const void*) { g_calls.push_back("UnlockDoor"); }
void fake_disable(void*, bool, bool, const void*) { g_calls.push_back("DisableOrEnableDoor"); }
void fake_lock_for_time(void*, float, bool, const void*) { g_calls.push_back("LockDoorForTime"); }
void fake_hunt_collider(void*, bool, const void*) { g_calls.push_back("ForceToggleHuntCollider"); }
std::vector<void*> g_wishExits;  // the exits MonkeyPaw.OpenExitsWish walks
int g_wishBodies = 0;
void fake_attempt(void*, const void*) { ++g_attemptBodies; }
void fake_grabbed(void*, const void*) { ++g_grabBodies; }
bool fake_is_master(const void*) { return g_master; }
void* fake_network(const void*) { return g_netRef; }
bool fake_in_room(const void*) { return g_inRoom; }
bool fake_offline(const void*) { return g_offline; }
void* fake_player_list(const void*) { return g_playerList; }
void* g_localPlayer = nullptr;  // what PhotonNetwork.get_LocalPlayer answers
void* fake_local_player(const void*) { return g_localPlayer; }
void* fake_none(const void*) { return nullptr; }
int32_t fake_actor_number(void* p, const void*);
// The hunt close's RPC body as it arrives on this client: the game's own (here, a record of it) runs whatever it then
// refuses.
int g_networkedBodies = 0;
void fake_hunting_close_networked(void*, void*, const void*) {
  ++g_networkedBodies;
  g_calls.push_back("HuntingCloseDoorNetworked");
}
bool g_throwInWish = false;

// A door's Transform is the door itself here, and its euler angles are the test's.
void* fake_own_transform(void* component, const void*) { return component; }
phasmo::Vector3 fake_euler(void* t, const void*) { return g_euler[t]; }
// The leaf's place and its collider, which only the swing's side reads (leaf_of): no collider under any door here.
char g_colliderClass;
void* fake_no_child(void*, void*, bool, const void*) { return nullptr; }
phasmo::Bounds fake_bounds(void*, const void*) { return {}; }
phasmo::Vector3 fake_position(void*, const void*) { return {}; }
phasmo::Quaternion fake_rotation(void*, const void*) { return {0, 0, 0, 1}; }

// The quiet opener's game side: HingeDoor and SlideDoor classes, the opener, and the door's view.
char g_hingeClass, g_slideClass;
struct OpenCall {
  void* door;
  bool close;      // HingeDoor.SwingDoor's first argument: shut it rather than open it
  float seconds;   // how long the swing takes
  float fraction;  // how much of the way to its open angle
  bool buffered;   // whether its RPC is buffered
};
std::vector<OpenCall> g_opens;     // HingeDoor.SwingDoor's calls
std::vector<void*> g_viewsAsked;   // PhotonView.RequestOwnership's calls
int g_pushes = 0;                  // HingeDoor.OpenDoor's calls: the physics push
std::vector<OpenCall> g_pushed;    // and what each pushed: its door, its shut flag and its strength (in seconds)
void fake_swing(void* d, bool close, float seconds, float fraction, bool buffered, const void*) {
  g_calls.push_back("SwingDoor");
  g_opens.push_back({d, close, seconds, fraction, buffered});
}
void fake_push(void* d, bool close, float strength, bool buffered, const void*) {
  ++g_pushes;
  g_calls.push_back("OpenDoor");
  g_pushed.push_back({d, close, strength, 0.0f, buffered});
}
bool fake_is_mine(void* view, const void*) { return view && at<bool>(*static_cast<Obj*>(view), kViewMine); }
void fake_request_ownership(void* view, const void*) {
  g_calls.push_back("RequestOwnership");
  g_viewsAsked.push_back(view);
}

// A player's try at `d` through the hooked game methods.
void attempt(void* d) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("Door.AttemptToUnlockDoor"))(d, nullptr);
}
void grab(void* d) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("Door.GrabbedDoor"))(d, nullptr);
}

// The game's door calls, through whatever hook the code under test made on each, else straight to the game's body.
void* entry_of(const char* key, void* body) { return g_detour.count(key) ? g_detour.at(key) : body; }
void unlock(void* d) {
  using F = void (*)(void*, bool, const void*);
  reinterpret_cast<F>(entry_of("Door.UnlockDoor", reinterpret_cast<void*>(&fake_unlock)))(d, true, nullptr);
}
void toggle_lock(void* d, bool on) {
  using F = void (*)(void*, bool, bool, const void*);
  void* body = reinterpret_cast<void*>(&fake_toggle_lock);
  reinterpret_cast<F>(entry_of("Door.ToggleDoorLock", body))(d, on, false, nullptr);
}
void lock_for_time(void* d) {
  using F = void (*)(void*, float, bool, const void*);
  reinterpret_cast<F>(entry_of("Door.LockDoorForTime", reinterpret_cast<void*>(&fake_lock_for_time)))(d, 6.0f, true,
                                                                                                     nullptr);
}
void hunt_collider(void* d) {
  using F = void (*)(void*, bool, const void*);
  reinterpret_cast<F>(entry_of("Door.ForceToggleHuntCollider", reinterpret_cast<void*>(&fake_hunt_collider)))(
      d, false, nullptr);
}
// MonkeyPaw.OpenExitsWish as the game runs it (sdk/map/CursedItems.h): for every exit, on the master the unlock with
// its click, on the view's owner the door back on, and the hunt collider off; here this client is both.
void fake_open_exits_wish(void*, void*, const void*) {
  ++g_wishBodies;
  for (void* d : g_wishExits) {
    unlock(d);
    toggle_lock(d, true);
    hunt_collider(d);
  }
  if (g_throwInWish) throw 1;  // a body that ends in an exception
}
void open_exits_wish() {
  using F = void (*)(void*, void*, const void*);
  static int paw = 0, wisher = 0;
  reinterpret_cast<F>(entry_of("MonkeyPaw.OpenExitsWish", reinterpret_cast<void*>(&fake_open_exits_wish)))(
      &paw, &wisher, nullptr);
}

void* refs(const std::vector<void*>& items) {
  void* arr = fake::array(sizeof(void*), items.size());
  for (size_t i = 0; i < items.size(); ++i) pm::api::RefArray{arr}.set(i, items[i]);
  return arr;
}

int32_t fake_actor_number(void* p, const void*) { return at<int32_t>(*static_cast<Obj*>(p), kActorNumber); }

// The master's hunt close for `d` arriving on this client, from actor `sender` (0: no sender), through whatever hook
// the code under test made on the RPC's body.
void lock_arrives(void* d, int sender) {
  Obj info;
  at<void*>(info, kInfoSender) = sender ? &g_photon[sender - 1] : nullptr;
  using F = void (*)(void*, void*, const void*);
  void* body = reinterpret_cast<void*>(&fake_hunting_close_networked);
  reinterpret_cast<F>(entry_of("Door.HuntingCloseDoorNetworked", body))(d, &info, nullptr);
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
  g_front = g_back = g_gateLeft = g_gateRight = Obj{};
  g_closeSwitchesOff = true;
  g_lockRunsHere = false;
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
  fake::method(map::LevelController_UnlockHuntingDoors, reinterpret_cast<void*>(&fake_unlock_hunting_doors));
  g_ghost = Obj{};
  at<void*>(g_level, kLevelGhost) = &g_ghost;
  fake::field(map::LevelController_currentGhost, kLevelGhost);
  fake::field(map::GhostAI_isHunting, kGhostHunting);
  fake::method(map::Door_LockDoor, reinterpret_cast<void*>(&fake_lock));
  fake::method(map::Door_UnlockDoor, reinterpret_cast<void*>(&fake_unlock));
  fake::method(map::Door_DisableOrEnableDoor, reinterpret_cast<void*>(&fake_disable));
  fake::method(map::Door_AttemptToUnlockDoor, reinterpret_cast<void*>(&fake_attempt));
  fake::method(map::Door_GrabbedDoor, reinterpret_cast<void*>(&fake_grabbed));
  fake::method(map::Door_LockDoorForTime, reinterpret_cast<void*>(&fake_lock_for_time));
  fake::method(map::Door_ForceToggleHuntCollider, reinterpret_cast<void*>(&fake_hunt_collider));
  fake::method(map::MonkeyPaw_OpenExitsWish, reinterpret_cast<void*>(&fake_open_exits_wish));
  fake::method(map::Door_HuntingCloseDoorNetworked, reinterpret_cast<void*>(&fake_hunting_close_networked));
  fake::method(map::PhotonNetwork_get_InRoom, reinterpret_cast<void*>(&fake_in_room));
  fake::method(map::PhotonNetwork_get_OfflineMode, reinterpret_cast<void*>(&fake_offline));
  fake::method(map::PhotonNetwork_get_PlayerList, reinterpret_cast<void*>(&fake_player_list));
  fake::method(map::PhotonPlayer_get_ActorNumber, reinterpret_cast<void*>(&fake_actor_number));
  fake::field(map::PhotonMessageInfo_Sender, kInfoSender);
  fake::field(map::Door_grabbable, kDoorGrabbable);
  fake::field(map::Door_locked, kDoorLocked);
  fake::type(map::HingeDoor, &g_hingeClass);
  fake::method(map::HingeDoor_SwingDoor, reinterpret_cast<void*>(&fake_swing));
  fake::method(map::HingeDoor_OpenDoor, reinterpret_cast<void*>(&fake_push));
  fake::field(map::HingeDoor_hingeAxis, kDoorAxis);
  fake::field(map::Door_closedAngle, kDoorShutAngle);
  fake::value(map::Axis_X, kAxisX);
  fake::value(map::Axis_Z, kAxisZ);
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_own_transform));
  fake::method(map::Transform_get_localEulerAngles, reinterpret_cast<void*>(&fake_euler));
  fake::field(map::Door_body, kDoorBody);
  fake::field(map::HingeDoor_openAngle, kDoorOpenAngle);
  fake::field(map::Door_useForceInsteadOfAnimations, kDoorForce);
  fake::field(map::HingeDoor_openAngleReversed, kDoorReversed);
  fake::field(map::Door_closed, kDoorClosed);
  fake::type(map::Collider, &g_colliderClass);
  fake::method(map::Component_GetComponentInChildren, reinterpret_cast<void*>(&fake_no_child));
  fake::method(map::Collider_get_bounds, reinterpret_cast<void*>(&fake_bounds));
  fake::method(map::Transform_get_position, reinterpret_cast<void*>(&fake_position));
  fake::method(map::Transform_get_rotation, reinterpret_cast<void*>(&fake_rotation));
  g_euler.clear();
  fake::field(map::Door_kind, kDoorKind);
  fake::field(map::Door_interact, kDoorInteract);
  fake::field(map::Door_view, kDoorView);
  fake::value(map::DoorKind_None, kKindNone);
  fake::field(map::PhotonObjectInteract_inUse, kInteractInUse);
  fake::method(map::PhotonView_get_IsMine, reinterpret_cast<void*>(&fake_is_mine));
  fake::method(map::PhotonView_RequestOwnership, reinterpret_cast<void*>(&fake_request_ownership));
  g_opens.clear();
  g_pushes = 0;
  g_pushed.clear();
  g_viewsAsked.clear();
  // What the room's other gates read (core/room_gates.h), which the binding needs though nothing here calls them.
  for (const pm::sdk::EntryDesc* gate :
       {&map::PhotonNetwork_get_CurrentRoom, &map::PhotonNetwork_get_MasterClient, &map::PhotonRoom_get_IsVisible})
    fake::method(*gate, reinterpret_cast<void*>(&fake_none));
  fake::method(map::PhotonNetwork_get_LocalPlayer, reinterpret_cast<void*>(&fake_local_player));
  for (int i = 0; i < 3; ++i) {
    g_photon[i] = Obj{};
    at<int32_t>(g_photon[i], kActorNumber) = i + 1;
  }
  g_localPlayer = &g_photon[0];  // this client is actor 1, the room's first
  g_inRoom = true;
  g_offline = false;
  g_networkedBodies = 0;
  g_throwInWish = false;
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  CHECK(pm::api::bind(&g_host, missing));
  if (!missing.empty()) std::printf("missing: %s\n", missing.c_str());
  CHECK(door::create_hooks());
  // Made once bound: before the first bind the array reads empty.
  g_playerList = refs({&g_photon[1], &g_photon[0], &g_photon[2]});  // the list's order is not the actors'
  g_master = master;
  g_calls.clear();
  g_closed.clear();
  g_attemptBodies = g_grabBodies = g_wishBodies = 0;
  g_wishExits.clear();
  g_reported.clear();
  door::on_attempt([](void* d) { g_reported.push_back(d); });
  door::set_haunting(true);
  exits_are({&g_front});
  players_in({&g_bedroom});
}

// Another client, which holds what the master locked once the master's lock reaches it: `exits` are the map's.
void hold_from_the_master(const std::vector<void*>& exits) {
  start(false);
  exits_are(exits);
  lock_arrives(exits.front());
  g_calls.clear();
}

// Locks the map's exits as each client comes to hold them: the master by its own lock_all, another client when the
// master's lock of its first exit (`first`) reaches it.
bool lock_on(bool master, void* first) {
  if (master) return door::lock_all();
  lock_arrives(first);
  return door::locked();
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

// The hunt lock's RPCs refuse any sender but the room's first actor, with a report, so a master that is not the first
// actor (Photon's choice of a new master is its server's) locks nothing and locks nothing again (photon.md).
void a_master_not_the_first_actor_sends_no_lock() {
  start();
  exits_are({&g_front, &g_back}, {});
  g_localPlayer = &g_photon[1];  // actor 2: the master, but not the room's first actor
  CHECK(!door::lock_all() && g_closed.empty() && !door::locked());
  g_localPlayer = &g_photon[0];
  CHECK(door::lock_all() && g_closed.size() == 2);
  at<bool>(g_front, kDoorGrabbable) = true;  // a raced wish reopens it
  g_localPlayer = &g_photon[1];
  CHECK(door::relock(100000) == 0 && g_closed.size() == 2);
  g_localPlayer = &g_photon[0];
  CHECK(door::relock(100000) == 1 && g_closed.size() == 3);
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
    CHECK(lock_on(master, &g_front));
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
// Another client's lock_all calls nothing in the game and holds nothing: its own count of the players inside never
// decides; it holds the exits once the master's lock reaches it. Nothing locks on a friendly ghost's difficulty, or
// when the difficulty cannot be read, or with no exits.
void locking_takes_the_games_own_path() {
  start();
  CHECK(door::lock_all());
  CHECK(g_calls == std::vector<std::string>{"HuntingCloseDoor"});
  start(false);
  players_in({&g_outside});
  CHECK(!door::lock_all() && g_calls.empty() && !door::locked());
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

// Whether the Squatter holds a door: an exit it has locked on this client, for the map it locked it on. Nothing before
// the lock, no door that is not an exit, nothing once the map's LevelController is gone.
void the_squatter_holds_only_its_locked_exits() {
  start();
  exits_are({&g_front, &g_back});
  CHECK(!door::holds(&g_front) && !door::holds(nullptr));
  CHECK(door::lock_all());
  CHECK(door::holds(&g_front) && door::holds(&g_back) && !door::holds(&g_bedroom) && !door::holds(nullptr));
  hold_from_the_master({&g_front, &g_back});  // another client holds what the master locked
  CHECK(door::holds(&g_front) && door::holds(&g_back) && !door::holds(&g_bedroom));
  g_levelRef = nullptr;  // the contract is over
  CHECK(!door::holds(&g_front));
}

// The Monkey Paw's timed lock (Door.LockDoorForTime, the 6 s, 120 s and room wishes) and Door.ToggleDoorLock, either
// way, never start on a held exit: neither the clearing of the lock at the window's start nor the reopening at its end.
// Other doors, and every door before the lock, keep the game's own.
void the_paws_timed_lock_leaves_a_held_exit_shut() {
  for (const bool master : {true, false}) {
    start(master);
    exits_are({&g_front});
    lock_for_time(&g_front);
    toggle_lock(&g_front, true);
    CHECK((g_calls == std::vector<std::string>{"LockDoorForTime", "ToggleDoorLock"}));  // before the lock
    CHECK(lock_on(master, &g_front));
    g_calls.clear();
    lock_for_time(&g_front);
    toggle_lock(&g_front, false);
    toggle_lock(&g_front, true);
    CHECK(g_calls.empty());
    lock_for_time(&g_bedroom);
    toggle_lock(&g_bedroom, true);
    CHECK((g_calls == std::vector<std::string>{"LockDoorForTime", "ToggleDoorLock"}));
  }
}

// The wish that opens every exit (MonkeyPaw.OpenExitsWish) still runs, but inside it a held exit is neither unlocked,
// switched back on nor stripped of its hunt collider, on every client. Before the lock it opens them as the game does.
void the_open_exits_wish_leaves_held_exits_locked() {
  for (const bool master : {true, false}) {
    start(master);
    exits_are({&g_front, &g_back});
    g_wishExits = {&g_front, &g_back};
    open_exits_wish();
    CHECK(g_wishBodies == 1 && g_calls.size() == 6);  // before the lock: both exits opened
    CHECK(lock_on(master, &g_front));
    g_calls.clear();
    open_exits_wish();
    CHECK(g_wishBodies == 2 && g_calls.empty());
    g_wishExits = {&g_bedroom};  // a door that is not held keeps the game's own
    open_exits_wish();
    CHECK((g_calls == std::vector<std::string>{"UnlockDoor", "ToggleDoorLock", "ForceToggleHuntCollider"}));
  }
}

// Outside the wish, Door.UnlockDoor and Door.ForceToggleHuntCollider on a held exit stay the game's own: the house
// key's unlock (Door.AttemptToUnlockDoor) is not the wish's.
void outside_the_wish_a_held_exit_keeps_the_games_unlock() {
  start();
  CHECK(door::lock_all());
  unlock(&g_front);
  hunt_collider(&g_front);
  CHECK((g_calls == std::vector<std::string>{"HuntingCloseDoor", "UnlockDoor", "ForceToggleHuntCollider"}));
}

// Every client holds the exits exactly when the master locked them: once the master's lock (the game's own RPC,
// Door.HuntingCloseDoorNetworked) reaches it, whatever its own count of the players inside says. The game's body runs
// first and unchanged; then every exit of the map is held, and a try at one is reported.
void the_hold_follows_the_masters_lock() {
  start(false);
  exits_are({&g_front, &g_back});
  players_in({&g_bedroom});  // this client still counts a player inside
  CHECK(!door::locked());
  lock_arrives(&g_front);
  CHECK(g_networkedBodies == 1 && g_calls == std::vector<std::string>{"HuntingCloseDoorNetworked"});
  CHECK(door::locked() && door::holds(&g_front) && door::holds(&g_back) && !door::holds(&g_bedroom));
  attempt(&g_back);
  CHECK(g_reported.size() == 1 && g_reported[0] == &g_back);
  lock_arrives(&g_back);  // the master's lock of its next exit holds nothing more
  CHECK(g_networkedBodies == 2 && door::locked());
  // The master's own lock reaches it too, as Photon runs an RPC sent to every client on its sender: no second lock.
  start();
  exits_are({&g_front});
  CHECK(door::lock_all());
  lock_arrives(&g_front);
  CHECK(g_closed.size() == 1 && door::locked());
}

// The gap Task 6 left: a client that had not yet held the exits ran the Monkey Paw's calls on them after the master's
// lock reached it, and its unlock went to everyone. Now the hold starts in the same call that brings the lock, so the
// very next call on this client, before any frame of the mod's, is refused: the timed lock and ToggleDoorLock on any
// exit, and the open-exits wish's unlock.
void the_masters_lock_closes_the_same_frame_gap() {
  start(false);
  exits_are({&g_front, &g_back});
  g_wishExits = {&g_front, &g_back};
  lock_for_time(&g_front);  // before the lock: the game's own
  CHECK((g_calls == std::vector<std::string>{"LockDoorForTime"}));
  g_calls.clear();
  lock_arrives(&g_front);
  lock_for_time(&g_front);
  toggle_lock(&g_front, true);
  toggle_lock(&g_back, true);  // an exit whose own lock is still on its way
  open_exits_wish();
  CHECK((g_calls == std::vector<std::string>{"HuntingCloseDoorNetworked"}));
  CHECK(g_wishBodies == 1);
}

// Only the master's lock, as the game takes it, holds the exits. A sender the game's own check refuses (any but the
// room's first actor, or none) holds nothing, though the game's body still runs and refuses it; so do a lock while
// the Squatter does not haunt (another ghost's hunt), one on a friendly ghost's difficulty or an unread one, and a door
// that is not an exit. Outside a room or offline the game takes every call, and so does the hold.
void a_lock_the_game_refuses_holds_nothing() {
  start(false);
  lock_arrives(&g_front, 2);
  lock_arrives(&g_front, 3);
  lock_arrives(&g_front, 0);
  CHECK(g_networkedBodies == 3 && !door::locked());
  g_playerList = refs({&g_photon[1], nullptr});  // a list the game would throw on
  lock_arrives(&g_front, 2);
  CHECK(!door::locked());
  start(false);
  door::set_haunting(false);
  lock_arrives(&g_front);
  CHECK(g_networkedBodies == 1 && !door::locked());
  start(false, true);
  lock_arrives(&g_front);
  CHECK(!door::locked());
  start(false);
  g_valuesRef = nullptr;
  lock_arrives(&g_front);
  CHECK(!door::locked());
  start(false);
  lock_arrives(&g_bedroom);
  CHECK(!door::locked());
  start(false);
  g_offline = true;
  lock_arrives(&g_front, 3);
  CHECK(door::locked());
  start(false);
  g_inRoom = false;
  lock_arrives(&g_front, 0);
  CHECK(door::locked());
}

// A client that held the exits from the master's lock and then becomes the master (a host migration) never locks
// them again: its lock_all finds them held and calls nothing.
void a_new_master_never_locks_twice() {
  hold_from_the_master({&g_front, &g_back});
  g_master = true;
  CHECK(door::lock_all() && g_closed.empty() && g_calls.empty());
}

// The open-exits wish's count of running calls returns to 0 when the wish ends, normally or by an exception, so the
// house key's unlock and the collider switch on a held exit stay the game's own afterwards.
void the_open_exits_wish_count_returns_to_zero() {
  start();
  exits_are({&g_front});
  g_wishExits = {&g_front};
  CHECK(door::lock_all());
  open_exits_wish();
  g_throwInWish = true;
  bool threw = false;
  try {
    open_exits_wish();
  } catch (int) {
    threw = true;
  }
  CHECK(threw && g_wishBodies == 2);
  g_calls.clear();
  unlock(&g_front);
  hunt_collider(&g_front);
  CHECK((g_calls == std::vector<std::string>{"UnlockDoor", "ForceToggleHuntCollider"}));
}

// The master's own lock reaches it within its own call (Photon runs an RPC sent to every client on its sender), so the
// hold may begin inside lock_all's first Door.HuntingCloseDoor. lock_all still locks every exit once, and holds them
// all once.
void the_masters_own_rpc_holds_before_lock_all_does() {
  start();
  exits_are({&g_front, &g_back});
  g_lockRunsHere = true;
  CHECK(door::lock_all());
  CHECK(g_closed.size() == 2 && count(g_closed, &g_front) == 1 && count(g_closed, &g_back) == 1);
  CHECK(g_networkedBodies == 2 && door::locked() && door::holds(&g_front) && door::holds(&g_back));
  CHECK(door::lock_all() && g_closed.size() == 2);
}

// A wish another client began before the master's lock reached it can unlock an exit after the lock, for every client
// (its unlock reaches them all after the lock), and switch it back on. The master answers with the game's own hunt
// lock (Door.HuntingCloseDoor) once more on that exit alone: when it reads grabbable again, or unlocked again after
// this client read it locked. One reopening gets one re-lock.
void the_master_relocks_an_exit_a_raced_wish_reopened() {
  start();
  exits_are({&g_front, &g_back});
  CHECK(door::lock_all());
  g_closed.clear();
  CHECK(door::relock(1000) == 0);  // both shut and switched off
  at<bool>(g_front, kDoorGrabbable) = true;  // the raced wish switched it back on
  CHECK(door::relock(2000) == 1 && g_closed == std::vector<void*>{&g_front});
  CHECK(door::relock(2001) == 0 && g_closed.size() == 1);  // the lock switched it off again
  at<bool>(g_back, kDoorLocked) = true;  // the lock reached it
  CHECK(door::relock(3000) == 0);
  at<bool>(g_back, kDoorLocked) = false;  // the raced unlock came after
  CHECK(door::relock(3001) == 1 && g_closed.back() == &g_back);
  CHECK(door::relock(3002) == 0 && g_closed.size() == 2);  // not read locked since: nothing more
  // Locked and then unlocked again later: one more re-lock.
  at<bool>(g_back, kDoorLocked) = true;
  CHECK(door::relock(9000) == 0);
  at<bool>(g_back, kDoorLocked) = false;
  CHECK(door::relock(9001) == 1 && g_closed.size() == 3);
}

// An exit another player's client owns keeps its lock on that client alone, so the master reads it unlocked from the
// start, though shut and switched off everywhere (sdk/systems/doors.md, "When a player owns the exit"). That is the
// game's own lock, not a raced wish: the master never re-locks it.
void an_exit_locked_on_its_owner_is_left_alone() {
  start();
  CHECK(door::lock_all());
  g_closed.clear();
  for (const uint64_t t : {1000u, 5000u, 60000u}) CHECK(door::relock(t) == 0);
  CHECK(g_closed.empty());
}

// An exit that stays grabbable is locked again at most once each kRelockMs, never every frame.
void a_relock_is_rate_limited() {
  start();
  CHECK(door::lock_all());
  g_closed.clear();
  g_closeSwitchesOff = false;  // say the switch-off never lands
  at<bool>(g_front, kDoorGrabbable) = true;
  CHECK(door::relock(1000) == 1);
  CHECK(door::relock(1001) == 0 && door::relock(1000 + door::kRelockMs - 1) == 0);
  CHECK(door::relock(1000 + door::kRelockMs) == 1 && g_closed.size() == 2);
}

// Only the master re-locks, only exits it holds, and never on a friendly ghost's difficulty or an unread one: another
// client holding the exits, a master before the lock, a door that is not an exit, and the map once its level is gone
// all call nothing.
void only_the_master_relocks_its_held_exits() {
  hold_from_the_master({&g_front});
  at<bool>(g_front, kDoorGrabbable) = true;
  CHECK(door::relock(1000) == 0 && g_calls.empty());
  start();
  at<bool>(g_front, kDoorGrabbable) = true;
  CHECK(door::relock(1000) == 0 && g_calls.empty());  // nothing held yet
  CHECK(door::lock_all());
  g_calls.clear();
  at<bool>(g_bedroom, kDoorGrabbable) = true;
  at<bool>(g_difficulty, kDifficultyFriendly) = true;
  at<bool>(g_front, kDoorGrabbable) = true;
  CHECK(door::relock(2000) == 0);
  at<bool>(g_difficulty, kDifficultyFriendly) = false;
  g_valuesRef = nullptr;
  CHECK(door::relock(3000) == 0);
  g_valuesRef = &g_values;
  g_levelRef = nullptr;
  CHECK(door::relock(4000) == 0 && g_calls.empty());
}

// The quiet opener (squatter_door.h open_quietly). The doors it may open: plain shut hinged doors inside the map.
Obj g_kitchen, g_study, g_kitchenUse, g_studyUse, g_kitchenView, g_studyView;

// `d` as a plain hinged door the game's contract-start opener would open: of kind None, shut, unlocked, grabbable,
// with an interact nothing uses and a view this client owns unless `mine` is false.
void plain_door(Obj& d, Obj& use, Obj& view, bool mine = true) {
  d = use = view = Obj{};
  d.klass = &g_hingeClass;
  at<int32_t>(d, kDoorKind) = kKindNone;
  at<int32_t>(d, kDoorAxis) = kAxisY;
  at<float>(d, kDoorShutAngle) = 90.0f;
  g_euler[&d] = {0, 90.0f, 0};  // its leaf at its shut angle
  at<bool>(d, kDoorGrabbable) = true;
  at<void*>(d, kDoorInteract) = &use;
  at<void*>(d, kDoorView) = &view;
  at<bool>(view, kViewMine) = mine;
}

// A hiding place's door reads shut as the game snaps a leaf shut (HingeDoor.Update: within 7 degrees of
// Door.closedAngle on its hinge axis); a door with no hinge reads as the game marks it (Door.closed). The Squatter kicks
// a shut one and walks into an open one (user, 2026-11-09).
void a_door_reads_shut_as_the_game_snaps_it() {
  CHECK(door::kShutDegrees == 7.0f);
  CHECK(door::reads_shut(0.0f) && door::reads_shut(door::kShutDegrees) && !door::reads_shut(door::kShutDegrees + 0.5f));
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  CHECK(door::shut(&g_kitchen));
  g_euler[&g_kitchen].y = 90.0f + door::kShutDegrees - 1.0f;  // within the snap
  CHECK(door::shut(&g_kitchen));
  g_euler[&g_kitchen].y = 90.0f + 30.0f;  // ajar
  CHECK(!door::shut(&g_kitchen));
  g_kitchen.klass = &g_slideClass;  // no hinge: the game's own mark
  at<bool>(g_kitchen, kDoorClosed) = true;
  CHECK(door::shut(&g_kitchen));
  at<bool>(g_kitchen, kDoorClosed) = false;
  CHECK(!door::shut(&g_kitchen));
  CHECK(!door::shut(nullptr));  // no door shuts nothing
}

// A door lets the Squatter through when its gap fits, or when its leaf stands at its own open angle (within the game's
// snap of HingeDoor.openAngle), as a closet door that swings less than kPassDegrees does once opened: it is as open as
// it goes, so opening it again would only replay the clip.
void a_door_open_as_far_as_it_goes_lets_it_through() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  at<float>(g_kitchen, kDoorOpenAngle) = 90.0f + 50.0f;  // swings only 50 degrees
  CHECK(!door::lets_through(&g_kitchen));                // shut
  g_euler[&g_kitchen].y = 90.0f + 30.0f;
  CHECK(!door::lets_through(&g_kitchen));  // ajar
  g_euler[&g_kitchen].y = 90.0f + 48.0f;
  CHECK(door::lets_through(&g_kitchen));  // as far as it goes
  at<float>(g_kitchen, kDoorOpenAngle) = 90.0f + 120.0f;
  g_euler[&g_kitchen].y = 90.0f + door::kPassDegrees;
  CHECK(door::lets_through(&g_kitchen));  // wide enough, short of its open angle
  CHECK(door::lets_through(nullptr));     // no door stands in the way
}

// The Squatter never passes through a door (user, 2026-11-09): it opens one whose gap is too narrow for it, measured by
// how far the leaf stands from shut (Door.closedAngle) on its hinge axis, the shorter way round. Door.closed is not it:
// the game sets it when a door snaps shut and clears it on a grab or the contract-start opener's RPC, so a door swung
// open by its physics still reads closed.
void a_door_too_narrow_to_pass_is_opened() {
  CHECK(door::gap_degrees(90.0f, 90.0f) == 0.0f && door::gap_degrees(150.0f, 90.0f) == 60.0f);
  CHECK(door::gap_degrees(1.0f, 359.0f) == 2.0f && door::gap_degrees(200.0f, 10.0f) == 170.0f);
  CHECK(!door::fits(door::kPassDegrees - 1.0f) && door::fits(door::kPassDegrees));
  CHECK(door::leaf_angle({10, 20, 30}, kAxisX, kAxisX, kAxisZ) == 10.0f);
  CHECK(door::leaf_angle({10, 20, 30}, kAxisZ, kAxisX, kAxisZ) == 30.0f);
  CHECK(door::leaf_angle({10, 20, 30}, kAxisY, kAxisX, kAxisZ) == 20.0f);
  // Through the game: shut or ajar, a plain door is opened; open wide enough, it is left alone, whatever Door.closed says.
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  CHECK(door::may_open(&g_kitchen));
  g_euler[&g_kitchen].y = 90.0f + door::kPassDegrees - 10.0f;  // ajar
  CHECK(door::may_open(&g_kitchen));
  g_euler[&g_kitchen].y = 90.0f + door::kPassDegrees + 5.0f;  // wide enough
  CHECK(!door::may_open(&g_kitchen));
  at<int32_t>(g_kitchen, kDoorAxis) = kAxisZ;  // the hinge's own axis is what counts
  g_euler[&g_kitchen] = {0, 175.0f, 90.0f};
  CHECK(door::may_open(&g_kitchen));
}

// Which way a leaf swings as it opens, so the Squatter pulls a door that comes toward it and pushes one that goes away
// (user, 2026-11-09: it pulled a door that swings away). The leaf turns about its own position toward its open angle
// (HingeDoor.openAngle), the shorter way round, whatever axis it hinges on and however its parent is turned.
// A door the game moves by force (Door.useForceInsteadOfAnimations) is pushed open with its leaf's physics, never turned
// by the animated swing, which sets the leaf's other angles to 0 and takes such a door off its hinges (user, 2026-11-09:
// doors came off their hinges and opened again and again). The push goes to the door's open side: the push to shut on a
// door whose open angle Awake turns the other way (HingeDoor.openAngleReversed).
void a_door_moved_by_force_is_pushed_to_its_open_side() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  at<bool>(g_kitchen, kDoorForce) = true;
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Opened);
  CHECK(g_opens.empty() && g_pushed.size() == 1 && g_pushed[0].door == &g_kitchen && !g_pushed[0].close &&
        g_pushed[0].seconds == door::kPushStrength && !g_pushed[0].buffered);
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  at<bool>(g_kitchen, kDoorForce) = true;
  at<bool>(g_kitchen, kDoorReversed) = true;
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Opened);
  CHECK(g_opens.empty() && g_pushed.size() == 1 && g_pushed[0].close);
}

void a_leaf_swings_toward_its_open_angle() {
  CHECK(door::delta_angle(0, 90) == 90.0f && door::delta_angle(0, 270) == -90.0f && door::delta_angle(350, 10) == 20.0f);
  // A leaf hinged at the origin on y, reaching out along +x, shut at 0. A turn of +y carries +x toward -z.
  door::Leaf leaf{.pivot = {0, 0, 0}, .rotation = {0, 0, 0, 1}, .euler = {0, 0, 0}, .centre = {0.45f, 1, 0},
                  .axis = kAxisY, .axisX = kAxisX, .axisZ = kAxisZ, .openAngle = 90};
  CHECK(door::swings_toward(leaf, {0.45f, 0, -1}) && !door::swings_toward(leaf, {0.45f, 0, 1}));
  leaf.openAngle = -90;  // the other side
  CHECK(!door::swings_toward(leaf, {0.45f, 0, -1}) && door::swings_toward(leaf, {0.45f, 0, 1}));
  leaf.openAngle = 270;  // the same, the shorter way round
  CHECK(door::swings_toward(leaf, {0.45f, 0, 1}));
  // Hinged on its own z, as a model turned upright by x -90: z then points up in the world, so a turn of +z swings it
  // as the leaf above; turned by x +90 instead, z points down and the same turn swings it the other way.
  door::Leaf upright{.pivot = {0, 0, 0}, .rotation = pm::custom::euler_quat({-90, 0, 0}), .euler = {-90, 0, 0},
                     .centre = {0.45f, 0, 0}, .axis = kAxisZ, .axisX = kAxisX, .axisZ = kAxisZ, .openAngle = 90};
  CHECK(door::swings_toward(upright, {0.45f, 0, -1}) && !door::swings_toward(upright, {0.45f, 0, 1}));
  upright.rotation = pm::custom::euler_quat({90, 0, 0});
  upright.euler = {90, 0, 0};
  CHECK(door::swings_toward(upright, {0.45f, 0, 1}) && !door::swings_toward(upright, {0.45f, 0, -1}));
}

// On the master, whose view it is (a scene door's view starts the master's), a shut plain door opens at once with the
// game's own swing to its open angle (HingeDoor.SwingDoor), all the way, called once and nothing else: no ownership
// asked, no other door method, and never HingeDoor.OpenDoor's push, which shuts a door whose open angle Awake reverses
// (user, 2026-11-09: a door that swings away stayed shut).
void a_shut_door_opens_quietly_on_the_master() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  CHECK(door::may_open(&g_kitchen));
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Opened);
  CHECK(g_opens.size() == 1 && g_opens[0].door == &g_kitchen && !g_opens[0].close &&
        g_opens[0].seconds == door::kSwingSeconds && g_opens[0].fraction == 1.0f && !g_opens[0].buffered);
  CHECK((g_calls == std::vector<std::string>{"SwingDoor"}) && g_pushes == 0);
  CHECK(door::open_step(1001) == door::Opening::Idle && g_opens.size() == 1);
}

// The leaf's angle reaches the other clients only from the view's owner (the door's PhotonTransformView), and a leaf
// moved anywhere else is pulled back to the owner's angle. So while another player owns the door's view the master asks
// for it once and opens nothing; it opens the door, once, only after the view is its own, so every client sees the
// same open door.
void every_client_sees_the_open_because_the_master_owns_the_view_first() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView, false);
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Waiting);
  CHECK(g_viewsAsked == std::vector<void*>{&g_kitchenView} && g_opens.empty());
  CHECK(door::open_step(1100) == door::Opening::Waiting && door::open_step(1200) == door::Opening::Waiting);
  CHECK(g_opens.empty() && g_viewsAsked.size() == 1);
  at<bool>(g_kitchenView, kViewMine) = true;  // the takeover came back
  CHECK(door::open_step(1300) == door::Opening::Opened);
  CHECK(g_opens.size() == 1 && g_opens[0].door == &g_kitchen);
  CHECK(door::open_step(1400) == door::Opening::Idle && g_opens.size() == 1);
  // Another client never opens anything: the angle comes to it from the master's view.
  start(false);
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
  CHECK(!door::may_open(&g_kitchen));
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Refused && g_calls.empty());
}

// A takeover that has not landed within kOpenWaitMs gives the open up, opening nothing, and frees the opener.
void an_open_whose_view_never_comes_is_given_up() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView, false);
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Waiting);
  CHECK(door::open_step(1000 + door::kOpenWaitMs - 1) == door::Opening::Waiting);
  CHECK(door::open_step(1000 + door::kOpenWaitMs) == door::Opening::Refused);
  at<bool>(g_kitchenView, kViewMine) = true;  // too late
  CHECK(door::open_step(1000 + door::kOpenWaitMs + 1) == door::Opening::Idle && g_opens.empty());
  CHECK(door::open_quietly(&g_kitchen, 9000) == door::Opening::Opened);
}

// It never acts on an exit: not one of the map's exits before the lock (even one whose kind reads None), not a held
// one, and refusing leaves the hold and the master's re-lock as they were.
void an_exit_is_never_opened() {
  start();
  plain_door(g_front, g_kitchenUse, g_kitchenView);
  plain_door(g_back, g_studyUse, g_studyView);
  at<int32_t>(g_front, kDoorKind) = kKindMain;
  exits_are({&g_front, &g_back});  // g_back reads None, but the map lists it as an exit
  CHECK(!door::may_open(&g_front) && !door::may_open(&g_back));
  CHECK(door::open_quietly(&g_front, 1000) == door::Opening::Refused);
  CHECK(door::open_quietly(&g_back, 1000) == door::Opening::Refused);
  CHECK(door::lock_all());
  at<bool>(g_back, kDoorGrabbable) = true;  // as if a raced wish switched it back on
  g_calls.clear();
  g_closed.clear();
  CHECK(door::open_quietly(&g_back, 2000) == door::Opening::Refused && g_calls.empty());
  CHECK(door::holds(&g_front) && door::holds(&g_back));
  CHECK(door::relock(3000) == 1 && g_closed == std::vector<void*>{&g_back} && g_opens.empty());
}

// Only a door the game's own contract-start opener would take opens: a live HingeDoor of kind None that reads shut and
// has an interact; and, as the game holds them, one that is unlocked, grabbable and that nothing is using; only on the
// master and only in a contract.
void only_a_door_the_game_would_open_opens() {
  using Change = void (*)();
  const Change changes[] = {
      [] { g_kitchen.klass = &g_slideClass; },                       // another kind of door
      [] { at<int32_t>(g_kitchen, kDoorKind) = kKindSecondary; },    // a key's door
      [] { g_euler[&g_kitchen].y = 175.0f; },                         // already open
      [] { at<void*>(g_kitchen, kDoorInteract) = nullptr; },         // no interact
      [] { at<bool>(g_kitchenUse, kInteractInUse) = true; },         // a player's hand or a ghost's force
      [] { at<bool>(g_kitchen, kDoorLocked) = true; },               // the paw's timed lock
      [] { at<bool>(g_kitchen, kDoorGrabbable) = false; },           // switched off
      [] { g_master = false; },                                      // another client
      [] { g_levelRef = nullptr; },                                  // no contract
  };
  for (const Change change : changes) {
    start();
    plain_door(g_kitchen, g_kitchenUse, g_kitchenView);
    change();
    CHECK(!door::may_open(&g_kitchen));
    CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Refused);
    CHECK(g_calls.empty() && door::open_step(1001) == door::Opening::Idle);
  }
  start();
  CHECK(!door::may_open(nullptr) && door::open_quietly(nullptr, 1000) == door::Opening::Refused);
}

// A waiting open is given up, opening nothing, when the door stops being one the game would open before the view
// lands: a player grabs it, or this client stops being the master.
void a_waiting_open_is_given_up_when_the_door_changes() {
  for (const bool grabbed : {true, false}) {
    start();
    plain_door(g_kitchen, g_kitchenUse, g_kitchenView, false);
    CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Waiting);
    if (grabbed) {
      at<bool>(g_kitchenUse, kInteractInUse) = true;
      g_euler[&g_kitchen].y = 175.0f;
    } else {
      g_master = false;
    }
    at<bool>(g_kitchenView, kViewMine) = true;  // the grab took the view, or the takeover landed anyway
    CHECK(door::open_step(1100) == door::Opening::Refused);
    CHECK(door::open_step(1200) == door::Opening::Idle && g_opens.empty());
  }
}

// One open waits at a time: another door is refused until it is done, and the same door again asks nothing more.
void one_open_waits_at_a_time() {
  start();
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView, false);
  plain_door(g_study, g_studyUse, g_studyView);
  CHECK(door::open_quietly(&g_kitchen, 1000) == door::Opening::Waiting);
  CHECK(door::open_quietly(&g_study, 1010) == door::Opening::Refused && g_opens.empty());
  CHECK(door::open_quietly(&g_kitchen, 1020) == door::Opening::Waiting && g_viewsAsked.size() == 1);
  at<bool>(g_kitchenView, kViewMine) = true;
  CHECK(door::open_step(1030) == door::Opening::Opened);
  CHECK(door::open_quietly(&g_study, 1040) == door::Opening::Opened && g_opens.size() == 2);
  // A new map forgets an open still waiting from the last one.
  plain_door(g_kitchen, g_kitchenUse, g_kitchenView, false);
  CHECK(door::open_quietly(&g_kitchen, 2000) == door::Opening::Waiting);
  static Obj nextLevel;
  nextLevel = Obj{};
  g_levelRef = &nextLevel;
  at<bool>(g_kitchenView, kViewMine) = true;
  CHECK(door::open_step(2100) != door::Opening::Opened && g_opens.size() == 2);
  CHECK(door::open_step(2200) == door::Opening::Idle);
}

// A build that lacks an entry the doors need fails the mod's binding, naming it, as Custom Ghosts' start does.
void a_missing_entry_fails_the_start() {
  for (const pm::sdk::EntryDesc* lacking : {&map::Door_HuntingCloseDoor, &map::Door_AttemptToUnlockDoor,
                                            &map::Door_GrabbedDoor, &map::LevelController_huntingDoors,
                                            &map::SmallMapVariation_huntingDoorsToLock, &map::RoomKind_outsideMap,
                                            &map::Door_LockDoorForTime, &map::Door_ToggleDoorLock,
                                            &map::Door_UnlockDoor, &map::Door_ForceToggleHuntCollider,
                                            &map::MonkeyPaw_OpenExitsWish,
                                            &map::Door_HuntingCloseDoorNetworked, &map::Door_grabbable,
                                            &map::Door_locked, &map::HingeDoor_SwingDoor,
                                            &map::HingeDoor, &map::Door_closedAngle, &map::HingeDoor_hingeAxis,
                                            &map::Transform_get_localEulerAngles, &map::Door_kind,
                                            &map::Door_interact, &map::Door_view, &map::DoorKind_None,
                                            &map::PhotonObjectInteract_inUse, &map::PhotonView_get_IsMine,
                                            &map::PhotonView_RequestOwnership}) {
    start();
    fake::forget(*lacking);
    std::string missing;
    CHECK(!pm::api::bind(&g_host, missing) && missing.find(lacking->key) != std::string::npos);
  }
}

}  // namespace

// The hold is the Squatter's, not the map's (user, 2026-11-13: once it had locked the door, changing to another ghost
// type never unlocked it). Once it no longer haunts, every exit it held takes the game's calls again, so another
// ghost's hunt ends with its exits open, and the master ends the lock as a hunt's end does
// (LevelController.UnlockHuntingDoors, sdk/systems/doors.md "How a hunt locks the exits").
void the_hold_ends_when_the_squatter_stops_haunting() {
  start();
  CHECK(door::lock_all());
  g_calls.clear();
  door::set_haunting(false);
  door::release();
  CHECK(!door::locked() && !door::holds(&g_front));
  CHECK(g_calls == std::vector<std::string>{"UnlockHuntingDoors"});
  g_calls.clear();
  toggle_lock(&g_front, true);  // a later hunt's end reaches the exit
  CHECK(g_calls == std::vector<std::string>{"ToggleDoorLock"});
  g_calls.clear();
  door::release();  // nothing is held any more: nothing to end
  CHECK(g_calls.empty());
}

// While the contract's ghost hunts, the hunt's own end unlocks the exits (GameController.ResetHuntEffects), so the
// master lets go and calls nothing.
void a_hunt_on_ends_the_lock_itself() {
  start();
  CHECK(door::lock_all());
  at<bool>(g_ghost, kGhostHunting) = true;
  g_calls.clear();
  door::release();
  CHECK(!door::locked() && g_calls.empty());
}

// Another client lets go of what the master's lock made it hold, and unlocks nothing: the master's unlock reaches it.
void another_client_lets_go_and_unlocks_nothing() {
  hold_from_the_master({&g_front});
  CHECK(door::locked());
  door::release();
  CHECK(!door::locked() && g_calls.empty());
}

int main() {
  the_hold_ends_when_the_squatter_stops_haunting();
  a_hunt_on_ends_the_lock_itself();
  another_client_lets_go_and_unlocks_nothing();
  a_door_too_narrow_to_pass_is_opened();
  a_door_reads_shut_as_the_game_snaps_it();
  a_door_open_as_far_as_it_goes_lets_it_through();
  locks_every_exit_once();
  a_master_not_the_first_actor_sends_no_lock();
  counts_only_living_inside();
  a_failed_count_reads_as_unknown();
  a_map_with_no_house_still_locks_its_door();
  an_attempt_on_a_locked_door_reports_it();
  an_attempt_on_an_unlocked_door_reports_nothing();
  locking_takes_the_games_own_path();
  the_squatter_holds_only_its_locked_exits();
  the_paws_timed_lock_leaves_a_held_exit_shut();
  the_open_exits_wish_leaves_held_exits_locked();
  outside_the_wish_a_held_exit_keeps_the_games_unlock();
  the_hold_follows_the_masters_lock();
  the_masters_lock_closes_the_same_frame_gap();
  a_lock_the_game_refuses_holds_nothing();
  a_new_master_never_locks_twice();
  the_open_exits_wish_count_returns_to_zero();
  the_masters_own_rpc_holds_before_lock_all_does();
  the_master_relocks_an_exit_a_raced_wish_reopened();
  an_exit_locked_on_its_owner_is_left_alone();
  a_relock_is_rate_limited();
  only_the_master_relocks_its_held_exits();
  a_leaf_swings_toward_its_open_angle();
  a_door_moved_by_force_is_pushed_to_its_open_side();
  a_shut_door_opens_quietly_on_the_master();
  every_client_sees_the_open_because_the_master_owns_the_view_first();
  an_open_whose_view_never_comes_is_given_up();
  an_exit_is_never_opened();
  only_a_door_the_game_would_open_opens();
  a_waiting_open_is_given_up_when_the_door_changes();
  one_open_waits_at_a_time();
  a_missing_entry_fails_the_start();
  door::reset();  // what it holds goes back before the fake's own storage does
  std::printf(g_failures ? "%d check(s) failed\n" : "all squatter door tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
