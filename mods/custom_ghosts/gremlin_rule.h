#pragma once

#include <cstring>

// The Gremlin's rules, kept free of game calls so they can be tested offline. It switches off
// electronic equipment that has been left running, never anything a player holds or stands beside.
namespace pg::custom {

constexpr float kGremlinGuard = 2.0f;    // metres: equipment this close to a living player is safe
constexpr float kGremlinChance = 0.5f;   // of its interactions with such equipment in its room, the share it sabotages
constexpr float kGremlinReach = 10.0f;   // metres: how far its player's "Short out" reaches

/**
 * Whether a class name is electronic equipment the Gremlin switches off: video cameras (CCTV), the
 * sensors, the EMF reader, parabolic, recorder, thermometer, spirit box (EVPRecorder), the D.O.T.S.
 * projector (IRLightSensor, per its prefab) and torches.
 */
inline bool gremlin_target_class(const char* name) {
  static constexpr const char* kElectronics[] = {"CCTV",          "MotionSensor", "SoundSensor", "EMFReader",
                                                 "ParabolicMicrophone", "SoundRecorder", "Thermometer", "EVPRecorder",
                                                 "IRLightSensor", "Torch"};
  if (!name) return false;
  for (const char* e : kElectronics)
    if (std::strcmp(name, e) == 0) return true;
  return false;
}

/** Whether a device that is `on`, `nearestPlayer` metres from the closest living player, may be switched off. */
inline bool gremlin_may_sabotage(bool on, float nearestPlayer) { return on && nearestPlayer > kGremlinGuard; }

}  // namespace pg::custom
