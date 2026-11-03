#include "walk_state.h"

#include "GhostHuntState.h"
#include "GhostWanderState.h"
#include "runtime.h"

namespace pm::walk_state {
namespace {

template <class State>
tether::Reach reach_in(void* state, float startX, float startZ) {
  const phasmo::Vector3 point = State::destination.get(state);
  return tether::reach_of(startX, startZ, State::radius.get(state), State::hasDestination.get(state), point.x, point.z);
}

}  // namespace

bool hunt_searching(void* state) {
  using phasmo::GhostHuntState;
  return GhostHuntState::type.is(state) && !api::alive(GhostHuntState::target.get(state)) &&
         api::alive(GhostHuntState::searchTarget.get(state));
}

bool wander_reach(void* state, float startX, float startZ, tether::Reach& out) {
  if (phasmo::GhostWanderState::type.is(state)) out = reach_in<phasmo::GhostWanderState>(state, startX, startZ);
  else if (phasmo::GhostDotsState::type.is(state)) out = reach_in<phasmo::GhostDotsState>(state, startX, startZ);
  else return false;
  return true;
}

}  // namespace pm::walk_state
