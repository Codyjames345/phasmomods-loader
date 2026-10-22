#pragma once

#include <windows.h>

#include "log.h"

namespace pg::startup {

/** Check cancellation and the shared deadline before a startup stage can change the game. */
inline bool permitted(void* stopEvent, unsigned long long deadline, const char* stage) {
  const DWORD result = WaitForSingleObject(static_cast<HANDLE>(stopEvent), 0);
  if (result != WAIT_TIMEOUT) {
    PG_LOG("startup: %s %s", stage,
           result == WAIT_OBJECT_0 ? "cancelled" : "event wait failed");
    return false;
  }
  if (GetTickCount64() >= deadline) {
    PG_LOG("startup: %s timeout", stage);
    return false;
  }
  return true;
}

/** Poll a startup boundary with interruptible waits that consume the same absolute budget. */
template <class Ready>
bool wait(void* stopEvent, unsigned long long deadline, const char* stage, Ready ready) {
  while (permitted(stopEvent, deadline, stage)) {
    if (ready()) return true;
    const auto current = GetTickCount64();
    if (current >= deadline) continue;
    const auto remaining = deadline - current;
    const DWORD delay = static_cast<DWORD>(remaining < 100 ? remaining : 100);
    if (WaitForSingleObject(static_cast<HANDLE>(stopEvent), delay) == WAIT_FAILED) {
      PG_LOG("startup: %s event wait failed", stage);
      return false;
    }
  }
  return false;
}

}  // namespace pg::startup
