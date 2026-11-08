#pragma once

// The contract ghost's sight outside a hunt, for any custom ghost that needs to know whom the ghost can see. The
// ghost's own vision sensor (GhostAI.vision) detects nobody outside a hunt, so this sees through the host's own copies
// of its two sensors (sensor_copy.h), set up from the ghost's and pulsed only when asked; the ghost's own sensors are
// never pulsed or changed. One set of copies serves every caller, made for the contract's ghost on the first pulse and
// remade when the ghost changes. The Lithobolia and the Squatter share it. Host (master) only, main thread.
namespace pm::custom::ghost_sight {

/**
 * Pulses the copies where the ghost's sensors are, with the ghost's settings now, making them first for `ghost` (a
 * GhostAI) when they are not made or were made for another ghost. False when this ghost has no sight: its sensors are
 * not the kinds the copies can share (logged once per ghost, not tried again), or a pulse threw.
 */
bool pulse(void* ghost);
/** Whether the copies, just pulsed, see `player` (a living Player, by any of their sight parts). */
bool sees(void* player);
/** Destroys the copies, as the ghost goes or the mods stop. */
void drop();

}  // namespace pm::custom::ghost_sight
