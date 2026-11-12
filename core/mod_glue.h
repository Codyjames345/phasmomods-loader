#pragma once

#include <string>

#include "mod_api.h"

// A mod's side of the host contract (core/mod_api.h). pmmod_start calls bind first; from then on the
// mod's log lines go to the host's log, pm::hooks goes through the host's MinHook, pm::net uses the
// mod's own channel, the game API (sdk/api/game_api.h) reaches the entries the host resolved, and
// the IL2CPP API is resolved for this module. Linking this also exports the mod's pmmod_requires.
// Main thread.
namespace pm::mod {

// False when `api` is not a compatible host contract (pm_api_compatible), when a game entry the mod
// requires did not resolve (logged with their keys), or when the IL2CPP API cannot be resolved.
bool bind(const PmHostApi* api, const PmModInfo* info);
const PmHostApi& host();
// Whether bind succeeded, so host() and info() may be asked.
bool bound();
const PmModInfo& info();

// Whether the client of `actor` is verified and the host runs the mod `id` (core/mod_api.h peer_has_mod). Deprecated, as
// peer_has_mod is: a mod has no need of it.
bool peer_has(int actor, const char* id);
// Whether the mod `id` has started on this client: for an optional dependency, from pmmod_start onwards (core/mod_api.h
// mod_started). False before bind.
bool mod_started(const char* id);
// This mod's name as the player reads it, its manifest's (core/mod_api.h mod_name), for a heading of its own in a game
// menu; PmModInfo::name when the host has none for it, and empty before bind.
std::string shown_name();
// A one-button message in the game's own popup on the menu's and the lobby's board (core/mod_api.h show_message): one asked
// for in a contract shows once the lobby is back. Nothing before bind.
void show_message(const std::string& title, const std::string& body);

// The bodies of a mod's pmmod_start, pmmod_tick and pmmod_stop, so that nothing thrown leaves the
// mod (a managed exception is logged with its stack). `name` names the mod in the log ("<name>
// active"); `running` is the mod's own flag: set by guarded_start, cleared by guarded_stop, and
// guarded_tick and guarded_stop do nothing while it is false.
bool guarded_start(const char* name, bool& running, bool (*start)(const PmHostApi*), const PmHostApi* api);
void guarded_tick(const bool& running, void (*tick)());
void guarded_stop(bool& running, void (*stop)());

}  // namespace pm::mod
