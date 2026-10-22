#include "onryo.h"

#include <algorithm>
#include <random>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"
#include "onryo_rule.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pg::tells::onryo {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. FireSource's fields as
// Spectral Flame reads them (REA of FireSource.Start/Update/BlowOutFire); the count from FireSource
// 0x656660, which adds one for an Onryo on every blow-out by a ghost.
constexpr size_t kLevelFireSources = 0xC0;     // LevelController: List<FireSource>
constexpr size_t kFireFlame = 0x30;            // FireSource: its flame GameObject
constexpr size_t kFireLight = 0x38;            // FireSource: its Light
constexpr size_t kFireLit = 0xB0;              // FireSource: burning
constexpr size_t kGhostBlownOut = 0x120;       // GhostAI: the Onryo's count of flames blown out
constexpr uintptr_t kBlowOutRva = 0x652090;    // FireSource.BlowOutFire(bool byGhost): host, then RPC
constexpr uintptr_t kIgniteRva = 0x6528C0;     // FireSource.IgniteFire(bool byGhost): the lighting client
constexpr uintptr_t kIgniteNetRva = 0x6526A0;  // FireSource.IgniteFireNetworked(bool byGhost, info): RPC, every client

struct Api {
  void* (*photonView)(void*, const void*) = nullptr;
  int (*viewId)(void*, const void*) = nullptr;
  float (*intensity)(void*, const void*) = nullptr;
  void (*setIntensity)(void*, float, const void*) = nullptr;
  bool ok = false;
} api;

using BlowOut_t = void (*)(void*, bool, const void*);
using Ignite_t = void (*)(void*, bool, const void*);
using IgniteNet_t = void (*)(void*, bool, void*, const void*);  // PhotonMessageInfo travels by reference
BlowOut_t g_blowOut = nullptr;
Ignite_t g_ignite = nullptr;
IgniteNet_t g_igniteNet = nullptr;

struct Snuff {
  il2cpp::Held fire;
  float left;
};

struct Host {
  il2cpp::Held ghost;
  bool onryo = false;
  int count = 0;
  std::vector<Snuff> snuffs;
} g;

struct Shown {  // every client
  int type = -1;
  Flicker flicker;
  Spite spite;
  struct Burning {
    il2cpp::Held fire, light, flame;  // the FireSource, its Light and its flame's Transform
    float intensity;
    Vector3 scale;
  };
  std::vector<Burning> changed;  // flames burning differently now, with their own light and size
} s;

std::mt19937 g_random{std::random_device{}()};

int view_of(void* fire) {
  void* view = api.photonView(fire, nullptr);
  return view ? api.viewId(view, nullptr) : 0;
}

bool lit(void* fire) { return fire && game::alive(fire) && il2cpp::field<bool>(fire, kFireLit); }

/** The Onryo haunts this contract and the tells are on (asked from detours, on any client). */
bool onryo_now() { return api.ok && tells_on() && world::behaviour_type(game::ghost_ai()) == kOnryo; }

bool spited(void* fire) { return onryo_now() && s.spite.spited(view_of(fire)); }

void detour_blow_out(void* fire, bool byGhost, const void* method) {
  hooks::Guard guard;
  const bool mark = byGhost && game::is_master() && lit(fire) && onryo_now();
  g_blowOut(fire, byGhost, method);
  if (!mark) return;
  const int view = view_of(fire);
  s.spite.mark(view, kOnryoSpite);
  const std::vector<uint8_t> m = net::onryo_spite(view, kOnryoSpite);
  pg::net::send(m.data(), m.size(), pg::net::To::Others, true);
}

void detour_ignite(void* fire, bool byGhost, const void* method) {
  hooks::Guard guard;
  if (!byGhost && spited(fire)) return;  // its spite: the wick will not take, so nothing is sent
  g_ignite(fire, byGhost, method);
}

// The networked lighting always goes through: refusing it here would leave each client deciding from its
// own spite timer, and they could disagree about whether the flame burns.
void detour_ignite_net(void* fire, bool byGhost, void* info, const void* method) {
  hooks::Guard guard;
  g_igniteNet(fire, byGhost, info, method);
  if (byGhost || !game::is_master() || !g.onryo || !g.ghost || !lit(fire) || world::hunting(g.ghost)) return;
  const Vector3 at = world::feet(fire), ghost = world::feet(g.ghost);
  if (!world::same_floor(at, ghost) || game::distance(at, ghost) > kOnryoReach) return;
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  const float delay = snuff_delay(roll(g_random), roll(g_random));
  if (delay >= 0) g.snuffs.push_back({il2cpp::Held(fire), delay});
}

/** Every flame this client changed back to its own light and size. */
void restore_flames() {
  for (Shown::Burning& b : s.changed) {
    if (game::alive(b.light)) api.setIntensity(b.light, b.intensity, nullptr);
    if (game::alive(b.flame)) game::set_local_scale(b.flame, b.scale);
  }
  s.changed.clear();
}

/** The record of a lit flame's own light and size, made the first time it burns differently. */
Shown::Burning& burning(void* fire) {
  for (Shown::Burning& b : s.changed)
    if (b.fire == fire) return b;
  void* l = il2cpp::field<void*>(fire, kFireLight);
  void* f = il2cpp::field<void*>(fire, kFireFlame);
  Shown::Burning b{il2cpp::Held(fire), il2cpp::Held(game::alive(l) ? l : nullptr),
                   il2cpp::Held(game::alive(f) ? game::object_transform(f) : nullptr), 0, {1, 1, 1}};
  if (b.light) b.intensity = api.intensity(b.light, nullptr);
  if (b.flame) b.scale = game::local_scale(b.flame);
  s.changed.push_back(std::move(b));
  return s.changed.back();
}

/** Every lit flame burning as the count says. */
void show_flames() {
  const float light = s.flicker.light(), scale = s.flicker.scale();
  void* level = game::level_controller();
  auto* fires = level && game::alive(level) ? il2cpp::field<il2cpp::List*>(level, kLevelFireSources) : nullptr;
  if ((light >= 0.999f && scale >= 0.999f) || !fires) {
    restore_flames();
    return;
  }
  for (int i = 0; fires->items && i < fires->size; ++i) {
    void* fire = fires->items->data<void*>()[i];
    if (!lit(fire)) continue;
    Shown::Burning& b = burning(fire);
    if (game::alive(b.light)) api.setIntensity(b.light, b.intensity * light, nullptr);
    if (game::alive(b.flame)) game::set_local_scale(b.flame, {b.scale.x * scale, b.scale.y * scale, b.scale.z * scale});
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* light = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Light");
  bool ok = il2cpp::bind(api.photonView, il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "MonoBehaviourPun"), "get_photonView", 0);
  ok &= il2cpp::bind(api.viewId, il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonView"), "get_ViewID", 0);
  ok &= il2cpp::bind(api.intensity, light, "get_intensity", 0);
  ok &= il2cpp::bind(api.setIntensity, light, "set_intensity", 1);
  api.ok = ok;
  PG_LOG("onryo: resolve %s", ok ? "ok" : "unavailable (it plays as in the game)");
  return true;  // optional: without it the Onryo is vanilla
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kBlowOutRva), reinterpret_cast<void*>(&detour_blow_out),
                       reinterpret_cast<void**>(&g_blowOut), "FireSource.BlowOutFire") &&
         hooks::create(game::game_assembly_address(kIgniteRva), reinterpret_cast<void*>(&detour_ignite),
                       reinterpret_cast<void**>(&g_ignite), "FireSource.IgniteFire") &&
         hooks::create(game::game_assembly_address(kIgniteNetRva), reinterpret_cast<void*>(&detour_ignite_net),
                       reinterpret_cast<void**>(&g_igniteNet), "FireSource.IgniteFireNetworked");
}

void tick_host(void* ghost, int type, float dt) {
  if (ghost != g.ghost || (type == kOnryo) != g.onryo) {
    g = Host{};
    g.ghost = ghost;
    g.onryo = type == kOnryo;
  }
  if (!g.onryo || !api.ok) return;
  const int count = il2cpp::field<int>(ghost, kGhostBlownOut);
  if (count != g.count) {
    g.count = count;
    const std::vector<uint8_t> m = net::onryo_count(count);
    pg::net::send(m.data(), m.size(), pg::net::To::Others, true);
    receive_count(count);
    PG_LOG("onryo: its count of blown-out flames is %d", count);
  }
  for (Snuff& sn : g.snuffs) {
    sn.left -= dt;
    // Its own blow-out, as any ghost's: EMF, the sync, and one more on its count.
    if (sn.left <= 0 && lit(sn.fire) && !world::hunting(ghost)) {
      reinterpret_cast<BlowOut_t>(game::game_assembly_address(kBlowOutRva))(sn.fire, true, nullptr);
      PG_LOG("onryo: it snuffs a flame lit near it");
    }
  }
  g.snuffs.erase(std::remove_if(g.snuffs.begin(), g.snuffs.end(), [](const Snuff& sn) { return sn.left <= 0; }), g.snuffs.end());
}

void tick_all(int type, float dt) {
  if (!api.ok || type != kOnryo) {
    if (s.type == kOnryo) reset();
    s.type = type;
    return;
  }
  s.type = type;
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

}  // namespace pg::tells::onryo
