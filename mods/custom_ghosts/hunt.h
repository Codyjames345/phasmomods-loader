#pragma once

// What Custom Ghosts reads of and writes to the game's hunt state (sdk/map/GhostHuntState.h), shared
// by the behaviour rules (behaviour.h). `hunt` below is a hunt state, null for none. Main thread.
namespace pm::custom::hunt {

/**
 * Whether the hunt may go for this player, as the game's own test (build 25811160)
 * decides: the player has a room (GhostHuntState's tests throw for one with none), is alive, is not
 * in the hunt's killed list, and their room is not outside the map (RoomKind.outsideMap, the truck).
 * False for a null hunt or player, and for a hunt with no killed list, where the game's test throws.
 * The game's test cannot be told apart from its look-alikes by the map's facts, so it is rebuilt here
 * from mapped fields, condition by condition.
 */
bool may_go_for(void* hunt, void* player);

// Whether the hunt's start delay is over (GhostHuntState.startDelay below 0): until then its gate
// (GhostHuntState.Started) runs no chase, speed or kill. False for a null hunt.
bool begun(void* hunt);

// Adds the player to the hunt's seen list unless it holds them already, as the game's sight test does
// for each player it passes. Nothing for a null hunt or player, or a hunt with no seen list.
void record_seen(void* hunt, void* player);

/**
 * The game's own sight test (GhostHuntState.Sees, skipDeogen false) for this player, with own_sight()
 * true for the call, so Custom Ghosts' detour on it passes the call straight on: no custom policy
 * applies and nothing re-enters it. own_sight() goes back to what it was however the call ends, a
 * managed exception from the game's test included (the test throws, for one, for a ghost with no
 * vision sensor); the exception goes on to the caller.
 */
bool own_sees(void* hunt, void* player);

// Whether an own_sees call is running: the sight detour then passes the call straight on.
bool own_sight();

// Clears own_sight(), for a reset between contracts.
void clear_own_sight();

}  // namespace pm::custom::hunt
