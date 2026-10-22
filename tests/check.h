#pragma once

#include <cstdio>

// The offline tests' assertion: a failed check prints where it is and the test carries on, so one
// run reports every failure. main returns failure when g_failures is non-zero.
inline int g_failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                               \
    }                                                             \
  } while (0)
