// Fixture for tests/cycles_test.ps1, not compiled: the partner of good_a.h, with the same
// after-the-entries include back, so the cycle is the layout and cycles.py must allow it.
#pragma once
namespace pm::map {
inline constexpr pm::sdk::EntryDesc CyclesGoodB{.key = "cycles.fixture.allowed.b"};
}  // namespace pm::map
#include "good_a.h"
