#include "lithobolia.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "Photon.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "Props.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "lithobolia_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"
#include "runtime.h"
#include "sensor_copy.h"
#include "sight_rule.h"

namespace pm::custom::lithobolia {
namespace {

using phasmo::Vector3;

// Decompiles of GhostActivity.InteractWithAProp/GetPropToThrow, the prop throw and
// PlayerSanity.ChangeSanity (build 25811160, 2026-10-20); the ownership calls as kikimora.cpp.
constexpr ULONGLONG kOwnershipWaitMs = 1500;
constexpr ULONGLONG kNoteMs = 5000;  // the throw diagnostics: one line at most this often
// The launch is readied as the game's own throw readies the object, in its order: the Rigidbody freed
// with PhotonObjectInteract.FreezeRigidbody(false, true) (the RPC that does the same on every client goes
// out only while the view's IsMine is set), then its PhotonView synchronised (UnreliableOnChange), then the
// force.
//
// The ghost's vision outside hunts (GhostAI.vision) is a LOSSensor that tests its input sensor's signals
// for line of sight. No game code pulses it directly (the only Assembly-CSharp calls into LOSSensor are
// GetAllResults from GhostHearing and get_InputSensor from DayanPlayerChecks); it pulses by its
// own pulse routine, whose mode is in the ghost's prefab, and outside hunts it detects nobody (in-game log
// of 5a7bfc3). So the Lithobolia sees through its own copies of the two sensors (sensor_copy.h), set up
// from the ghost's and pulsed only when a throw is decided: the ghost's own are never pulsed or changed.
//
// The LOSSensor's moving-average visibility: with averaging on, the averager returns the sum
// over max(window, samples), and each target's history carries over between pulses, so a copy
// pulsed once would score a visible player 1/window. A decision pulses the copy `window` times
// (LOSSensor.window, read from the ghost's sensor), so the average is full. The averaging flag itself
// cannot be read (no code fact tells it from the sensor's other bools); with averaging off every pulse
// tests afresh, so the last of the pulses gives what one would.

// The host's copies of the ghost's vision sensors.
struct Sight {
  api::Held ghost;
  api::Held losObject, los, inputObject, input;
  bool failed = false;  // could not be made for this ghost: logged once, not tried again
} g_sight;

struct Throw {
  api::Held item;          // PhotonObjectInteract
  api::Held target;        // Player
  int actor = 0;           // the target's Photon actor
  ULONGLONG deadline = 0;  // waiting for ownership until then
  ULONGLONG landed = 0;    // launched: watched for a hit until then
} g;

ULONGLONG g_nextNote = 0;
int g_unnoted = 0;  // throws decided since the last diagnostic line

/** Whether a throw's diagnostic line may be written now (one per kNoteMs, never per frame). */
bool note_now() {
  ++g_unnoted;
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextNote) return false;
  g_nextNote = now + kNoteMs;
  return true;
}

Vec vec(Vector3 v) { return {v.x, v.y, v.z}; }

Vec chest_of(void* player) {
  const Vector3 p = game::position(game::transform_of(player));
  return {p.x, p.y + kLithoboliaChest, p.z};
}

/** The Photon actor of a networked Player, or 0. */
int actor_of(void* player) {
  std::vector<int> actors;
  game::room_actors(actors);
  for (int a : actors)
    if (game::player_of_actor(a) == player) return a;
  return 0;
}

/** Metres from the ghost's feet to a player's chest. */
float chest_distance(void* ghost, void* player) {
  const Vector3 at = game::position(game::transform_of(ghost));
  const Vec chest = chest_of(player);
  return length({chest.x - at.x, chest.y - at.y, chest.z - at.z});
}

void drop_sight() {
  sensor_copy::drop(g_sight.losObject, g_sight.los);
  sensor_copy::drop(g_sight.inputObject, g_sight.input);
  g_sight = Sight{};
}

/**
 * Whether every signal processor on `sensor` (Sensor.signalProcessors) is one the copy can share,
 * appending what they are to `names`; false at the first that is not (named in `names` by its
 * GameObject). The two the copy shares declare only settings and read the game's state and the sensor they
 * are given (sdk/map/Sensor.h):
 *  - GhostHuntingProcessor passes a signal while GhostAI.isHunting equals its requirement, or always for
 *    a type 18 ghost when allowRaijuSkip is set;
 *  - PlayerProcessor passes a signal from a sight part whose Player is not marked dead and whose room is
 *    not outside the map, and for a type 28 ghost only near the sensor it is given (the copy, posed as
 *    the ghost's).
 * A shared processor so judges the copy's signals exactly as the ghost's.
 */
bool shareable_processors(void* sensor, std::string& names) {
  const api::List list{phasmo::Sensor::signalProcessors.get(sensor)};
  for (size_t i = 0; i < list.size(); ++i) {
    void* p = list[i];
    if (!p) continue;
    char part[96];
    if (phasmo::GhostHuntingProcessor::type.is(p)) {
      std::snprintf(part, sizeof part, "GhostHuntingProcessor(requires hunting %d, type 18 passes %d) ",
                    phasmo::GhostHuntingProcessor::requirement.get(p) ? 1 : 0,
                    phasmo::GhostHuntingProcessor::allowRaijuSkip.get(p) ? 1 : 0);
    } else if (phasmo::PlayerProcessor::type.is(p)) {
      std::snprintf(part, sizeof part, "PlayerProcessor ");
    } else {
      names += "an unknown processor on ";
      names += api::to_utf8(phasmo::Object::get_name(p));
      return false;
    }
    names += part;
  }
  return true;
}

/** Make the copies of the ghost's vision, once per ghost; false (logged once) when they cannot be. */
bool make_sight(void* ghost) {
  if (g_sight.ghost != ghost) drop_sight();
  if (g_sight.los) return true;
  if (g_sight.failed) return false;
  g_sight.ghost = ghost;
  const char* why = nullptr;
  const char* made = nullptr;
  std::string losProcessors, inputProcessors;
  void* los = phasmo::GhostAI::vision.get(ghost);
  void* input = api::alive(los) ? phasmo::LOSSensor::get_InputSensor(los) : nullptr;
  if (!api::alive(los) || !phasmo::LOSSensor::type.is(los)) why = "its vision is not a LOSSensor";
  else if (!api::alive(input) || !phasmo::RangeSensor::type.is(input)) why = "its vision's input is not a RangeSensor";
  else if (!shareable_processors(los, losProcessors) || !shareable_processors(input, inputProcessors))
    why = "its vision has a signal processor the copy cannot share";
  else if (!sensor_copy::make(sensor_copy::Kind::Range, "Lithobolia sight input", g_sight.inputObject, g_sight.input,
                              made) ||
           !sensor_copy::make(sensor_copy::Kind::LineOfSight, "Lithobolia sight", g_sight.losObject, g_sight.los, made))
    why = "a copy of its sensors could not be made";
  else if (!sensor_copy::share_filters(g_sight.input, input))  // the input as the ghost's: detection mode, filters
    why = "its vision's input could not give the copy its detection mode";
  if (why) {
    PM_LOG("lithobolia: no sight outside hunts for this ghost: %s%s%s (vision: %s; input: %s)", why, made ? ", " : "",
           made ? made : "", losProcessors.empty() ? "none" : losProcessors.c_str(),
           inputProcessors.empty() ? "none" : inputProcessors.c_str());
    drop_sight();
    g_sight.ghost = ghost;
    g_sight.failed = true;
    return false;
  }
  // The line-of-sight sensor as the ghost's: its filter. Both copies' settings, the curves among them,
  // are copied at each pulse.
  phasmo::Sensor::filter.set(g_sight.los, phasmo::Sensor::filter.get(los));
  // The ghost's own signal processors, shared (shareable_processors), set before either copy wakes.
  phasmo::Sensor::signalProcessors.set(g_sight.input, phasmo::Sensor::signalProcessors.get(input));
  phasmo::Sensor::signalProcessors.set(g_sight.los, phasmo::Sensor::signalProcessors.get(los));
  phasmo::GameObject::SetActive(g_sight.inputObject, true);
  phasmo::GameObject::SetActive(g_sight.losObject, true);
  phasmo::LOSSensor::set_InputSensor(g_sight.los, g_sight.input);  // as the setter wires it: after OnEnable subscribed
  PM_LOG("lithobolia: sight outside hunts through copies of the ghost's vision sensors (window %d; vision processors: "
         "%s; input processors: %s)",
         phasmo::LOSSensor::window.get(los), losProcessors.empty() ? "none" : losProcessors.c_str(),
         inputProcessors.empty() ? "none" : inputProcessors.c_str());
  return true;
}

void copy_pose(void* from, void* toObject) {
  void* f = game::transform_of(from);
  void* t = phasmo::GameObject::get_transform(toObject);
  const Vector3 p = game::position(f);
  const phasmo::Quaternion r = game::rotation(f);
  const phasmo::Vector3 at{p.x, p.y, p.z};
  const phasmo::Quaternion rotation{r.x, r.y, r.z, r.w};
  phasmo::Transform::set_position(t, &at);
  phasmo::Transform::set_rotation(t, &rotation);
}

/** Pulse the copies where the ghost's sensors are, with the ghost's settings now; false when there is no sight. */
bool pulse_sight(void* ghost) {
  if (!make_sight(ghost)) return false;
  void* los = phasmo::GhostAI::vision.get(ghost);
  void* input = api::alive(los) ? phasmo::LOSSensor::get_InputSensor(los) : nullptr;
  if (!api::alive(input) || !api::alive(g_sight.los) || !api::alive(g_sight.input)) return false;
  if (!sensor_copy::copy_settings(sensor_copy::Kind::Range, g_sight.input, input) ||
      !sensor_copy::copy_settings(sensor_copy::Kind::LineOfSight, g_sight.los, los))
    return false;
  copy_pose(input, g_sight.inputObject);
  copy_pose(los, g_sight.losObject);
  // Only the LOSSensor: its PulseAll pulses its input through the input's own PulseAll first.
  const int window = phasmo::LOSSensor::window.get(los);
  const int pulses = window > 1 ? window : 1;
  for (int i = 0; i < pulses; ++i)
    if (!sensor_copy::pulse(sensor_copy::Kind::LineOfSight, g_sight.los, "lithobolia: pulsing a sight copy threw"))
      return false;
  return true;
}

/** Whether the copy of the ghost's vision, just pulsed, sees `player` (any of their sight parts). */
bool sight_sees(void* player) {
  if (!game::living(player) || !g_sight.los) return false;
  const api::RefArray targets{phasmo::Player::losTargets.get(player)};
  std::vector<void*> parts;
  for (size_t i = 0; i < targets.size(); ++i) parts.push_back(targets[i]);
  return game::sees_a_part(parts.data(), parts.size(), [](void* part) {
    return api::alive(part) && game::sensor_detects(g_sight.los, game::game_object_of(part));
  });
}

/** The nearest living player the ghost's own vision sensor detects within range, and their actor; null for none. */
void* visible_target(void* ghost, int& actor, const char* why) {
  std::vector<void*> seen;
  std::vector<float> distances;
  int living = 0;
  // In a hunt the ghost's own sensor pulses (its sight test reads it); outside, the copies are pulsed now.
  const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
  const bool copied = !hunting && pulse_sight(ghost);
  game::each_player([&](void* player) {
    if (game::living(player)) ++living;
    if (!(hunting ? game::ghost_sees(ghost, player) : copied && sight_sees(player))) return;
    seen.push_back(player);
    distances.push_back(chest_distance(ghost, player));
  });
  const int best = nearest_in_range(distances, kLithoboliaRange);
  void* player = best >= 0 ? seen[best] : nullptr;
  actor = player ? actor_of(player) : 0;
  if (note_now()) {
    float nearest = -1;
    for (float d : distances) nearest = nearest < 0 ? d : std::min(nearest, d);
    PM_LOG("lithobolia: %s (%d since the last line, sight %s): %d living, %zu seen, nearest seen %.1f m -> %s", why,
           g_unnoted, hunting ? "the ghost's own" : copied ? "the copies" : "none", living, seen.size(), nearest,
           player ? "throws at them" : "nothing (nobody seen within 8 m)");
    g_unnoted = 0;
  }
  return player;
}

bool throwable(void* item) {
  if (!api::alive(item) || phasmo::PhotonObjectInteract::door.get(item)) return false;
  return api::alive(phasmo::PhotonObjectInteract::body.get(item)) && phasmo::PhotonObjectInteract::view.get(item);
}

void clear() { g = Throw{}; }

/** Aim and launch the taken item, with the EMF of a ghost's throw where it left. */
void launch(void* ghost) {
  void* rb = phasmo::PhotonObjectInteract::body.get(g.item);
  const bool wasKinematic = phasmo::Rigidbody::get_isKinematic(rb);
  // Readied as the game's throw readies it: freed (for every client while it is ours), then
  // synchronised. A velocity set on a kinematic Rigidbody does nothing, which is what 3f59df4..5a7bfc3 did.
  phasmo::PhotonObjectInteract::FreezeRigidbody(g.item, false, true);
  phasmo::PhotonView::Synchronization.set(phasmo::PhotonObjectInteract::view.get(g.item),
                                          phasmo::ViewSynchronization::UnreliableOnChange());
  const Vec from = vec(game::position(game::transform_of(g.item)));
  const Vec aim = aim_velocity(from, chest_of(g.target));
  const phasmo::Vector3 velocity{aim.x, aim.y, aim.z};
  phasmo::Rigidbody::set_velocity(rb, &velocity);
  if (void* interaction = phasmo::GhostAI::interaction.get(ghost))
    phasmo::GhostInteraction::CreateThrowingEMF(interaction, g.item, phasmo::EMFType::objectMoved(), false, 20.0f);
  g.landed = GetTickCount64() + static_cast<ULONGLONG>(kLithoboliaFlight * 1000);
  PM_LOG("lithobolia: launched at a player (kinematic before %d, after %d; aimed %.1f m/s, has %.1f m/s)",
         wasKinematic ? 1 : 0, phasmo::Rigidbody::get_isKinematic(rb) ? 1 : 0, length(aim),
         length(vec(phasmo::Rigidbody::get_velocity(rb))));
}

/** Apply a hit to this client's own player. */
void hit_me(float sanity) {
  void* me = game::local_player();
  void* mine = api::alive(me) ? phasmo::Player::sanity.get(me) : nullptr;
  if (api::alive(mine)) phasmo::PlayerSanity::ChangeSanity(mine, sanity);
}

}  // namespace

bool resolve() { return true; }

/** Take `prop` (a PhotonObjectInteract) over to throw it at `target` once the host owns it. */
void begin(void* prop, void* target, int actor) {
  g.item = prop;
  g.target = target;
  g.actor = actor;
  void* view = phasmo::PhotonObjectInteract::view.get(prop);
  if (!phasmo::PhotonView::get_IsMine(view)) phasmo::PhotonView::RequestOwnership(view);  // moved once it is the host's
  g.deadline = GetTickCount64() + kOwnershipWaitMs;
}

/** Throw `prop` at the nearest player in sight; false (nothing begun) when nobody is. */
bool throw_at_visible(void* ghost, void* prop, const char* why) {
  int actor = 0;
  void* target = visible_target(ghost, actor, why);
  if (target) begin(prop, target, actor);
  return target != nullptr;
}

bool intercept(void* ghost, void* prop) {
  if (!ghost || !throwable(prop)) return false;
  if (g.item) return true;  // one throw at a time: the game's own, possibly at nobody, is swallowed too
  throw_at_visible(ghost, prop, "a throw");
  return true;  // nobody in sight: the throw is swallowed
}

bool intercept_hunt_throw(void* ghost, void* prop) {
  void* interact = api::alive(prop) ? phasmo::Prop::interact.get(prop) : nullptr;
  if (!ghost || !throwable(interact)) return false;
  void* state = game::ghost_state(ghost) == phasmo::GhostState::hunting() ? game::ghost_state_object(ghost) : nullptr;
  void* target = state ? phasmo::GhostHuntState::target.get(state) : nullptr;
  if (g.item) return true;  // one throw at a time
  // None at a target the ghost does not see within range.
  const bool sees = game::ghost_sees(ghost, target);
  const float distance = api::alive(target) ? chest_distance(ghost, target) : -1;
  const bool throws = sees && distance <= kLithoboliaRange;
  if (note_now()) {
    PM_LOG("lithobolia: a hunt throw (%d since the last line): target %s, seen %d, %.1f m -> %s", g_unnoted,
           target ? "set" : "none", sees ? 1 : 0, distance, throws ? "throws at them" : "nothing");
    g_unnoted = 0;
  }
  if (throws) begin(interact, target, actor_of(target));
  return true;
}

bool throw_now(void* ghost) {
  void* activity = ghost ? game::ghost_activity(ghost) : nullptr;
  void* prop = activity ? phasmo::GhostActivity::GetPropToThrow(activity) : nullptr;
  void* interact = api::alive(prop) ? phasmo::Prop::interact.get(prop) : nullptr;
  return !g.item && throwable(interact) && throw_at_visible(ghost, interact, "the ability");
}

void tick(void* ghost, int type) {
  if ((g_sight.ghost || g_sight.losObject) && (!ghost || type != kLithobolia || ghost != g_sight.ghost)) drop_sight();
  if (!g.item) return;
  if (!ghost || type != kLithobolia || !api::alive(g.item) || !api::alive(g.target)) {
    clear();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (!g.landed) {
    if (phasmo::PhotonView::get_IsMine(phasmo::PhotonObjectInteract::view.get(g.item))) launch(ghost);
    else if (now >= g.deadline) {
      PM_LOG("lithobolia: the object never became the host's; no throw");
      clear();
    }
    return;
  }
  void* rb = phasmo::PhotonObjectInteract::body.get(g.item);
  const Vec at = vec(game::position(game::transform_of(g.item)));
  if (lithobolia_hit(at, length(vec(phasmo::Rigidbody::get_velocity(rb))), chest_of(g.target))) {
    PM_LOG("lithobolia: the throw hits");
    if (g.actor == game::local_actor()) {
      hit_me(kLithoboliaHit);
    } else if (g.actor) {
      const std::vector<uint8_t> message = net_msg::hit(kLithoboliaHit);
      net::send(message.data(), message.size(), net::To::Actor, true, g.actor);
    }
    clear();
  } else if (now >= g.landed) {
    clear();
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  float sanity = 0;
  if (sender != game::master_actor() || !net_msg::read_hit(data, size, sanity)) return;
  hit_me(std::clamp(sanity, 0.0f, kLithoboliaHit));
}

void reset() {
  clear();
  drop_sight();
}

}  // namespace pm::custom::lithobolia
