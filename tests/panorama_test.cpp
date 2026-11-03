// Offline tests for the ghost player's room view (mods/playable_ghost/panorama.cpp) over the fake backend: which room
// it shows, the pan's direction, that an unlit view still shows the room, and what hide and cover clean up.
// Run: build\panorama_test.exe
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>

#include "check.h"
#include "fake_game.h"
#include "hooks.h"

namespace {
ULONGLONG g_now = 1000;
ULONGLONG WINAPI clock_ms() { return g_now; }
}  // namespace

#define GetTickCount64 clock_ms
#include "../mods/playable_ghost/panorama.cpp"
#undef GetTickCount64

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace panorama = pm::panorama;

int g_camera = 0, g_light = 0, g_lightClass = 0, g_canvas = 0, g_image = 0, g_lightObject = 0;
int g_askedRoom = -1, g_lightsMade = 0, g_destroyed = 0, g_canvases = 0, g_uiDestroyed = 0, g_ranges = 0;
bool g_roomKnown = true;
phasmo::Vector3 g_lastPosition{}, g_cameraPosition{};  // the light's, the camera's
phasmo::Quaternion g_lastRotation{};

void* add_component(void*, void*, const void*) { return &g_light; }
void set_type(void*, int32_t, const void*) {}
void set_range(void*, float, const void*) { ++g_ranges; }
void set_intensity(void*, float, const void*) {}
void set_color(void*, void*, const void*) {}

void give_entries(bool lit) {
  fake::reset();
  fake::type(map::Light, &g_lightClass);
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&add_component));
  fake::method(map::Light_set_intensity, reinterpret_cast<void*>(&set_intensity));
  fake::method(map::Light_set_color, reinterpret_cast<void*>(&set_color));
  if (lit) {
    fake::method(map::Light_set_type, reinterpret_cast<void*>(&set_type));
    fake::method(map::Light_set_range, reinterpret_cast<void*>(&set_range));
    fake::value(map::LightType_Point, 2);
  }
  std::string missing;
  pm::api::bind(fake::host(), missing);
  panorama::resolve();
  panorama::hide();
  panorama::cover(false);
  g_lightsMade = g_destroyed = g_canvases = g_uiDestroyed = g_ranges = 0;
  g_askedRoom = -1;
  g_roomKnown = true;
  g_now = 1000;
}

// A room to show: the given one, or the last, or the first.
void shows_the_room_asked_for_or_the_last() {
  give_entries(true);
  panorama::show(&g_camera, -1);
  CHECK(g_askedRoom == 0);  // nothing shown yet: the first
  panorama::show(&g_camera, 3);
  CHECK(g_askedRoom == 3);
  panorama::show(&g_camera, -1);
  CHECK(g_askedRoom == 3);  // no room in the view now: it stays
  CHECK(g_lastPosition.x == 3.0f && g_lastPosition.y == 2.0f);
  panorama::hide();
  panorama::show(&g_camera, -1);
  CHECK(g_askedRoom == 0);  // hidden: it starts over
}

// It pans at its own speed from when the room changed, and starts level with its pitch.
void pans_from_when_the_room_changed() {
  give_entries(true);
  panorama::show(&g_camera, 1);
  const phasmo::Quaternion first = g_lastRotation;
  CHECK(std::fabs(first.y) < 1e-5f && std::fabs(first.w - std::cos(8 * 3.14159265f / 360)) < 1e-5f);
  g_now += 15000;  // 90 degrees at 6 a second
  panorama::show(&g_camera, 1);
  CHECK(std::fabs(g_lastRotation.y - std::sin(90 * 3.14159265f / 360) * std::cos(8 * 3.14159265f / 360)) < 1e-4f);
  panorama::show(&g_camera, 2);  // a new room: the pan starts again
  CHECK(std::fabs(g_lastRotation.y) < 1e-5f);
}

void the_light_is_made_once_and_goes_with_hide() {
  give_entries(true);
  panorama::show(&g_camera, 1);
  panorama::show(&g_camera, 1);
  CHECK(g_lightsMade == 1 && g_ranges == 1);
  panorama::hide();
  CHECK(g_destroyed == 1);
  panorama::hide();
  CHECK(g_destroyed == 1);
}

// Light.set_type and the like are optional: without them it is shown unlit, its camera moved all the same.
void without_the_light_entries_it_shows_unlit() {
  give_entries(false);
  g_cameraPosition = {};
  panorama::show(&g_camera, 3);
  CHECK(g_lightsMade == 0);
  CHECK(g_cameraPosition.x == 3.0f && g_cameraPosition.y == 1.0f);
}

void nothing_moves_without_a_camera_or_a_room() {
  give_entries(true);
  g_cameraPosition = {-1, -1, -1};
  panorama::show(nullptr, 2);
  CHECK(g_askedRoom == -1 && g_cameraPosition.x == -1);
  g_roomKnown = false;
  panorama::show(&g_camera, 2);
  CHECK(g_cameraPosition.x == -1 && g_lightsMade == 0);
}

void the_cover_is_one_and_comes_off() {
  give_entries(true);
  panorama::cover(true);
  panorama::cover(true);
  CHECK(g_canvases == 1);
  panorama::cover(false);
  CHECK(g_uiDestroyed == 1);
  panorama::cover(false);
  CHECK(g_uiDestroyed == 1);
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::mapview {
bool room_center(int index, phasmo::Vector3& out) {
  g_askedRoom = index;
  if (!g_roomKnown) return false;
  out = {static_cast<float>(index), 1, 0};
  return true;
}
}  // namespace pm::mapview
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
void* new_game_object(const char*) {
  ++g_lightsMade;
  return &g_lightObject;
}
void destroy(void*) { ++g_destroyed; }
void* object_transform(void* go) { return go; }
void* transform_of(void* c) { return c; }
void set_position(void* t, phasmo::Vector3 p) {
  if (t == &g_lightObject) g_lastPosition = p;
  if (t == &g_camera) g_cameraPosition = p;
}
void set_rotation(void*, phasmo::Quaternion q) { g_lastRotation = q; }
}  // namespace pm::game
namespace pm::ui {
void* create_canvas(const char*, int) {
  ++g_canvases;
  return &g_canvas;
}
void* create_image(void*, const char*, Color) { return &g_image; }
void place(void*, Vec2, Vec2, Vec2, Vec2) {}
void destroy(void*) { ++g_uiDestroyed; }
}  // namespace pm::ui

int main() {
  shows_the_room_asked_for_or_the_last();
  pans_from_when_the_room_changed();
  the_light_is_made_once_and_goes_with_hide();
  without_the_light_entries_it_shows_unlit();
  nothing_moves_without_a_camera_or_a_room();
  the_cover_is_one_and_comes_off();
  std::printf(g_failures ? "%d check(s) failed\n" : "all panorama tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
