#pragma once

#include <span>

// The Bogeyman (bogeyman_rule.h) in the game, on the host. Its hunts begin from the closet nearest the
// youngest living player, and go only for its target (target below), whom the host holds its hunts to
// (host/content_target.h). With no target it searches the hiding places it reaches (the game's ClosetZones and
// HidingSpots), each once per hunt, each closet as the game's own hunt does (ClosetZone.CheckCloset). In an open
// room it is slow (behaviour.cpp) and drops prey beyond a short sight. Outside hunts most of its door
// interactions are on a closet door, when its door sensor detects one. Its AI and a player controlling it follow
// each of these alike, but for the route: its AI walks to the nearest hiding place itself, and a player walks
// where they choose.
namespace pm::custom::bogeyman {

bool resolve();
// Every frame on the host, before behaviour::tick: ghost is the custom ghost's GhostAI (null for
// none), type its type, steered whether a player controls it (then it does not choose where to go, and searches
// what their route reaches). In a hunt it walks nowhere of its own until the hunt's start delay is over
// (behaviour::hunt_begun).
void tick(void* ghost, int type, bool steered);
// It stands in an open room now (its speed falls there).
bool in_open_room();
// Its idle door interaction, its AI's or a ghost player's aimed one (the same door chance), once the game's
// own pick has found a door (restraint.cpp, GhostActivity.GetDoorToOpen, bogeyman_rule.h
// bogeyman_turns_door): 3 in 4 times, a closet door its door sensor (GhostActivity.interactionSensor, the one
// the game's pick reads) detects now, at random and never a main door, as the game's pick; null when it
// detects none or the roll fails (bogeyman_rule.h bogeyman_closet_door), and the game's own pick stands.
void* closet_door(void* ghost);
// Right after the game has begun its hunt on the host, its AI's or a ghost player's alike (bogeyman_rule.h
// bogeyman_steps_out): it steps out of the closet nearest the youngest player its hunt may go for (its
// target, while the hunt may go for them), other than the one they hide in, when one lies on their floor
// within kBogeymanLurkReach of them (bogeyman_rule.h lurk_place). The game's own ghost teleport moves it there
// (NavMesh.SamplePosition at the closet's nav spot, then NavMeshAgent.Warp, as vanilla warps do), and that
// closet's door opens. Otherwise the hunt begins where it is.
void hunt_begins(void* ghost);
// Its rule in the hunt's sight test (behaviour::choose_target): never proposed when they are beyond its short
// sight in an open room (bogeyman_rule.h open_sight_lost); else proposed. Whom the hunt may go for at all is
// the host's hold to its target.
void* choose_target(void* ghost, void* proposed);
// Its one target (PmGhostDef.target), to which the host holds its hunts, its AI's and a player's alike: the
// youngest living player (bogeyman_rule.h bogeyman_target): the lobby's prestige and level, equal progress to
// the lowest actor; null for none. On every client alike: it reads only the lobby's spots (synced by a
// buffered RPC), the players' deaths and their actors. ghost is the contract's ghost; the players in
// `excluded` are left out (where the game re-targets, the dying player).
void* target(void* ghost, std::span<const void* const> excluded);
void reset();

}  // namespace pm::custom::bogeyman
