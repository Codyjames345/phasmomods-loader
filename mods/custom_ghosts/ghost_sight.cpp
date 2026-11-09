#include "ghost_sight.h"

#include <cstdio>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "Player.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityObject.h"
#include "game.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "players.h"
#include "runtime.h"
#include "sensor_copy.h"
#include "sight_rule.h"

namespace pm::custom::ghost_sight {
namespace {

using phasmo::Vector3;

// The ghost's vision outside hunts (GhostAI.vision) is a LOSSensor that tests its input sensor's signals
// for line of sight. No game code pulses it directly (the only Assembly-CSharp calls into LOSSensor are
// GetAllResults from GhostHearing and get_InputSensor from DayanPlayerChecks); it pulses by its
// own pulse routine, whose mode is in the ghost's prefab, and outside hunts it detects nobody (in-game log
// of 5a7bfc3). So a mod sees through its own copies of the two sensors (sensor_copy.h), set up
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

void drop_copies() {
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
  if (g_sight.ghost != ghost) drop_copies();
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
  else if (!sensor_copy::make(sensor_copy::Kind::Range, "Ghost sight input", g_sight.inputObject, g_sight.input,
                              made) ||
           !sensor_copy::make(sensor_copy::Kind::LineOfSight, "Ghost sight", g_sight.losObject, g_sight.los, made))
    why = "a copy of its sensors could not be made";
  else if (!sensor_copy::share_filters(g_sight.input, input))  // the input as the ghost's: detection mode, filters
    why = "its vision's input could not give the copy its detection mode";
  if (why) {
    PM_LOG("ghost sight: no sight outside hunts for this ghost: %s%s%s (vision: %s; input: %s)", why, made ? ", " : "",
           made ? made : "", losProcessors.empty() ? "none" : losProcessors.c_str(),
           inputProcessors.empty() ? "none" : inputProcessors.c_str());
    drop_copies();
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
  PM_LOG("ghost sight: sight outside hunts through copies of the ghost's vision sensors (window %d; vision processors: "
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

bool pulse_copies(void* ghost) {
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
    if (!sensor_copy::pulse(sensor_copy::Kind::LineOfSight, g_sight.los, "ghost sight: pulsing a sight copy threw"))
      return false;
  return true;
}

bool copies_see(void* player) {
  if (!game::living(player) || !g_sight.los) return false;
  const api::RefArray targets{phasmo::Player::losTargets.get(player)};
  std::vector<void*> parts;
  for (size_t i = 0; i < targets.size(); ++i) parts.push_back(targets[i]);
  return game::sees_a_part(parts.data(), parts.size(), [](void* part) {
    return api::alive(part) && game::sensor_detects(g_sight.los, game::game_object_of(part));
  });
}

}  // namespace

bool pulse(void* ghost) { return pulse_copies(ghost); }

bool sees(void* player) { return copies_see(player); }

void drop() { drop_copies(); }

}  // namespace pm::custom::ghost_sight