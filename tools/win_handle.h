#pragma once
#include <windows.h>

namespace pg::tools {
/** Own one local Windows handle without changing the referenced process lifetime. */
struct Handle {
  HANDLE value;
  /** Adopt a handle or a Windows failure sentinel. */
  explicit Handle(HANDLE handle) : value(handle) {}
  /** Release only valid local references on every return path. */
  ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
};
}
