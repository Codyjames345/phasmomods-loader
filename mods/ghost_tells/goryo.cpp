#include "goryo.h"

#include <windows.h>

#include <cmath>
#include <random>
#include <vector>

#include "Equipment.h"
#include "GhostAI.h"
#include "Scene.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "apparition.h"
#include "brownie_rule.h"
#include "content.h"
#include "game.h"
#include "game_api.h"
#include "gate.h"
#include "goryo_rule.h"
#include "held.h"
#include "log.h"
#include "net.h"
#include "runtime.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pm::tells::goryo {
namespace {

using world::Vector3;

constexpr float kSteerEvery = 1.0f;   // seconds between the destinations it is given on its way home
constexpr float kSampleRange = 3.0f;  // metres from its room's middle a point on the navmesh is sought
// Unity's NavMesh.AllAreas, a documented engine constant the compiler folds into callers.
constexpr int32_t kAllAreas = -1;

bool g_figures = false;  // the model copy (Custom Ghosts' apparition code) resolved

struct Host {
  api::Held ghost;
  CameraShow show;
  Extra extra;
  HomeSearch home;
  float sinceSteer = 0;
  game::CameraScan cameras;  // the placed video cameras, rescanned every 2 s (core/game.h)
} g;

struct Shown {  // every client: the figure the host has asked for
  bool on = false;
  custom::apparition::Copy copy;
} s;

std::mt19937 g_random{std::random_device{}()};

/** A running placed video camera has `target` in its view (the Brownie's camera cone and range). */
bool on_camera(Vector3 target) {
  bool seen = false;
  game::each_running_camera(g.cameras, GetTickCount64(), [&](void* camera) {
    const Vector3 at = world::feet(camera), facing = world::forward(camera);
    seen = custom::in_view(at.x, at.z, facing.x, facing.z, target.x, target.z, custom::kBrownieCameraCone,
                           custom::kBrownieCameraRange) &&
           world::clear_line(at, target);
    return seen;
  });
  return seen;
}

/** In its favourite room with nobody near, calm, and a running video camera on it. */
bool alone_on_camera(void* ghost) {
  void* room = world::favourite_room(ghost);
  const Vector3 at = world::feet(ghost);
  return room && world::in_room(room, at) && world::living_within(at, kGoryoAlone) == 0 && !world::hunting(ghost) &&
         !world::appeared(ghost) && world::state(ghost) != phasmo::GhostState::randomEvent() &&
         on_camera(world::chest(ghost));
}

/** A point of its favourite room on the navmesh: the middle of its largest collider box. */
bool home_point(void* room, Vector3& out) {
  float best = -1;
  for (const world::Bounds& b : world::room_bounds(room)) {
    const float volume = b.extents.x * b.extents.y * b.extents.z;
    if (volume > best) {
      best = volume;
      out = b.center;
    }
  }
  if (best < 0) return false;
  phasmo::NavMeshHit hit{};
  if (!phasmo::NavMesh::SamplePosition(&out, &hit, kSampleRange, kAllAreas) || !hit.hit) return false;
  out = hit.position;
  return true;
}

void announce(bool on) {
  const std::vector<uint8_t> m = net::goryo_show(on);
  pm::net::send(m.data(), m.size(), pm::net::To::Others, true);
  receive_show(on);
}

void hide() {
  if (s.copy.root) custom::apparition::destroy(s.copy);
  s.copy = custom::apparition::Copy{};
}

}  // namespace

void resolve() {
  g_figures = custom::apparition::resolve();
  if (!g_figures) PM_LOG("goryo: the model copy is unavailable: it shows no figure to the camera");
}

void tick_host(void* ghost, float dt) {
  if (ghost != g.ghost.get()) {
    if (g.show.showing()) announce(false);
    g = Host{};
    g.ghost = ghost;
  }
  const bool alone = alone_on_camera(ghost);
  switch (g.show.step(alone, dt, std::uniform_real_distribution<float>(0.0f, 1.0f)(g_random))) {
    case 1:
      announce(true);
      PM_LOG("goryo: it stands visible to the camera alone");
      break;
    case -1: announce(false); break;
    default: break;
  }
  if (g.extra.step(alone, dt)) gate::try_interact(ghost);
  void* room = world::favourite_room(ghost);
  const bool hunting = world::hunting(ghost);
  const float distance = room ? world::distance_to_room(room, world::feet(ghost)) : 0.0f;
  const bool wasGoing = g.home.going;
  if (!g.home.step(hunting, world::hunt_target(ghost) != nullptr, world::hunt_search(ghost) != nullptr,
                   api::content::player_controlled(), distance))
    return;
  g.sinceSteer += dt;
  Vector3 point{};
  if ((!wasGoing || g.sinceSteer >= kSteerEvery) && home_point(room, point)) {
    g.sinceSteer = 0;
    phasmo::NavMeshAgent::SetDestination(phasmo::GhostAI::agent.get(ghost), &point);
    if (!wasGoing) PM_LOG("goryo: it lost its prey far from its room and heads home");
  }
}

void tick_all(void* ghost) {
  const int layer = phasmo::Layers::LaserGhost.index();
  if (!g_figures || layer < 0 || !s.on || world::hunting(ghost) || world::appeared(ghost)) {
    hide();
    return;
  }
  if (!s.copy.root) {
    void* model = custom::apparition::model_of(ghost);
    if (!model) return;
    s.copy = custom::apparition::make(model);
    if (!s.copy.root) return;
    custom::apparition::set_layer(s.copy, layer);
    custom::apparition::show(s.copy, true);
  }
  const Vector3 facing = world::forward(ghost), at = world::feet(ghost);
  custom::apparition::place(s.copy, {at.x, at.y, at.z}, std::atan2(facing.x, facing.z) * 180.0f / 3.14159265f);
}

void receive_show(bool on) { s.on = on; }

void reset() {
  hide();
  s.on = false;
  g = Host{};
}

}  // namespace pm::tells::goryo
