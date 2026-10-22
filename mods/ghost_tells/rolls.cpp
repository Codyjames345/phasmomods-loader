#include "rolls.h"

#include <vector>

#include "game.h"
#include "hooks.h"

namespace pg::tells::rolls {
namespace {

using RandomRangeInt_t = int (*)(int, int, const void*);
RandomRangeInt_t g_randomRangeInt = nullptr;
std::vector<Roller> g_rollers;

int detour_random_range_int(int min, int max, const void* method) {
  hooks::Guard guard;
  int result = g_randomRangeInt(min, max, method);
  for (Roller roller : g_rollers)
    if (roller(min, max, result)) break;
  return result;
}

}  // namespace

void add_roller(Roller roller) { g_rollers.push_back(roller); }

bool create_hooks() {
  return hooks::create(game::random_range_int(), reinterpret_cast<void*>(&detour_random_range_int),
                       reinterpret_cast<void**>(&g_randomRangeInt), "Random.RandomRangeInt");
}

}  // namespace pg::tells::rolls
