#include "panorama.h"

#include <windows.h>

#include <cmath>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "mapview.h"
#include "ui.h"

namespace pg::panorama {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;

constexpr float kDegreesPerSecond = 6;  // a slow pan, like the haunted mirror's view
constexpr float kPitchDegrees = 8;      // looking slightly down into the room
constexpr int kLightPoint = 2;          // LightType.Point
constexpr float kLightRange = 9;
constexpr float kLightIntensity = 1.6f;
// The light hangs this far above the room's centre, near its ceiling: lit from the camera itself, the
// journal held in front of it outshone the room and the game's exposure then darkened the room.
constexpr float kLightHeight = 1.0f;
constexpr ui::Color kLightColour = {0.85f, 0.9f, 1, 1};  // a cold, ghostly white

struct Api {
  il2cpp::Object* lightType = nullptr;
  void (*setType)(void*, int, const void*) = nullptr;
  void (*setRange)(void*, float, const void*) = nullptr;
  void (*setIntensity)(void*, float, const void*) = nullptr;
  void (*setColour)(void*, const ui::Color*, const void*) = nullptr;
} api;

il2cpp::Held g_light;                    // GameObject
il2cpp::Held g_cover;                    // overlay canvas
constexpr int kCoverSortingOrder = 120;  // above everything the game and the mod draw
int g_room = -2;
ULONGLONG g_since = 0;

Quaternion yaw_pitch(float yawDegrees, float pitchDegrees) {
  constexpr float kHalfRadians = 3.14159265f / 360;
  const float cy = std::cos(yawDegrees * kHalfRadians), sy = std::sin(yawDegrees * kHalfRadians);
  const float cp = std::cos(pitchDegrees * kHalfRadians), sp = std::sin(pitchDegrees * kHalfRadians);
  return {cy * sp, sy * cp, -sy * sp, cy * cp};  // yaw about y, then pitch about the turned x
}

void make_light() {
  if (!api.lightType) return;
  g_light = game::new_game_object("PlayableGhost panorama light");
  void* light = g_light ? game::add_component(g_light, api.lightType) : nullptr;
  if (!light) return;
  api.setType(light, kLightPoint, nullptr);
  api.setRange(light, kLightRange, nullptr);
  api.setIntensity(light, kLightIntensity, nullptr);
  api.setColour(light, &kLightColour, nullptr);
}

}  // namespace

bool resolve() {
  il2cpp::Class* light = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Light");
  api.lightType = light ? il2cpp::type_object(light) : nullptr;
  api.setType = reinterpret_cast<decltype(api.setType)>(il2cpp::method_ptr(light, "set_type", 1));
  api.setRange = reinterpret_cast<decltype(api.setRange)>(il2cpp::method_ptr(light, "set_range", 1));
  api.setIntensity = reinterpret_cast<decltype(api.setIntensity)>(il2cpp::method_ptr(light, "set_intensity", 1));
  api.setColour = reinterpret_cast<decltype(api.setColour)>(il2cpp::method_ptr(light, "set_color", 1));
  const bool ok = api.lightType && api.setType && api.setRange && api.setIntensity && api.setColour;
  if (!ok) api = Api{};  // optional: the panorama is shown unlit
  PG_LOG("panorama: resolve %s", ok ? "ok" : "without its light");
  return true;
}

void show(void* camera, int room) {
  if (!game::alive(camera)) return;
  const int target = room >= 0 ? room : g_room >= 0 ? g_room : 0;
  if (target != g_room) {
    g_room = target;
    g_since = GetTickCount64();
  }
  Vector3 centre{};
  if (!mapview::room_center(target, centre)) return;
  if (!g_light || !game::alive(g_light)) make_light();
  if (g_light) game::set_position(game::object_transform(g_light), {centre.x, centre.y + kLightHeight, centre.z});
  void* t = game::transform_of(camera);
  game::set_position(t, centre);
  const float seconds = static_cast<float>(GetTickCount64() - g_since) / 1000.0f;
  game::set_rotation(t, yaw_pitch(seconds * kDegreesPerSecond, kPitchDegrees));
}

void cover(bool on) {
  if (!on) {
    if (g_cover) ui::destroy(g_cover);
    g_cover = nullptr;
    return;
  }
  if (g_cover) return;
  g_cover = ui::create_canvas("PlayableGhostCover", kCoverSortingOrder);
  if (void* black = g_cover ? ui::create_image(g_cover, "Black", {0, 0, 0, 1}) : nullptr)
    ui::place(black, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {8000, 6000});
}

void hide() {
  if (g_light && game::alive(g_light)) game::destroy(g_light);
  g_light = nullptr;
  g_room = -2;
}

}  // namespace pg::panorama
