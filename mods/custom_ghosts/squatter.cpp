#include "squatter.h"

#include <cmath>

#include "Difficulty.h"
#include "Transform.h"
#include "UnityObject.h"
#include "game_api.h"
#include "ghosts.h"
#include "runtime.h"
#include "squatter_door.h"
#include "squatter_figure.h"

namespace pm::custom::squatter {
namespace {

std::optional<Phase> g_phase;

// Whether the contract's difficulty is a friendly ghost's (Difficulty.friendlyGhost); false when none is read.
bool friendly() {
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  return difficulty && phasmo::Difficulty::friendlyGhost.get(difficulty);
}

// Every client shows its own figure where the contract's ghost is, facing the way it faces.
void show(void* ghost) {
  void* t = phasmo::Component::get_transform(ghost);
  if (!t) return;
  const phasmo::Vector3 at = phasmo::Transform::get_position(t);
  if (!figure::ready()) figure::create(at);
  const phasmo::Vector3 f = phasmo::Transform::get_forward(t);
  figure::place(at, std::atan2(f.x, f.z));
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() { return door::create_hooks(); }

void tick(void* haunting, int type, bool /*steered: a player's own lines come in a later task*/) {
  if (!haunting || type != kSquatter) {
    g_phase.reset();
    figure::destroy();
    door::reset();
    return;
  }
  if (!g_phase) g_phase = first_phase(friendly());
  show(haunting);
}

std::optional<Phase> phase() { return g_phase; }

void reset() {
  g_phase.reset();
  figure::destroy();
  door::reset();
}

}  // namespace pm::custom::squatter
