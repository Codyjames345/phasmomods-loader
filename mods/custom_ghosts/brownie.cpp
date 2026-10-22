#include "brownie.h"

#include <windows.h>

#include <cmath>
#include <vector>

#include "brownie_rule.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "players.h"

namespace pg::custom::brownie {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kEquipmentOn = 0x281;   // Equipment: switched on
constexpr size_t kGhostHunting = 0xF3;   // GhostAI: hunting, on every client
constexpr float kPlayerEye = 1.6f, kGhostChest = 1.2f;  // metres above the feet
constexpr ULONGLONG kCameraScanMs = 2000;

// Build 25811160. The game's footstep play helper sets the Noise's pitch and reach, then plays it.
constexpr uintptr_t kFootstepPlayRva = 0x243E380;  // void(Footstep, float volume, name, method)
constexpr uintptr_t kSetMaxDistanceRva = 0x245AA40;  // Noise.SetMaxDistance(Noise, float, method)
constexpr size_t kFootstepNoise = 0x30;   // Footstep: its Noise
constexpr size_t kFootstepGhost = 0x40;   // Footstep: the GhostAI it belongs to
constexpr size_t kFootstepReach = 0x48;   // Footstep: the default distance Footstep.Awake saved

struct Api {
  il2cpp::Object* cameraType = nullptr;  // CCTV: the placed video camera
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  bool ok = false;
} api;

using FootstepPlay_t = void (*)(void*, float, void*, const void*);
using SetMaxDistance_t = void (*)(void*, float, const void*);
FootstepPlay_t g_footstepPlay = nullptr;
SetMaxDistance_t g_setMaxDistance = nullptr;

struct State {
  bool seen = false, watched = false;
  std::vector<il2cpp::Held> cameras;
  ULONGLONG nextCameraScan = 0;
  il2cpp::Held haunting;  // every client: the Brownie's GhostAI while a Brownie haunts, else empty
} g;

/** Whether the straight line from `from` to `to` is clear (nothing solid within it). */
bool clear_line(Vector3 from, Vector3 to) {
  const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
  const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (d < 0.5f) return true;
  return !game::raycast(from, {dx / d, dy / d, dz / d}, d - 0.4f);  // stops short of the ghost itself
}

bool viewed_from(void* viewer, Vector3 eye, Vector3 target, float cone, float range) {
  const Vector3 f = game::forward(game::transform_of(viewer));
  return in_view(eye.x, eye.z, f.x, f.z, target.x, target.z, cone, range) && clear_line(eye, target);
}

/**
 * After the game plays a footstep, cuts the reach of a hunting Brownie's step the way the game cuts a
 * hunting Myling's (quiet_step_distance). Runs on every client: footsteps are spawned by an RPC.
 */
void detour_footstep_play(void* footstep, float volume, void* name, const void* method) {
  g_footstepPlay(footstep, volume, name, method);
  void* ghost = g.haunting.get();
  if (!footstep || !ghost || il2cpp::field<void*>(footstep, kFootstepGhost) != ghost) return;
  if (!il2cpp::field<bool>(ghost, kGhostHunting)) return;
  if (void* noise = il2cpp::field<void*>(footstep, kFootstepNoise))
    g_setMaxDistance(noise, quiet_step_distance(il2cpp::field<float>(footstep, kFootstepReach)), nullptr);
}

}  // namespace

bool resolve() {
  il2cpp::Class* camera = il2cpp::cls("Assembly-CSharp.dll", "", "CCTV");
  api.cameraType = camera ? il2cpp::type_object(camera) : nullptr;
  bool ok = api.cameraType;
  ok &= il2cpp::bind(api.findObjects, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1);
  api.ok = ok;
  PG_LOG("brownie: resolve %s", ok ? "ok" : "unavailable (it acts as any ghost)");
  return true;  // optional
}

bool create_hooks() {
  g_setMaxDistance = reinterpret_cast<SetMaxDistance_t>(game::game_assembly_address(kSetMaxDistanceRva));
  return hooks::create(game::game_assembly_address(kFootstepPlayRva), reinterpret_cast<void*>(&detour_footstep_play),
                       reinterpret_cast<void**>(&g_footstepPlay), "Footstep.Play");
}

void tick_host(void* ghost, int type) {
  g.seen = g.watched = false;
  if (!api.ok || !ghost || type != kBrownie || !game::alive(ghost)) return;
  const Vector3 feet = game::position(game::transform_of(ghost));
  const Vector3 target{feet.x, feet.y + kGhostChest, feet.z};
  game::each_player([&](void* player) {
    if (g.seen || !game::living(player)) return;
    const Vector3 p = game::position(game::transform_of(player));
    g.seen = viewed_from(player, {p.x, p.y + kPlayerEye, p.z}, target, kBrowniePlayerCone, kBrowniePlayerRange);
  });
  g.watched = g.seen;
  const ULONGLONG now = GetTickCount64();
  if (now >= g.nextCameraScan) {
    g.nextCameraScan = now + kCameraScanMs;
    g.cameras.clear();
    il2cpp::Array* all = api.findObjects(api.cameraType, nullptr);
    for (uintptr_t i = 0; all && i < all->max_length; ++i)
      if (void* c = all->data<void*>()[i]; c && game::alive(c)) g.cameras.emplace_back(c);
  }
  for (const il2cpp::Held& c : g.cameras) {
    if (g.watched) break;
    void* camera = c.get();
    if (!game::alive(camera) || !il2cpp::field<bool>(camera, kEquipmentOn)) continue;
    g.watched = viewed_from(camera, game::position(game::transform_of(camera)), target, kBrownieCameraCone, kBrownieCameraRange);
  }
}

void tick_all(void* ghost, int type) {
  const bool brownie = ghost && type == kBrownie && game::alive(ghost);
  g.haunting = brownie ? il2cpp::Held(ghost) : il2cpp::Held();
}

bool available() { return api.ok; }

bool seen() { return g.seen; }
bool watched() { return g.watched; }

void reset() {
  g = State{};
}

}  // namespace pg::custom::brownie
