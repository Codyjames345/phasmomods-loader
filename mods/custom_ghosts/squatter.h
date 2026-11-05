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
//  - It is rolled only while available(): until its figure is ready (set_assets_ready), the host gives it a weight of
//    0 in the game's type roll (registration.h rollable), while every other ghost keeps the game's weight. A Custom
//    difficulty that names it still gets it, as the game sets that type after the roll.
//  - tick keeps its phase, which begins at first_phase on the frame it starts to haunt.
namespace pm::custom::squatter {

/** True: what it reads of the game are map entries the mod requires. Main thread, from pmmod_start. */
bool resolve();
/** Its own hooks, before hooks::enable_all; none yet (its restraint is restraint.cpp's). True. */
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
/** Whether it may be rolled: its figure is ready. False until set_assets_ready(true). */
bool available();
/** Its figure (the model, clips and sheet) is ready or not; the figure says so once it has loaded or failed. */
void set_assets_ready(bool ready);
/** Forgets the phase, as when the mods stop. */
void reset();

}  // namespace pm::custom::squatter
