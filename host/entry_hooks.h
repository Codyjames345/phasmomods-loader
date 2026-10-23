#pragma once

#include <cstdint>
#include <span>
#include <type_traits>

#include "game_api.h"
#include "hooks.h"

struct PgGameEntry;

// Hooks by game map entry (the host contract's hook_entry): a method's canonical body and its twins.
namespace pg::entry_hooks {

// Hooks the method body `canonical` with `detour` (chained, not enabled): through hooks::create, or
// with `allTwins` through hooks::create_shared on the canonical body and every twin. Every body runs
// the same detour, and `original` reaches the canonical chain: twins are the same behaviour, so a call
// that entered through a twin may continue there. A twin folded onto another body, or listed twice,
// is hooked once. A hook that would mix the two policies on one method is refused (logged), as is
// any failure, with nothing patched.
bool hook(void* canonical, std::span<void* const> twins, void* detour, void** original, const char* name,
          bool allTwins);

}  // namespace pg::entry_hooks

// Caller context (the host contract's inside_entry): for each game method the host tracks, how many
// of its calls are running on the calling thread. host/context.cpp lists the tracked methods as
// PG_TRACK(phasmo::Type::Method).
namespace pg::context {

namespace detail {

struct Tracked {
  const char* key;
  void* detour;
  void** original;
  int32_t (*depth)();  // the calling thread's depth
  bool installed;
};
// Adds a tracked method once per key.
void add(const Tracked& tracked);

template <class W>
struct Tracker;

// The detour for one tracked method, typed from its wrapper. The depth is per thread, and the RAII
// counter decrements it even when an exception (a managed one too) unwinds through the call.
template <const sdk::EntryDesc& D, class R, class... A>
struct Tracker<api::Method<D, R(A...)>> {
  static inline thread_local int32_t depth = 0;
  static inline R (*original)(A..., const void*) = nullptr;

  struct Count {
    Count() { ++depth; }
    ~Count() { --depth; }
    Count(const Count&) = delete;
    Count& operator=(const Count&) = delete;
  };

  static R detour(A... a, const void* method) {
    hooks::Guard guard;
    Count count;
    return original(a..., method);
  }
  static int32_t current() { return depth; }
};

}  // namespace detail

// Tracks the game method M names (a Method wrapper). install hooks it.
template <auto& M>
void track() {
  using W = std::remove_cvref_t<decltype(M)>;
  using T = detail::Tracker<W>;
  detail::add({W::desc.key, reinterpret_cast<void*>(&T::detour), reinterpret_cast<void**>(&T::original),
               &T::current, false});
}

// Hooks every tracked method not hooked yet, on all its twins, through `lookup` (gamemap::entry in
// the host). A method that did not resolve is logged and left untracked; false when a hook failed.
// Runs before the mods start, as do the PG_TRACK registrations.
bool install(const PgGameEntry* (*lookup)(const char* key));

// How many calls of the tracked method `key` are running on the calling thread; 0 when the host does
// not track it.
int32_t depth(const char* key);

}  // namespace pg::context
