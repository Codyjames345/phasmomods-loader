// Fixture for tests/cycles_test.ps1, not compiled: a map-shaped header that includes its partner
// only after its pm::map entries, the layout the rule allows. cycles.py must print the cycle
// through good_b.h as allowed and exit 0.
#pragma once
namespace pm::map {
inline constexpr pm::sdk::EntryDesc CyclesGoodA{.key = "cycles.fixture.allowed.a"};
}  // namespace pm::map
#include "good_b.h"
