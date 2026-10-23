#pragma once

#include <string_view>

#include "resolver.h"

struct PgGameEntry;

// The game map (sdk/map) resolved against the running game, once at startup.
namespace pg::gamemap {

// What resolution leaves of the startup deadline for the stages after it (the hooks).
inline constexpr unsigned long long kHooksBudgetMs = 10000;

// Builds the game model from the runtime for the assemblies the map names, resolves every entry
// but the icalls (left to resolve_icalls) against it with the loaded module's code, and logs each entry that did not resolve and the summary
// "game map: <r>/<n> resolved in <ms> ms". Runs on the attached worker thread after il2cpp::init.
// Between types and before resolving it checks the stop event and the deadline, less kHooksBudgetMs:
// when either says stop, or no model can be built, it abandons the map (every entry unresolved, the
// reason logged once) and returns false. Nothing thrown escapes. It runs once: mods keep the
// published entries for the process lifetime, so a second call is refused (logged), returns false
// and leaves every entry as published.
bool resolve(void* gameAssembly, void* stopEvent, unsigned long long deadline);

// An entry's result by key, or null before resolve or for an unknown key.
const sdk::Result* result(std::string_view key);

// Resolves the map's icall entries through the runtime and publishes them. On the main thread at
// host activation, before any mod starts: the worker that runs resolve never asks for an icall,
// since the runtime's icall table is not locked and the engine's registration may not be done.
// Logs each icall that still did not resolve, and the count. Nothing thrown escapes.
void resolve_icalls();

// An entry as the host has it, or null when it is unknown or unresolved. Host code only: it includes
// GameAssembly's exports.
const PgGameEntry* entry(const char* key);

// An entry as the host contract gives it to mods: as entry, but null for an export, which only the
// host may use. A mod that requires an export is therefore never started.
const PgGameEntry* mod_entry(const char* key);

}  // namespace pg::gamemap
