#include "frame_time.h"

#include "UnityAudio.h"

namespace pm::api {

FrameTime frame_time() {
  using phasmo::Time;
  return {Time::get_deltaTime(), Time::get_unscaledDeltaTime(), Time::get_time(), Time::get_unscaledTime()};
}

}  // namespace pm::api
