#pragma once

#include <cstddef>
#include <cstdint>

// The Hydra (hydra_rule.h) in the game. The host counts smudges in a hunt and walks a false head for
// each on a NavMeshAgent of its own, sending every client where the heads are; outside hunts it sends
// some appearances a second, fading apparition. Every client draws them as apparitions (apparition.h)
// that flicker and move as the real ghost does, silent and harmless. Main thread.
namespace pg::custom::hydra {

bool resolve();
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pg::custom::hydra
