// Offline tests for the ghost's own state on its player's HUD (mods/playable_ghost/typestate.h).
// Run: build\typestate_test.exe
#include "typestate.h"

#include "check.h"

using namespace pg::typestate;

namespace {

void a_custom_ghost_shows_its_own_state_others_their_rules() {
  CHECK(subject(24, kEcho) == kEcho);
  CHECK(subject(24, kDraugr) == kDraugr);
  CHECK(subject(kGallu, 20) == kGallu);  // a Mimic copying a Gallu shows the Gallu's rage
  CHECK(subject(kDayan, kDayan) == kDayan);
  CHECK(subject(3, -1) == 3);
}

void each_state_reads_well() {
  const uint8_t calm[2] = {0, 0}, one[2] = {1, 0}, two[2] = {2, 0}, odd[2] = {9, 0};
  CHECK(describe(kGallu, calm) == "Rage: calm");
  CHECK(describe(kGallu, two) == "Rage: exhausted");
  CHECK(describe(kGallu, odd) == "Rage: exhausted");  // out of range: clamped, never past the table
  CHECK(describe(kDayan, two) == "Strength: weakened (they stand still near you)");
  const uint8_t obambo[2] = {1, 75};
  CHECK(describe(kObambo, obambo) == "Mood: <color=#E04030>aggressive</color> for 1:15");
  const uint8_t thaye[2] = {4, 60};
  CHECK(describe(kThaye, thaye) == "Age: 4 of 10 (hunts at 60% sanity)");
  const uint8_t moved[2] = {5, 0};
  CHECK(describe(kDeildegast, moved) == "Speed: 2.5 m/s (5 things moved)");
  CHECK(describe(kEcho, calm) == "Voices: you hear the living near you");
  CHECK(describe(kEcho, one).find("dormant") != std::string::npos);
  const uint8_t charge[2] = {140, 0};
  CHECK(describe(kDullahan, charge) == "Charge: 140% speed");
  CHECK(describe(kDullahan, calm).empty());  // not hunting
  CHECK(describe(kRaiju, calm).empty());
  CHECK(describe(kRaiju, one).find("faster") != std::string::npos);
  CHECK(describe(kKikimora, calm).empty());
  CHECK(describe(kKikimora, one).find("lights are on") != std::string::npos);
  CHECK(describe(kDraugr, calm) == "Your room is whole");
  CHECK(describe(kDraugr, one).find("65% sanity") != std::string::npos);
  CHECK(describe(3, one).empty());  // a type with nothing to show
}

void the_deildegast_slows_to_a_floor() {
  CHECK(deildegast_speed(0) == 3.0f);
  CHECK(deildegast_speed(26) == 0.4f);
  CHECK(deildegast_speed(200) == 0.4f);
}

}  // namespace

int main() {
  a_custom_ghost_shows_its_own_state_others_their_rules();
  each_state_reads_well();
  the_deildegast_slows_to_a_floor();
  std::printf(g_failures ? "%d check(s) failed\n" : "all typestate tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
