#pragma once

#include <cstddef>
#include <cstdint>

// The Echo's voice. Every client keeps the last minute of what each other investigator said, from
// the voice audio Vivox hands the game for them, but only while an Echo's rules run. A contract only begins once every player's
// mods are confirmed (sdk/README.md, "The lobby's rules"): every player's client then runs this mod, so only people who agreed
// to the notice are ever recorded, and this mod checks nothing itself. When the Echo speaks, a
// client that heard the chosen investigator sends their last phrase to everyone as telephone-quality
// audio, and every client plays it from where the ghost is. The Echo's weakness: with nobody heard
// near it for a minute it falls quiet, and most of its interactions and events do not happen.
// Nothing is written to disk; recordings are dropped when the contract ends. It also repeats what players
// do (echo_rule.h): the host notes each player's use of a light switch or a door in its favourite room, and
// within a minute its next interaction, its AI's or a ghost player's, once its interaction sensor detects that
// switch or door uses it.
// Main thread.
namespace pm::custom::echo {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void receive(int sender, const uint8_t* data, size_t size);

// Every frame. ghost: the Echo's GhostAI on the host (null otherwise, and on other clients);
// recording: the contract's ghost acts as an Echo, so its rules run on this client.
void tick(void* ghost, bool steered, bool recording);
// The Echo speaks now with an investigator's voice (actor 0: one it heard recently). Host only.
bool speak(void* ghost, int actor);
void reset();
bool dormant_now();  // host: the Echo has heard nobody near it for a minute
// The Echo's interaction on the host, its AI's roll or a ghost player's aimed interaction alike (restraint.cpp,
// echo_rule.h echo_tries_repeat): true when it used the switch or door a player last used
// in its room, within a minute, through the game's own light or door interaction (the game's interaction is
// then skipped). It does so only once the ghost's interaction sensor detects that switch or door; until then
// the use waits. With a use waiting, each call pulses that sensor where the ghost stands now, as TryInteract
// does, with the ghost's own interaction (the sensor's OnPulsed delegate) set aside, so only its detections
// change: a dormant Echo, which skips most of the game's TryInteracts, still sees what is in reach.
bool repeat(void* ghost);

}  // namespace pm::custom::echo
