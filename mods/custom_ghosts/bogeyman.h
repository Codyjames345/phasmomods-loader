#pragma once

// The Bogeyman (bogeyman_rule.h) in the game, on the host. Its AI's hunts begin from the closet nearest
// the youngest living player, and in a hunt it goes for the youngest of the players it sees. With nobody in
// sight it walks to the hiding places (the game's ClosetZones and HidingSpots), nearest first, and
// searches each closet as the game's own hunt does (ClosetZone.CheckCloset). In an open room it is slow
// (behaviour.cpp) and drops prey beyond a short sight. Outside hunts most of its AI's door interactions
// near a closet are on the closet's door.
namespace pg::custom::bogeyman {

bool resolve();
// Every frame on the host, before behaviour::tick: ghost is the custom ghost's GhostAI (null for
// none), type its type, steered whether a player controls it (then it does not choose where to go).
void tick(void* ghost, int type, bool steered);
// It stands in an open room now (its speed falls there).
bool in_open_room();
// Its AI's door interaction outside hunts (restraint.cpp, GhostActivity.GetDoorToOpen): with a closet
// within kBogeymanCreakReach, 3 in 4 times one of its doors (never a main door, as the game's own pick),
// chosen at random; otherwise null, and the game's own pick stands.
void* closet_door(void* ghost);
// Right after the game has begun its AI's hunt on the host (never a player's): it steps out of the closet
// nearest the youngest player its hunt may go for, other than the one they hide in, when one lies on their
// floor within kBogeymanLurkReach of them (bogeyman_rule.h lurk_place). The game's own ghost teleport moves
// it there (NavMesh.SamplePosition at the closet's nav spot, then NavMeshAgent.Warp, as vanilla warps do),
// and that closet's door opens. Otherwise the hunt begins where it is.
void hunt_begins(void* ghost);
// Its rule in the hunt's target policy (behaviour::choose_target): while the game's own sight test passes
// for more than one player its hunt may go for (leaving out those beyond its short sight in an open room),
// it keeps proposed only if they are the youngest of them, so the game's own target code goes for the
// youngest; with fewer it keeps proposed.
void* choose_target(void* ghost, void* proposed);
void reset();

}  // namespace pg::custom::bogeyman
