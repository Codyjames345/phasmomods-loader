#pragma once

// The Goryo's tells (goryo_rule.h) in the game. The host decides when, in its room with nobody near and
// a running video camera on it, it stands visible to the camera alone, and every client shows that
// figure: a copy of its model on the game's camera-only layer (LaserGhost). The host also gives it
// extra interactions there, and in a hunt sends it home when it has lost everyone far from its room.
// Main thread.
namespace pm::tells::goryo {

void resolve();
// Every frame on the host while the Goryo's rules run: the ghost's GhostAI.
void tick_host(void* ghost, float dt);
// Every frame on every client while the Goryo's rules run.
void tick_all(void* ghost);
void receive_show(bool on);  // the host's word, on every other client
void reset();

}  // namespace pm::tells::goryo
