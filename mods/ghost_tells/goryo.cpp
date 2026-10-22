#include "goryo.h"

#include <windows.h>

#include <cmath>
#include <random>
#include <vector>

#include "apparition.h"
#include "brownie_rule.h"
#include "game.h"
#include "gate.h"
#include "goryo_rule.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "services.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pg::tells::goryo {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kEquipmentOn = 0x281;       // Equipment: switched on (a placed video camera: running)
constexpr uintptr_t kSampleRva = 0x518F100;  // NavMesh.SamplePosition(Vector3, out NavMeshHit, float, int)
constexpr int kStateRandomEvent = 14;
constexpr ULONGLONG kCameraScanMs = 2000;
constexpr float kSteerEvery = 1.0f;          // seconds between the destinations it is given on its way home
constexpr float kSampleRange = 3.0f;         // metres from its room's middle a point on the navmesh is sought

struct NavMeshHit {
  Vector3 position, normal;
  float distance;
  int mask;
  bool hit;
};

struct Api {
  il2cpp::Object* cameraType = nullptr;  // CCTV: the placed video camera
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  int (*nameToLayer)(il2cpp::String*, const void*) = nullptr;
  bool (*sample)(Vector3, NavMeshHit*, float, int, const void*) = nullptr;
  int laserGhost = -1;
  bool ok = false;
} api;

struct Host {
  il2cpp::Held ghost;
  bool goryo = false;
  CameraShow show;
  Extra extra;
  HomeSearch home;
  float sinceSteer = 0;
  std::vector<il2cpp::Held> cameras;
  ULONGLONG nextCameraScan = 0;
} g;

struct Shown {  // every client: the figure the host has asked for
  bool on = false;
  custom::apparition::Copy copy;
} s;

std::mt19937 g_random{std::random_device{}()};

bool steered() {
  const auto* pg = static_cast<const PgPlayableGhostService*>(mod::host().service(PG_SERVICE_PLAYABLE_GHOST));
  return pg && pg->version >= 1 && pg->ghostControlled && pg->ghostControlled();
}

/** A running placed video camera has `target` in its view (the Brownie's camera cone and range). */
bool on_camera(Vector3 target) {
  const ULONGLONG now = GetTickCount64();
  if (now >= g.nextCameraScan) {
    g.nextCameraScan = now + kCameraScanMs;
    g.cameras.clear();
    il2cpp::Array* all = api.findObjects(api.cameraType, nullptr);
    for (uintptr_t i = 0; all && i < all->max_length; ++i)
      if (void* c = all->data<void*>()[i]; c && game::alive(c)) g.cameras.emplace_back(c);
  }
  for (const il2cpp::Held& c : g.cameras) {
    void* camera = c.get();
    if (!game::alive(camera) || !il2cpp::field<bool>(camera, kEquipmentOn)) continue;
    const Vector3 at = world::feet(camera), facing = game::forward(game::transform_of(camera));
    if (custom::in_view(at.x, at.z, facing.x, facing.z, target.x, target.z, custom::kBrownieCameraCone,
                        custom::kBrownieCameraRange) &&
        world::clear_line(at, target))
      return true;
  }
  return false;
}

/** In its favourite room with nobody near, calm, and a running video camera on it. */
bool alone_on_camera(void* ghost) {
  void* room = world::favourite_room(ghost);
  const Vector3 at = world::feet(ghost);
  return room && game::in_room(room, at) && world::living_within(at, kGoryoAlone) == 0 && !world::hunting(ghost) &&
         !world::appeared(ghost) && game::ghost_state(ghost) != kStateRandomEvent && on_camera(world::chest(ghost));
}

/** A point of its favourite room on the navmesh: the middle of its largest collider box. */
bool home_point(void* room, Vector3& out) {
  float best = -1;
  for (const game::Bounds& b : game::room_bounds(room)) {
    const float volume = b.extents.x * b.extents.y * b.extents.z;
    if (volume > best) {
      best = volume;
      out = b.center;
    }
  }
  if (best < 0) return false;
  NavMeshHit hit{};
  if (!api.sample(out, &hit, kSampleRange, -1, nullptr) || !hit.hit) return false;
  out = hit.position;
  return true;
}

void announce(bool on) {
  const std::vector<uint8_t> m = net::goryo_show(on);
  pg::net::send(m.data(), m.size(), pg::net::To::Others, true);
  receive_show(on);
}

void hide() {
  if (s.copy.root) custom::apparition::destroy(s.copy);
  s.copy = custom::apparition::Copy{};
}

}  // namespace

bool resolve() {
  il2cpp::Class* camera = il2cpp::cls("Assembly-CSharp.dll", "", "CCTV");
  api.cameraType = camera ? il2cpp::type_object(camera) : nullptr;
  api.sample = reinterpret_cast<decltype(api.sample)>(game::game_assembly_address(kSampleRva));
  bool ok = api.cameraType && api.sample && custom::apparition::resolve();
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1);
  ok &= il2cpp::bind(api.nameToLayer, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "LayerMask"), "NameToLayer", 1);
  if (ok) api.laserGhost = api.nameToLayer(il2cpp::string_new("LaserGhost"), nullptr);
  api.ok = ok && api.laserGhost >= 0;
  PG_LOG("goryo: resolve %s (camera-only layer %d)", api.ok ? "ok" : "unavailable (it plays as in the game)", api.laserGhost);
  return true;  // optional: without it the Goryo is vanilla
}

void tick_host(void* ghost, int type, float dt) {
  if (ghost != g.ghost || (type == kGoryo) != g.goryo) {
    if (g.show.showing()) announce(false);
    g = Host{};
    g.ghost = ghost;
    g.goryo = type == kGoryo;
  }
  if (!g.goryo || !api.ok) return;
  const bool alone = alone_on_camera(ghost);
  switch (g.show.step(alone, dt, std::uniform_real_distribution<float>(0.0f, 1.0f)(g_random))) {
    case 1:
      announce(true);
      PG_LOG("goryo: it stands visible to the camera alone");
      break;
    case -1: announce(false); break;
    default: break;
  }
  if (g.extra.step(alone, dt)) gate::try_interact(ghost);
  void* room = world::favourite_room(ghost);
  const bool hunting = world::hunting(ghost);
  const float distance = room ? world::distance_to_room(room, world::feet(ghost)) : 0.0f;
  const bool wasGoing = g.home.going;
  if (!g.home.step(hunting, world::hunt_target(ghost) != nullptr, world::hunt_search(ghost) != nullptr, steered(), distance))
    return;
  g.sinceSteer += dt;
  Vector3 point{};
  if ((!wasGoing || g.sinceSteer >= kSteerEvery) && home_point(room, point)) {
    g.sinceSteer = 0;
    game::set_destination(game::ghost_agent(ghost), point);
    if (!wasGoing) PG_LOG("goryo: it lost its prey far from its room and heads home");
  }
}

void tick_all(void* ghost, int type) {
  if (!api.ok || !s.on || type != kGoryo || world::hunting(ghost) || world::appeared(ghost)) {
    hide();
    return;
  }
  if (!s.copy.root) {
    void* model = custom::apparition::model_of(ghost);
    if (!model) return;
    s.copy = custom::apparition::make(model);
    if (!s.copy.root) return;
    custom::apparition::set_layer(s.copy, api.laserGhost);
    custom::apparition::show(s.copy, true);
  }
  const Vector3 facing = game::forward(game::transform_of(ghost));
  custom::apparition::place(s.copy, world::feet(ghost), std::atan2(facing.x, facing.z) * 180.0f / 3.14159265f);
}

void receive_show(bool on) { s.on = on; }

void reset() {
  hide();
  s.on = false;
  g = Host{};
}

}  // namespace pg::tells::goryo
