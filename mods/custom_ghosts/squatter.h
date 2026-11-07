#pragma once

#include <optional>

#include "squatter_rule.h"

// The Squatter (squatter_rule.h) in the game: a person in a bedsheet, not a ghost
// (docs/superpowers/specs/2026-11-04-squatter-design.md). Main thread.
//  - It gives no evidence (ghosts.h: three empty slots) and attempts nothing: restraint.cpp refuses its
//    interactions, every pulse of its interaction sensor and every state change but idling, wandering and its
//    favourite room (squatter_rule.h keeps_quiet), forced or not, its AI's and a player's alike. A cursed
//    possession's effect on a player alone stays the game's (sdk/systems/ghost-ai.md, "Every route to a state
//    change").
//  - A Mimic never copies it (ghosts.h, the content layer's PM_GHOST_NO_MIMIC_COPY).
//  - tick keeps its phase, which begins at first_phase on the frame it starts to haunt.
// When something it needs cannot load, the whole mod fails its start (mod.cpp), as any mod with an error does.
namespace pm::custom::squatter {

/** True: what it reads of the game are map entries the mod requires. Main thread, from pmmod_start. */
bool resolve();
/**
 * Its own hooks, before hooks::enable_all: a player's try at a locked exit (squatter_door.h). Its restraint is
 * restraint.cpp's.
 */
bool create_hooks();
/**
 * Every frame on every client. `haunting` is the contract's ghost, its GhostAI (null for none), `type` the custom
 * ghost it acts as (ghosts.h Type, 0 for none), `steered` whether a player controls it (on the host). While it
 * haunts as the Squatter its phase is kept, from first_phase(Difficulty.friendlyGhost) on the frame it begins;
 * otherwise it holds none.
 */
void tick(void* haunting, int type, bool steered);
/** Its phase while it haunts as the Squatter, else none. */
std::optional<Phase> phase();
/** Forgets the phase and the exits' lock, as when the mods stop. */
void reset();

}  // namespace pm::custom::squatter
