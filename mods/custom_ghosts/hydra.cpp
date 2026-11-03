#include "hydra.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "GhostTypes.h"
#include "Props.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "apparition.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "hydra_rule.h"
#include "log.h"
#include "net.h"
#include "runtime.h"
#include "sensor_copy.h"

namespace pm::custom::hydra {
namespace {

using phasmo::Vector3;

constexpr ULONGLONG kSendMs = 100;
constexpr ULONGLONG kStaleMs = 1000;     // a client with no heads message for this long shows none
constexpr ULONGLONG kRepickMs = 6000;    // a head walking this long chooses a new place anyway
constexpr float kArrived = 0.6f;         // metres from its destination a head chooses again
constexpr float kFollow = 10.0f;         // how quickly a client's head catches up with the host's
constexpr ULONGLONG kThrowNoteMs = 5000;  // the throw diagnostics: one line at most this often

struct Walker {  // the host's: a head's own NavMeshAgent
  api::Held holder;
  api::Held agent;
  ULONGLONG chose = 0;
  api::Held sensorObject;  // the head's own RangeSensor and the GameObject it is on (sensor_copy.h)
  api::Held sensor;
  bool sensorFailed = false;  // its sensor could not be made: it throws nothing this hunt
};

struct Shown {  // every client's: a head as drawn
  apparition::Copy copy;
  Head at{}, target{};
  bool placed = false;
};

struct State {
  // host
  api::Held ghost;  // held, so a destroyed ghost never reads as alive
  api::Held lastSmudge;
  int smudges = 0;
  std::vector<Walker> walkers;
  ULONGLONG nextSend = 0;
  bool wasAppeared = false;
  bool sentEmpty = true;
  api::Held throwHunt;        // the hunt state whose throw timer is read
  float throwTimer = 0;
  bool throwNoted = false;    // the heads' sensor setup was logged for this hunt
  bool throwing = false;      // a head's TryThrow is running (not the ghost's)
  std::vector<api::Held> ghostThrew;  // what the ghost itself threw since the last tick
  ULONGLONG nextThrowNote = 0;
  int ticks = 0, rolled = 0, thrown = 0;  // since the last throw diagnostic line
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

phasmo::Vector3 vec(Vector3 v) { return {v.x, v.y, v.z}; }

void drop_walker(Walker& w) {
  sensor_copy::drop(w.sensorObject, w.sensor);
  if (api::alive(w.holder)) phasmo::Object::Destroy(w.holder, 0.0f);
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
    phasmo::NavMeshHit hit{};
    const phasmo::Vector3 guess{from.x + d(g_random), from.y, from.z + d(g_random)};
    if (phasmo::NavMesh::SamplePosition(&guess, &hit, 3.0f, -1) && hit.hit)
      return {hit.position.x, hit.position.y, hit.position.z};
  }
  return from;
}

/** One more head at the ghost; false when the engine would not make one. */
bool grow_walker(void* ghost) {
  Walker w;
  void* holder = phasmo::GameObject::type.make();
  if (holder) phasmo::GameObject::ctor(holder, api::make_string("Hydra head"));
  if (!holder) return false;
  w.holder = holder;
  const phasmo::Vector3 at = vec(game::position(game::transform_of(ghost)));
  phasmo::Transform::set_position(phasmo::GameObject::get_transform(holder), &at);
  w.agent = phasmo::GameObject::AddComponent(holder, phasmo::NavMeshAgent::type.type_object());
  if (!w.agent) {
    drop_walker(w);
    return false;
  }
  phasmo::NavMeshAgent::Warp(w.agent, &at);
  g.walkers.push_back(std::move(w));
  PM_LOG("hydra: a smudge grows head %zu", g.walkers.size());
  return true;
}

Vec3 vec3(Vector3 v) { return {v.x, v.y, v.z}; }
Quat quat(phasmo::Quaternion q) { return {q.x, q.y, q.z, q.w}; }

/**
 * The ghost's interaction sensor, when the heads can copy it: a Micosmo.SensorToolkit.RangeSensor with no
 * signal processors (a copy would not run them as the ghost's does).
 */
void* copyable_sensor(void* ghost, const char*& why) {
  void* activity = game::ghost_activity(ghost);
  void* sensor = activity ? phasmo::GhostActivity::interactionSensor.get(activity) : nullptr;
  why = nullptr;
  if (!api::alive(sensor) || !phasmo::RangeSensor::type.is(sensor))
    why = "it is not a Micosmo.SensorToolkit.RangeSensor";
  else if (api::List{phasmo::Sensor::signalProcessors.get(sensor)}.size() > 0)
    why = "it has signal processors, which a copy would not run as it does";
  return why ? nullptr : sensor;
}

/**
 * A head's own RangeSensor, set up as the ghost's (sensor_copy.h): its detection mode and filters are the
 * ghost's before it wakes, and its settings are copied at each pulse. When the engine will not make one,
 * the head is marked and logged once, and does not try again: a head lives only for its hunt, so it
 * throws nothing for the rest of the hunt.
 */
void make_head_sensor(Walker& w, void* ghostSensor) {
  const char* failed = nullptr;
  if (!sensor_copy::make(sensor_copy::Kind::Range, "Hydra head sensor", w.sensorObject, w.sensor, failed)) {
    w.sensorFailed = true;
    PM_LOG("hydra: a head's sensor could not be made (%s); that head throws nothing this hunt", failed);
    return;
  }
  if (!sensor_copy::share_filters(w.sensor, ghostSensor)) {
    sensor_copy::drop(w.sensorObject, w.sensor);
    w.sensorFailed = true;
    PM_LOG("hydra: a head's sensor could not take the ghost's detection mode; that head throws nothing this hunt");
    return;
  }
  phasmo::GameObject::SetActive(w.sensorObject, true);
}

/** Pulse a head's sensor where the ghost's would be on the head, with the ghost's sensor's settings now. */
bool pulse_head_sensor(Walker& w, void* ghost, void* ghostSensor) {
  if (!api::alive(w.sensor)) return false;
  // The shape and its size as the ghost's has them now (TryInteract sets its box before each pulse).
  sensor_copy::copy_settings(sensor_copy::Kind::Range, w.sensor, ghostSensor);
  void* ghostAt = game::transform_of(ghost);
  void* ghostSensorAt = game::transform_of(ghostSensor);
  void* headAt = game::transform_of(w.agent);
  const SensorPose pose = hydra_sensor_pose(vec3(game::position(ghostAt)), quat(game::rotation(ghostAt)),
                                            vec3(game::position(ghostSensorAt)), quat(game::rotation(ghostSensorAt)),
                                            vec3(game::position(headAt)), quat(game::rotation(headAt)));
  void* t = phasmo::GameObject::get_transform(w.sensorObject);
  const phasmo::Vector3 at{pose.at.x, pose.at.y, pose.at.z};
  const phasmo::Quaternion rotation{pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w};
  phasmo::Transform::set_position(t, &at);
  phasmo::Transform::set_rotation(t, &rotation);
  return sensor_copy::pulse(sensor_copy::Kind::Range, w.sensor, "hydra: pulsing a head's sensor threw");
}

/**
 * The Prop a head throws, picked as GetPropToThrow picks the ghost's: the Props its sensor
 * detects (each detected object's own Prop, as GetDetectedComponents<Prop> takes them), shuffled,
 * the first the game's test passes; null for none. `taken`: thrown this tick by the ghost or another head.
 */
void* prop_for_head(const Walker& w, void* activity, const api::RefArray& props, const std::vector<void*>& taken,
                    int& seen) {
  std::vector<void*> detected;
  for (size_t i = 0; i < props.size(); ++i) {
    void* prop = props[i];
    if (api::alive(prop) && phasmo::Prop::interact.get(prop) && game::sensor_detects(w.sensor, game::game_object_of(prop)))
      detected.push_back(prop);
  }
  seen += static_cast<int>(detected.size());
  std::shuffle(detected.begin(), detected.end(), g_random);
  const int pick = hydra_head_pick(true, detected, taken,
                                   [&](void* prop) { return phasmo::GhostActivity::throwable(activity, prop); });
  return pick >= 0 ? detected[pick] : nullptr;
}

/** The host, in a hunt: on each tick of the hunt's own throw timer, each head throws as the hunt does. */
void host_throws(void* ghost, ULONGLONG now) {
  void* hunt = game::ghost_state_object(ghost);
  if (!hunt) return;
  const float timer = phasmo::GhostHuntState::throwTimer.get(hunt);
  const bool ticked = hydra_throw_tick(hunt == g.throwHunt, g.throwTimer, timer);
  g.throwHunt = hunt;
  g.throwTimer = timer;
  if (!ticked) return;
  std::vector<void*> taken;  // the user's rule: one object thrown once a tick, the ghost's own throw included
  for (const api::Held& prop : g.ghostThrew) taken.push_back(prop);
  g.ghostThrew.clear();
  if (g.walkers.empty()) return;
  const char* why = nullptr;
  void* ghostSensor = copyable_sensor(ghost, why);
  // Every head gets its sensor before anything is said about them (a head grown later gets its own here).
  size_t ready = 0;
  for (Walker& w : g.walkers) {
    if (ghostSensor && api::alive(w.agent) && !w.sensor && !w.sensorFailed) make_head_sensor(w, ghostSensor);
    if (w.sensor) ++ready;
  }
  if (!g.throwNoted) {
    g.throwNoted = true;
    if (why) PM_LOG("hydra: the heads do not throw this hunt: the ghost's interaction sensor cannot be copied (%s)", why);
    else if (!ready) PM_LOG("hydra: the heads do not throw this hunt: no head's sensor could be made");
    else PM_LOG("hydra: %zu of %zu heads throw with their own copy of the ghost's interaction sensor (shape %d, layers 0x%X, "
                "detection mode %d)", ready, g.walkers.size(), phasmo::RangeSensor::shape.get(ghostSensor),
                static_cast<unsigned>(phasmo::RangeSensor::layers.get(ghostSensor)),
                phasmo::BaseVolumeSensor::detectionMode.get(ghostSensor));
  }
  if (!ghostSensor) return;
  void* activity = game::ghost_activity(ghost);
  const int huntType = phasmo::GhostHuntState::ghostType.get(hunt);
  const int poltergeist = phasmo::GhostType::Poltergeist();
  api::Held props;  // found once a head first needs them this tick
  bool found = false;
  int seen = 0;
  ++g.ticks;
  for (Walker& w : g.walkers) {
    if (!api::alive(w.agent)) continue;
    const int roll = std::uniform_int_distribution<int>(0, kHuntThrowRollEnd - 1)(g_random);
    if (!hydra_throw_rolls(huntType, poltergeist, roll)) continue;
    ++g.rolled;
    if (!w.sensor || !pulse_head_sensor(w, ghost, ghostSensor)) continue;  // made above, or failed for the hunt
    if (!found) {
      props = phasmo::Object::FindObjectsOfType(phasmo::Prop::type.type_object());
      found = true;
    }
    if (void* prop = prop_for_head(w, activity, api::RefArray{props.get()}, taken, seen)) {
      struct Throwing {  // the TryThrow detour then knows it is a head's throw, not the ghost's
        Throwing() { g.throwing = true; }
        ~Throwing() { g.throwing = false; }
      } throwing;
      phasmo::Prop::TryThrow(prop, true, true);  // as the hunt calls it: the game's throw, EMF and all
      taken.push_back(prop);
      ++g.thrown;
    }
  }
  if (now >= g.nextThrowNote) {
    g.nextThrowNote = now + kThrowNoteMs;
    PM_LOG("hydra: %d throw ticks with %zu heads: %d rolled to throw, %d threw (%d objects their sensors detected this tick)",
           g.ticks, g.walkers.size(), g.rolled, g.thrown, seen);
    g.ticks = g.rolled = g.thrown = 0;
  }
}

/** The host: count smudges, walk the heads, tell every client where they are. */
void host_tick(void* ghost, ULONGLONG now) {
  const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
  // A smudge is a new live WhiteSage in the field (the game tests it as a Unity object). Watched
  // outside hunts too, so a sage left from before the hunt is not counted when it starts.
  void* smudge = phasmo::GhostAI::smudge.get(ghost);
  const bool smudged = smudge && smudge != g.lastSmudge && api::alive(smudge);
  g.lastSmudge = smudge;
  if (!hunting) {
    for (Walker& w : g.walkers) drop_walker(w);
    g.walkers.clear();
    g.smudges = 0;
    g.throwHunt = nullptr;
    g.throwNoted = false;
    g.ghostThrew.clear();
  } else {
    host_throws(ghost, now);
    if (smudged) ++g.smudges;
    while (static_cast<int>(g.walkers.size()) < hydra_heads(g.smudges))
      if (!grow_walker(ghost)) break;  // tried again next frame
    const float speed = game::agent_speed(game::ghost_agent(ghost));
    for (Walker& w : g.walkers) {
      if (!api::alive(w.agent)) continue;
      phasmo::NavMeshAgent::set_speed(w.agent, speed);  // as fast as the real one, so nothing gives them away
      const float left = game::agent_path_length(w.agent);
      if (!w.chose || now - w.chose > kRepickMs || (left >= 0 && left < kArrived)) {
        const phasmo::Vector3 to = vec(somewhere_near(game::position(game::transform_of(w.agent))));
        phasmo::NavMeshAgent::SetDestination(w.agent, &to);
        w.chose = now;
      }
    }
  }
  if (now < g.nextSend || (g.walkers.empty() && g.sentEmpty)) return;
  g.nextSend = now + kSendMs;
  std::vector<Head> heads;
  for (const Walker& w : g.walkers) {
    if (!api::alive(w.agent)) continue;
    const Vector3 p = game::position(game::transform_of(w.agent));
    const phasmo::Vector3 v = phasmo::NavMeshAgent::get_velocity(w.agent);
    heads.push_back({p.x, p.y, p.z, v.x * v.x + v.z * v.z > 0.01f ? yaw_of({v.x, v.y, v.z}) : 0});
  }
  const std::vector<uint8_t> message = encode_heads(heads);
  net::send(message.data(), message.size(), net::To::Others, false);
  g.sentEmpty = heads.empty();
  g.heard = heads;  // the host draws them as every client does
  g.heardAt = now;
}

/** The host: some appearances outside hunts show a second apparition beside the ghost. */
void host_double(void* ghost) {
  const bool appeared =
      phasmo::GhostAI::isAppeared.get(ghost) && game::ghost_state(ghost) != phasmo::GhostState::hunting();
  const bool began = appeared && !g.wasAppeared;
  g.wasAppeared = appeared;
  if (!began || std::uniform_real_distribution<float>(0, 1)(g_random) >= kHydraDoubleChance) return;
  void* t = game::transform_of(ghost);
  const Vector3 at = game::position(t), f = game::forward(t);
  const Head h{at.x + f.z * kHydraDoubleSide, at.y, at.z - f.x * kHydraDoubleSide, yaw_of(f)};  // to its side
  const std::vector<uint8_t> message = encode_double(h);
  net::send(message.data(), message.size(), net::To::Others, true);
  receive(game::local_actor(), message.data(), message.size());  // the host sees it too
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

bool resolve() { return true; }

void tick(void* ghost, int type) {
  if (!ghost || type != kHydra || !api::alive(ghost)) {
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
    host_double(ghost);
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

void ghost_threw(void* ghost, void* prop) {
  if (g.throwing || !prop || !ghost || ghost != g.ghost || g.walkers.empty()) return;
  g.ghostThrew.emplace_back(prop);
}

void reset() {
  for (Walker& w : g.walkers) drop_walker(w);
  drop_heads();
  apparition::destroy(g.second);
  g = State{};
}

}  // namespace pm::custom::hydra
