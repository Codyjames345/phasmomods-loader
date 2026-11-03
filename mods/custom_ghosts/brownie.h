#pragma once

#include <cstddef>
#include <cstdint>

// The Brownie (brownie_rule.h) in the game. The host works out every frame whether a living
// player sees it (their facing, range and line of sight) and whether a running video camera does;
// behaviour.cpp sets its hunt speed by the first, restraint.cpp holds back its interactions and events by
// either. The host says the first to every client (flame_net.h kSeen), and every client's hook on the
// game's footstep play gives a hunting Brownie's steps their rule by it (brownie_rule.h brownie_step): silent
// to players and equipment while nobody looks, the game's Myling rule while anyone does. Main thread.
namespace pm::custom::brownie {

bool resolve();
bool create_hooks();  // the footstep play hook; before hooks::enable_all
// Every frame on the host: ghost is the custom ghost's GhostAI (null for none), type its type. It runs for
// the ghost the host runs, whoever steers it, and says whether anyone looks at it to every client.
void tick_host(void* ghost, int type);
// Every frame on every client: records which ghost haunts, for the footstep hook; once no Brownie haunts,
// what the host last said goes with it.
void tick_all(void* ghost, int type);
bool available();  // true while the mod runs: its map entries are the mod's requirements
bool seen();     // a living investigator looks at it, or someone views it on the truck's monitor, now (the host's own)
bool watched();  // a player or a running video camera has it in view now
// A message on the mod's channel: the host's word on whether anyone looks at the Brownie (flame_net.h kSeen),
// taken only from the master client.
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pm::custom::brownie
