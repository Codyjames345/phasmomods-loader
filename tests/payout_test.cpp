// Offline tests for pm::payout (no game). Run: build\payout_test.exe
#include "payout.h"

#include <cstdio>

using pm::payout::ghost;

namespace {

int g_failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                               \
    }                                                             \
  } while (0)

void no_points_pay_nothing() {
  CHECK(ghost(0, 3, 200, 1.0f).money == 0);
  CHECK(ghost(0, 3, 200, 1.0f).xp == 0);
  CHECK(ghost(1, 0, 200, 1.0f).money == 0);  // no investigators: no division, no pay
  CHECK(ghost(-1, 3, 200, 1.0f).money == 0);
}

void every_point_is_full_pay() {
  CHECK(ghost(3, 3, 2000, 1.0f).money == 2000);
  CHECK(ghost(1, 1, 2000, 1.0f).money == 2000);  // one investigator
  CHECK(ghost(9, 3, 2000, 1.0f).money == 2000);  // one point per investigator at most
}

void equal_shares_of_full_pay() {
  CHECK(ghost(2, 3, 2000, 1.0f).money == 1333);  // rounded down
  CHECK(ghost(2, 3, 2000, 1.0f).xp == 1333);
  CHECK(ghost(1, 3, 2000, 1.0f).money == 667);   // rounded up
  CHECK(ghost(1, 2, 2000, 1.0f).money == 1000);
  CHECK(ghost(1, 4, 2000, 2.0f).money == 250);   // a share of the scaled pay
  CHECK(ghost(1, 2, 200, 3.0f).money == 34);     // full pay rounds to 67 first: half is 33.5, not 33.33
}

void scaled_inversely_to_the_difficulty() {
  CHECK(ghost(1, 1, 200, 1.0f).money == 200);
  CHECK(ghost(1, 1, 200, 2.0f).money == 100);
  CHECK(ghost(1, 1, 200, 4.0f).money == 50);
  CHECK(ghost(1, 1, 200, 3.0f).money == 67);  // rounded
  CHECK(ghost(1, 1, 200, 4.0f).xp == ghost(1, 1, 200, 4.0f).money);
  CHECK(ghost(1, 1, 200, 6.0f).money < ghost(1, 1, 200, 2.0f).money);  // harder pays the ghost less
}

void no_pay_difficulty_pays_nothing_and_easy_ones_are_capped() {
  CHECK(ghost(1, 1, 200, 0.0f).money == 0);
  CHECK(ghost(1, 1, 200, -1.0f).money == 0);
  CHECK(ghost(1, 1, 200, 0.1f).money == 600);  // capped at 3x
  CHECK(ghost(1, 1, 0, 1.0f).money == 0);
}

}  // namespace

int main() {
  no_points_pay_nothing();
  every_point_is_full_pay();
  equal_shares_of_full_pay();
  scaled_inversely_to_the_difficulty();
  no_pay_difficulty_pays_nothing_and_easy_ones_are_capped();
  std::printf(g_failures ? "%d check(s) failed\n" : "all payout tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
