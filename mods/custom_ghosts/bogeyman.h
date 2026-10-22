#pragma once

// The Bogeyman (bogeyman_rule.h) in the game, on the host. Its AI's hunts begin from the closet nearest
// the youngest living player, and in a hunt it goes for the youngest of the players it sees. With nobody in
// sight it walks to the hiding places (the game's ClosetZones and HidingSpots), nearest first, and
// searches each closet as the game's own hunt does (ClosetZone.CheckCloset). In an open room it is slow
// (behaviour.cpp) and drops prey beyond a short sight. Outside hunts most of its interactions near a
// closet open the closet's door instead.
namespace pg::custom::bogeyman {

bool resolve();
// Every frame on the host, before behaviour::tick: ghost is the custom ghost's GhostAI (null for
// none), type its type, steered whether a player controls it (then it does not choose where to go).
void tick(void* ghost, int type, bool steered);
// It stands in an open room now (its speed falls there).
bool in_open_room();
// The host's interaction roll: true when it creaked a closet open instead.
bool creak(void* ghost);
// Right after the game has begun its AI's hunt on the host (never a player's): it steps out of the closet
// nearest the youngest player its hunt may go for, when one lies within kBogeymanLurkReach of them, moved
// there by the game's own ghost teleport (NavMeshAgent.Warp), and that closet's door opens. Otherwise
// the hunt begins where it is.
void hunt_begins(void* ghost);
// Its rule in the hunt's target policy (behaviour::choose_target): while its vision sensor detects more
// than one player its hunt may go for, it keeps proposed only if they are the youngest of them, so the
// game's own target code goes for the youngest; with fewer it keeps proposed.
void* choose_target(void* ghost, void* proposed);
void reset();

}  // namespace pg::custom::bogeyman
