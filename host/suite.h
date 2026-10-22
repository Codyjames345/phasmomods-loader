#pragma once

// What the host does once the game is verified: show the consent notice, then run the chosen mods
// (and run them again with a new choice from the in-game Mods panel), tell modded peers which mods
// and builds run here, keep a modded lobby to players with the same mods (host/join_guard.h), and
// serve the mods' host API (core/mod_api.h).
namespace pg::suite {

bool resolve();       // worker thread, after game::resolve: bindings the host itself needs
bool create_hooks();  // worker thread, before hooks::enable_all
bool start(void* gameAssembly, void* unityPlayer);  // main thread, first frame
void tick();                                        // main thread, every frame after start
void stop();                                        // main thread

}  // namespace pg::suite
