#pragma once

#include <cstddef>
#include <cstdint>

// The Hydra (hydra_rule.h) in the game. The host counts smudges in a hunt and walks a false head for
// each on a NavMeshAgent of its own, sending every client where the heads are; outside hunts it sends
// some appearances a second, fading apparition. Every client draws them as apparitions (apparition.h)
// that flicker and move as the real ghost does, silent and harmless. In a hunt the host has each head
// throw as the hunting ghost throws: on the hunt's own throw ticks, through its own copy of the ghost's
// interaction sensor, the game's test and Prop.TryThrow (hydra_rule.h). Main thread.
namespace pm::custom::hydra {

bool resolve();
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
// The host: the hunting ghost itself is throwing `prop` (a Prop; the restraint.cpp TryThrow detour), so no
// head throws it on this tick.
void ghost_threw(void* ghost, void* prop);
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pm::custom::hydra
