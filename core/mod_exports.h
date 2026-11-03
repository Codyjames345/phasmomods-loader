#pragma once

#include <string>
#include <string_view>

// The names of a mod DLL's exports: pmmod_info, pmmod_start, pmmod_tick, pmmod_stop and pmmod_requires (core/mod_api.h).
// Before stage 2b Task 14 they were named with the old prefix below. A mod built then still loads: its struct layouts and
// values never changed, only the names. The fallback is removed at the next major contract version. This is the one place
// the old prefix is written (tests/raw_scan_test.cpp, no_old_prefix_left).
namespace pm::modexports {

inline constexpr std::string_view kPrefix = "pmmod_";
inline constexpr std::string_view kOldPrefix = "pgmod_";

struct Found {
  void* address = nullptr;
  bool old = false;  // found only under the old prefix
};

/// The export `name` ("info", "start", ...) through `lookup` (a full export name to its address, or null): the new name
/// first, then the old one.
template <class Lookup>
Found find(Lookup&& lookup, std::string_view name) {
  std::string full = std::string(kPrefix) + std::string(name);
  if (void* address = lookup(full.c_str())) return {address, false};
  full = std::string(kOldPrefix) + std::string(name);
  if (void* address = lookup(full.c_str())) return {address, true};
  return {};
}

}  // namespace pm::modexports
