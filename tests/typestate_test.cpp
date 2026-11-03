// Offline tests for the ghost's own state on its player's HUD (mods/playable_ghost/typestate.h). A
// registered ghost's line is its own definition's (tests/custom_ghosts_test.cpp custom_ghost_hud_lines).
// Run: build\typestate_test.exe
#include "typestate.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::typestate;

namespace {

void each_state_reads_well() {
  const uint8_t calm[2] = {0, 0}, one[2] = {1, 0}, two[2] = {2, 0}, odd[2] = {9, 0};
  CHECK(describe("vanilla.gallu", calm) == "Rage: calm");
  CHECK(describe("vanilla.gallu", two) == "Rage: exhausted");
  CHECK(describe("vanilla.gallu", odd) == "Rage: exhausted");  // out of range: clamped, never past the table
  CHECK(describe("vanilla.dayan", two) == "Strength: weakened (they stand still near you)");
  const uint8_t obambo[2] = {1, 75};
  CHECK(describe("vanilla.obambo", obambo) == "Mood: <color=#E04030>aggressive</color> for 1:15");
  const uint8_t thaye[2] = {4, 60};
  CHECK(describe("vanilla.thaye", thaye) == "Age: 4 of 10 (hunts at 60% sanity)");
  const uint8_t moved[2] = {5, 0};
  CHECK(describe("vanilla.deildegast", moved) == "Speed: 2.5 m/s (5 things moved)");
  CHECK(describe("vanilla.raiju", calm).empty());
  CHECK(describe("vanilla.raiju", one).find("faster") != std::string::npos);
  CHECK(describe("vanilla.poltergeist", one).empty());  // a type with nothing to show
  CHECK(describe("", one).empty());                      // no type (a custom ghost's behaviour type)
  CHECK(describe("some_mod.ghost", one).empty());        // a registered ghost's line is its definition's
}

void the_deildegast_slows_to_a_floor() {
  CHECK(deildegast_speed(0) == 3.0f);
  CHECK(deildegast_speed(26) == 0.4f);
  CHECK(deildegast_speed(200) == 0.4f);
}

}  // namespace

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  each_state_reads_well();
  the_deildegast_slows_to_a_floor();
  std::printf(g_failures ? "%d check(s) failed\n" : "all typestate tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
