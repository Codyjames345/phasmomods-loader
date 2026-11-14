#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "Difficulty.h"
#include "GameController.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "GhostWanderState.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "LevelStats.h"
#include "LocalisationSystem.h"
#include "Photon.h"
#include "check.h"
#include "text_fixture.h"
#include "fake_game.h"
#include "game.h"
#include "game_api.h"
#include "notice.h"
#include "round.h"
#include "runtime.h"
#include "session.h"

// Offline test of a ghost-player contract's setup (mods/playable_ghost/round.cpp) over the fake game: the spawn held
// at its type roll, the chosen type and room answering the native rolls only inside the spawn, the favourite room's
// claim window only after the wander's own change, the truck's warp and its door. Run: build\round_test.exe
namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct Object {
  void* klass = nullptr;
  char slots[0x100] = {};
};
template <class T>
void put(Object& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

// The test's own offsets; the fake backend reads them in the object itself.
constexpr int32_t kLevel = 0x10, kType = 0x18, kRooms = 0x10, kCurrent = 0x18, kInfo = 0x10, kFavourite = 0x10;
constexpr int32_t kClosed = 0x10, kChanged = 0x10, kSender = 0x08, kRoomIndex = 0x28;

constexpr int kGhostNone = 24, kSpirit = 3, kWraith = 5, kBanshee = 9;

Object controller, level, ghost, info, stats, room0, room1, room2, button;
void* controllerStatic = &controller;
void* valuesStatic = nullptr;
void* rooms = nullptr;
bool privateRoom = true;
int dictionary;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

int nativeType = kSpirit, nativeRoom = 2, spawnSends = 0, warps = 0, changes = 0, presses = 0, waitNotices = 0;
bool rollOutside = false;
bool noRoll = false;       // the spawn returns before its type roll (the game's own early return)
bool spawnThrows = false;  // the spawn throws once it has rolled
unsigned long long clockMs = 1000;
unsigned long long test_clock() { return clockMs; }
void* lastChangedTo = nullptr;

int32_t call_pick_type(void* dict) {
  return reinterpret_cast<int32_t (*)(void*, const void*)>(detours.at(map::GhostController_PickType.key))(dict, nullptr);
}
int32_t room_index_of(Object& o) {
  int32_t index = 0;
  std::memcpy(&index, reinterpret_cast<char*>(&o) + kRoomIndex, sizeof index);
  return index;
}
// The creation RPC, through its hook: the ghost takes the traits' room as they are then.
void call_create_ghost(void* self) {
  Object messageInfo;
  reinterpret_cast<void (*)(void*, bool, int32_t, void*, const void*)>(detours.at(map::GhostController_CreateGhost.key))(
      self, false, 0, &messageInfo, nullptr);
}
void create_ghost_body(void* self, bool, int32_t, void*, const void*) {
  put<void*>(info, kFavourite, pm::api::RefArray{rooms}[static_cast<size_t>(room_index_of(*static_cast<Object*>(self)))]);
}

// The game's roll: the type through its hooked roll, the favourite room into the traits, then the creation RPC,
// which runs at once inside the spawn.
void spawn_body(void* self, const void*) {
  fake::spawn_running(true);
  fake::set_inside(map::GhostController_Spawn, 1);
  try {
    if (rollOutside) {  // a roll and a creation while the choice stands, by something other than the spawn
      fake::set_inside(map::GhostController_Spawn, 0);
      CHECK(call_pick_type(&dictionary) == nativeType);
      put<int32_t>(*static_cast<Object*>(self), kRoomIndex, nativeRoom);
      call_create_ghost(self);
      CHECK(room_index_of(*static_cast<Object*>(self)) == nativeRoom);
      fake::set_inside(map::GhostController_Spawn, 1);
    }
    if (noRoll) {
      ++spawnSends;  // its setup without a roll the hook could answer
      fake::spawn_running(false);
      fake::set_inside(map::GhostController_Spawn, 0);
      return;
    }
    const int32_t type = call_pick_type(&dictionary);
    put<int32_t>(*static_cast<Object*>(self), kType, type);
    if (spawnThrows) throw std::runtime_error("spawn");
    put<int32_t>(*static_cast<Object*>(self), kRoomIndex, nativeRoom);
    call_create_ghost(self);
    ++spawnSends;
  } catch (...) {
    fake::spawn_running(false);
    fake::set_inside(map::GhostController_Spawn, 0);
    throw;
  }
  fake::spawn_running(false);
  fake::set_inside(map::GhostController_Spawn, 0);
}
std::vector<int> rolls;  // the native roll's draws in order, before it falls back to nativeType
int32_t pick_type_body(void*, const void*) {
  if (rolls.empty()) return nativeType;
  const int r = rolls.front();
  rolls.erase(rolls.begin());
  return r;
}
// The content layer's flags of a type: kNotPlayable carries PM_GHOST_NOT_PLAYABLE (a made-up custom type).
constexpr int kNotPlayable = 30;
uint32_t test_flags_of(int32_t type) { return type == kNotPlayable ? PM_GHOST_NOT_PLAYABLE : 0u; }
void change_room_body(void*, void* to, const void*) {
  ++changes;
  lastChangedTo = to;
  put<void*>(info, kFavourite, to);
  put<int32_t>(stats, kChanged, *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(&stats) + kChanged) + 1);
}
void warp_body(void*, const void*) { ++warps; }
void attempt_use_body(void*, void*, const void*) { ++presses; }
void* get_stats(const void*) { return &stats; }
void* localised(void* key, bool, const void*) { return key; }

void give_entries() {
  fake::reset();
  fake::method(map::GhostController_Spawn, reinterpret_cast<void*>(&spawn_body));
  fake::method(map::GhostController_PickType, reinterpret_cast<void*>(&pick_type_body));
  fake::method(map::GhostController_CreateGhost, reinterpret_cast<void*>(&create_ghost_body));
  fake::method(map::GhostController_ChangeFavouriteRoomTo, reinterpret_cast<void*>(&change_room_body));
  fake::method(map::GhostController_WarpGhostToFavouriteRoom, reinterpret_cast<void*>(&warp_body));
  fake::method(map::LiftButton_AttemptUse, reinterpret_cast<void*>(&attempt_use_body));
  fake::method(map::LevelStats_get_Instance, reinterpret_cast<void*>(&get_stats));
  fake::method(map::LocalisationSystem_GetLocalisedValue, reinterpret_cast<void*>(&localised));
  fake::method(map::GhostWanderState_Exit, nullptr);
  fake::method(map::LiftButton_NetworkedUse, nullptr);
  fake::static_field(map::GhostController_instance, &controllerStatic, sizeof controllerStatic, true);
  fake::static_field(map::LevelValues_instance, &valuesStatic, sizeof valuesStatic, true);
  fake::field(map::GhostController_levelController, kLevel);
  fake::field(map::GhostController_ghostType, kType);
  fake::field(map::GhostController_favouriteRoomIndex, kRoomIndex);
  fake::field(map::GhostController_allGhostsCollection, 0x20);
  fake::field(map::LevelController_rooms, kRooms);
  fake::field(map::LevelController_currentGhostRoom, kCurrent);
  fake::field(map::GhostAI_info, kInfo);
  fake::field(map::GhostInfo_favouriteRoom, kFavourite);
  fake::field(map::LevelStats_roomsChanged, kChanged);
  fake::field(map::LiftButton_isClosed, kClosed);
  fake::field(map::PhotonMessageInfo_Sender, kSender);
  fake::field(map::LevelRoom_name, 0x20);
  fake::field(map::GhostCollection_ghosts, 0x10);
  fake::field(map::GhostData_type, 0x10);
  fake::field(map::GhostData_ghostNameLocalisationKey, 0x18);
  fake::field(map::LevelValues_difficulty, 0x10);
  fake::field(map::Difficulty_ghostTypeOverride, 0x10);
  fake::value(map::GhostType_None, kGhostNone);
}

pm::session::Session reset() {
  pm::round::teardown();
  give_entries();
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  capturing.content_flags_of = &test_flags_of;
  rolls.clear();
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::round::create_hooks());
  rooms = pm::fake::array(sizeof(void*), 3);
  for (size_t i = 0; i < 3; ++i) pm::api::RefArray{rooms}.set(i, i == 0 ? &room0 : i == 1 ? &room1 : &room2);
  put<void*>(controller, kLevel, &level);
  put<int32_t>(controller, kType, -1);
  put<void*>(level, kRooms, rooms);
  put<void*>(ghost, kInfo, &info);
  put<void*>(info, kFavourite, &room0);
  put<int32_t>(stats, kChanged, 0);
  put<bool>(button, kClosed, true);
  fake::dictionary(&dictionary, {{kSpirit, 0}, {kWraith, 10}, {kBanshee, 20}, {300, 5}});
  nativeType = kSpirit;
  nativeRoom = 2;
  spawnSends = warps = changes = presses = waitNotices = 0;
  privateRoom = true;
  noRoll = spawnThrows = false;
  clockMs = 1000;
  pm::round::set_clock(&test_clock);
  pm::session::Session s;
  s.set_volunteer(true);
  for (uint64_t now = 1000; now < 60000 && !s.local_is_ghost(); now += 100) s.observe(1, 1, {1}, now);
  CHECK(s.local_is_ghost() && s.local_is_master());
  s.start_round();
  return s;
}

void call_spawn() {
  reinterpret_cast<void (*)(void*, const void*)>(detours.at(map::GhostController_Spawn.key))(&controller, nullptr);
}
void call_change(void* to) {
  reinterpret_cast<void (*)(void*, void*, const void*)>(detours.at(map::GhostController_ChangeFavouriteRoomTo.key))(&controller, to, nullptr);
}

// The spawn is held: the native roll reached, what it could give offered (positive weights, numbers the session can
// carry), and nothing written or sent.
void the_spawn_is_held_at_its_roll() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  CHECK(fake::spawn_abandons() == 1);
  CHECK(spawnSends == 0);
  CHECK((s.pick_offer().types == std::vector<uint8_t>{kWraith, kBanshee}));
  CHECK(s.pick_offer().rooms == 3);
  CHECK(s.pick_offer().fixedType == pm::session::kNoType);
  CHECK(s.locked(clockMs));
}

// Without a positive weight the roll's own fallback: every key.
void no_positive_weight_offers_every_key() {
  auto s = reset();
  pm::round::tick(s);
  fake::dictionary(&dictionary, {{kSpirit, 0}, {kWraith, 0}});
  call_spawn();
  CHECK((s.pick_offer().types == std::vector<uint8_t>{kSpirit, kWraith}));
}

// The choice answers the spawn's two rolls. Outside a running spawn the rolls are the game's.
void the_choice_answers_only_inside_the_spawn() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  pm::session::Choice choice;
  choice.type = kBanshee;
  choice.room = 1;
  s.send_pick(choice);
  rollOutside = true;
  pm::round::tick(s);
  rollOutside = false;
  CHECK(spawnSends == 1);
  int32_t type = 0;
  std::memcpy(&type, reinterpret_cast<char*>(&controller) + kType, sizeof type);
  CHECK(type == kBanshee);
  void* favourite = nullptr;
  std::memcpy(&favourite, reinterpret_cast<char*>(&info) + kFavourite, sizeof favourite);
  CHECK(favourite == &room1 && room_index_of(controller) == 1);
  CHECK(warps == 1);  // the ghost player starts in its room
  CHECK(!s.locked(clockMs));

  // A roll outside a running spawn is the game's, and a type the session did not choose stands.
  CHECK(call_pick_type(&dictionary) == nativeType);
}

// The difficulty's fixed type is offered as fixed and left to the native roll.
void a_fixed_type_is_the_rolls() {
  Object values, difficulty;
  auto s = reset();
  valuesStatic = &values;
  put<void*>(values, 0x10, &difficulty);
  put<int32_t>(difficulty, 0x10, kWraith);
  pm::round::tick(s);
  call_spawn();
  CHECK(s.pick_offer().fixedType == kWraith);
  pm::session::Choice choice;
  choice.type = kBanshee;  // refused: the difficulty fixed it
  choice.room = 1;
  s.send_pick(choice);
  pm::round::tick(s);
  int32_t type = 0;
  std::memcpy(&type, reinterpret_cast<char*>(&controller) + kType, sizeof type);
  CHECK(type == nativeType);
  valuesStatic = nullptr;
}

// A change the wander's exit makes opens the claim window; any other change does not.
void the_claim_window_follows_the_wanders_change() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  pm::session::Choice choice;
  choice.type = kWraith;
  choice.room = 0;
  s.send_pick(choice);
  pm::round::tick(s);

  call_change(&room1);  // a Monkey Paw, say: not the wander's exit
  CHECK(changes == 1);
  s.send_room_claim(0);
  pm::round::tick(s);
  CHECK(changes == 1);  // no window, so no claim is taken

  fake::set_inside(map::GhostWanderState_Exit, 1);
  call_change(&room2);
  fake::set_inside(map::GhostWanderState_Exit, 0);
  CHECK(changes == 2);

  // Claimed: the same native change moves it, and the count stays at one change more than before the claim.
  const int32_t counted = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(&stats) + kChanged);
  s.send_room_claim(0);
  pm::round::tick(s);
  CHECK(changes == 3 && lastChangedTo == &room0);
  CHECK(*reinterpret_cast<int32_t*>(reinterpret_cast<char*>(&stats) + kChanged) == counted);
}

// Only the truck's own use leaves the ghost where it is: other warps go on.
void the_trucks_warp_is_left_out() {
  auto s = reset();
  pm::round::tick(s);
  const auto warp = reinterpret_cast<void (*)(void*, const void*)>(detours.at(map::GhostController_WarpGhostToFavouriteRoom.key));
  warp(&controller, nullptr);
  CHECK(warps == 1);
  fake::set_inside(map::LiftButton_NetworkedUse, 1);
  warp(&controller, nullptr);
  fake::set_inside(map::LiftButton_NetworkedUse, 0);
  CHECK(warps == 1);
}

// The truck's button refuses its press while the ghost player chooses, and only while its doors are closed.
void the_truck_waits_for_the_choice() {
  auto s = reset();
  pm::round::tick(s);
  const auto press = reinterpret_cast<void (*)(void*, void*, const void*)>(detours.at(map::LiftButton_AttemptUse.key));
  Object infoStruct;
  press(&button, &infoStruct, nullptr);
  CHECK(presses == 1);  // nobody is choosing
  call_spawn();
  press(&button, &infoStruct, nullptr);
  CHECK(presses == 1);  // refused
  put<bool>(button, kClosed, false);
  press(&button, &infoStruct, nullptr);
  CHECK(presses == 2);  // closing is always allowed
}

// Leaving with the spawn still held runs it: a contract never lacks its ghost.
void teardown_runs_a_held_spawn() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  CHECK(spawnSends == 0);
  pm::round::teardown();
  CHECK(spawnSends == 1);
}

int32_t controller_type() {
  int32_t type = 0;
  std::memcpy(&type, reinterpret_cast<char*>(&controller) + kType, sizeof type);
  return type;
}
void* favourite_room_object() {
  void* favourite = nullptr;
  std::memcpy(&favourite, reinterpret_cast<char*>(&info) + kFavourite, sizeof favourite);
  return favourite;
}

// A client that is not the master runs the game's spawn as it is: nothing is held or offered.
void a_client_not_the_master_spawns_natively() {
  auto s = reset();
  s.observe(1, 2, {1, 2}, 70000);  // actor 2 is now the room's master
  CHECK(!s.local_is_master());
  pm::round::tick(s);
  call_spawn();
  CHECK(spawnSends == 1 && fake::spawn_abandons() == 0);
  CHECK(s.pick_offer().types.empty());
  CHECK(controller_type() == nativeType && favourite_room_object() == &room2);
}

// No choice before the deadline: the spawn runs with the native rolls, and the truck opens again.
void an_expired_pick_leaves_the_native_rolls() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  CHECK(spawnSends == 0 && s.locked(clockMs));
  pm::round::tick(s);
  CHECK(spawnSends == 0);  // still choosing
  clockMs += pm::round::kPickMs;
  pm::round::tick(s);
  CHECK(spawnSends == 1);
  CHECK(controller_type() == nativeType && favourite_room_object() == &room2);
  CHECK(!s.locked(clockMs));
}

// A chosen type the weights no longer allow is left to the native roll; the chosen room still holds.
void a_type_no_longer_allowed_leaves_the_native_roll() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  pm::session::Choice choice;
  choice.type = kBanshee;
  choice.room = 1;
  s.send_pick(choice);
  fake::dictionary(&dictionary, {{kSpirit, 0}, {kWraith, 10}, {kBanshee, 0}});
  pm::round::tick(s);
  CHECK(spawnSends == 1);
  CHECK(controller_type() == nativeType);
  CHECK(favourite_room_object() == &room1);
}

// A ghost no player plays (PM_GHOST_NOT_PLAYABLE, user 2026-11-07, the Squatter) is never offered to the ghost player,
// never chosen at random for them and never kept from a pick; another custom type still is.
void a_ghost_no_player_plays_is_never_theirs() {
  auto s = reset();
  pm::round::tick(s);
  fake::dictionary(&dictionary, {{kWraith, 10}, {kNotPlayable, 50}, {31, 5}});
  call_spawn();
  CHECK((s.pick_offer().types == std::vector<uint8_t>{kWraith, 31}));
  // Chosen anyway (an old or forged pick): not kept, since it was never offered; the native roll comes, drawn again
  // past it.
  pm::session::Choice choice;
  choice.type = kNotPlayable;
  choice.room = 1;
  s.send_pick(choice);
  rolls = {kNotPlayable, kNotPlayable, kWraith};
  pm::round::tick(s);
  clockMs += pm::round::kPickMs;
  pm::round::tick(s);
  CHECK(spawnSends == 1);
  CHECK(controller_type() == kWraith);
  // Unchosen: the native roll, drawn again past it.
  s = reset();
  pm::round::tick(s);
  fake::dictionary(&dictionary, {{kWraith, 10}, {kNotPlayable, 50}, {31, 5}});
  call_spawn();
  rolls = {kNotPlayable, 31};
  clockMs += pm::round::kPickMs;
  pm::round::tick(s);
  CHECK(spawnSends == 1 && controller_type() == 31);
}

// Outside a ghost-player round the roll stands, whatever it gives.
void without_a_ghost_player_the_roll_stands() {
  give_entries();
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  capturing.content_flags_of = &test_flags_of;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::round::create_hooks());
  rolls = {kNotPlayable};
  CHECK(call_pick_type(&dictionary) == kNotPlayable);
}

// A spawn that returns before its type roll has run natively already: nothing is offered or held, and
// later calls run as they are.
void a_probe_without_a_roll_holds_nothing() {
  auto s = reset();
  noRoll = true;
  pm::round::tick(s);
  call_spawn();
  CHECK(spawnSends == 1 && fake::spawn_abandons() == 0);
  CHECK(s.pick_offer().types.empty() && !s.locked(clockMs));
  call_spawn();
  CHECK(spawnSends == 2);
}

// A held spawn that throws as it is run through does not leave the spawn let through afterwards: a
// trigger of it meanwhile is still ignored, as for any held spawn.
void a_throwing_resume_lets_nothing_through_afterwards() {
  auto s = reset();
  pm::round::tick(s);
  call_spawn();
  spawnThrows = true;
  bool threw = false;
  try {
    pm::round::teardown();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw && spawnSends == 0);
  spawnThrows = false;
  call_spawn();
  CHECK(spawnSends == 0);  // not run through
}

}  // namespace

// Core's helpers the file still uses, modelled.
namespace il2cpp {
namespace {
std::vector<void*> g_holds;  // gc_hold's handles (index + 1); a released one is null
}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  g_holds.push_back(object);
  return static_cast<uint32_t>(g_holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) g_holds[handle - 1] = nullptr;
}
}  // namespace il2cpp
namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::game {
bool private_room() { return privateRoom; }
api::Ref<pm::map::LevelController> level_controller() { return api::Ref<pm::map::LevelController>::unchecked(&level); }
api::Ref<pm::map::GhostAI> ghost_ai() { return api::Ref<pm::map::GhostAI>::unchecked(&ghost); }
bool alive(void* o) { return o != nullptr; }
int actor_number(void*) { return 2; }
}  // namespace pm::game
namespace pm::notice {
void show_wait(unsigned, const std::string&, const std::string&) { ++waitNotices; }
void tick_wait(unsigned, const std::string&) {}
}  // namespace pm::notice

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  the_spawn_is_held_at_its_roll();
  no_positive_weight_offers_every_key();
  the_choice_answers_only_inside_the_spawn();
  a_fixed_type_is_the_rolls();
  the_claim_window_follows_the_wanders_change();
  the_trucks_warp_is_left_out();
  the_truck_waits_for_the_choice();
  teardown_runs_a_held_spawn();
  a_client_not_the_master_spawns_natively();
  an_expired_pick_leaves_the_native_rolls();
  a_type_no_longer_allowed_leaves_the_native_roll();
  a_ghost_no_player_plays_is_never_theirs();
  without_a_ghost_player_the_roll_stands();
  a_probe_without_a_roll_holds_nothing();
  a_throwing_resume_lets_nothing_through_afterwards();
  pm::round::teardown();
  std::printf("round: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
