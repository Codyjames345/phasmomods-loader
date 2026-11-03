#include "mare.h"

#include <random>
#include <vector>

#include "GhostAI.h"
#include "LevelRoom.h"
#include "LightSwitch.h"
#include "game_api.h"
#include "gate.h"
#include "held.h"
#include "log.h"
#include "mare_rule.h"
#include "rolls.h"
#include "runtime.h"
#include "tells.h"
#include "world.h"

namespace pm::tells::mare {
namespace {

using phasmo::LightSwitch;

int g_inUse = 0;  // inside LightSwitch.UseLightSwitch, which runs the switch-off coroutine's first step

struct State {
  api::Held ghost;
  bool wasHunting = false;
  bool endEvent = false;  // an appearance it withheld: its event ends next frame
  float sinceLook = 0;
  RoomTrail trail;
} g;

std::mt19937 g_random{std::random_device{}()};

template <class F>
void each_switch(void* room, F visit) {
  const api::List list{api::alive(room) ? phasmo::LevelRoom::lightSwitches.get(room) : nullptr};
  for (size_t i = 0; i < list.size(); ++i)
    if (void* s = list[i]; api::alive(s)) visit(s);
}

// UseLightSwitch returns at once for the fuse box's switch, and for a ghost's use while it is refused.
bool usable(void* light) { return !LightSwitch::isFuseboxSwitch.get(light) && !LightSwitch::ghostUseRefused.get(light); }

/** Every lit, usable switch of a room off, as the Mare's own switch-off does it. */
void darken(void* room) {
  each_switch(room, [](void* light) {
    if (LightSwitch::get_IsSwitchOn(light) && usable(light)) LightSwitch::UseLightSwitch(light, false, true, true);
  });
}

// UseLightSwitch restarts the Mare's switch-off coroutine with StartCoroutine for a type-6 ghost, and
// Unity runs the coroutine's first step, where it rolls, inside that call (LightSwitch.h).
void detour_use(api::Original<LightSwitch::UseLightSwitch> original, void* light, bool use, bool deadMayUse,
                bool byGhost) {
  struct Inside {  // cleared however the call ends, a managed exception included
    Inside() { ++g_inUse; }
    ~Inside() { --g_inUse; }
  } inside;
  original(light, use, deadMayUse, byGhost);
}

bool roll_switch_off(int min, int max, int& result) {
  if (!mare_roll_applies(g_inUse > 0, min, max) || told_now() != kMare) return false;
  result = mare_roll(std::uniform_int_distribution<int>(0, kMareSwitchOffOneIn - 1)(g_random));
  return true;
}

void detour_appear(api::Original<phasmo::GhostAI::Appear> original, void* ai, int32_t form) {
  if (told_now() != kMare || ai != g.ghost.get() || !world::is_master() || world::hunting(ai) ||
      cursed_pull(gate::states(), world::state(ai))) {
    original(ai, form);
    return;
  }
  void* room = world::room_at(world::feet(ai));
  bool flickering = false;
  const bool lit = room && phasmo::LevelRoom::AreRoomLightsOn(room, &flickering);
  int litSwitches = 0, usableLit = 0;
  each_switch(room, [&](void* light) {
    if (!LightSwitch::get_IsSwitchOn(light)) return;
    ++litSwitches;
    if (usable(light)) ++usableLit;
  });
  switch (mare_appearance(lit, litSwitches, usableLit)) {
    case Appearance::DarkenFirst:
      darken(room);
      PM_LOG("mare: the lights go out before it shows itself");
      break;
    case Appearance::Withhold:
      // Not shown at all: the event that asked for the appearance ends as the next frame begins, rather
      // than from inside the event's own call here, through the game's own change to idle.
      g.endEvent = true;
      PM_LOG("mare: it will not show itself in a room it cannot darken");
      return;
    case Appearance::Appear: break;
  }
  original(ai, form);
}

}  // namespace

void resolve() { rolls::add_roller(&roll_switch_off); }

bool create_hooks() {
  return api::hook<phasmo::GhostAI::Appear>(&detour_appear, "GhostAI.Appear", api::Twins::CanonicalOnly) &&
         api::hook<LightSwitch::UseLightSwitch>(&detour_use, "LightSwitch.UseLightSwitch", api::Twins::CanonicalOnly);
}

void tick(void* ghost, bool host, float dt) {
  if (ghost != g.ghost.get()) {
    reset();
    g.ghost = ghost;
  }
  if (!host) return;
  if (g.endEvent) {
    g.endEvent = false;
    const int state = world::state(ghost);
    const GateStates& s = gate::states();
    if (state == s.events[1] || state == s.events[0]) gate::change_state(ghost, phasmo::GhostState::idle());
  }
  const bool hunting = world::hunting(ghost);
  if (hunting) {
    g.sinceLook += dt;
    if (g.sinceLook >= kMareTrailStep) {
      g.sinceLook = 0;
      g.trail.pass(world::room_index(world::room_at(world::feet(ghost))));
    }
  } else if (g.wasHunting) {
    // Light switches cannot be used in a hunt, so the rooms it passed through go dark as it ends.
    for (int index : g.trail.rooms()) darken(world::room_by_index(index));
    PM_LOG("mare: the hunt leaves %zu rooms dark", g.trail.rooms().size());
    g.trail.clear();
  }
  g.wasHunting = hunting;
}

void reset() { g = State{}; }

}  // namespace pm::tells::mare
