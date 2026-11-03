#pragma once

// What every host detour of content does with its own work: runs it and contains what it throws, so
// the game's call goes on whatever happens there. Main thread, as the hooked call.

#include "log.h"

namespace pm::host {

/// Runs `body` and contains what it throws, logged once per call site (its own `logged` flag) with
/// `what`.
template <class F>
void guarded(bool& logged, const char* what, F body) {
  try {
    body();
  } catch (...) {
    if (!logged) PM_LOG("content: exception while %s; the game's call goes on", what);
    logged = true;
  }
}

}  // namespace pm::host
