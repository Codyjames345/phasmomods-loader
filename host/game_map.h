#pragma once

#include <string_view>

#include "resolver.h"

struct PgGameEntry;

// The game map (sdk/map) resolved against the running game, once at startup.
namespace pg::gamemap {

// What resolution leaves of the startup deadline for the stages after it (the hooks).
inline constexpr unsigned long long kHooksBudgetMs = 10000;

// Builds the game model from the runtime for the assemblies the map names, resolves every entry
// against it with the loaded module's code, and logs each entry that did not resolve and the summary
// "game map: <r>/<n> resolved in <ms> ms". Runs on the attached worker thread after il2cpp::init.
// Between types and before resolving it checks the stop event and the deadline, less kHooksBudgetMs:
// when either says stop, or no model can be built, it abandons the map (every entry unresolved, the
// reason logged once) and returns false. Nothing thrown escapes.
bool resolve(void* gameAssembly, void* stopEvent, unsigned long long deadline);

// An entry's result by key, or null before resolve or for an unknown key.
const sdk::Result* result(std::string_view key);

// An entry as the host contract gives it to mods, or null when it is unknown or unresolved.
const PgGameEntry* entry(const char* key);

}  // namespace pg::gamemap
