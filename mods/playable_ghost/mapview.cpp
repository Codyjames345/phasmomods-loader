#include "mapview.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Camera.h"
#include "LevelRoom.h"
#include "MapController.h"
#include "game.h"
#include "held.h"
#include "Transform.h"
#include "log.h"
#include "runtime.h"
#include "room_pick.h"
#include "round.h"

namespace pm::mapview {
namespace {

using phasmo::Vector3;

using game::Bounds;
using phasmo::Camera;
using phasmo::MapController;

bool g_available = false;  // whether the map's entries resolved

struct Shown {
  api::Held controller;
  api::Held camera;
  api::Held image;
  api::Held eventCamera;
  bool cameraWasEnabled = false;
  int floorWas = -1;
  api::Held card;  // the black card the map sits on
} g;

api::RefArray floors() { return api::RefArray{g.controller ? MapController::allFloors.get(g.controller) : nullptr}; }

void* floor_at(int index) {
  const api::RefArray a = floors();
  return index >= 0 && static_cast<size_t>(index) < a.size() ? a[static_cast<size_t>(index)] : nullptr;
}

int shown_floor_index() { return g.controller ? MapController::floorIndex.get(g.controller) : -1; }

int shown_floor_type() {
  void* f = floor_at(shown_floor_index());
  return f ? phasmo::MapFloor::floorType.get(f) : -1;
}

// Every collider of a candidate room, as its world bounds.
std::vector<Bounds> room_bounds(void* room) { return game::room_bounds(room); }

// A favourite-room candidate's colliders on the shown floor, in the map image's 0..1 space.
std::vector<ui::Rect> texture_outline(int index) {
  std::vector<ui::Rect> out;
  void* room = round::room(index);
  if (!room || !game::alive(g.camera) || phasmo::LevelRoom::floor.get(room) != shown_floor_type()) return out;
  for (const Bounds& b : room_bounds(room)) {
    // All four corners, so the rectangle is right whichever way the map camera is turned.
    float x0 = 1, y0 = 1, x1 = 0, y1 = 0;
    for (float sx : {-1.0f, 1.0f}) {
      for (float sz : {-1.0f, 1.0f}) {
        const phasmo::Vector3 v = Camera::WorldToViewportPoint(
            g.camera.get(), phasmo::Vector3{b.center.x + sx * b.extents.x, b.center.y, b.center.z + sz * b.extents.z});
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
  g_available = api::all_resolved<MapController::instance, MapController::mapCamera, MapController::allFloors,
                                  MapController::floorIndex, MapController::ChangeFloor, phasmo::MapFloor::floorType,
                                  phasmo::MapFloor::floor, phasmo::FloorType::basement, Camera::get_targetTexture,
                                  Camera::ViewportPointToRay, Camera::WorldToViewportPoint>();
  // Optional: without it the picker falls back to its plain room list.
  PM_LOG("mapview: resolve %s", g_available ? "ok" : "unavailable (the picker will list rooms instead)");
  return true;
}

bool show(void* parent, ui::Vec2 position, ui::Vec2 size, void* eventCamera) {
  hide();
  if (!g_available) return false;
  void* controller = MapController::instance.get();
  void* camera = game::alive(controller) ? MapController::mapCamera.get(controller) : nullptr;
  void* texture = game::alive(camera) ? Camera::get_targetTexture(camera) : nullptr;
  if (!texture) {
    PM_LOG("mapview: this level's map has no camera texture (controller %p camera %p)", controller, camera);
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
  PM_LOG("mapview: showing the map (floor %d of %zu)", g.floorWas, floors().size());
  return true;
}

void hide() {
  if (game::alive(g.camera)) game::set_enabled(g.camera, g.cameraWasEnabled);
  if (game::alive(g.controller) && g.floorWas >= 0) {
    for (int guard = 0; guard < 16 && shown_floor_index() != g.floorWas; ++guard)
      MapController::ChangeFloor(g.controller, shown_floor_index() < g.floorWas);
  }
  if (g.image) ui::destroy(g.image);
  if (g.card) ui::destroy(g.card);
  g = Shown{};
}

void change_floor(bool up) {
  if (game::alive(g.controller)) MapController::ChangeFloor(g.controller, up);
}

std::string floor_name() {
  const int type = shown_floor_type();
  return type < 0 ? "" : type == phasmo::FloorType::basement() ? "Basement" : "Floor " + std::to_string(type);
}

int room_at(ui::Vec2 screenPoint) {
  ui::Vec2 uv{};
  if (!game::alive(g.camera) || !ui::normalized_point(g.image, screenPoint, uv, g.eventCamera)) return -1;
  const phasmo::Ray ray = Camera::ViewportPointToRay(g.camera.get(), phasmo::Vector3{uv.x, uv.y, 0});
  // Where the ray meets the shown floor's level (a top-down map camera, orthographic or not).
  void* floor = floor_at(shown_floor_index());
  void* floorTransform = floor ? phasmo::MapFloor::floor.get(floor) : nullptr;
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
    r.floor = room ? phasmo::LevelRoom::floor.get(room) : -1;
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

}  // namespace pm::mapview
