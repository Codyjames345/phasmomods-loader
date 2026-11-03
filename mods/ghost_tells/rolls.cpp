#include "rolls.h"

#include <vector>

#include "UnityWorld.h"
#include "game_api.h"

namespace pm::tells::rolls {
namespace {

std::vector<Roller> g_rollers;

int32_t detour_random_range_int(api::Original<phasmo::Random::RandomRangeInt> original, int32_t min, int32_t max) {
  int result = original(min, max);
  for (Roller roller : g_rollers)
    if (roller(min, max, result)) break;
  return result;
}

}  // namespace

void add_roller(Roller roller) { g_rollers.push_back(roller); }

bool create_hooks() {
  return api::hook<phasmo::Random::RandomRangeInt>(&detour_random_range_int, "Random.RandomRangeInt",
                                                    api::Twins::CanonicalOnly);
}

}  // namespace pm::tells::rolls
