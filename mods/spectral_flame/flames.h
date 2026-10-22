#pragma once

#include <cstddef>
#include <cstdint>

// The flames in the game: every lit Firelight candle or lantern and every candelabra candle in the
// contract. The host works out each flame's blueness (flame_rules.h) and sends it to every client on
// the mod's channel; every client shows it (flame and light colour), burns a blue candle down
// faster, and leaves a flame that is more than half blue out of the Firelight sanity effect. The host
// also rolls the ghost's extra blow-out of a fully blue flame, and keeps a ghost that gives Spectral
// Flame from blowing out a flame set down before it is fully blue. Main thread.
namespace pg::flame::flames {

bool resolve();
bool create_hooks();  // before hooks::enable_all

// Every frame. `enabled`: every client runs Spectral Flame and a ghost type in the contract carries
// it. While false, every flame shows its own colours again and nothing else changes.
void tick(bool enabled);

// The host's message (flame_rules.h encode), accepted from the master client only.
void receive(int sender, const uint8_t* data, size_t size);

void teardown();  // every flame back to its own colours

}  // namespace pg::flame::flames
