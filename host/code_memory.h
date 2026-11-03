#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace pm::host {

// The protections of code this process may read: executable and readable. An execute-only page
// (PAGE_EXECUTE), a NOACCESS page and a guard page are not.
inline constexpr DWORD kReadableCode = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

// Whether `size` bytes at `at` are committed code this process may read, every page of them. Used
// before the host reads game code it did not write: a site it patches (host/hooks.cpp) and
// UnityPlayer's .text, which the native sites' search reads in full (host/game_map.cpp).
inline bool readable_code(const void* at, size_t size) {
  for (uintptr_t p = reinterpret_cast<uintptr_t>(at), end = p + size; p < end;) {
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<const void*>(p), &region, sizeof region) || region.State != MEM_COMMIT ||
        (region.Protect & PAGE_GUARD) || !(region.Protect & kReadableCode))
      return false;
    p = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
  }
  return true;
}

}  // namespace pm::host
