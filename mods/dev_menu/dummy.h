#pragma once

#include "Transform.h"

// The dummy investigator (the user's ask, 2026-11-10, widened 2026-11-12 to "any ghost's target"): a second body in a
// single player contract that the ghost's hunts treat as a player, for testing what otherwise needs a second person
// (the Banshee's one target, a hunt choosing between two people, the closet search). It is a copy of the local
// investigator's networked body, made under a parent that is switched off and stripped of every game script but its
// Player, PlayerSanity and PlayerLOSTarget (all three switched off) before it is ever switched on, so none of its own
// code runs (no Photon view, no voice, no spot). It is never in Network.playerSpots, so the game's own player walks
// never see it; the hunt sees it through the mod's hooks on the hunt's pickers, which offer it exactly as the game
// offers a player (re/dump/notes/2026-11-12-dev-menu-dummy.md). Main thread.
namespace pm::devmenu::dummy {

/** Makes the dummy beside the local investigator. False with no contract or investigator, or one already there. */
bool spawn();
/** Destroys the dummy and takes it out of the ghost's Banshee target. */
void despawn();
/** The dummy's Player, or null with none. */
void* player();
/** Whether the dummy may be chosen now: it exists and is not dead. */
bool available();
/** Moves the dummy to stand where the local investigator stands. */
bool to_me();
/** Makes the dummy the ghost's Banshee target (GhostAI.bansheeTarget), which StartPick keeps for the whole hunt. */
bool make_banshee_target();
/** The hunt's kill of `victim`, from the mod's kill hook: true when the victim is the dummy, which then dies here
 *  (Player.isDead set on it) and the game's own kill is not run, since its RPCs go to the dummy's Photon owner,
 *  which it has none of. */
bool on_kill(void* victim);
/** Gives the dummy its life back after a kill. */
void revive();
/** Each frame: keeps the dummy's room current (the hunt's tests read Player.currentRoom) and forgets a dummy a scene
 *  change destroyed. */
void tick();
/** The dummy's position, for ESP; false with none. */
bool position(phasmo::Vector3& out);

/** Creates the hooks on the hunt's pickers and its kill. False when any cannot be made (the mod then stays off). */
bool hook();

}  // namespace pm::devmenu::dummy
