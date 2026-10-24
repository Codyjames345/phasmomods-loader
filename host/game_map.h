#pragma once

#include <cstdint>
#include <string_view>

#include "resolver.h"

struct PgGameEntry;

namespace pg::sdk {
struct SceneEngine;
}

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

// Resolves the map's scene facts and publishes them, with `engine` asking the running game
// (host/scene_engine.h). On the main thread at host activation, after resolve_icalls and before any mod
// starts: a layer resolves by its name through LayerMask.NameToLayer, and is logged when no layer has
// it; a well-formed Scene or AnimatorParam entry is published as resolved and checked where a mod
// uses it (scene_find, animator_param). Logs the count of layers resolved. Nothing thrown escapes.
void resolve_scene_facts(const sdk::SceneEngine* engine);

// Releases what the scene facts keep (the objects found, the roots and Animators asked about). Main
// thread, when the host stops; the facts are found again on their next use.
void forget_scene_facts();

// The host contract's scene_find and animator_param (core/mod_api.h) for the resolved Scene or
// AnimatorParam entry `key`: null or false for any other key, and before resolve_scene_facts. Main
// thread.
void* scene_find(const char* key, void* root);
bool animator_param(const char* key, const void* animator, int32_t* hash);

// An entry as the host has it, or null when it is unknown or unresolved. Host code only: it includes
// GameAssembly's exports.
const PgGameEntry* entry(const char* key);

// An entry as the host contract gives it to mods: as entry, but null for an export, which only the
// host may use. A mod that requires an export is therefore never started.
const PgGameEntry* mod_entry(const char* key);

}  // namespace pg::gamemap
