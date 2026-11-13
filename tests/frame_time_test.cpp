// Offline tests for the frame's time (sdk/api/frame_time.h): Unity's four Time getters through the fake backend, read
// together by pm::api::frame_time(). Run: build\frame_time_test.exe
#include <atomic>
#include <string>

#include "UnityAudio.h"
#include "check.h"
#include "fake_game.h"
#include "frame_time.h"
#include "game_api.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace fake = pm::fake;
namespace map = pm::map;

// What the fake game's Time answers this frame. A static getter is called with its MethodInfo alone.
float g_delta = 0, g_unscaledDelta = 0, g_time = 0, g_unscaledTime = 0;
float delta(const void*) { return g_delta; }
float unscaled_delta(const void*) { return g_unscaledDelta; }
float game_time(const void*) { return g_time; }
float unscaled_time(const void*) { return g_unscaledTime; }

void give_time() {
  fake::method(map::Time_get_deltaTime, reinterpret_cast<void*>(&delta));
  fake::method(map::Time_get_unscaledDeltaTime, reinterpret_cast<void*>(&unscaled_delta));
  fake::method(map::Time_get_time, reinterpret_cast<void*>(&game_time));
  fake::method(map::Time_get_unscaledTime, reinterpret_cast<void*>(&unscaled_time));
}

bool bind() {
  std::string missing;
  return pm::api::bind(fake::host(), missing);
}

// Each getter answers through its own map entry, as Time.get_frameCount does.
void the_getters_read_unitys_time() {
  fake::reset();
  give_time();
  CHECK(bind());
  g_delta = 0.02f, g_unscaledDelta = 0.04f, g_time = 12.5f, g_unscaledTime = 30.25f;
  CHECK(phasmo::Time::get_deltaTime() == 0.02f);
  CHECK(phasmo::Time::get_unscaledDeltaTime() == 0.04f);
  CHECK(phasmo::Time::get_time() == 12.5f);
  CHECK(phasmo::Time::get_unscaledTime() == 30.25f);
}

// frame_time gives all four at once, each from its own getter: a frozen game (time scale 0) has no game time passing,
// while real time goes on.
void frame_time_gives_the_four_values() {
  fake::reset();
  give_time();
  CHECK(bind());
  g_delta = 0.0f, g_unscaledDelta = 0.016f, g_time = 100.0f, g_unscaledTime = 140.5f;
  const pm::api::FrameTime t = pm::api::frame_time();
  CHECK(t.deltaTime == 0.0f);
  CHECK(t.unscaledDeltaTime == 0.016f);
  CHECK(t.time == 100.0f);
  CHECK(t.unscaledTime == 140.5f);
  g_delta = 0.033f;
  CHECK(pm::api::frame_time().deltaTime == 0.033f);
}

// A build that lacks a getter answers 0 for that value alone, and the others still answer.
void an_unresolved_getter_answers_zero() {
  fake::reset();
  give_time();
  fake::forget(map::Time_get_unscaledTime);
  bind();  // false: frame_time makes the four required, and one is missing
  g_delta = 0.5f, g_unscaledDelta = 0.25f, g_time = 3.0f, g_unscaledTime = 9.0f;
  const pm::api::FrameTime t = pm::api::frame_time();
  CHECK(t.deltaTime == 0.5f && t.unscaledDeltaTime == 0.25f && t.time == 3.0f);
  CHECK(t.unscaledTime == 0.0f);
}

}  // namespace

int main() {
  the_getters_read_unitys_time();
  frame_time_gives_the_four_values();
  an_unresolved_getter_answers_zero();
  return g_failures ? 1 : 0;
}
