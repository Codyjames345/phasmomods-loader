#include "hold.h"

namespace pm::tells::hold {
namespace {

bool g_still = false;      // hold the ghost still
bool g_resetSight = false; // set the line-of-sight speed-up back to 1 once

}  // namespace

void hold_still(bool on) { g_still = on; }

void reset_sight() { g_resetSight = true; }

bool hunt_speed(float* base, float* losScale) {
  if (!g_still && !g_resetSight) return false;
  if (g_still && base) *base = 0.0f;
  if (g_resetSight && losScale) *losScale = 1.0f;
  g_resetSight = false;
  return true;
}

void reset() {
  g_still = false;
  g_resetSight = false;
}

}  // namespace pm::tells::hold
