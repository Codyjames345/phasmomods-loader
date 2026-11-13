#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

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
// One installed mod, copied out of the host contract's mod_entry (core/mod_api.h), so it outlives the next ask.
struct ModEntry {
  std::string id, name, version, author, offReason;
  uint32_t flags = 0;  // PmModEntryFlags bits
};
// Every installed mod, running or not, in the host's order (core/mod_api.h mod_entry). Empty before bind, and from a
// host older than contract 2.5.
std::vector<ModEntry> mod_entries();
/// @brief This mod's own folder for the files it keeps between games (settings, saved keys, records):
/// `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`, as SMAPI's DirectoryPath gives a mod a folder of its own.
///
/// The first ask after bind makes the folder. For a mod with a new-format id (`<author>.<mod>`) it also moves in, once,
/// the files the mod kept before in `%LOCALAPPDATA%\PhasmoMods\<mod id>\`, the folder mods built for themselves before
/// this helper (core/data_folder.h): each file the new folder lacks (sub-folders kept) is moved, and logged. One the new
/// folder already has stays in the old folder, and one that cannot be moved or copied stays too (a warning, tried again
/// next start); nothing is ever deleted that was not copied. The old folder goes once nothing is left in it; one that
/// keeps only files the new folder has is renamed `<mod id>.moved`, with one line in the log, so the move never runs
/// again and a kept file never comes back. An old-format id (a flat name such as `logs`) moves nothing, since it may
/// name one of the loader's own folders. Later asks answer the same folder at once.
/// @return The folder, which exists; empty before bind, when the host gives no data folder, and when the folder cannot
///   be made (logged). Build a file's path from it only when it is not empty, so nothing is written in the game's
///   working folder.
/// @pre Main thread, after bind (from pmmod_start on).
/// @since Contract 2.7
std::filesystem::path data_folder();
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
