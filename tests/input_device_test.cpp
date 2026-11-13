// Offline tests for which input the local player uses (sdk/api/input_device.h): over the fake backend, the answer is
// the game's own check (InputDevices.UsesKeyboard) of the rig's PlayerInput (LocalPlayer.playerInput), and nothing
// without a rig or a PlayerInput. Run: build\input_device_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "Input.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "input_device.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x20]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

constexpr int32_t kNetRig = 0x08;          // Network.localRig
constexpr int32_t kRigPlayerInput = 0x08;  // LocalPlayer.playerInput

Obj g_net, g_rig, g_input;
bool g_keyboard = false;        // what the game's check answers
void* g_askedAbout = nullptr;   // the PlayerInput the check was given

void* fake_network(void*) { return &g_net; }

// InputDevices.UsesKeyboard, a static method: its argument, then the method's info.
bool fake_uses_keyboard(void* playerInput, void*) {
  g_askedAbout = playerInput;
  return g_keyboard;
}

void give() {
  fake::reset();
  g_net = Obj{};
  g_rig = Obj{};
  g_input = Obj{};
  g_askedAbout = nullptr;
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localRig, kNetRig);
  fake::field(map::LocalPlayer_playerInput, kRigPlayerInput);
  fake::method(map::InputDevices_UsesKeyboard, reinterpret_cast<void*>(&fake_uses_keyboard));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  at<void*>(&g_net, kNetRig) = &g_rig;
  at<void*>(&g_rig, kRigPlayerInput) = &g_input;
}

void keyboard_is_the_games_answer() {
  give();
  g_keyboard = true;
  CHECK(pm::api::input::keyboard_in_use() == true);
  CHECK(g_askedAbout == &g_input);  // the rig's own PlayerInput
}

void a_gamepad_is_the_games_answer() {
  give();
  g_keyboard = false;
  CHECK(pm::api::input::keyboard_in_use() == false);
  CHECK(g_askedAbout == &g_input);
}

void no_rig_gives_nothing() {
  give();
  at<void*>(&g_net, kNetRig) = nullptr;
  CHECK(!pm::api::input::keyboard_in_use().has_value());
  CHECK(g_askedAbout == nullptr);
}

void no_player_input_gives_nothing() {
  give();
  at<void*>(&g_rig, kRigPlayerInput) = nullptr;
  CHECK(!pm::api::input::keyboard_in_use().has_value());
  CHECK(g_askedAbout == nullptr);
}

}  // namespace

int main() {
  keyboard_is_the_games_answer();
  a_gamepad_is_the_games_answer();
  no_rig_gives_nothing();
  no_player_input_gives_nothing();
  std::printf("%s\n", g_failures ? "input device tests FAILED" : "input device tests passed");
  return g_failures ? 1 : 0;
}
