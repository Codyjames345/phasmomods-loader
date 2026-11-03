#pragma once

#include <cstddef>
#include <cstdint>

// The flames in the game: every lit Firelight candle or lantern and every candelabra candle in the
// contract. The host works out each flame's blueness (flame_rules.h) and sends it to every client on
// the mod's channel; every client shows it (flame and light colour), burns a blue candle down
// faster, and leaves a flame that is more than half blue out of the Firelight sanity effect. The host
// also rolls the ghost's extra blow-out of a fully blue flame, and keeps a ghost that gives Spectral
// Flame from blowing out a flame set down before it is fully blue. Main thread.
namespace pm::flame::flames {

// Reads the ids of the shader properties a flame is painted by (one that did not resolve leaves what it
// colours as the game's, logged), and whether this build has every texture function the grey flame
// sprites need (else flames are tinted over the game's sprite), after the game API is bound.
void resolve();
bool create_hooks();  // before hooks::enable_all

// What the mod decided for one frame.
struct Frame {
  // The flames are on in the contract (contract.h flame_evidence). While false, every flame shows its
  // own colours again and nothing else changes.
  bool enabled = false;
  int32_t evidence = -1;  // Spectral Flame's game value in this contract
  uint64_t now = 0;       // the time in milliseconds (GetTickCount64)
};

void tick(const Frame& frame);  // every frame

// The host's message (flame_rules.h encode), accepted from the master client only.
void receive(int sender, const uint8_t* data, size_t size);

void teardown();  // every flame back to its own colours

}  // namespace pm::flame::flames
