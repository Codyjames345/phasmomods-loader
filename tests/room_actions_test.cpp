// Offline test of the join guard's game steps (host/room_actions.cpp) over the fake game: the kick reaches the card that
// names the actor and no other, and an entry that did not resolve, or a card that cannot be proved to be theirs, makes the
// step report that it did not run, so the guard asks again rather than believing it did. Run: build\room_actions_test.exe
#include <windows.h>

#include <vector>

#include "LevelController.h"
#include "Photon.h"
#include "Player.h"
#include "ServerManager.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "managed_exception.h"
#include "room_actions.h"
#include "runtime.h"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

int managerClass, profileClass, spotClass, otherClass;
constexpr int32_t kCards = 0x10, kSpot = 0x18, kPlayer = 0x20, kActor = 0x28, kStartButton = 0x30;

struct Node {
  void* klass = nullptr;
  char fields[0x40]{};
};
Node sceneManager, firstCard, secondCard, firstSpot, secondSpot, firstPlayer, secondPlayer;
int kicks = 0, leaves = 0;
bool kickThrows = false, leaveThrows = false;
void* kickedCard = nullptr;
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}

/** Supply deterministic time for the production scene-search throttle. */
ULONGLONG WINAPI clock_ms() { return 1000; }
void* find_manager(void* type, bool includeInactive, const void*) {
  CHECK(type == phasmo::ServerManager::type.type_object());
  CHECK(!includeInactive);
  return &sceneManager;
}
void leave_server(void* manager, const void*) {
  CHECK(manager == &sceneManager);
  ++leaves;
  if (leaveThrows) throw Il2CppExceptionWrapper{nullptr};
}
void kick_player(void* manager, void* card, const void*) {
  CHECK(manager == &sceneManager);
  ++kicks;
  kickedCard = card;
  if (kickThrows) throw Il2CppExceptionWrapper{nullptr};
}
bool queueRunning = true;
bool message_queue_running(const void*) { return queueRunning; }
void* levelController = nullptr;
void answer_nothing() {}  // stands for each of the guard's readings: nothing here calls them
int32_t actor_number(void* player, const void*) {
  return *reinterpret_cast<int32_t*>(static_cast<char*>(player) + kActor);
}
template <class T>
void put(Node& object, int32_t offset, T value) {
  *reinterpret_cast<T*>(reinterpret_cast<char*>(&object) + offset) = value;
}
}  // namespace

#define GetTickCount64 clock_ms
#include "../host/room_actions.cpp"
#undef GetTickCount64

namespace il2cpp {
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
void log_exception(const char*, const void*) {}
}  // namespace il2cpp

namespace {

// The lobby with two cards, actors 4 and 9, and every entry the steps read but the one `without` names.
void give(const pm::sdk::EntryDesc* without = nullptr) {
  pm::room_actions::g_manager = nullptr;
  pm::room_actions::g_nextSearch = 0;
  fake::reset();
  for (Node* n : {&sceneManager, &firstCard, &secondCard, &firstSpot, &secondSpot, &firstPlayer, &secondPlayer}) *n = {};
  sceneManager.klass = &managerClass;
  firstCard.klass = secondCard.klass = &profileClass;
  firstSpot.klass = secondSpot.klass = &spotClass;
  put(firstPlayer, kActor, 4);
  put(secondPlayer, kActor, 9);
  put(firstSpot, kPlayer, static_cast<void*>(&firstPlayer));
  put(secondSpot, kPlayer, static_cast<void*>(&secondPlayer));
  put(firstCard, kSpot, static_cast<void*>(&firstSpot));
  put(secondCard, kSpot, static_cast<void*>(&secondSpot));
  put(sceneManager, kCards, fake::list({&firstCard, &secondCard}));
  fake::type(map::ServerManager, &managerClass);
  fake::type(map::MultiplayerProfile, &profileClass);
  fake::type(map::PlayerSpot, &spotClass);
  fake::field(map::ServerManager_cards, kCards);
  fake::field(map::ProfileCard_spot, kSpot);
  fake::field(map::PlayerSpot_photonPlayer, kPlayer);
  fake::method(map::UnityObject_FindObjectOfType, reinterpret_cast<void*>(&find_manager));
  fake::method(map::ServerManager_LeaveServer, reinterpret_cast<void*>(&leave_server));
  fake::method(map::ServerManager_KickPlayer, reinterpret_cast<void*>(&kick_player));
  fake::method(map::PhotonPlayer_get_ActorNumber, reinterpret_cast<void*>(&actor_number));
  fake::method(map::PhotonNetwork_get_IsMessageQueueRunning, reinterpret_cast<void*>(&message_queue_running));
  for (const pm::sdk::EntryDesc* d : {&map::PhotonNetwork_get_InRoom, &map::PhotonNetwork_get_LocalPlayer,
                                      &map::PhotonNetwork_get_MasterClient, &map::PhotonNetwork_get_PlayerList,
                                      &map::PhotonNetwork_get_CurrentRoom, &map::PhotonNetwork_get_OfflineMode,
                                      &map::PhotonRoom_get_IsVisible})
    fake::method(*d, reinterpret_cast<void*>(&answer_nothing));
  fake::static_field(map::LevelController_instance, &levelController, sizeof levelController, true);
  fake::field(map::ServerManager_startGameButton, kStartButton);
  put(sceneManager, kStartButton, static_cast<void*>(&firstSpot));  // any object stands for the button
  queueRunning = true;
  if (without) fake::forget(*without);
  std::string missing;
  pm::api::bind(fake::host(), missing);
  kicks = leaves = 0;
  kickThrows = leaveThrows = false;
  kickedCard = nullptr;
}

void kick_reaches_the_actors_card() {
  give();
  CHECK(pm::room_actions::lobby_open());
  CHECK(pm::room_actions::kick(9));
  CHECK(kicks == 1 && kickedCard == &secondCard);
  CHECK(!pm::room_actions::kick(27));
  CHECK(kicks == 1);
  CHECK(pm::room_actions::leave_lobby());
  CHECK(leaves == 1);
}

// ServerManager.LoadScene, a contract's start on every client, stops Photon's queue while the old scene is still up and the
// lobby's manager still alive. That is no lobby frame: nothing is judged and no mod is switched while a contract's scene
// loads. It is one again when the game turns the queue back on. A build without the getter never reads as a lobby frame,
// and the host starts no mod on such a build (room_actions::missing).
void a_contract_loading_is_no_lobby_frame() {
  give();
  CHECK(pm::room_actions::lobby_open());
  queueRunning = false;
  CHECK(!pm::room_actions::lobby_open());
  queueRunning = true;
  CHECK(pm::room_actions::lobby_open());
  give(&map::PhotonNetwork_get_IsMessageQueueRunning);
  CHECK(!pm::room_actions::lobby_open());
  CHECK(pm::room_actions::missing().find("PhotonNetwork.get_IsMessageQueueRunning") != std::string::npos);
  give();
  CHECK(pm::room_actions::missing().empty());
}

// Actor numbers that cannot be read are all 0: an ask for actor 0 must not find the first card.
void an_unknown_actor_has_no_card() {
  give(&map::PhotonPlayer_get_ActorNumber);
  CHECK(!pm::room_actions::kick(0));
  CHECK(!pm::room_actions::kick(9));
  CHECK(kicks == 0);
  give();
  CHECK(!pm::room_actions::kick(0));
  CHECK(kicks == 0);
}

// Without any one entry the step did not run, and says so: the guard asks again later and believes nothing happened.
void each_entry_missing_stops_the_step() {
  give();
  CHECK(pm::room_actions::available());
  CHECK(pm::room_actions::missing().empty());
  for (const pm::sdk::EntryDesc* d :
       {&map::ServerManager, &map::MultiplayerProfile, &map::ServerManager_LeaveServer, &map::PhotonPlayer_get_ActorNumber,
        &map::PlayerSpot, &map::ServerManager_cards, &map::ProfileCard_spot, &map::PlayerSpot_photonPlayer,
        &map::UnityObject_FindObjectOfType, &map::ServerManager_KickPlayer}) {
    give(d);
    CHECK(!pm::room_actions::available());
    CHECK(pm::room_actions::missing().find(d->key) != std::string::npos);  // the start gate's line names it
  }
  for (const pm::sdk::EntryDesc* d :
       {&map::ServerManager, &map::MultiplayerProfile, &map::PlayerSpot, &map::ServerManager_cards, &map::ProfileCard_spot,
        &map::PlayerSpot_photonPlayer, &map::UnityObject_FindObjectOfType, &map::ServerManager_KickPlayer}) {
    give(d);
    CHECK(!pm::room_actions::kick(9));
    CHECK(kicks == 0);
  }
  for (const pm::sdk::EntryDesc* d : {&map::ServerManager, &map::UnityObject_FindObjectOfType, &map::ServerManager_LeaveServer}) {
    give(d);
    CHECK(!pm::room_actions::leave_lobby());
    CHECK(leaves == 0);
  }
  for (const pm::sdk::EntryDesc* d : {&map::ServerManager, &map::UnityObject_FindObjectOfType}) {
    give(d);
    CHECK(!pm::room_actions::lobby_open());
  }
}

// What the guard reads to know where it is. A build lacking one could not see a public lobby or a player, so `opening`
// answers Declined and its line names the key (mod_session_test).
void the_guards_own_readings_are_required() {
  for (const pm::sdk::EntryDesc* d : {&map::PhotonNetwork_get_InRoom, &map::PhotonNetwork_get_LocalPlayer,
                                      &map::PhotonNetwork_get_MasterClient, &map::PhotonNetwork_get_PlayerList,
                                      &map::PhotonNetwork_get_CurrentRoom, &map::PhotonNetwork_get_OfflineMode,
                                      &map::PhotonRoom_get_IsVisible, &map::LevelController_instance,
                                      &map::PhotonPlayer_get_ActorNumber, &map::PhotonNetwork_get_IsMessageQueueRunning}) {
    give(d);
    CHECK(!pm::room_actions::available());
    CHECK(pm::room_actions::missing().find(d->key) != std::string::npos);
  }
  give();
  CHECK(pm::room_actions::missing().empty());
}

void the_start_button_is_the_lobbys_own() {
  give();
  CHECK(pm::room_actions::is_start_button(&firstSpot));
  CHECK(!pm::room_actions::is_start_button(&secondSpot) && !pm::room_actions::is_start_button(nullptr));
  give(&map::ServerManager_startGameButton);
  CHECK(!pm::room_actions::is_start_button(&firstSpot));
  give(&map::ServerManager);
  CHECK(!pm::room_actions::is_start_button(&firstSpot));
}

void a_card_of_another_shape_is_no_card() {
  give();
  firstSpot.klass = secondSpot.klass = &otherClass;
  CHECK(!pm::room_actions::kick(9));
  CHECK(kicks == 0);
  give();
  firstCard.klass = secondCard.klass = &otherClass;
  CHECK(!pm::room_actions::kick(9));
  CHECK(kicks == 0);
  give();
  put(firstSpot, kPlayer, static_cast<void*>(nullptr));
  put(secondSpot, kPlayer, static_cast<void*>(nullptr));
  CHECK(!pm::room_actions::kick(9));
  CHECK(kicks == 0);
  give();
  fake::destroy(&secondCard);
  CHECK(!pm::room_actions::kick(9));
  CHECK(kicks == 0);
}

void a_step_that_threw_did_not_run() {
  give();
  kickThrows = true;
  CHECK(!pm::room_actions::kick(9));
  leaveThrows = true;
  CHECK(!pm::room_actions::leave_lobby());
}

// The ServerManager kept between frames stays reachable for the GC until it is dropped.
void the_manager_is_held() {
  give();
  CHECK(pm::room_actions::leave_lobby());
  CHECK(held(&sceneManager));
  pm::room_actions::g_manager = nullptr;
  CHECK(!held(&sceneManager));
}

}  // namespace

int main() {
  kick_reaches_the_actors_card();
  a_contract_loading_is_no_lobby_frame();
  an_unknown_actor_has_no_card();
  each_entry_missing_stops_the_step();
  the_guards_own_readings_are_required();
  the_start_button_is_the_lobbys_own();
  a_card_of_another_shape_is_no_card();
  a_step_that_threw_did_not_run();
  the_manager_is_held();
  return g_failures ? 1 : 0;
}
