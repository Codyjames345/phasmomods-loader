#include "shade.h"

#include "brownie_rule.h"
#include "GhostAI.h"
#include "game.h"
#include "gate.h"
#include "hold.h"
#include "il2cpp.h"
#include "log.h"
#include "players.h"
#include "shade_rule.h"
#include "tells.h"
#include "world.h"

namespace pg::tells::shade {
namespace {

using il2cpp::Vector3;

constexpr int kStateIdle = 0;

struct State {
  il2cpp::Held ghost;
  bool shade = false;
  Gaze gaze;
  Falter falter;
} g;

/** Whether a player faces `target` closely enough to be staring at it, with nothing between. */
bool stares_at(void* player, Vector3 target) {
  const Vector3 eye = world::eye(player), facing = game::forward(game::transform_of(player));
  return custom::in_view(eye.x, eye.z, facing.x, facing.z, target.x, target.z, kShadeGazeCone, kShadeGazeRange) &&
         world::same_floor(eye, target) && world::clear_line(eye, target);
}

bool stared_at(void* ghost) {
  const Vector3 target = world::chest(ghost);
  bool seen = false;
  game::each_player([&](void* player) { seen = seen || (game::living(player) && stares_at(player, target)); });
  return seen;
}

/** In a hunt: its prey and another living player beside them, both in clear view of it. */
bool group_in_sight(void* ghost) {
  void* prey = world::hunt_target(ghost);
  if (!prey || !game::living(prey)) return false;
  const Vector3 from = world::chest(ghost), at = world::feet(prey);
  if (!world::clear_line(from, world::eye(prey))) return false;
  bool group = false;
  game::each_player([&](void* other) {
    if (group || other == prey || !game::living(other)) return;
    const Vector3 o = world::feet(other);
    group = game::distance(o, at) <= kShadeGroupRange && world::clear_line(from, world::eye(other));
  });
  return group;
}

bool company_rule(gate::Kind kind) {
  (void)kind;  // interactions, events and hunts alike
  return g.shade && g.ghost && game::alive(g.ghost) && crowded(world::living_within(world::feet(g.ghost), kShadeCompanyRange));
}

}  // namespace

bool resolve() {
  gate::add_rule(&company_rule);
  return true;
}

void tick_host(void* ghost, int type, float dt) {
  if (ghost != g.ghost || (type == kShade) != g.shade) {
    reset();
    g.ghost = ghost;
    g.shade = type == kShade;
  }
  if (!g.shade) return;
  const bool hunting = world::hunting(ghost);
  const bool showing = world::appeared(ghost) && !hunting && !cursed_pull(game::ghost_state(ghost));
  if (g.gaze.step(showing, showing && stared_at(ghost), dt)) {
    gate::change_state(ghost, kStateIdle);  // its event ends here
    if (world::appeared(ghost)) phasmo::GhostAI::UnAppear(ghost);
    PG_LOG("shade: stared at, it vanishes");
  }
  if (g.falter.step(hunting, hunting && group_in_sight(ghost), dt)) {
    hold::reset_sight(ghost);
    PG_LOG("shade: it hesitates at a group");
  }
  hold::hold_still(ghost, g.falter.holding());
}

void reset() {
  if (g.ghost) hold::hold_still(g.ghost, false);
  g = State{};
}

}  // namespace pg::tells::shade
