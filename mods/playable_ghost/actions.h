#pragma once

#include "session.h"
#include "tether.h"

// Ghost actions chosen by the ghost player, run by the host through native code.
//
// Pacing and the mix of actions come from the native idle roll (every 2-6 s). Each time it would
// have acted, the ghost player is given one action of the kind it chose instead (up to two of each):
// an interaction (GhostActivity.TryInteract), an event, the type's ability or the fuse box
// (GhostActivity.GhostAbility's choice, with its type and difficulty odds and its Shade, Hantu and
// setup rules). The roll's wanders and walks home run natively. While the Tower card's interaction
// boost runs, the roll's actions happen natively, as in vanilla.
//
// Restlessness: once the ghost player has not tried to act for tether::kRestlessMs, the roll acts on
// its own again (hunts included) until they next try. What they have saved up stays theirs across a
// short gap in their input; only a new ghost starts from nothing.
//
// Hunts: the native hunt branch runs untouched, its final 1-in-3/4/5 chance included. When it would
// hunt, the hunt is offered to the ghost player instead, until the roll next reaches its hunt check
// and while the native checks still pass. Native forced hunts (cursed objects, the Death card, a
// Demon's ability) are never intercepted. Crucifixes are handled natively when the hunt starts.
//
// An aimed interaction pulses the ghost's native interaction sensor with its candidates narrowed to
// the aimed object and the per-type category chance forced, so the native chain (Shade presence rule,
// door eligibility, state-specific type rules) decides what happens. The native per-type odds become
// its cost instead: base odds / type odds. A Twins ghost's twin then interacts on its own at the
// twin's vanilla rate (TryInteract: one roll in two).
namespace pg::actions {

// kDisguise: an event in an investigator's form (form.h), paid for with an event charge under the
// event's own vanilla checks; the investigator is the one nearest where the ghost looks.
enum Kind { kInteract, kEvent, kAbility, kHunt, kFuse, kDisguise, kKindCount };

bool create_hooks();  // before hooks::enable_all

// Host only, main thread, once per frame. ghost is null when nobody controls the ghost.
void tick(void* ghost);  // null releases it to the native AI, keeping what was saved up
// Who moves the ghost this frame (src/tether.h). kSteer before a ghost is controlled.
tether::Reason movement();
bool perform(int kind, const float aim[3]);  // false if not allowed now or nothing happened
session::Status status();
// The investigator (Photon actor) whose form the ghost has just taken, once; 0 for none.
int take_disguise();
void clear();  // forget everything (leaving a contract, unloading)

}  // namespace pg::actions
