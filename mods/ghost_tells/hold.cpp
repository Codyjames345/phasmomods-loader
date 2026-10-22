#include "hold.h"

#include "il2cpp.h"
#include "world.h"

namespace pg::tells::hold {
namespace {

constexpr size_t kHuntBase = 0xB0;   // hunt state: base speed (refreshed from GhostAI +0xC8 every 2 s)
constexpr size_t kHuntSight = 0xB4;  // hunt state: line-of-sight speed-up, 1 to 1.65

il2cpp::Held g_state;  // the hunt state held still, while a hold runs
float g_base = 0;

}  // namespace

void hold_still(void* ai, bool on) {
  void* state = world::hunt_state(ai);
  if (!on) {
    // The base goes back only into the hunt it was taken from; a new hunt starts from the game's own.
    if (g_state && state == g_state) il2cpp::set_field<float>(state, kHuntBase, g_base);
    g_state = nullptr;
    return;
  }
  if (!state) return;
  if (state != g_state) {
    g_state = state;
    g_base = il2cpp::field<float>(state, kHuntBase);
  }
  il2cpp::set_field<float>(state, kHuntBase, 0.0f);
}

void reset_sight(void* ai) {
  if (void* state = world::hunt_state(ai)) il2cpp::set_field<float>(state, kHuntSight, 1.0f);
}

void reset() { g_state = nullptr; }

}  // namespace pg::tells::hold
