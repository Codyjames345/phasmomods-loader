#pragma once

#include <atomic>
#include <string>

#include "il2cpp.h"
#include "log.h"

namespace pm::host {

// Logs a caught managed exception: its class, message and stack, led by `where`. The object came with the exception, and
// no member of the game is named.
inline void log_managed(const char* where, const void* exception) {
  il2cpp::log_exception(where, exception);  // pm:not-game-data: the runtime's exception object
}

// Runs `body` and contains what it throws: a managed exception is logged with its stack, anything
// else with `what`, each line led by `area`, and the caller gets `fallback`. Given `logged`, the call
// site's own flag, only the first exception there is logged.
template <class R, class F>
R contained(std::atomic<bool>* logged, const char* area, const char* what, R fallback, F body) {
  try {
    return body();
  } catch (const Il2CppExceptionWrapper& e) {
    if (!logged || !logged->exchange(true))
      log_managed((std::string(area) + ": managed exception while " + what).c_str(), e.ex);
  } catch (...) {
    if (!logged || !logged->exchange(true)) PM_LOG("%s: exception while %s", area, what);
  }
  return fallback;
}

template <class R, class F>
R contained(const char* area, const char* what, R fallback, F body) {
  return contained(static_cast<std::atomic<bool>*>(nullptr), area, what, fallback, body);
}

}  // namespace pm::host
