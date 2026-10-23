#pragma once

#include <string_view>

#include "resolver.h"

struct PgGameEntry;

// The game map (sdk/map) resolved against the running game, once at startup.
namespace pg::gamemap {

// Builds the game model from the runtime for the assemblies the map names, resolves every entry
// against it with the loaded module's code, and logs each entry that did not resolve and the summary
// "game map: <r>/<n> resolved in <ms> ms". Runs on the attached worker thread after il2cpp::init.
// False when no model could be built (every entry then stays unresolved); nothing thrown escapes.
bool resolve(void* gameAssembly);

// An entry's result by key, or null before resolve or for an unknown key.
const sdk::Result* result(std::string_view key);

// An entry as the host contract gives it to mods, or null when it is unknown or unresolved.
const PgGameEntry* entry(const char* key);

}  // namespace pg::gamemap
