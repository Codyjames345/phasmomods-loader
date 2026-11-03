// Offline tests for the deception score (mods/playable_ghost/score.h). Run: build\score_test.exe
#include "score.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::score;

namespace {

void deaths_and_wrong_guesses_score() {
  std::map<int, Guess> g{{2, {31, false}}, {3, {9, true}}, {4, {5, false}}, {5, {kNoGuess, false}}};
  const Tally t = tally({2, 3, 4, 5}, {1, 2, 3, 4, 5}, g, 31);
  CHECK((t == Tally{1, 2, 0, 4}));  // 3 died; 4 wrong; 5 no guess; 2 named the ghost
  CHECK(t.points() == 3);
  CHECK(tally({}, {1}, {}, 31).points() == 0);
  CHECK(tally({2}, {2}, {{2, {31, true}}}, 31).killed == 1);  // a right guess does not save the dead
}

// Present and silent is no guess: misled, not a quit.
void a_silent_investigator_who_stayed_is_misled() {
  CHECK((tally({2}, {1, 2}, {}, 31) == Tally{0, 1, 0, 1}));
}

// A leaver's last message said alive, and even named the ghost: they quit.
void one_who_left_quit_whatever_they_last_said() {
  CHECK((tally({2}, {1}, {{2, {31, false}}}, 31) == Tally{0, 0, 1, 1}));
  CHECK((tally({2}, {1}, {{2, {4, false}}}, 31) == Tally{0, 0, 1, 1}));
}

// Counted from the contract's start, so a first guess is not what makes an investigator.
void a_quitter_who_never_sent_a_guess_counts() {
  CHECK((tally({2, 3}, {1, 3}, {{3, {31, false}}}, 31) == Tally{0, 0, 1, 2}));
}

// A guess from someone who was not an investigator gives no point and no place in the count.
void only_the_contracts_investigators_count() {
  CHECK((tally({2}, {1, 2, 9}, {{2, {31, false}}, {9, {4, true}}}, 31) == Tally{0, 0, 0, 1}));
}

// One point each at most, and the order changes the line, never the points: died then quit reads as the death.
void one_point_per_investigator_whatever_the_order() {
  const Tally t = tally({2}, {1}, {{2, {4, true}}}, 31);
  CHECK((t == Tally{1, 0, 0, 1}));
  CHECK(t.points() == tally({2}, {1}, {}, 31).points());
}

// Died and quit mixed: nobody is left, so every point is earned.
void any_mix_of_deaths_and_quits_is_every_point() {
  const Tally t = tally({2, 3, 4}, {1}, {{2, {4, true}}, {3, {31, false}}}, 31);
  CHECK((t == Tally{1, 0, 2, 3}));
  CHECK(t.points() == t.investigators);
}

void the_popup_reads_well() {
  const Tally t{1, 2, 0, 4};
  CHECK(headline(t, true) == "Your deception score: 3 of 4");
  CHECK(headline(t, false) == "The ghost's deception score: 3 of 4");
  CHECK(details(t) == "1 killed, 2 misled");
  CHECK(details({0, 0, 0, 1}) == "0 killed, 0 misled");
  CHECK(details({1, 0, 2, 3}) == "1 killed, 0 misled, 2 quit");
}

}  // namespace

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  deaths_and_wrong_guesses_score();
  a_silent_investigator_who_stayed_is_misled();
  one_who_left_quit_whatever_they_last_said();
  a_quitter_who_never_sent_a_guess_counts();
  only_the_contracts_investigators_count();
  one_point_per_investigator_whatever_the_order();
  any_mix_of_deaths_and_quits_is_every_point();
  the_popup_reads_well();
  return g_failures ? 1 : 0;
}
