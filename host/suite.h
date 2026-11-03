#pragma once

// What the host does once the map has resolved: show the consent notice, then run the chosen mods
// (and run them again with a new choice from the in-game Mods panel), tell modded peers which mods
// and builds run here, keep a modded lobby to players with the same mods (host/join_guard.h), and
// serve the mods' host API (core/mod_api.h). Only the frame hook (host/main.cpp) comes before the
// player's answer: everything else this hooks waits for "I agree".
#include "build_record.h"
#include "content_registry.h"

namespace pm::suite {

// Main thread, first frame. `game`: the running game's library hashes (host/build_check.h).
bool start(void* gameAssembly, void* unityPlayer, const build::Hashes& game);
// Main thread, every frame after start. False once no mod can run this session and nothing is left to tell the player
// ("Play without mods", or no mod to agree to): the caller stops the suite and takes the frame hook off.
bool tick();
void stop();  // main thread

// Where custom content stands on this client (content::State): Active (this client is the host, or matches it), Off (the
// host lacks the content) or Pending (the host is not heard from yet, or this client's mods do not match its list). The host's
// own content code treats Pending as not active: the lobby offers no custom ghost and shows none in its journals, and a
// contract that loads then has none. Main thread.
content::State content_state();

}  // namespace pm::suite
