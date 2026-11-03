#include "gate.h"

#include <vector>

#include "GhostAI.h"
#include "game_api.h"
#include "held.h"
#include "runtime.h"
#include "world.h"

namespace pm::tells::gate {
namespace {

using phasmo::GhostAI;

std::vector<Rule> g_rules;
std::vector<StateListener> g_listeners;
api::Held g_ghost;
GateStates g_states;

bool holds(Kind kind) {
  for (Rule rule : g_rules)
    if (rule(kind)) return true;
  return false;
}

void detour_try_interact(api::Original<phasmo::GhostActivity::TryInteract> original, void* activity) {
  void* ghost = g_ghost.get();
  if (ghost && world::is_master() && activity == GhostAI::activity.get(ghost) && holds(kInteract)) return;
  original(activity);
}

void detour_change_state(api::Original<GhostAI::ChangeState> original, void* ai, int32_t state, void* interact,
                         bool forced) {
  if (!g_ghost.get() || ai != g_ghost.get() || !world::is_master()) {
    original(ai, state, interact, forced);
    return;
  }
  // A hunt's own throws and door slams are its own; nothing here reaches into a running hunt.
  const bool hunting = GhostAI::state.get(ai) == g_states.hunting;
  if (gate_may_hold(g_states, state, forced, hunting) && holds(static_cast<Kind>(gate_kind(g_states, state)))) return;
  for (StateListener l : g_listeners) l(ai, state, true);
  original(ai, state, interact, forced);
  for (StateListener l : g_listeners) l(ai, state, false);
}

}  // namespace

void add_rule(Rule rule) { g_rules.push_back(rule); }
void around_state(StateListener listener) { g_listeners.push_back(listener); }

void resolve() {
  using namespace phasmo::GhostState;
  g_states.hunting = hunting();
  g_states.events = {appear(), randomEvent(), GhostAbility()};
  g_states.interactions = {light(),   door(),   throwing(),  fusebox(),   doorKnock(),      windowKnock(), carAlarm(),
                           flicker(), cctv(),   mannequin(), teleportObject(), interact(),  ignite()};
  g_states.pulls = {summoningCircle(), musicBox()};
}

const GateStates& states() { return g_states; }

bool create_hooks() {
  return api::hook<phasmo::GhostActivity::TryInteract>(&detour_try_interact, "GhostActivity.TryInteract",
                                                        api::Twins::CanonicalOnly) &&
         api::hook<GhostAI::ChangeState>(&detour_change_state, "GhostAI.ChangeState", api::Twins::CanonicalOnly);
}

void tick(void* ghost) {
  if (ghost != g_ghost.get()) g_ghost = ghost;
}

bool held_back(Kind kind) { return g_ghost.get() && holds(kind); }

void change_state(void* ai, int state) { GhostAI::ChangeState(ai, state, nullptr, false); }

void try_interact(void* ai) {
  if (void* activity = api::alive(ai) ? GhostAI::activity.get(ai) : nullptr; api::alive(activity))
    phasmo::GhostActivity::TryInteract(activity);
}

}  // namespace pm::tells::gate
