#include "onryo.h"

#include <algorithm>
#include <random>
#include <vector>

#include "FireSource.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "Photon.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "net.h"
#include "onryo_rule.h"
#include "runtime.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pm::tells::onryo {
namespace {

using phasmo::FireSource;
using world::Vector3;

struct Snuff {
  api::Held fire;
  float left;
};

struct Host {
  api::Held ghost;
  int count = 0;
  std::vector<Snuff> snuffs;
} g;

struct Shown {  // every client
  Flicker flicker;
  Spite spite;
  struct Burning {
    api::Held fire, light, flame;  // the FireSource, its Light and its flame's Transform
    float intensity;
    Vector3 scale;
  };
  std::vector<Burning> changed;  // flames burning differently now, with their own light and size
} s;

std::mt19937 g_random{std::random_device{}()};

int view_of(void* fire) {
  void* view = phasmo::MonoBehaviourPun::get_photonView(fire);
  return view ? phasmo::PhotonView::get_ViewID(view) : 0;
}

bool lit(void* fire) { return api::alive(fire) && FireSource::isOn.get(fire); }

bool spited(void* fire) { return told_now() == kOnryo && s.spite.spited(view_of(fire)); }

void detour_blow_out(api::Original<FireSource::BlowOutFire> original, void* fire, bool byGhost) {
  const bool mark = byGhost && told_now() == kOnryo && world::is_master() && lit(fire);
  original(fire, byGhost);
  if (!mark) return;
  const int view = view_of(fire);
  s.spite.mark(view, kOnryoSpite);
  const std::vector<uint8_t> m = net::onryo_spite(view, kOnryoSpite);
  pm::net::send(m.data(), m.size(), pm::net::To::Others, true);
}

void detour_ignite(api::Original<FireSource::IgniteFire> original, void* fire, bool byGhost) {
  if (!byGhost && spited(fire)) return;  // its spite: the wick will not take, so nothing is sent
  original(fire, byGhost);
}

// The networked lighting always goes through: refusing it here would leave each client deciding from its
// own spite timer, and they could disagree about whether the flame burns.
void detour_ignite_net(api::Original<FireSource::IgniteFireNetworked> original, void* fire, bool byGhost, void* info) {
  original(fire, byGhost, info);
  void* ghost = g.ghost.get();
  if (byGhost || told_now() != kOnryo || !world::is_master() || !api::alive(ghost) || !lit(fire) ||
      world::hunting(ghost))
    return;
  const Vector3 at = world::feet(fire), ghostAt = world::feet(ghost);
  if (!world::same_floor(at, ghostAt) || world::distance(at, ghostAt) > kOnryoReach) return;
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  const float delay = snuff_delay(roll(g_random), roll(g_random));
  if (delay >= 0) g.snuffs.push_back({api::Held(fire), delay});
}

/** Every flame this client changed back to its own light and size. */
void restore_flames() {
  for (Shown::Burning& b : s.changed) {
    if (api::alive(b.light.get())) phasmo::Light::set_intensity(b.light.get(), b.intensity);
    if (api::alive(b.flame.get())) phasmo::Transform::set_localScale(b.flame.get(), &b.scale);
  }
  s.changed.clear();
}

/** The record of a lit flame's own light and size, made the first time it burns differently. */
Shown::Burning& burning(void* fire) {
  for (Shown::Burning& b : s.changed)
    if (b.fire.get() == fire) return b;
  void* l = FireSource::flameLight.get(fire);
  void* f = FireSource::flame.get(fire);
  void* flame = api::alive(f) ? phasmo::GameObject::get_transform(f) : nullptr;
  Shown::Burning b{api::Held(fire), api::Held(api::alive(l) ? l : nullptr), api::Held(flame), 0, {1, 1, 1}};
  if (b.light.get()) b.intensity = phasmo::Light::get_intensity(b.light.get());
  if (b.flame.get()) b.scale = phasmo::Transform::get_localScale(b.flame.get());
  s.changed.push_back(std::move(b));
  return s.changed.back();
}

/** Every lit flame burning as the count says. */
void show_flames() {
  const float light = s.flicker.light(), scale = s.flicker.scale();
  void* level = phasmo::LevelController::instance.get();
  void* fires = api::alive(level) ? phasmo::LevelController::fireSources.get(level) : nullptr;
  if ((light >= 0.999f && scale >= 0.999f) || !fires) {
    restore_flames();
    return;
  }
  const api::List list{fires};
  for (size_t i = 0; i < list.size(); ++i) {
    void* fire = list[i];
    if (!lit(fire)) continue;
    Shown::Burning& b = burning(fire);
    if (api::alive(b.light.get())) phasmo::Light::set_intensity(b.light.get(), b.intensity * light);
    if (api::alive(b.flame.get())) {
      const Vector3 scaled{b.scale.x * scale, b.scale.y * scale, b.scale.z * scale};
      phasmo::Transform::set_localScale(b.flame.get(), &scaled);
    }
  }
}

}  // namespace

bool create_hooks() {
  return api::hook<FireSource::BlowOutFire>(&detour_blow_out, "FireSource.BlowOutFire", api::Twins::CanonicalOnly) &&
         api::hook<FireSource::IgniteFire>(&detour_ignite, "FireSource.IgniteFire", api::Twins::CanonicalOnly) &&
         api::hook<FireSource::IgniteFireNetworked>(&detour_ignite_net, "FireSource.IgniteFireNetworked",
                                                    api::Twins::CanonicalOnly);
}

void tick_host(void* ghost, float dt) {
  if (ghost != g.ghost.get()) {
    g = Host{};
    g.ghost = ghost;
  }
  const int count = phasmo::GhostAI::flamesBlownOut.get(ghost);
  if (count != g.count) {
    g.count = count;
    const std::vector<uint8_t> m = net::onryo_count(count);
    pm::net::send(m.data(), m.size(), pm::net::To::Others, true);
    receive_count(count);
    PM_LOG("onryo: its count of blown-out flames is %d", count);
  }
  for (Snuff& sn : g.snuffs) {
    sn.left -= dt;
    // Its own blow-out, as any ghost's: EMF, the sync, and one more on its count.
    if (sn.left <= 0 && lit(sn.fire.get()) && !world::hunting(ghost)) {
      FireSource::BlowOutFire(sn.fire.get(), true);
      PM_LOG("onryo: it snuffs a flame lit near it");
    }
  }
  std::erase_if(g.snuffs, [](const Snuff& sn) { return sn.left <= 0; });
}

void tick_all(float dt) {
  s.flicker.step(dt);
  s.spite.step(dt);
  show_flames();
}

void receive_count(int count) { s.flicker.count(count); }
void receive_spite(int view, float seconds) { s.spite.mark(view, seconds); }

void reset() {
  restore_flames();
  s = Shown{};
  g = Host{};
}

}  // namespace pm::tells::onryo
