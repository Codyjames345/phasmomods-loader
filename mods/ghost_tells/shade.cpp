#include "shade.h"

#include "GhostAI.h"
#include "brownie_rule.h"
#include "game_api.h"
#include "gate.h"
#include "held.h"
#include "hold.h"
#include "log.h"
#include "runtime.h"
#include "shade_rule.h"
#include "tells.h"
#include "world.h"

namespace pm::tells::shade {
namespace {

using world::Vector3;

struct State {
  api::Held ghost;
  Gaze gaze;
  Falter falter;
} g;

/** Whether a player faces `target` closely enough to be staring at it, with nothing between. */
bool stares_at(void* player, Vector3 target) {
  const Vector3 eye = world::eye(player), facing = world::forward(player);
  return custom::in_view(eye.x, eye.z, facing.x, facing.z, target.x, target.z, kShadeGazeCone, kShadeGazeRange) &&
         world::same_floor(eye, target) && world::clear_line(eye, target);
}

bool stared_at(void* ghost) {
  const Vector3 target = world::chest(ghost);
  bool seen = false;
  world::each_player([&](void* player) { seen = seen || (world::living(player) && stares_at(player, target)); });
  return seen;
}

/** In a hunt: its prey and another living player beside them, both in clear view of it. */
bool group_in_sight(void* ghost) {
  void* prey = world::hunt_target(ghost);
  if (!prey || !world::living(prey)) return false;
  const Vector3 from = world::chest(ghost), at = world::feet(prey);
  if (!world::clear_line(from, world::eye(prey))) return false;
  bool group = false;
  world::each_player([&](void* other) {
    if (group || other == prey || !world::living(other)) return;
    const Vector3 o = world::feet(other);
    group = world::distance(o, at) <= kShadeGroupRange && world::clear_line(from, world::eye(other));
  });
  return group;
}

bool company_rule(gate::Kind kind) {
  (void)kind;  // interactions, events and hunts alike
  void* ghost = g.ghost.get();
  return told_now() == kShade && api::alive(ghost) &&
         crowded(world::living_within(world::feet(ghost), kShadeCompanyRange));
}

}  // namespace

void resolve() { gate::add_rule(&company_rule); }

void tick_host(void* ghost, float dt) {
  if (ghost != g.ghost.get()) {
    reset();
    g.ghost = ghost;
  }
  const bool hunting = world::hunting(ghost);
  const bool showing = world::appeared(ghost) && !hunting && !cursed_pull(gate::states(), world::state(ghost));
  if (g.gaze.step(showing, showing && stared_at(ghost), dt)) {
    gate::change_state(ghost, phasmo::GhostState::idle());  // its event ends here
    if (world::appeared(ghost)) phasmo::GhostAI::UnAppear(ghost);
    PM_LOG("shade: stared at, it vanishes");
  }
  if (g.falter.step(hunting, hunting && group_in_sight(ghost), dt)) {
    hold::reset_sight();
    PM_LOG("shade: it hesitates at a group");
  }
  hold::hold_still(g.falter.holding());
}

void reset() {
  hold::hold_still(false);
  g = State{};
}

}  // namespace pm::tells::shade
