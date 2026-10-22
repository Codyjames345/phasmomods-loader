#pragma once

#include <cstddef>
#include <cstdint>

// The Lithobolia (lithobolia_rule.h) in the game. On the host, its throws (the ghost's change to the
// throwing state) go at a player the ghost's own vision sensor detects within range: it takes the object
// over, launches it at their chest and watches it fly; a hit tells that player's client, which takes the
// sanity off its own player. With nobody in sight it throws nothing. In a hunt the throws the game makes
// fly at the hunt's target instead, and only while it sees them within range. Main thread.
namespace pg::custom::lithobolia {

bool resolve();
// The host: the ghost is about to throw `prop` (a PhotonObjectInteract); true when the Lithobolia
// throws it at someone itself or swallows it, for want of anyone in sight or while its last throw is still
// in flight (the game's throw is skipped).
bool intercept(void* ghost, void* prop);
// The host: the hunt is about to call Prop.TryThrow on `prop` (a Prop); true when the Lithobolia flies it
// at the hunt's target, or swallows the throw unless the ghost's sensor sees that target within range.
bool intercept_hunt_throw(void* ghost, void* prop);
// The host: throw the nearest loose object at a player in sight (the player ability); false if
// there is nothing to throw or nobody to throw it at.
bool throw_now(void* ghost);
// Every frame on the host: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pg::custom::lithobolia
