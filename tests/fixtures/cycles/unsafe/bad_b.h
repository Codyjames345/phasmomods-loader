// Fixture for tests/cycles_test.ps1, not compiled: the partner of bad_a.h; it includes it back at
// the top of the file too, so neither edge crosses after the entries and cycles.py must flag the
// cycle.
#pragma once
#include "bad_a.h"
namespace pm::map {
inline constexpr pm::sdk::EntryDesc CyclesBadB{.key = "cycles.fixture.bad.b"};
}  // namespace pm::map
