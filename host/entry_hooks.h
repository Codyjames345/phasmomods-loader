#pragma once

#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "game_api.h"
#include "hooks.h"

struct PgGameEntry;

// Hooks by game map entry: a method's bodies are its canonical body and its twins, and each body
// has its own chain (hooks::create on that body).
namespace pg::entry_hooks {

// The method's distinct bodies: `canonical` first (index 0), then each twin that is not null and not
// folded onto an earlier body, in order.
std::vector<void*> bodies(void* canonical, std::span<void* const> twins);

// The host contract's hook_body: hooks body `index` of the method (its index in bodies()) with
// `detour` (chained on that body alone, not enabled). False, with nothing patched, when there is no
// such body (logged) or hooks::create refuses it.
bool hook_body(void* canonical, std::span<void* const> twins, uint32_t index, void* detour, void** original,
               const char* name);

// The host contract's hook_entry (version 2.0), on top of the per-body hooks: `detour` on body 0,
// or with `allTwins` on every body, each in its own chain. `original` receives body 0's, so a call
// that entered through a twin continues on the canonical body's chain (core/mod_api.h says what that
// means for other hooks). A twin folded onto another body, or listed twice, is hooked once. Nothing
// is hooked unless every body is (a failure is logged).
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
  void* (*entry)(uint32_t body);  // the detour for each body slot
  void** originals;               // each body slot's `original`
  int32_t (*depth)();             // the calling thread's depth
  bool installed;
};
// Adds a tracked method once per key.
void add(const Tracked& tracked);

template <class W>
struct Tracker;

// What runs for a call of one tracked method, typed from its wrapper, whichever body it entered
// through. The depth is per thread, and the RAII counter decrements it even when an exception (a
// managed one too) unwinds through the call.
template <const sdk::EntryDesc& D, class R, class... A>
struct Tracker<api::Method<D, R(A...)>> {
  static inline thread_local int32_t depth = 0;

  struct Count {
    Count() { ++depth; }
    ~Count() { --depth; }
    Count(const Count&) = delete;
    Count& operator=(const Count&) = delete;
  };

  static R run(api::OriginalOf<api::Method<D, R(A...)>> original, A... a) {
    Count count;
    return original(a...);
  }
  static int32_t current() { return depth; }
};

}  // namespace detail

// Tracks the game method M names (a Method wrapper). install hooks it.
template <auto& M>
void track() {
  using W = std::remove_cvref_t<decltype(M)>;
  using T = detail::Tracker<W>;
  using B = api::detail::Bodies<W, T>;
  detail::add({W::desc.key, &B::entry, reinterpret_cast<void**>(B::original), &T::current, false});
}

// Hooks every tracked method not hooked yet, on each of its bodies in that body's own chain, through
// `lookup` (gamemap::entry in the host). A method that did not resolve, or that has more bodies than
// api::kMaxBodies, is logged and left untracked with nothing patched; false when a hook failed.
// Runs before the mods start, as do the PG_TRACK registrations.
bool install(const PgGameEntry* (*lookup)(const char* key));

// How many calls of the tracked method `key` are running on the calling thread; 0 when the host does
// not track it.
int32_t depth(const char* key);

}  // namespace pg::context
