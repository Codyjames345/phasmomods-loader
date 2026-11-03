#include "chances.h"

#include "GhostAI.h"
#include "roll.h"
#include "runtime.h"

namespace pm::chances {
namespace {

using phasmo::GhostActivity;

roll::Chance g_chance;  // a door, light or prop chance began: its roll is due (roll.h)

bool detour_door_chance(api::Original<GhostActivity::doorChance> original, void* activity) {
  roll::ChanceScope roll{g_chance};
  return original(activity);
}

bool detour_light_chance(api::Original<GhostActivity::lightChance> original, void* activity, bool force) {
  roll::ChanceScope roll{g_chance};
  return original(activity, force);
}

bool detour_prop_chance(api::Original<GhostActivity::InteractWithARandomProp> original, void* activity, bool force) {
  roll::ChanceScope roll{g_chance};
  return original(activity, force);
}

}  // namespace

bool create_hooks() {
  return api::hook<GhostActivity::doorChance>(&detour_door_chance, "GhostActivity.doorChance") &&
         api::hook<GhostActivity::lightChance>(&detour_light_chance, "GhostActivity.lightChance") &&
         api::hook<GhostActivity::InteractWithARandomProp>(&detour_prop_chance, "GhostActivity.InteractWithARandomProp");
}

int draw(int min, int max, int r, bool aimed) { return g_chance.draw(min, max, r, aimed); }

}  // namespace pm::chances
