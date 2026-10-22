#include "mapview.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "room_pick.h"
#include "round.h"

namespace pg::mapview {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified at load); obfuscated names, so by offset.
constexpr size_t kMapFloorIndex = 0x38;  // MapController: index into allFloors of the shown floor
constexpr size_t kFloorType = 0x10;      // MapController.Floor.floorType
constexpr size_t kFloorTransform = 0x18;  // MapController.Floor.floor
constexpr size_t kRoomFloor = 0x68;      // LevelRoom: its floor (same enum as Floor.floorType)

struct Ray {
  Vector3 origin, direction;
};
using game::Bounds;

struct Api {
  il2cpp::Class* mapController = nullptr;
  il2cpp::Field* mapCamera = nullptr;
  il2cpp::Field* allFloors = nullptr;
  void* (*targetTexture)(void*, const void*) = nullptr;
  Ray (*viewportRay)(void*, Vector3, const void*) = nullptr;
  Vector3 (*worldToViewport)(void*, Vector3, const void*) = nullptr;
  void (*changeFloor)(void*, bool, const void*) = nullptr;
} api;

struct Shown {
  il2cpp::Held controller;
  il2cpp::Held camera;
  il2cpp::Held image;
  il2cpp::Held eventCamera;
  bool cameraWasEnabled = false;
  int floorWas = -1;
  il2cpp::Held card;  // the black card the map sits on
} g;

il2cpp::Array* floors() { return g.controller ? static_cast<il2cpp::Array*>(il2cpp::reference_field(g.controller, api.allFloors)) : nullptr; }

void* floor_at(int index) {
  il2cpp::Array* a = floors();
  return a && index >= 0 && static_cast<uintptr_t>(index) < a->max_length ? a->data<void*>()[index] : nullptr;
}

int shown_floor_index() { return g.controller ? il2cpp::field<int>(g.controller, kMapFloorIndex) : -1; }

int shown_floor_type() {
  void* f = floor_at(shown_floor_index());
  return f ? il2cpp::field<int>(f, kFloorType) : -1;
}

// Every collider of a candidate room, as its world bounds.
std::vector<Bounds> room_bounds(void* room) { return game::room_bounds(room); }

// A favourite-room candidate's colliders on the shown floor, in the map image's 0..1 space.
std::vector<ui::Rect> texture_outline(int index) {
  std::vector<ui::Rect> out;
  void* room = round::room(index);
  if (!room || !game::alive(g.camera) || il2cpp::field<int>(room, kRoomFloor) != shown_floor_type()) return out;
  for (const Bounds& b : room_bounds(room)) {
    // All four corners, so the rectangle is right whichever way the map camera is turned.
    float x0 = 1, y0 = 1, x1 = 0, y1 = 0;
    for (float sx : {-1.0f, 1.0f}) {
      for (float sz : {-1.0f, 1.0f}) {
        const Vector3 v = api.worldToViewport(
            g.camera, {b.center.x + sx * b.extents.x, b.center.y, b.center.z + sz * b.extents.z}, nullptr);
        x0 = std::min(x0, v.x), x1 = std::max(x1, v.x), y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
      }
    }
    x0 = std::clamp(x0, 0.0f, 1.0f), x1 = std::clamp(x1, 0.0f, 1.0f), y0 = std::clamp(y0, 0.0f, 1.0f), y1 = std::clamp(y1, 0.0f, 1.0f);
    if (x1 > x0 && y1 > y0) out.push_back({x0, y0, x1 - x0, y1 - y0});
  }
  return out;
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  api.mapController = il2cpp::cls("Assembly-CSharp.dll", "", "MapController");
  api.mapCamera = il2cpp::field_info(api.mapController, "mapCamera");
  api.allFloors = il2cpp::field_info(api.mapController, "allFloors");
  il2cpp::Class* camera = il2cpp::cls(kCore, "UnityEngine", "Camera");
  api.targetTexture = reinterpret_cast<decltype(api.targetTexture)>(il2cpp::method_ptr(camera, "get_targetTexture", 0));
  api.viewportRay = reinterpret_cast<decltype(api.viewportRay)>(il2cpp::method_ptr(camera, "ViewportPointToRay", 1));
  api.worldToViewport = reinterpret_cast<decltype(api.worldToViewport)>(il2cpp::method_ptr(camera, "WorldToViewportPoint", 1));
  api.changeFloor = reinterpret_cast<decltype(api.changeFloor)>(il2cpp::method_ptr(api.mapController, "ChangeFloor", 1));
  const bool ok = api.mapController && api.mapCamera && api.allFloors && api.targetTexture &&
                  api.viewportRay && api.worldToViewport && api.changeFloor;
  // Optional: without it the picker falls back to its plain room list.
  PG_LOG("mapview: resolve %s", ok ? "ok" : "unavailable (the picker will list rooms instead)");
  if (!ok) api = Api{};
  return true;
}

bool show(void* parent, ui::Vec2 position, ui::Vec2 size, void* eventCamera) {
  hide();
  if (!api.changeFloor) return false;
  void* controller = il2cpp::static_field_by_type(api.mapController, "MapController");
  void* camera = game::alive(controller) ? il2cpp::reference_field(controller, api.mapCamera) : nullptr;
  void* texture = game::alive(camera) ? api.targetTexture(camera, nullptr) : nullptr;
  if (!texture) {
    PG_LOG("mapview: this level's map has no camera texture (controller %p camera %p)", controller, camera);
    return false;
  }
  g.controller = controller;
  g.camera = camera;
  g.eventCamera = eventCamera;
  g.cameraWasEnabled = game::enabled(camera);
  g.floorWas = shown_floor_index();
  game::set_enabled(camera, true);  // the truck only renders its map while someone is inside
  // The floor plans are drawn for the truck's dark monitor: some levels' sprites have black baked in,
  // others are transparent. A black card under the map shows every floor the same way.
  g.card = ui::create_image(parent, "PlayableGhost map card", {0, 0, 0, 1});
  if (g.card) ui::place(g.card, {0.5f, 0.5f}, {0.5f, 0.5f}, position, size);
  g.image = ui::create_raw_image(parent, "PlayableGhost map", texture);
  if (!g.image) {
    hide();
    return false;
  }
  ui::place(g.image, {0.5f, 0.5f}, {0.5f, 0.5f}, position, size);
  PG_LOG("mapview: showing the map (floor %d of %d)", g.floorWas, floors() ? static_cast<int>(floors()->max_length) : 0);
  return true;
}

void hide() {
  if (game::alive(g.camera)) game::set_enabled(g.camera, g.cameraWasEnabled);
  if (game::alive(g.controller) && g.floorWas >= 0) {
    for (int guard = 0; guard < 16 && shown_floor_index() != g.floorWas; ++guard)
      api.changeFloor(g.controller, shown_floor_index() < g.floorWas, nullptr);
  }
  if (g.image) ui::destroy(g.image);
  if (g.card) ui::destroy(g.card);
  g = Shown{};
}

void change_floor(bool up) {
  if (game::alive(g.controller)) api.changeFloor(g.controller, up, nullptr);
}

std::string floor_name() {
  const int type = shown_floor_type();
  return type < 0 ? "" : type == 0 ? "Basement" : "Floor " + std::to_string(type);
}

int room_at(ui::Vec2 screenPoint) {
  ui::Vec2 uv{};
  if (!game::alive(g.camera) || !ui::normalized_point(g.image, screenPoint, uv, g.eventCamera)) return -1;
  const Ray ray = api.viewportRay(g.camera, {uv.x, uv.y, 0}, nullptr);
  // Where the ray meets the shown floor's level (a top-down map camera, orthographic or not).
  void* floor = floor_at(shown_floor_index());
  void* floorTransform = floor ? il2cpp::field<void*>(floor, kFloorTransform) : nullptr;
  float x = ray.origin.x, z = ray.origin.z;
  if (game::alive(floorTransform) && std::fabs(ray.direction.y) > 1e-4f) {
    const float t = (game::position(floorTransform).y - ray.origin.y) / ray.direction.y;
    x += ray.direction.x * t;
    z += ray.direction.z * t;
  }
  std::vector<room_pick::Room> rooms;
  for (int i = 0; i < round::room_count(); ++i) {
    void* room = round::room(i);
    room_pick::Room r;
    r.floor = room ? il2cpp::field<int>(room, kRoomFloor) : -1;
    for (const Bounds& b : room_bounds(room))
      r.boxes.push_back({b.center.x - b.extents.x, b.center.z - b.extents.z, b.center.x + b.extents.x, b.center.z + b.extents.z});
    rooms.push_back(std::move(r));
  }
  return room_pick::at(rooms, shown_floor_type(), x, z);
}

std::vector<ui::Rect> room_outline(int index) { return texture_outline(index); }

void* image() { return g.image; }

bool room_center(int index, Vector3& out) {
  const std::vector<Bounds> b = room_bounds(round::room(index));
  if (b.empty()) return false;
  out = b[0].center;
  return true;
}

}  // namespace pg::mapview
