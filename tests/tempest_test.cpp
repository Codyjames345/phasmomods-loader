// Offline tests for the Tempest's model over the fake backend: what its strikes do to the ghost's renderers,
// and what a Dev Menu re-type mid-contract gives back (the rule for lasting effects, 2026-11-14). While a
// Tempest hunts, the mod keeps the model's renderers enabled only while a strike lights it up, past the game's
// own flicker. The game's own writes, read from their bodies, are absolute through
// GhostModel.SetVisibility: on for a shown ghost's hunt (GhostAI.MakeGhostAppear), off at its end
// (GhostAI.Hunting); see sdk/systems/ghost-ai.md. The source is included so its clock can be the
// test's. Run: build\tempest_test.exe
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "check.h"
#include "fake_game.h"

namespace {
ULONGLONG g_now = 100000;
ULONGLONG test_clock() { return g_now; }
}  // namespace
#define GetTickCount64 test_clock
#include "../mods/custom_ghosts/tempest.cpp"
#undef GetTickCount64

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace tempest = pm::custom::tempest;
using namespace pm::custom;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};

template <class T>
T& at(Obj& o, int32_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&o) + offset);
}

// The fake's field offsets.
constexpr int32_t kAiHunting = 0x08;      // GhostAI.isHunting
constexpr int32_t kAiState = 0x0C;        // GhostAI.state
constexpr int32_t kAiModel = 0x10;        // GhostAI.model
constexpr int32_t kModelRenderers = 0x08;  // GhostModel.renderers
// Made-up numbers for GhostState's hunting and idle and for the three weathers: the code reads them from the
// map, so none is the game's.
constexpr int32_t kStateHunting = 6, kStateIdle = 7;
constexpr int32_t kRain = 2, kHeavyRain = 3, kDead = 9;

Obj g_ai, g_model, g_renderer;
bool g_isMaster = true;
int g_enableCalls = 0;  // Renderer.set_enabled's calls this test
bool g_shown = false;   // what the last set_enabled asked for

bool fake_master(const void*) { return g_isMaster; }
void* fake_no_values(const void*) { return nullptr; }  // no LevelValues: the sky reads unknown
void* fake_no_object(const void*, bool, const void*) { return nullptr; }  // no LightningController
void fake_set_enabled(void* r, bool on, const void*) {
  ++g_enableCalls;
  g_shown = on;
  at<bool>(*static_cast<Obj*>(r), 0x08) = on;
}

// One contract: a hunting Tempest, its model's one renderer hidden as after any hunt.
void start() {
  tempest::reset();  // what it holds goes back before the fake's own storage does
  fake::reset();
  g_ai = g_model = g_renderer = Obj{};
  at<bool>(g_ai, kAiHunting) = true;
  at<int32_t>(g_ai, kAiState) = kStateHunting;
  at<void*>(g_ai, kAiModel) = &g_model;
  at<bool>(g_renderer, 0x08) = false;
  g_now = 100000;
  g_isMaster = true;
  g_enableCalls = 0;
  g_shown = false;
  fake::method(map::PhotonNetwork_get_IsMasterClient, reinterpret_cast<void*>(&fake_master));
  fake::method(map::LevelValues_get_Instance, reinterpret_cast<void*>(&fake_no_values));
  fake::method(map::UnityObject_FindObjectOfType, reinterpret_cast<void*>(&fake_no_object));
  fake::method(map::Renderer_set_enabled, reinterpret_cast<void*>(&fake_set_enabled));
  fake::field(map::GhostAI_isHunting, kAiHunting);
  fake::field(map::GhostAI_state, kAiState);
  fake::field(map::GhostAI_model, kAiModel);
  fake::field(map::GhostModel_renderers, kModelRenderers);
  fake::value(map::GhostState_hunting, kStateHunting);
  fake::value(map::GhostState_idle, kStateIdle);
  fake::value(map::WeatherType_rain, kRain);
  fake::value(map::WeatherType_heavyRain, kHeavyRain);
  fake::value(map::WeatherType_dead, kDead);
  std::string missing;
  pm::api::bind(fake::host(), missing);
  // The renderer goes into the array only after bind: storing a reference goes through the host.
  void* renderers = fake::array(sizeof(void*), 1);
  at<void*>(g_model, kModelRenderers) = renderers;
  pm::api::RefArray{renderers}.set(0, &g_renderer);
}

// A hunt that opened with a strike shows the model; past the strike window the mod hides it again.
void show_then_dark() {
  tempest::tick(&g_ai, kTempest);  // the hunt opens with a strike
  CHECK(at<bool>(g_renderer, 0x08));
  g_now += 3000;  // past the 1.5 s strike window, before the next strike (9 s at the soonest)
  tempest::tick(&g_ai, kTempest);
  CHECK(!at<bool>(g_renderer, 0x08));
}

// A re-type in the dark between strikes gives the model back: enabled, as the game keeps it in a hunt.
void a_retype_mid_hunt_gives_the_model_back() {
  start();
  show_then_dark();
  tempest::tick(nullptr, 0);  // the content layer's last frame once the rules stop
  CHECK(at<bool>(g_renderer, 0x08));
  CHECK(!tempest::striking());  // and its own state is gone
  int after = g_enableCalls;
  tempest::tick(nullptr, 0);  // more frames of the new type write nothing
  CHECK(g_enableCalls == after);
}

// A re-type while a strike still lights it leaves the model shown, as the game has it through a hunt.
void a_retype_during_a_strike_leaves_it_shown() {
  start();
  tempest::tick(&g_ai, kTempest);  // the hunt opens with a strike
  tempest::tick(nullptr, 0);
  CHECK(at<bool>(g_renderer, 0x08));
}

// A hunt that ended in a strike leaves the model hidden, as after any hunt; a re-type after it writes nothing.
void a_retype_after_the_hunt_leaves_it_hidden() {
  start();
  tempest::tick(&g_ai, kTempest);  // a strike shows it
  at<bool>(g_ai, kAiHunting) = false;
  at<int32_t>(g_ai, kAiState) = kStateIdle;
  tempest::tick(&g_ai, kTempest);  // the hunt ended in a strike: hidden, as after any hunt
  CHECK(!at<bool>(g_renderer, 0x08));
  int after = g_enableCalls;
  tempest::tick(nullptr, 0);
  CHECK(g_enableCalls == after);
  CHECK(!at<bool>(g_renderer, 0x08));
}

}  // namespace

int main() {
  a_retype_mid_hunt_gives_the_model_back();
  a_retype_during_a_strike_leaves_it_shown();
  a_retype_after_the_hunt_leaves_it_hidden();
  pm::fake::reset();
  return g_failures ? 1 : 0;
}
