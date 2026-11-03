#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "Difficulty.h"
#include "GameController.h"
#include "Photon.h"
#include "body.h"
#include "check.h"
#include "contract_end.h"
#include "fake_game.h"
#include "game_api.h"
#include "leave.h"
#include "outcome.h"
#include "reward.h"
#include "runtime.h"

// Offline test of the contract's end on the ghost player's client (mods/playable_ghost/contract_end.cpp) over the
// fake game: which way each call goes, and in what order the mod's and the game's steps run. Run:
// build\contract_end_test.exe
namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct Object {
  void* klass = nullptr;
  char slots[0x40] = {};
};
template <class T>
void put(Object& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}
template <class T>
T read(Object& o, int32_t offset) {
  T value{};
  std::memcpy(&value, reinterpret_cast<char*>(&o) + offset, sizeof value);
  return value;
}

// The test's own offsets; the fake backend reads them in the object itself.
constexpr int32_t kSender = 0x08, kActor = 0x10, kCompleted = 0x10, kBlocked = 0x11, kPlayerDied = 0x12;

Object exitLevel, messageInfo, values, controller, firstActor, secondActor, settings;
void* valuesStatic = &values;
void* controllerStatic = &controller;
void* players = nullptr;
bool inRoom = true, offline = false, steppedOut = true, comesBack = true;
std::vector<std::string> events;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

bool exitThrows = false, gameExitThrows = false, investigatorDead = false;
void missions_body(const void*) { events.push_back("missions"); }
void challenges_body(const void*) { events.push_back("challenges"); }
void call_check(const pm::sdk::EntryDesc& entry) { reinterpret_cast<void (*)(const void*)>(detours.at(entry.key))(nullptr); }
// The game's body, as far as the mod meets it: it calls the two checks through their hooks.
// ExitLevel.Exit writes the death status only when this client's investigator is dead.
void exit_body(void*, void*, const void*) {
  events.push_back("exit");
  if (investigatorDead) put<bool>(settings, kPlayerDied, true);
  if (exitThrows) throw std::runtime_error("exit");
  call_check(map::ExitLevel_CheckMissions);
  call_check(map::ExitLevel_CheckChallenges);
  events.push_back("exit done");
}
// GameController.Exit writes the death status on every client, before anything that can throw here.
void game_exit_body(void*, void*, const void*) {
  events.push_back("game exit");
  put<bool>(settings, kPlayerDied, true);
  if (gameExitThrows) throw std::runtime_error("game exit");
  call_check(map::ExitLevel_CheckMissions);
  call_check(map::ExitLevel_CheckChallenges);
  events.push_back("game exit done");
}
void load_back(void*, const void*) { events.push_back("load back"); }
bool get_in_room(const void*) { return inRoom; }
bool get_offline(const void*) { return offline; }
void* get_players(const void*) {
  events.push_back("players read");
  return players;
}
void* get_settings(const void*) { return &settings; }
int32_t actor_number(void* player, const void*) { return read<int32_t>(*static_cast<Object*>(player), kActor); }

void call_exit() {
  reinterpret_cast<void (*)(void*, void*, const void*)>(detours.at(map::ExitLevel_Exit.key))(&exitLevel, &messageInfo,
                                                                                            nullptr);
}

void call_game_exit() {
  reinterpret_cast<void (*)(void*, void*, const void*)>(detours.at(map::GameController_Exit.key))(
      &controller, &messageInfo, nullptr);
}

void reset() {
  fake::reset();
  fake::method(map::GameController_Exit, reinterpret_cast<void*>(&game_exit_body));
  fake::method(map::ExitLevel_Exit, reinterpret_cast<void*>(&exit_body));
  fake::method(map::ExitLevel_CheckMissions, reinterpret_cast<void*>(&missions_body));
  fake::method(map::ExitLevel_CheckChallenges, reinterpret_cast<void*>(&challenges_body));
  fake::method(map::GameController_LoadBackToMenu, reinterpret_cast<void*>(&load_back));
  fake::method(map::PhotonNetwork_get_InRoom, reinterpret_cast<void*>(&get_in_room));
  fake::method(map::PhotonNetwork_get_OfflineMode, reinterpret_cast<void*>(&get_offline));
  fake::method(map::PhotonNetwork_get_PlayerList, reinterpret_cast<void*>(&get_players));
  fake::method(map::PhotonPlayer_get_ActorNumber, reinterpret_cast<void*>(&actor_number));
  fake::method(map::GameSettings_gameplay, reinterpret_cast<void*>(&get_settings));
  fake::field(map::GameplaySettings_PlayerDied, kPlayerDied);
  fake::static_field(map::LevelValues_instance, &valuesStatic, sizeof valuesStatic, true);
  fake::static_field(map::GameController_instance, &controllerStatic, sizeof controllerStatic, true);
  fake::field(map::LevelValues_completed, kCompleted);
  fake::field(map::LevelValues_eventsBlocked, kBlocked);
  fake::field(map::PhotonMessageInfo_Sender, kSender);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::contract_end::create_hooks());
  players = fake::array(sizeof(void*), 2);
  pm::api::RefArray{players}.set(0, &secondActor);  // the list's first is not the lowest number
  pm::api::RefArray{players}.set(1, &firstActor);
  put<int32_t>(firstActor, kActor, 3);
  put<int32_t>(secondActor, kActor, 8);
  put<void*>(messageInfo, kSender, &firstActor);
  put<bool>(values, kCompleted, false);
  put<bool>(values, kBlocked, false);
  inRoom = steppedOut = comesBack = true;
  put<bool>(settings, kPlayerDied, false);
  offline = exitThrows = gameExitThrows = investigatorDead = false;
  events.clear();
}

bool sequence(const std::vector<std::string>& expected) {
  std::vector<std::string> seen;
  for (const std::string& e : events)
    if (e != "players read") seen.push_back(e);
  if (seen == expected) return true;
  std::printf("  events:");
  for (const std::string& e : seen) std::printf(" [%s]", e.c_str());
  std::printf("\n");
  return false;
}

// An investigator's client: straight to the game's body, without reading the room.
void an_investigators_client_runs_the_game() {
  reset();
  steppedOut = false;
  put<void*>(messageInfo, kSender, &secondActor);
  call_exit();
  CHECK((events == std::vector<std::string>{"exit", "missions", "challenges", "exit done"}));
}

// A sender other than the room's first actor reaches the game's refusal: the contract does not end.
void another_sender_goes_to_the_game() {
  reset();
  put<void*>(messageInfo, kSender, &secondActor);
  call_exit();
  CHECK(sequence({"exit", "missions", "challenges", "exit done"}));
  CHECK(!read<bool>(values, kCompleted));
}

// A null sender is the game's to meet.
void a_null_sender_goes_to_the_game() {
  reset();
  put<void*>(messageInfo, kSender, nullptr);
  call_exit();
  CHECK(sequence({"exit", "missions", "challenges", "exit done"}));
}

// So is a null in the room's players, even from the sender who is the lowest number left.
void a_null_player_goes_to_the_game() {
  reset();
  pm::api::RefArray{players}.set(1, nullptr);
  put<void*>(messageInfo, kSender, &secondActor);
  call_exit();
  CHECK(sequence({"exit", "missions", "challenges", "exit done"}));
}

// While LevelValues.eventsBlocked is set: the investigator comes back, then the game's own body runs, and
// the mod's payout does not.
void events_blocked_runs_the_game_as_the_investigator() {
  reset();
  put<bool>(values, kBlocked, true);
  call_exit();
  CHECK(sequence({"teardown", "left", "exit", "missions", "challenges", "exit done"}));
}

// Without an investigator the game's body cannot run: the ghost player's own end instead.
void events_blocked_without_an_investigator_ends_the_ghost_players_way() {
  reset();
  put<bool>(values, kBlocked, true);
  comesBack = false;
  call_exit();
  CHECK(sequence({"teardown", "paid", "score", "load back"}));
  CHECK(read<bool>(values, kCompleted));
}

// The first actor's call with no block: the ghost's payout with the body still parked, then the investigator
// back, then the game's own body, whose two checks do nothing.
void the_first_actors_call_runs_the_games_bookkeeping_without_the_checks() {
  reset();
  call_exit();
  CHECK(sequence({"paid", "score", "teardown", "left", "exit", "exit done"}));
  CHECK(!read<bool>(values, kCompleted));  // the game's body's to set
}

// Without an investigator: the payout stands, then the old way back.
void without_an_investigator_the_ghost_player_goes_back_to_the_lobby() {
  reset();
  comesBack = false;
  call_exit();
  CHECK(sequence({"paid", "score", "teardown", "load back"}));
  CHECK(read<bool>(values, kCompleted));
}

// The checks are skipped only inside the ghost player's run of the game's body: any other caller, later, gets them.
void only_that_run_skips_the_checks() {
  reset();
  call_exit();
  events.clear();
  call_check(map::ExitLevel_CheckMissions);
  call_check(map::ExitLevel_CheckChallenges);
  CHECK(sequence({"missions", "challenges"}));
}

// Every player dead or gone is the other end, the one the game takes when the last investigator quits too: the
// ghost's score and pay come from it, the investigator comes back, and the game's own
// body runs without the two checks. LevelValues.eventsBlocked picks nothing here: that body has no path for it.
void the_everyone_dead_end_pays_and_runs_the_game() {
  reset();
  put<bool>(values, kBlocked, true);
  call_game_exit();
  CHECK(sequence({"paid", "score", "teardown", "left", "game exit", "game exit done"}));
}

void the_everyone_dead_end_on_an_investigators_client_is_the_games() {
  reset();
  steppedOut = false;
  put<void*>(messageInfo, kSender, &secondActor);
  call_game_exit();
  CHECK((events == std::vector<std::string>{"game exit", "missions", "challenges", "game exit done"}));
  CHECK(read<bool>(settings, kPlayerDied));  // the game's death status, for everyone else
}

// The ghost player never died: the everyone-dead end leaves their death status as it found it, so the reward
// screen shows no death and keeps their equipment. The rest of the game's body runs as before.
void the_everyone_dead_end_writes_no_death_status_for_the_ghost_player() {
  reset();
  call_game_exit();
  CHECK(sequence({"paid", "score", "teardown", "left", "game exit", "game exit done"}));
  CHECK(!read<bool>(settings, kPlayerDied));
  reset();
  put<bool>(settings, kPlayerDied, true);  // whatever it held before, it holds after
  call_game_exit();
  CHECK(read<bool>(settings, kPlayerDied));
}

// A throw out of the game's body still leaves no death status, and goes on.
void a_throwing_everyone_dead_end_writes_no_death_status() {
  reset();
  gameExitThrows = true;
  bool thrown = false;
  try {
    call_game_exit();
  } catch (const std::runtime_error&) {
    thrown = true;
  }
  CHECK(thrown);
  CHECK(!read<bool>(settings, kPlayerDied));
}

// The truck's end is the game's own: it writes the status only for a dead investigator, so the mod keeps
// nothing there.
void the_truck_end_leaves_the_death_status_to_the_game() {
  reset();
  call_exit();
  CHECK(!read<bool>(settings, kPlayerDied));
  reset();
  investigatorDead = true;
  call_exit();
  CHECK(read<bool>(settings, kPlayerDied));
}

// The refused call and the investigator's own run of the everyone-dead end are the game's, status included.
void a_refused_everyone_dead_end_keeps_nothing() {
  reset();
  put<void*>(messageInfo, kSender, &secondActor);
  call_game_exit();
  CHECK(read<bool>(settings, kPlayerDied));
}

void the_everyone_dead_end_from_another_sender_is_refused_by_the_game() {
  reset();
  put<void*>(messageInfo, kSender, &secondActor);
  call_game_exit();
  CHECK(sequence({"game exit", "missions", "challenges", "game exit done"}));
}

// A throw from the game's body goes on, and leaves nothing skipped.
void a_throwing_body_leaves_nothing_skipped() {
  reset();
  exitThrows = true;
  bool thrown = false;
  try {
    call_exit();
  } catch (const std::runtime_error&) {
    thrown = true;
  }
  CHECK(thrown);
  events.clear();
  call_check(map::ExitLevel_CheckMissions);
  CHECK(sequence({"missions"}));
}

// Offline or outside a room the game's check lets every sender through.
void outside_a_room_every_sender_counts() {
  reset();
  inRoom = false;
  put<void*>(messageInfo, kSender, &secondActor);
  call_exit();
  CHECK(sequence({"paid", "score", "teardown", "left", "exit", "exit done"}));
  reset();
  offline = true;
  put<void*>(messageInfo, kSender, &secondActor);
  call_exit();
  CHECK(sequence({"paid", "score", "teardown", "left", "exit", "exit done"}));
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::body {
bool stepped_out() { return steppedOut; }
void teardown() {
  events.push_back("teardown");
  steppedOut = false;
}
bool investigator_back() { return comesBack; }
}  // namespace pm::body
namespace pm::reward {
void contract_ended() { events.push_back("paid"); }
}  // namespace pm::reward
namespace pm::outcome {
void contract_over() { events.push_back("score"); }
}  // namespace pm::outcome
namespace pm::leave {
void left() { events.push_back("left"); }
}  // namespace pm::leave

int main() {
  an_investigators_client_runs_the_game();
  another_sender_goes_to_the_game();
  a_null_sender_goes_to_the_game();
  a_null_player_goes_to_the_game();
  events_blocked_runs_the_game_as_the_investigator();
  events_blocked_without_an_investigator_ends_the_ghost_players_way();
  the_first_actors_call_runs_the_games_bookkeeping_without_the_checks();
  without_an_investigator_the_ghost_player_goes_back_to_the_lobby();
  only_that_run_skips_the_checks();
  the_everyone_dead_end_pays_and_runs_the_game();
  the_everyone_dead_end_on_an_investigators_client_is_the_games();
  the_everyone_dead_end_writes_no_death_status_for_the_ghost_player();
  a_throwing_everyone_dead_end_writes_no_death_status();
  the_truck_end_leaves_the_death_status_to_the_game();
  a_refused_everyone_dead_end_keeps_nothing();
  the_everyone_dead_end_from_another_sender_is_refused_by_the_game();
  a_throwing_body_leaves_nothing_skipped();
  outside_a_room_every_sender_counts();
  std::printf("contract_end: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
