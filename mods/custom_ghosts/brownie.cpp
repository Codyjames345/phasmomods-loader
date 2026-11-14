#include "brownie.h"

#include <windows.h>

#include <cmath>
#include <utility>
#include <vector>

#include "Equipment.h"
#include "Footstep.h"
#include "GhostAI.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Transform.h"
#include "UnityObject.h"
#include "brownie_rule.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "net.h"
#include "players.h"
#include "runtime.h"

namespace pm::custom::brownie {
namespace {

using phasmo::Vector3;

constexpr float kPlayerEye = 1.6f, kGhostChest = 1.2f;  // metres above the feet

struct State {
  bool seen = false, watched = false;
  game::CameraScan cameras;  // the placed video cameras, rescanned every 2 s (core/game.h)
  SeenTeller teller;   // the host: when it says `seen` to the clients
  api::Held haunting;  // every client: the Brownie's GhostAI while a Brownie haunts, else empty
  bool heardSeen = false;  // a client: what the host last said of `seen` while this Brownie haunts
  // Every client: each Noise whose cube collider a silent step switched off, with that collider.
  std::vector<std::pair<api::Held, api::Held>> muted;
} g;

/** Switches back on the cube collider a silent step switched off for `noise`, or every one for null. */
void unmute(void* noise) {
  for (auto it = g.muted.begin(); it != g.muted.end();) {
    if (noise && it->first.get() != noise) {
      ++it;
      continue;
    }
    if (api::alive(it->second.get())) phasmo::Collider::set_enabled(it->second.get(), true);
    it = g.muted.erase(it);
  }
}

/**
 * Switches off the collider on the Noise's own object (the footstep's "Noise Cube", a trigger on the Noise
 * layer), so that no sensor's physics query finds the step. It stays off while the cube plays and is inactive,
 * until the footstep's next step that is not silent, or until no Brownie haunts.
 */
void mute(void* noise) {
  for (const auto& m : g.muted)
    if (m.first.get() == noise) return;
  void* collider = phasmo::Component::GetComponent(noise, phasmo::Collider::type.type_object());
  if (!collider) return;
  phasmo::Collider::set_enabled(collider, false);
  g.muted.emplace_back(noise, collider);
}

/** Whether anyone looks at the Brownie, as this client knows it: the host's own, or what the host last said. */
bool seen_now() { return phasmo::PhotonNetwork::get_IsMasterClient() ? g.seen : g.heardSeen; }

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
 * Whether someone sees it on the truck's monitor: a player is actively viewing the monitor, and a switched-on camera of the
 * kind it shows has the Brownie in view. A viewer's grid page stays on their own client, so every camera of that kind counts.
 */
bool on_the_monitor(Vector3 target) {
  void* monitor = phasmo::CCTVController::instance.get();
  if (!game::alive(monitor)) return false;
  void* viewers = phasmo::CCTVController::viewers.get(monitor);
  if (!viewers || api::List{viewers}.size() == 0) return false;
  void* shown = phasmo::CCTVController::GetCCTVCamerasByCurrentCameraType(monitor);
  if (!shown) return false;
  const api::List cameras{shown};
  for (size_t i = 0; i < cameras.size(); ++i) {
    void* camera = cameras[i];
    if (game::alive(camera) && viewed_from(camera, game::position(game::transform_of(camera)), target, kBrownieCameraCone,
                                           kBrownieCameraRange))
      return true;
  }
  return false;
}

/**
 * A footstep of the haunting Brownie through the game's own play (Footstep.Play: the Noise's pitch and reach,
 * then the clip through Noise.PlaySound, then the AudioSource's volume times Play's argument), by its rule
 * (brownie_step). Unchanged outside hunts. In a hunt while anyone looks at it, the game cuts its reach as it
 * cuts a hunting Myling's (quiet_step_distance). While nobody looks, its Noise Cube's collider is switched off
 * before the game's play activates the cube, so neither sensor detects the step; the game's play gets volume 0,
 * which silences the step's AudioSource and nothing else; and then the Noise's volume and loudness, which the
 * sound sensor and the parabolic microphone read, are zeroed. The step's timing, pitch, reach, clip and print stay
 * the game's. A pooled footstep's next step that is not silent gets its collider back before the game plays it.
 * Runs on every client: footsteps are spawned by an RPC.
 */
void detour_footstep_play(api::Original<phasmo::Footstep::Play> original, void* footstep, float volume, void* name) {
  void* ghost = g.haunting.get();
  const bool its = footstep && ghost && phasmo::Footstep::ghost.get(footstep).raw() == ghost;
  const BrownieStep step = its ? brownie_step(phasmo::GhostAI::isHunting.get(ghost), seen_now()) : BrownieStep::Vanilla;
  void* noise = footstep ? phasmo::Footstep::noise.get(footstep) : nullptr;
  if (noise) {
    if (step == BrownieStep::Silent) mute(noise);
    else if (!g.muted.empty()) unmute(noise);
  }
  original(footstep, brownie_step_volume(step, volume), name);
  if (step == BrownieStep::Vanilla || !noise) return;
  if (step == BrownieStep::Quiet) {
    phasmo::Noise::SetMaxDistance(noise, quiet_step_distance(phasmo::Footstep::reach.get(footstep)));
  } else {
    phasmo::Noise::volume.set(noise, 0.0f);
    phasmo::Noise::loudness.set(noise, 0.0f);
  }
}

/** The host says whether anyone looks at the Brownie when its teller says it is due. */
void tell_seen(void* ghost) {
  if (!g.teller.due(g.seen, phasmo::GhostAI::isHunting.get(ghost), GetTickCount64())) return;
  const std::vector<uint8_t> message = net_msg::seen(g.seen);
  net::send(message.data(), message.size(), net::To::Others, true);
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::Footstep::Play>(&detour_footstep_play, "Footstep.Play", api::Twins::CanonicalOnly);
}

void tick_host(void* ghost, int type) {
  g.seen = g.watched = false;
  if (!ghost || type != kBrownie || !game::alive(ghost)) {
    g.teller = SeenTeller{};  // the next Brownie's word goes out at once
    return;
  }
  const Vector3 feet = game::position(game::transform_of(ghost));
  const Vector3 target{feet.x, feet.y + kGhostChest, feet.z};
  game::each_player([&](void* player) {
    if (g.seen || !game::living(player)) return;
    const Vector3 p = game::position(game::transform_of(player));
    g.seen = viewed_from(player, {p.x, p.y + kPlayerEye, p.z}, target, kBrowniePlayerCone, kBrowniePlayerRange);
  });
  if (!g.seen) g.seen = on_the_monitor(target);
  tell_seen(ghost);
  g.watched = g.seen;
  if (!g.watched)
    game::each_running_camera(g.cameras, GetTickCount64(), [&](void* camera) {
      g.watched = viewed_from(camera, game::position(game::transform_of(camera)), target, kBrownieCameraCone,
                              kBrownieCameraRange);
      return g.watched;
    });
}

void tick_all(void* ghost, int type) {
  const bool brownie = ghost && type == kBrownie && api::alive(ghost);
  g.haunting = brownie ? ghost : nullptr;  // the same ghost keeps its handle
  if (!brownie) {
    g.heardSeen = false;
    unmute(nullptr);
  }
}

bool available() { return true; }

bool seen() { return g.seen; }
bool watched() { return g.watched; }

void receive(int sender, const uint8_t* data, size_t size) {
  bool seen = false;
  const int master = game::master_actor();
  if (!master || sender != master || !net_msg::read_seen(data, size, seen)) return;
  g.heardSeen = seen;
}

void reset() {
  unmute(nullptr);
  g = State{};
}

}  // namespace pm::custom::brownie
