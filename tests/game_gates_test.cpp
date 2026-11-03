// Offline tests of the room gates (core/room_gates.h, core/game.cpp and host/host_game.cpp), over the fake game backend: whether the room is a private
// one and whether this client is its master. Each fails closed when a Photon member it reads did not
// resolve, since every privacy and authority gate of the host and the mods runs on them.
// Run: build\game_gates_test.exe
#include <cstdio>
#include <string>
#include <vector>

#include "LevelController.h"
#include "Photon.h"
#include "fake_game.h"
#include "game.h"
#include "game_api.h"
#include "host_game.h"
#include "Input.h"

#include "check.h"

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace game = pm::game;
namespace hostgame = pm::hostgame;
namespace map = pm::map;

// The room as the fake Photon answers it.
bool g_inRoom = true, g_offline = false, g_visible = false;
int g_room = 0;  // the current room object's address stands for it
int g_localPlayer = 0, g_masterPlayer = 0;
void* g_playerList = nullptr;  // PhotonNetwork.get_PlayerList
int g_localActor = 1, g_masterActor = 1;
void* g_levelController = nullptr;  // LevelController.instance's storage
int g_level[8] = {};                 // a live controller

bool in_room(const void*) { return g_inRoom; }
bool offline(const void*) { return g_offline; }
void* current_room(const void*) { return &g_room; }
bool visible(void*, const void*) { return g_visible; }
void* local_player(const void*) { return &g_localPlayer; }
void* master_client(const void*) { return &g_masterPlayer; }
int32_t actor_number(void* player, const void*) { return player == &g_localPlayer ? g_localActor : g_masterActor; }

// Every member the gates read, less the one `without` names.
void give_room(const pm::sdk::EntryDesc* without = nullptr) {
  fake::reset();
  const auto give = [&](const pm::sdk::EntryDesc& d, auto fn) {
    if (&d != without) fake::method(d, reinterpret_cast<void*>(fn));
  };
  give(map::PhotonNetwork_get_InRoom, &in_room);
  give(map::PhotonNetwork_get_OfflineMode, &offline);
  give(map::PhotonNetwork_get_CurrentRoom, &current_room);
  give(map::PhotonRoom_get_IsVisible, &visible);
  give(map::PhotonNetwork_get_LocalPlayer, &local_player);
  give(map::PhotonNetwork_get_MasterClient, &master_client);
  give(map::PhotonPlayer_get_ActorNumber, &actor_number);
  if (&map::LevelController_instance != without)
    fake::static_field(map::LevelController_instance, &g_levelController, sizeof g_levelController, true);
  std::string missing;
  api::bind(fake::host(), missing);
}

void private_room_needs_every_member() {
  g_inRoom = true;
  g_offline = false;
  g_visible = false;
  give_room();
  CHECK(game::private_room());  // an unlisted room
  CHECK(hostgame::private_room());  // the host asks the same gate
  g_visible = true;
  CHECK(!game::private_room());  // a public one
  g_visible = false;
  // A room the game lists publicly reads as unlisted through an unresolved getter's default (false), so each
  // member missing makes the room no private one.
  for (const pm::sdk::EntryDesc* d : {&map::PhotonRoom_get_IsVisible, &map::PhotonNetwork_get_CurrentRoom,
                                      &map::PhotonNetwork_get_OfflineMode, &map::PhotonNetwork_get_InRoom}) {
    give_room(d);
    CHECK(!game::private_room());
    CHECK(!hostgame::private_room());
    if (game::private_room()) std::printf("  without %s the room reads as private\n", d->key);
  }
  g_offline = true;  // single player is private, as long as the gate can read that it is
  give_room();
  CHECK(game::private_room());
  give_room(&map::PhotonRoom_get_IsVisible);
  CHECK(!game::private_room());
  g_offline = false;
}

// Every client would be the master when neither actor number can be read (0 == 0), and a client that is not
// would send the host's RPCs, which the game refuses and counts against the account.
void master_needs_actor_numbers() {
  g_inRoom = true;
  g_localActor = 2;
  g_masterActor = 2;
  give_room();
  CHECK(hostgame::local_is_master());
  g_masterActor = 1;
  CHECK(!hostgame::local_is_master());
  g_masterActor = 2;
  for (const pm::sdk::EntryDesc* d : {&map::PhotonPlayer_get_ActorNumber, &map::PhotonNetwork_get_LocalPlayer,
                                      &map::PhotonNetwork_get_MasterClient, &map::PhotonNetwork_get_InRoom}) {
    give_room(d);
    CHECK(!hostgame::local_is_master());
    if (hostgame::local_is_master()) std::printf("  without %s this client reads as the master\n", d->key);
  }
  g_localActor = g_masterActor = 1;
}

void out_of_contract_needs_the_instance() {
  g_levelController = nullptr;
  give_room();
  CHECK(hostgame::out_of_contract());
  g_levelController = g_level;
  CHECK(!hostgame::out_of_contract());
  g_levelController = nullptr;
  give_room(&map::LevelController_instance);  // a controller it cannot read is no proof of the lobby
  CHECK(!hostgame::out_of_contract());
  give_room();
  // The host refuses the read (a static constructor that threw, a size other than the field's): no proof either.
  fake::static_field(map::LevelController_instance, &g_levelController, sizeof(int32_t), true);
  CHECK(!hostgame::out_of_contract());
}

void out_of_room_needs_in_room() {
  g_inRoom = false;
  give_room();
  CHECK(hostgame::out_of_room());
  g_inRoom = true;
  CHECK(!hostgame::out_of_room());
  g_inRoom = false;
  give_room(&map::PhotonNetwork_get_InRoom);  // a room it cannot read is no proof of being outside one
  CHECK(!hostgame::out_of_room());
  g_inRoom = true;
}

// A cursor it cannot read is not one to free, whichever way the default would compare.
int32_t g_lockState = 1;
int32_t lock_state(const void*) { return g_lockState; }
void cursor_needs_both_entries() {
  fake::reset();
  fake::method(map::Cursor_get_lockState, reinterpret_cast<void*>(&lock_state));
  fake::value(map::CursorLockMode_Locked, 1);
  std::string missing;
  api::bind(fake::host(), missing);
  CHECK(hostgame::cursor_locked());
  g_lockState = 0;
  CHECK(!hostgame::cursor_locked());
  fake::forget(map::CursorLockMode_Locked);
  api::bind(fake::host(), missing);
  CHECK(!hostgame::cursor_locked());  // 0 == 0 through both defaults
  fake::forget(map::Cursor_get_lockState);
  api::bind(fake::host(), missing);
  CHECK(!hostgame::cursor_locked());
  g_lockState = 1;
}


// The room's player list, told apart from a room with nobody in it: an online room always holds this client, so an empty
// list, a null one or a number that cannot be read is no reading. Single player is alone however its list reads.
void* player_list(const void*) { return g_playerList; }
void player_list_says_whether_it_read() {
  give_room();
  fake::method(map::PhotonNetwork_get_PlayerList, reinterpret_cast<void*>(&player_list));
  std::string missing;
  api::bind(fake::host(), missing);
  void** two = nullptr;
  g_playerList = fake::array(sizeof(void*), 2);
  two = static_cast<void**>(fake::host()->array_data(g_playerList));
  two[0] = &g_localPlayer;
  two[1] = &g_masterPlayer;
  g_inRoom = true;
  g_offline = false;
  g_localActor = 1;
  g_masterActor = 2;
  std::vector<int> actors;
  CHECK(hostgame::room_actors(actors) && (actors == std::vector<int>{1, 2}));
  // A number that cannot be read.
  g_masterActor = 0;
  CHECK(!hostgame::room_actors(actors));
  g_masterActor = 2;
  // A slot with nobody in it.
  two[1] = nullptr;
  CHECK(!hostgame::room_actors(actors));
  // An empty list, and no list, in an online room.
  g_playerList = fake::array(sizeof(void*), 0);
  CHECK(!hostgame::room_actors(actors) && actors.empty());
  g_playerList = nullptr;
  CHECK(!hostgame::room_actors(actors) && actors.empty());
  // Single player is alone, whatever the list says.
  g_offline = true;
  CHECK(hostgame::room_actors(actors) && actors.empty());
  g_offline = false;
  // Out of a room there is nobody to read.
  g_inRoom = false;
  CHECK(hostgame::room_actors(actors) && actors.empty());
  g_inRoom = true;
  // The member missing: the same as an unreadable list.
  give_room(&map::PhotonNetwork_get_PlayerList);
  CHECK(!hostgame::room_actors(actors) && actors.empty());
  g_localActor = g_masterActor = 1;
}

}  // namespace

int main() {
  player_list_says_whether_it_read();
  cursor_needs_both_entries();
  out_of_room_needs_in_room();
  out_of_contract_needs_the_instance();
  private_room_needs_every_member();
  master_needs_actor_numbers();
  std::printf(g_failures ? "%d check(s) failed\n" : "all game gates tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
