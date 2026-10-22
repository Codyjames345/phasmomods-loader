// Offline tests for pg::payout (no game). Run: build\payout_test.exe
#include "payout.h"

#include <cstdio>

using pg::payout::ghost;

namespace {

int g_failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                               \
    }                                                             \
  } while (0)

void nothing_unless_every_investigator_died() {
  CHECK(ghost(false, 200, 1.0f).money == 0);
  CHECK(ghost(false, 200, 1.0f).xp == 0);
}

void scaled_inversely_to_the_difficulty() {
  CHECK(ghost(true, 200, 1.0f).money == 200);
  CHECK(ghost(true, 200, 2.0f).money == 100);
  CHECK(ghost(true, 200, 4.0f).money == 50);
  CHECK(ghost(true, 200, 3.0f).money == 67);  // rounded
  CHECK(ghost(true, 200, 4.0f).xp == ghost(true, 200, 4.0f).money);
  CHECK(ghost(true, 200, 6.0f).money < ghost(true, 200, 2.0f).money);  // harder pays the ghost less
}

void no_pay_difficulty_pays_nothing_and_easy_ones_are_capped() {
  CHECK(ghost(true, 200, 0.0f).money == 0);
  CHECK(ghost(true, 200, -1.0f).money == 0);
  CHECK(ghost(true, 200, 0.1f).money == 600);  // capped at 3x
  CHECK(ghost(true, 0, 1.0f).money == 0);
}

}  // namespace

int main() {
  nothing_unless_every_investigator_died();
  scaled_inversely_to_the_difficulty();
  no_pay_difficulty_pays_nothing_and_easy_ones_are_capped();
  std::printf(g_failures ? "%d check(s) failed\n" : "all payout tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
