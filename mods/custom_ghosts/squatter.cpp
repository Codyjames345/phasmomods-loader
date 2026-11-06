#include "squatter.h"

#include "Difficulty.h"
#include "game_api.h"
#include "ghosts.h"
#include "runtime.h"

namespace pm::custom::squatter {
namespace {

std::optional<Phase> g_phase;

// Whether the contract's difficulty is a friendly ghost's (Difficulty.friendlyGhost); false when none is read.
bool friendly() {
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  return difficulty && phasmo::Difficulty::friendlyGhost.get(difficulty);
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() { return true; }

void tick(void* haunting, int type, bool /*steered: a player's own lines come in a later task*/) {
  if (!haunting || type != kSquatter) {
    g_phase.reset();
    return;
  }
  if (!g_phase) g_phase = first_phase(friendly());
}

std::optional<Phase> phase() { return g_phase; }

void reset() { g_phase.reset(); }

}  // namespace pm::custom::squatter
