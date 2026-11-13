#pragma once

#include <cstdint>
#include <vector>

#include "session.h"

// The Mimic's copies, chosen by its player. The game switches a Mimic's copied type on its own timer
// (GhostController.AttemptGhostTypeStateChange) to a random one of the types it may copy. On the host,
// the ghost player is offered those types except the one it is copying now and may pick the next copy;
// the pick is put in the game's own override when the game's timer next switches, so the copy changes
// exactly when it would have, to the player's choice instead of a random one. Main thread.
namespace pm::mimic {

bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s, void* ghost);  // host, every frame (ghost null: none)
uint8_t next();  // the pick waiting for the next switch, or session::kNoType

// The types the Mimic may copy now (GhostController.mimicCopies), those below session::kNoType, in the
// game's order. Empty when `controller` is null, destroyed or not a Mimic's, and before its first copy.
std::vector<uint8_t> copies(void* controller);
// The types the ghost player chooses from, kept free of game calls so it can be tested offline: all of
// `list` except `current`, the type it is copying now (session::kNoType when none), in the game's
// order. The game's own pick at a switch is a random element of the whole list and may copy the type
// it is copying now (GhostController.MimicCopy); its player is never offered that one (the user's rule,
// 2026-11-01).
inline std::vector<uint8_t> offer(const std::vector<uint8_t>& list, uint8_t current) {
  std::vector<uint8_t> out;
  out.reserve(list.size());
  for (uint8_t t : list)
    if (t != current) out.push_back(t);
  return out;
}
// Puts `pick` in the game's override (GhostController.mimicNext) when the controller is a Mimic's, its
// own timer asks for a switch (typeChangeDue), no override waits, the pick is one it may copy and it is
// not the type it is copying now. True when it did; it then copies `pick` at this switch instead of a
// random type.
bool place_pick(void* controller, uint8_t pick);

}  // namespace pm::mimic
