// Offline tests for whether the local investigator sprints now (sdk/api/movement.h): the game's own rule
// (sprints: FirstPersonController.running, PlayerStamina.canSprint and Difficulty.sprinting,
// LocalPCPlayer.UpdatePlayerSpeed), and over the fake backend the three values it reads on the PC rig,
// nothing on a VR rig and nothing with no rig. The game's side is in sdk/systems/players.md,
// "Speed and stamina". Run: build\sprint_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "Difficulty.h"
#include "GameController.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "movement.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace move = pm::api::movement;
namespace map = pm::map;
namespace fake = pm::fake;

// ---- The pure rule ----

void the_rule_is_the_games() {
  // Outside a contract the setting is not consulted: LocalPCPlayer.UpdatePlayerSpeed gates the sprint on
  // Difficulty.sprinting only while a GameController exists (map note on the entry).
  CHECK(move::sprints(true, true, false, 0));
  CHECK(move::sprints(true, true, false, 1));
  CHECK(move::sprints(true, true, false, 2));
  // In a contract: 0 off, 1 on, 2 infinite (Difficulty.sprinting's tooltip).
  CHECK(!move::sprints(true, true, true, 0));
  CHECK(move::sprints(true, true, true, 1));
  CHECK(move::sprints(true, true, true, 2));
  // Run not held, or out of breath, and the game gives the walk speed wherever it is.
  CHECK(!move::sprints(false, true, false, 1));
  CHECK(!move::sprints(false, true, true, 1));
  CHECK(!move::sprints(true, false, false, 1));
  CHECK(!move::sprints(true, false, true, 1));
}

// ---- Over the fake game ----

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

// The fake's own layouts: the fields sit at offsets of the test's choosing.
constexpr int32_t kNetRig = 0x08;            // Network.localRig
constexpr int32_t kRigController = 0x08;     // LocalPCPlayer.firstPersonController
constexpr int32_t kRigStamina = 0x10;        // LocalPlayer.stamina
constexpr int32_t kRunning = 0x08;           // FirstPersonController.running
constexpr int32_t kCanSprint = 0x08;         // PlayerStamina.canSprint
constexpr int32_t kValuesDifficulty = 0x08;  // LevelValues.difficulty
constexpr int32_t kSprinting = 0x08;         // Difficulty.sprinting

Obj g_net, g_rig, g_controller, g_stamina, g_values, g_difficulty, g_game;
Obj g_pcClass,  // the class fake::type gives the LocalPCPlayer entry; a rig of it passes type.is
    g_vrClass;  // a VR rig stands as one of this class instead, which it is not
void* g_controllerStatic = nullptr;  // GameController.instance's storage: null outside a level
void* g_valuesStatic = nullptr;      // LevelValues.instance's storage
int g_lookups = 0;                   // Network.get_Instance calls the answer so far took

// sprinting()'s own first step (sdk/api/movement.h): Network.get_Instance, then Network.localRig on it.
// Counting the calls tells a real lookup off from the stub's answer of nothing without looking.
void* fake_network(void*) {
  ++g_lookups;
  return &g_net;
}

// A lobby: a PC investigator with Network.localRig, Run held, the stamina full, and no GameController.
void give() {
  fake::reset();
  g_lookups = 0;
  g_net = Obj{};
  g_rig = Obj{};
  g_controller = Obj{};
  g_stamina = Obj{};
  g_values = Obj{};
  g_difficulty = Obj{};
  g_game = Obj{};
  g_controllerStatic = nullptr;
  g_valuesStatic = nullptr;
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localRig, kNetRig);
  fake::type(map::LocalPCPlayer, &g_pcClass);
  fake::field(map::LocalPCPlayer_firstPersonController, kRigController);
  fake::field(map::FirstPersonController_running, kRunning);
  fake::field(map::LocalPlayer_stamina, kRigStamina);
  fake::field(map::PlayerStamina_canSprint, kCanSprint);
  fake::static_field(map::GameController_instance, &g_controllerStatic, sizeof g_controllerStatic, true);
  fake::static_field(map::LevelValues_instance, &g_valuesStatic, sizeof g_valuesStatic, true);
  fake::field(map::LevelValues_difficulty, kValuesDifficulty);
  fake::field(map::Difficulty_sprinting, kSprinting);
  std::string missing;
  pm::api::bind(fake::host(), missing);
  g_rig.klass = &g_pcClass;
  at<void*>(&g_net, kNetRig) = &g_rig;
  at<void*>(&g_rig, kRigController) = &g_controller;
  at<void*>(&g_rig, kRigStamina) = &g_stamina;
  at<bool>(&g_controller, kRunning) = true;
  at<bool>(&g_stamina, kCanSprint) = true;
}

// A level running: an alive GameController, a LevelValues holding a difficulty with that sprint setting.
void in_contract(int32_t setting) {
  g_controllerStatic = &g_game;
  g_valuesStatic = &g_values;
  at<void*>(&g_values, kValuesDifficulty) = &g_difficulty;
  at<int32_t>(&g_difficulty, kSprinting) = setting;
}

void run_held_with_stamina_in_the_lobby_sprints() {
  give();  // no GameController: the setting never enters, as outside a level
  const std::optional<bool> got = move::sprinting();
  CHECK(got.has_value());
  CHECK(got.value_or(false));
}

void run_released_does_not_sprint() {
  give();
  at<bool>(&g_controller, kRunning) = false;
  CHECK(move::sprinting() == false);
}

void out_of_breath_does_not_sprint() {
  give();
  at<bool>(&g_stamina, kCanSprint) = false;
  CHECK(move::sprinting() == false);
}

void sprinting_off_in_a_contract_does_not_sprint() {
  give();
  in_contract(0);  // Difficulty.sprinting off: the game gives the walk speed (map note)
  CHECK(move::sprinting() == false);
}

void sprinting_on_in_a_contract_sprints() {
  give();
  in_contract(1);
  CHECK(move::sprinting() == true);
}

void a_vr_rig_gives_nothing() {
  give();
  in_contract(1);  // everything else would sprint, were the rig a LocalPCPlayer's
  g_rig.klass = &g_vrClass;
  const std::optional<bool> got = move::sprinting();
  CHECK(!got.has_value());
  CHECK(g_lookups == 1);  // the answer came from reading Network.localRig and the type check, not from never looking
}

void no_rig_gives_nothing() {
  give();
  at<void*>(&g_net, kNetRig) = nullptr;
  const std::optional<bool> got = move::sprinting();
  CHECK(!got.has_value());
  CHECK(g_lookups == 1);  // it looked for the rig and found none, rather than answering without looking
}

}  // namespace

int main() {
  the_rule_is_the_games();
  run_held_with_stamina_in_the_lobby_sprints();
  run_released_does_not_sprint();
  out_of_breath_does_not_sprint();
  sprinting_off_in_a_contract_does_not_sprint();
  sprinting_on_in_a_contract_sprints();
  a_vr_rig_gives_nothing();
  no_rig_gives_nothing();
  std::printf("%s\n", g_failures ? "sprint tests FAILED" : "sprint tests passed");
  return g_failures ? 1 : 0;
}
