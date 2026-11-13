#pragma once

/// @file
/// @brief The frame's time, as Unity gives it to the game's own scripts: how long the last frame took and how long the
/// game has run, each in game time and in real time.
///
/// `pmmod_tick` takes no time of its own. Read this in it, rather than measuring the time with a clock of the mod's own:
/// the game's scripts read the same four values in the same frame (Unity's Time.deltaTime, Time.unscaledDeltaTime,
/// Time.time and Time.unscaledTime), so a timer a mod keeps runs at the game's pace. Game time is scaled by the time
/// scale (Time.get_timeScale; 1 in real time) and stands still while it is 0; real time is not scaled.
///
/// @code
/// void tick() {  // pmmod_tick
///   g_cooldown -= pm::api::frame_time().deltaTime;  // counts down with the game, and waits while it is frozen
/// }
/// @endcode
///
/// Main thread, while the game plays (pmmod_tick, a hook, a rule's tick). Local only: nothing is sent, and each client
/// reads its own frame. Calling it makes the four Time entries required by the mod (pmmod_requires).
/// @ingroup runtime
/// @since Contract 2.7

namespace pm::api {

/// The frame's time. Every value is in seconds and 0 when its entry did not resolve.
struct FrameTime {
  float deltaTime = 0;          ///< from the last frame to this one, in game time (Time.get_deltaTime): 0 while the time scale is 0
  float unscaledDeltaTime = 0;  ///< from the last frame to this one, in real time (Time.get_unscaledDeltaTime)
  float time = 0;               ///< game time at the start of this frame, since the game started (Time.get_time)
  float unscaledTime = 0;       ///< real time at the start of this frame, since the game started (Time.get_unscaledTime)
};

/// The frame's time, read from Unity's Time through the map. The values stay the same for the whole frame.
/// @return The four values; each is 0 when its entry did not resolve (logged once), or before the mod is bound.
/// @pre Main thread.
/// @since Contract 2.7
FrameTime frame_time();

}  // namespace pm::api
