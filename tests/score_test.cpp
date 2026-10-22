// Offline tests for the deception score (mods/playable_ghost/score.h). Run: build\score_test.exe
#include "score.h"

#include "check.h"

using namespace pg::score;

namespace {

void deaths_and_wrong_guesses_score() {
  std::map<int, Guess> g{{2, {31, false}}, {3, {9, true}}, {4, {5, false}}, {5, {kNoGuess, false}}};
  const Tally t = tally(g, 31);
  CHECK(t.investigators == 4 && t.killed == 1 && t.misled == 2 && t.points() == 3);  // 3 died; 4 wrong; 5 no guess
  CHECK(tally({}, 31).points() == 0);
  CHECK(tally({{2, {31, true}}}, 31).killed == 1);  // a right guess does not save the dead
}

void the_popup_reads_well() {
  const Tally t{1, 2, 4};
  CHECK(headline(t, true) == "Your deception score: 3 of 4");
  CHECK(headline(t, false) == "The ghost's deception score: 3 of 4");
  CHECK(details(t) == "1 investigator killed, 2 misled about the ghost");
  CHECK(details({0, 0, 1}) == "0 investigators killed, 0 misled about the ghost");
}

}  // namespace

int main() {
  deaths_and_wrong_guesses_score();
  the_popup_reads_well();
  return g_failures ? 1 : 0;
}
