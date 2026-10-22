#include "gate.h"

#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"

namespace pg::tells::gate {
namespace {

constexpr int kStateHunting = 2;  // GhostAI.State.hunting

using TryInteract_t = void (*)(void*, const void*);
using ChangeState_t = void (*)(void*, int, void*, bool, const void*);
TryInteract_t g_tryInteract = nullptr;
ChangeState_t g_changeState = nullptr;

std::vector<Rule> g_rules;
std::vector<StateListener> g_listeners;
il2cpp::Held g_ghost;

bool holds(Kind kind) {
  for (Rule rule : g_rules)
    if (rule(kind)) return true;
  return false;
}

void detour_try_interact(void* activity, const void* method) {
  hooks::Guard guard;
  if (g_ghost && game::is_master() && activity == game::ghost_activity(g_ghost) && holds(kInteract)) return;
  g_tryInteract(activity, method);
}

void detour_change_state(void* ai, int state, void* target, bool flag, const void* method) {
  hooks::Guard guard;
  if (!g_ghost || ai != g_ghost || !game::is_master()) {
    g_changeState(ai, state, target, flag, method);
    return;
  }
  // A hunt's own throws and door slams are its own; nothing here reaches into a running hunt.
  if (gate_may_hold(state, flag, game::ghost_state(ai) == kStateHunting) && holds(static_cast<Kind>(gate_kind(state)))) return;
  for (StateListener l : g_listeners) l(ai, state, true);
  g_changeState(ai, state, target, flag, method);
  for (StateListener l : g_listeners) l(ai, state, false);
}

}  // namespace

void add_rule(Rule rule) { g_rules.push_back(rule); }
void around_state(StateListener listener) { g_listeners.push_back(listener); }

bool create_hooks() {
  return hooks::create(game::try_interact(), reinterpret_cast<void*>(&detour_try_interact),
                       reinterpret_cast<void**>(&g_tryInteract), "GhostActivity.TryInteract") &&
         hooks::create(game::change_state(), reinterpret_cast<void*>(&detour_change_state),
                       reinterpret_cast<void**>(&g_changeState), "GhostAI.ChangeState");
}

void tick(void* ghost) { g_ghost = ghost; }

bool held_back(Kind kind) { return g_ghost && holds(kind); }

void change_state(void* ai, int state) {
  reinterpret_cast<ChangeState_t>(game::change_state())(ai, state, nullptr, false, nullptr);
}

void try_interact(void* ai) {
  if (void* activity = game::ghost_activity(ai)) reinterpret_cast<TryInteract_t>(game::try_interact())(activity, nullptr);
}

}  // namespace pg::tells::gate
