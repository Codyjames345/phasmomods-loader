#pragma once

#include <cstdint>

// Tables of C function pointers that cooperating mods publish to each other through the host
// (PmHostApi::register_service and ::service). A table stays valid for the process lifetime; its
// version changes whenever its layout does. Any mod may be absent: callers check for null.
extern "C" {

// "playable-ghost": what the Playable Ghost mod knows about who controls the ghost.
#define PM_SERVICE_PLAYABLE_GHOST "playable-ghost.v1"
struct PmPlayableGhostService {
  uint32_t version;  // 3
  // A player controls the ghost in this contract (the native AI only acts when they are restless).
  bool (*ghostControlled)(void);
  // Version 2. The Photon actor of the player who plays (or will play) the ghost; 0 for none.
  int32_t (*ghostActor)(void);
  // Version 3. The ghost's player takes the actions of the game's idle roll running now: each is granted to
  // them to use later instead of happening. False outside that roll, and while the roll acts on its own (they
  // have been idle a while, or the Tower card's boost runs).
  bool (*rollTaken)(void);
};

}
