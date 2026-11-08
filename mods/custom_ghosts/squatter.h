#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "squatter_net.h"
#include "squatter_rule.h"

// The Squatter (squatter_rule.h) in the game: a person in a bedsheet, not a ghost
// (docs/superpowers/specs/2026-11-04-squatter-design.md). Main thread.
//  - It gives no evidence (ghosts.h: three empty slots) and attempts nothing: restraint.cpp refuses its
//    interactions, every pulse of its interaction sensor and every state change but idling, wandering and its
//    favourite room (squatter_rule.h keeps_quiet), forced or not, its AI's and a player's alike. A cursed
//    possession's effect on a player alone stays the game's (sdk/systems/ghost-ai.md, "Every route to a state
//    change").
//  - A Mimic never copies it (ghosts.h, the content layer's PM_GHOST_NO_MIMIC_COPY).
//  - Only the master runs it. The other clients show what the master says (squatter_net.h Shown, on Custom Ghosts'
//    one channel), tell the master their own player's tries at a locked exit and their sprint, and hold the exits once
//    the master's lock reaches them (squatter_door.h).
//  - tick keeps its phase, which begins at first_phase on the first frame it haunts with the difficulty read.
// When something it needs cannot load, the whole mod fails its start (mod.cpp), as any mod with an error does.
namespace pm::custom::squatter {

/** True: what it reads of the game are map entries the mod requires. Main thread, from pmmod_start. */
bool resolve();
/**
 * Its own hooks, before hooks::enable_all: a player's try at a locked exit and the master's lock arriving
 * (squatter_door.h). Its restraint is restraint.cpp's.
 */
bool create_hooks();
/**
 * Every frame on every client. `haunting` is the contract's ghost, its GhostAI (null for none), `type` the custom
 * ghost it acts as (ghosts.h Type, 0 for none), `steered` whether a player controls it (on the host). While it haunts
 * as the Squatter its phase is kept, from first_phase(Difficulty.friendlyGhost) on the first frame the difficulty can
 * be read (none before it); on a client that is not the master it follows the last Shown instead, so a client that
 * becomes the master goes on from there (squatter_net.h kept_phase). Each client also tells the master its own
 * player's sprint. Otherwise it holds no phase, and forgets what the channel brought.
 */
void tick(void* haunting, int type, bool steered);
/** Its phase while it haunts as the Squatter, else none. */
std::optional<Phase> phase();

/**
 * One message of the Squatter's from Custom Ghosts' channel (squatter_net.h), `sender` being Photon's actor number
 * from the event: a Shown, taken only from the room's master (Photon's master client); a try at a locked exit and a
 * sprint report, taken only on the master, from a player in the room. Anything else, or anything malformed, is
 * dropped. Main thread, from the channel's handler (mod.cpp).
 */
void receive(int sender, const uint8_t* data, size_t size);
/**
 * The last Shown: on the master its own, as share last sent it; on another client the master's last word. None
 * before the first, and once the Squatter no longer haunts.
 */
std::optional<Shown> shown();
/**
 * On the master: sends `s` to every other client, reliably, when it differs from the last Shown, and keeps it as
 * shown(). False, sending nothing, when it is unchanged or this client is not the master.
 */
bool share(const Shown& s);
/**
 * On the master: who has tried a locked exit since the last call, by actor number, oldest first (0 for its own player
 * when it has no actor number, in single player); a double report from one player within kTryWindowMs counts once.
 * Empties the queue. Empty on another client.
 */
std::vector<int> take_tries();
/**
 * On the master: player `actor`'s last sprint report (Sprint, as their own client reads it), or none when they have not
 * reported within kSprintStaleMs. Its own player's comes from this client every frame, under game::local_actor().
 */
std::optional<Sprint> sprint_of(int actor);

/** Forgets the phase, what the channel brought and the exits' lock, as when the mods stop. */
void reset();

}  // namespace pm::custom::squatter
