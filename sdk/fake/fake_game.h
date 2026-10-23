#pragma once

// The fake game backend: a host contract for testing a mod's game logic offline, without the game.
// A test says what each map entry the code under test uses resolves to (a C++ function standing in
// for a game method, an offset into a test buffer standing in for a field, an enum number, a class
// pointer), then binds the game API to fake::host() and calls the mod's code:
//
//   pg::fake::reset();
//   pg::fake::method(pg::map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
//   std::string missing;
//   pg::api::bind(pg::fake::host(), missing);
//   phasmo::GhostAI::UnAppear(ghost);  // runs fake_unappear(ghost, nullptr)
//
// An entry the test did not give stays unresolved, as in a game build that lacks it. A fake method is
// called with a null MethodInfo as its last argument. Type::is sees only the exact class: the fake
// models no inheritance. Single-threaded, like the game API.

#include <cstdint>
#include <string>
#include <vector>

#include "entry.h"
#include "mod_api.h"

namespace pg::fake {

/// Forgets every entry, hook and barrier write, so the next test starts from an empty game. Bind the
/// game API again after it (and after giving entries): a bound wrapper keeps the entry it was given.
void reset();

/// A complete host contract in which only the game functions are live: game_entry answers from the
/// entries given below, hook_entry records the hook (see hooked) and hands back the fake method as
/// `original`, set_reference stores the reference and counts the write (see barrier_writes), and
/// inside_entry is always 0. Every other function does nothing and answers false or null. The
/// pointer stays valid for the process lifetime.
const PgHostApi* host();

// Giving an entry again replaces it.

/// The method entry `d` resolves to `fn`, called as fn(args..., nullptr).
void method(const sdk::EntryDesc& d, void* fn);
/// The field entry `d` resolves to `offset` from the start of the object.
void field(const sdk::EntryDesc& d, int32_t offset);
/// The enum value entry `d` resolves to the number `v`.
void value(const sdk::EntryDesc& d, int64_t v);
/// The type entry `d` resolves to `klass`, which the fake compares with an object's first pointer.
void type(const sdk::EntryDesc& d, void* klass);

/// The keys hooked through hook_entry since reset, in order.
std::vector<std::string> hooked();
/// How many managed references were stored through set_reference (the GC write barrier) since reset.
int barrier_writes();

}  // namespace pg::fake
