#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Difficulty.h"
#include "GameController.h"
#include "Photon.h"
#include "Player.h"
#include "UnityAudio.h"
#include "body.h"
#include "check.h"
#include "fake_game.h"
#include "game.h"
#include "game_api.h"
#include "leave.h"
#include "runtime.h"

// Offline test of the ghost player's Leave and Quit (mods/playable_ghost/leave.cpp) over the fake game: the
// investigator comes back, then the game's own button runs; without one the mod finishes the game's last
// steps. Run: build\leave_test.exe
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

constexpr int32_t kCompleted = 0x10, kSceneChanging = 0x11, kReady = 0x12;

Object menu, values, network, loading;
void* valuesStatic = &values;
void* loadingStatic = &loading;
bool steppedOut = true, comesBack = true;
void* level = &menu;
std::vector<std::string> events;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

void leave_body(void*, const void*) { events.push_back("game leave"); }
void quit_body(void*, const void*) { events.push_back("game quit"); }
void app_quit(int32_t, const void*) { events.push_back("application quit"); }
void load_scene(void*, bool, int32_t, void*, bool, const void*) { events.push_back("load scene"); }
void disconnect(const void*) { events.push_back("disconnect"); }
void set_volume(float, const void*) { events.push_back("silence"); }
void* get_network(const void*) { return &network; }

void call(const pm::sdk::EntryDesc& entry) {
  reinterpret_cast<void (*)(void*, const void*)>(detours.at(entry.key))(&menu, nullptr);
}

void reset() {
  fake::reset();
  fake::method(map::PauseMenuController_Leave, reinterpret_cast<void*>(&leave_body));
  fake::method(map::PauseMenuController_Quit, reinterpret_cast<void*>(&quit_body));
  fake::method(map::Application_Quit, reinterpret_cast<void*>(&app_quit));
  fake::method(map::LoadingController_LoadScene, reinterpret_cast<void*>(&load_scene));
  fake::method(map::PhotonNetwork_Disconnect, reinterpret_cast<void*>(&disconnect));
  fake::method(map::AudioListener_set_volume, reinterpret_cast<void*>(&set_volume));
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&get_network));
  fake::static_field(map::LevelValues_instance, &valuesStatic, sizeof valuesStatic, true);
  fake::static_field(map::LoadingController_instance, &loadingStatic, sizeof loadingStatic, true);
  fake::field(map::LevelValues_completed, kCompleted);
  fake::field(map::Network_sceneChanging, kSceneChanging);
  fake::field(map::LoadingController_ready, kReady);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::leave::create_hooks());
  put<bool>(values, kCompleted, true);
  put<bool>(network, kSceneChanging, false);
  put<bool>(loading, kReady, false);
  steppedOut = comesBack = true;
  events.clear();
}

using Events = std::vector<std::string>;

void an_investigators_buttons_are_the_games() {
  reset();
  steppedOut = false;
  call(map::PauseMenuController_Leave);
  call(map::PauseMenuController_Quit);
  CHECK((events == Events{"game leave", "game quit"}));
  CHECK(!pm::leave::leaving());
}

// The investigator comes back first, so the game's own Leave and Quit find it and make their own writes.
void the_ghost_players_quit_is_the_games_once_the_investigator_is_back() {
  reset();
  call(map::PauseMenuController_Quit);
  CHECK((events == Events{"teardown", "game quit"}));
}

void the_ghost_players_leave_is_the_games_once_the_investigator_is_back() {
  reset();
  call(map::PauseMenuController_Leave);
  CHECK((events == Events{"teardown", "game leave"}));
  CHECK(pm::leave::leaving());
  // The game's coroutine disconnects: the mod's own steps do not run as well.
  put<bool>(loading, kReady, true);
  pm::leave::tick_background();
  pm::leave::tick_background();
  CHECK((events == Events{"teardown", "game leave"}));
}

// Vanilla's Quit would throw without an investigator, and the game would not close.
void without_an_investigator_the_game_still_closes() {
  reset();
  comesBack = false;
  call(map::PauseMenuController_Quit);
  CHECK((events == Events{"teardown", "application quit"}));
}

// Vanilla's Leave would throw without an investigator: the steps of its coroutine.
void without_an_investigator_the_leave_still_finishes() {
  reset();
  comesBack = false;
  call(map::PauseMenuController_Leave);
  CHECK((events == Events{"teardown", "silence", "load scene"}));
  CHECK(!read<bool>(values, kCompleted));
  CHECK(read<bool>(network, kSceneChanging));
  put<bool>(loading, kReady, true);
  pm::leave::tick_background();  // ready this frame
  CHECK((events == Events{"teardown", "silence", "load scene"}));
  pm::leave::tick_background();  // disconnects the frame after
  CHECK((events == Events{"teardown", "silence", "load scene", "disconnect"}));
}

// The cover over the truck reads leaving() only while a level exists, so nothing there may be what clears
// it: the frames without a level do, and the session's next contract starts clean.
void leaving_ends_with_the_level_for_the_next_contract() {
  reset();
  pm::leave::left();
  pm::leave::tick_background();
  CHECK(pm::leave::leaving());  // the level is still unloading
  level = nullptr;
  pm::leave::tick_background();
  level = &menu;  // the next contract's level
  pm::leave::tick_background();
  CHECK(!pm::leave::leaving());
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::body {
void* camera() { return steppedOut ? &menu : nullptr; }
void teardown() {
  events.push_back("teardown");
  steppedOut = false;
}
bool investigator_back() { return comesBack; }
}  // namespace pm::body
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
api::Ref<pm::map::LevelController> level_controller() { return api::Ref<pm::map::LevelController>::unchecked(level); }
}  // namespace pm::game

int main() {
  an_investigators_buttons_are_the_games();
  the_ghost_players_quit_is_the_games_once_the_investigator_is_back();
  the_ghost_players_leave_is_the_games_once_the_investigator_is_back();
  without_an_investigator_the_game_still_closes();
  without_an_investigator_the_leave_still_finishes();
  leaving_ends_with_the_level_for_the_next_contract();
  std::printf("leave: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
