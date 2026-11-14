// Fixture for tests/cycles_test.ps1, not compiled: a map-shaped header whose partner include sits
// at the top of the file. tools/cycles.py must report the cycle through bad_b.h and exit 1, because
// the layout rule (sdk/internals/mapping.md) lets headers cycle only through after-the-entries
// includes, like the pair in the allowed directory next door.
#pragma once
#include "bad_b.h"
namespace pm::map {
inline constexpr pm::sdk::EntryDesc CyclesBadA{.key = "cycles.fixture.bad.a"};
}  // namespace pm::map
