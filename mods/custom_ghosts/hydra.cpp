#include "hydra.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "apparition.h"
#include "game.h"
#include "ghosts.h"
#include "hydra_rule.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"

namespace pg::custom::hydra {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (REA, 2026-10-20).
constexpr size_t kGhostAppeared = 0xA8;  // GhostAI: showing itself (Appear / UnAppear RPCs, every client)
constexpr size_t kGhostSmudge = 0xF8;    // GhostAI: the WhiteSage that smudged it in this hunt (host)
constexpr uintptr_t kSampleRva = 0x518F100;  // NavMesh.SamplePosition(Vector3, out hit, float, int)
constexpr ULONGLONG kSendMs = 100;
constexpr ULONGLONG kStaleMs = 1000;     // a client with no heads message for this long shows none
constexpr ULONGLONG kRepickMs = 6000;    // a head walking this long chooses a new place anyway
constexpr float kArrived = 0.6f;         // metres from its destination a head chooses again
constexpr float kFollow = 10.0f;         // how quickly a client's head catches up with the host's

struct NavMeshHit {
  Vector3 position, normal;
  float distance;
  int mask;
  bool hit;
};

struct Api {
  il2cpp::Object* agentType = nullptr;
  bool (*warp)(void*, Vector3, const void*) = nullptr;
  void (*setSpeed)(void*, float, const void*) = nullptr;
  Vector3 (*velocity)(void*, const void*) = nullptr;
  bool (*sample)(Vector3, NavMeshHit*, float, int, const void*) = nullptr;
  bool ok = false;
} api;

struct Walker {  // the host's: a head's own NavMeshAgent
  il2cpp::Held holder;
  il2cpp::Held agent;
  ULONGLONG chose = 0;
};

struct Shown {  // every client's: a head as drawn
  apparition::Copy copy;
  Head at{}, target{};
  bool placed = false;
};

struct State {
  // host
  il2cpp::Held ghost;  // held, so a destroyed ghost never reads as alive
  il2cpp::Held lastSmudge;
  int smudges = 0;
  std::vector<Walker> walkers;
  ULONGLONG nextSend = 0;
  bool wasAppeared = false;
  bool sentEmpty = true;
  // every client
  std::vector<Shown> heads;
  std::vector<Head> heard;
  ULONGLONG heardAt = 0;
  apparition::Copy second;
  ULONGLONG secondAt = 0;
  ULONGLONG lastTick = 0;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

float yaw_of(Vector3 v) { return std::atan2(v.x, v.z) * 180.0f / 3.14159265f; }

void drop_walker(Walker& w) {
  if (w.holder && game::alive(w.holder)) game::destroy(w.holder);
  w = Walker{};
}

void drop_heads() {
  for (Shown& s : g.heads) apparition::destroy(s.copy);
  g.heads.clear();
}

/** A place on the NavMesh within kHydraWander of `from`, or `from` when none is found. */
Vector3 somewhere_near(Vector3 from) {
  std::uniform_real_distribution<float> d(-kHydraWander, kHydraWander);
  for (int tries = 0; tries < 6; ++tries) {
    NavMeshHit hit{};
    const Vector3 guess{from.x + d(g_random), from.y, from.z + d(g_random)};
    if (api.sample(guess, &hit, 3.0f, -1, nullptr) && hit.hit) return hit.position;
  }
  return from;
}

/** One more head at the ghost; false when the engine would not make one. */
bool grow_walker(void* ghost) {
  Walker w;
  w.holder = game::new_game_object("Hydra head");
  if (!w.holder) return false;
  const Vector3 at = game::position(game::transform_of(ghost));
  game::set_position(game::object_transform(w.holder), at);
  w.agent = game::add_component(w.holder, api.agentType);
  if (!w.agent) {
    drop_walker(w);
    return false;
  }
  api.warp(w.agent, at, nullptr);
  g.walkers.push_back(std::move(w));
  PG_LOG("hydra: a smudge grows head %zu", g.walkers.size());
  return true;
}

/** The host: count smudges, walk the heads, tell every client where they are. */
void host_tick(void* ghost, ULONGLONG now) {
  const bool hunting = game::ghost_state(ghost) == game::kStateHunting;
  // A smudge is a new live WhiteSage in the field (the game tests it as a Unity object). Watched
  // outside hunts too, so a sage left from before the hunt is not counted when it starts.
  void* smudge = il2cpp::field<void*>(ghost, kGhostSmudge);
  const bool smudged = smudge && smudge != g.lastSmudge && game::alive(smudge);
  g.lastSmudge = smudge;
  if (!hunting) {
    for (Walker& w : g.walkers) drop_walker(w);
    g.walkers.clear();
    g.smudges = 0;
  } else {
    if (smudged) ++g.smudges;
    while (static_cast<int>(g.walkers.size()) < hydra_heads(g.smudges))
      if (!grow_walker(ghost)) break;  // tried again next frame
    const float speed = game::agent_speed(game::ghost_agent(ghost));
    for (Walker& w : g.walkers) {
      if (!game::alive(w.agent)) continue;
      api.setSpeed(w.agent, speed, nullptr);  // as fast as the real one, so nothing gives them away
      const float left = game::agent_path_length(w.agent);
      if (!w.chose || now - w.chose > kRepickMs || (left >= 0 && left < kArrived)) {
        game::set_destination(w.agent, somewhere_near(game::position(game::transform_of(w.agent))));
        w.chose = now;
      }
    }
  }
  if (now < g.nextSend || (g.walkers.empty() && g.sentEmpty)) return;
  g.nextSend = now + kSendMs;
  std::vector<Head> heads;
  for (const Walker& w : g.walkers) {
    if (!game::alive(w.agent)) continue;
    const Vector3 p = game::position(game::transform_of(w.agent));
    const Vector3 v = api.velocity(w.agent, nullptr);
    heads.push_back({p.x, p.y, p.z, v.x * v.x + v.z * v.z > 0.01f ? yaw_of(v) : 0});
  }
  const std::vector<uint8_t> message = encode_heads(heads);
  net::send(message.data(), message.size(), net::To::Others, false);
  g.sentEmpty = heads.empty();
  g.heard = heads;  // the host draws them as every client does
  g.heardAt = now;
}

/** The host: some appearances outside hunts show a second apparition beside the ghost. */
void host_double(void* ghost, ULONGLONG now) {
  const bool appeared = il2cpp::field<bool>(ghost, kGhostAppeared) && game::ghost_state(ghost) != game::kStateHunting;
  const bool began = appeared && !g.wasAppeared;
  g.wasAppeared = appeared;
  if (!began || std::uniform_real_distribution<float>(0, 1)(g_random) >= kHydraDoubleChance) return;
  void* t = game::transform_of(ghost);
  const Vector3 at = game::position(t), f = game::forward(t);
  const Head h{at.x + f.z * kHydraDoubleSide, at.y, at.z - f.x * kHydraDoubleSide, yaw_of(f)};  // to its side
  const std::vector<uint8_t> message = encode_double(h);
  net::send(message.data(), message.size(), net::To::Others, true);
  receive(game::local_actor(), message.data(), message.size());  // the host sees it too
  (void)now;
}

/** Every client: draw the heads where the host says, flickering and moving as the real ghost. */
void draw(void* ghost, float dt, ULONGLONG now) {
  void* model = apparition::model_of(ghost);
  const bool fresh = now - g.heardAt < kStaleMs;
  const size_t want = fresh && model ? g.heard.size() : 0;
  while (g.heads.size() > want) {
    apparition::destroy(g.heads.back().copy);
    g.heads.pop_back();
  }
  while (g.heads.size() < want) {
    Shown s;
    s.copy = apparition::make(model);
    if (!s.copy.root) break;
    g.heads.push_back(std::move(s));
  }
  const float k = std::min(1.0f, dt * kFollow);
  for (size_t i = 0; i < g.heads.size(); ++i) {
    Shown& s = g.heads[i];
    s.target = g.heard[i];
    if (!s.placed) s.at = s.target;
    s.placed = true;
    s.at.x += (s.target.x - s.at.x) * k;
    s.at.y += (s.target.y - s.at.y) * k;
    s.at.z += (s.target.z - s.at.z) * k;
    s.at.yaw = s.target.yaw;
    apparition::place(s.copy, {s.at.x, s.at.y, s.at.z}, s.at.yaw);
    apparition::mirror(s.copy, model);
  }
  if (g.second.root) {
    apparition::mirror(g.second, model);  // its walk, from the real one
    const float t = (now - g.secondAt) / 1000.0f;
    apparition::show(g.second, hydra_double_visible(t));
    if (t >= kHydraFade) apparition::destroy(g.second);
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* agent = il2cpp::cls("UnityEngine.AIModule.dll", "UnityEngine.AI", "NavMeshAgent");
  api.agentType = agent ? il2cpp::type_object(agent) : nullptr;
  bool ok = api.agentType != nullptr;
  ok &= il2cpp::bind(api.warp, agent, "Warp", 1);
  ok &= il2cpp::bind(api.setSpeed, agent, "set_speed", 1);
  ok &= il2cpp::bind(api.velocity, agent, "get_velocity", 0);
  api.sample = reinterpret_cast<decltype(api.sample)>(game::game_assembly_address(kSampleRva));
  api.ok = ok && api.sample;
  PG_LOG("hydra: resolve %s", api.ok ? "ok" : "unavailable (no false heads)");
  return true;  // optional: the Hydra still hunts
}

void tick(void* ghost, int type) {
  if (!api.ok) return;
  if (!ghost || type != kHydra || !game::alive(ghost)) {
    if (g.ghost || !g.heads.empty() || g.second.root) reset();
    return;
  }
  if (ghost != g.ghost) {
    reset();
    g.ghost = ghost;
  }
  const ULONGLONG now = GetTickCount64();
  const float dt = g.lastTick ? (now - g.lastTick) / 1000.0f : 0;
  g.lastTick = now;
  if (game::is_master()) {
    host_tick(ghost, now);
    host_double(ghost, now);
  }
  draw(ghost, dt, now);
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (sender != game::master_actor() && sender != game::local_actor()) return;
  const uint8_t kind = net_msg::kind_of(data, size);
  if (kind == net_msg::kDecoys && decode_heads(data, size, g.heard)) {
    g.heardAt = GetTickCount64();
  } else if (kind == net_msg::kDouble) {
    Head h{};
    if (!decode_double(data, size, h) || !g.ghost) return;
    apparition::destroy(g.second);
    g.second = apparition::make(apparition::model_of(g.ghost));
    apparition::place(g.second, {h.x, h.y, h.z}, h.yaw);
    g.secondAt = GetTickCount64();
  }
}

void reset() {
  for (Walker& w : g.walkers) drop_walker(w);
  drop_heads();
  apparition::destroy(g.second);
  g = State{};
}

}  // namespace pg::custom::hydra
