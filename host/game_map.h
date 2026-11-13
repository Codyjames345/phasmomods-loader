#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "content_ids.h"
#include "resolver.h"

struct PmGameEntry;

namespace pm::sdk {
struct SceneEngine;
}

// The game map (sdk/map) resolved against the running game, once at startup.
namespace pm::gamemap {

// What resolution leaves of the startup deadline for the stages after it (the hooks).
inline constexpr unsigned long long kHooksBudgetMs = 10000;

// Builds the game model from the runtime for the assemblies the map names, resolves every entry
// but the icalls (left to resolve_icalls) against it with the loaded GameAssembly's code, and the
// native sites in the loaded UnityPlayer's code (each stays missing, naming it, when that cannot be
// read), and logs each entry that did not resolve and the summary
// "game map: <r>/<n> resolved in <ms> ms". Runs on the attached worker thread after il2cpp::init.
// Between types and before resolving it checks the stop event and the deadline, less kHooksBudgetMs:
// when either says stop, or no model can be built, it abandons the map (every entry unresolved, the
// reason logged once) and returns false. Nothing thrown escapes. It runs once: mods keep the
// published entries for the process lifetime, so a second call is refused (logged), returns false
// and leaves every entry as published.
bool resolve(void* gameAssembly, void* unityPlayer, void* stopEvent, unsigned long long deadline);

// The digest of the map this host holds (sdk/resolver/map_digest.h), which a checked-build record names.
std::string map_digest();

// An entry's result by key, or null before resolve or for an unknown key.
const sdk::Result* result(std::string_view key);

// Resolves the map's icall entries through the runtime and publishes them. On the main thread at
// host activation, before any mod starts: the worker that runs resolve never asks for an icall,
// since the runtime's icall table is not locked and the engine's registration may not be done.
// Logs each icall that still did not resolve, and the count. Nothing thrown escapes.
void resolve_icalls();

// Makes the map's Inflated entries (a method of a generic instance) in the running game and publishes them. On the
// main thread at host activation, after resolve_icalls and before any mod starts: it runs the game's managed
// reflection, which no other time may. An instance this game build has no code for stays unresolved, as does one the
// runtime will not make. Logs each one that did not resolve, and the count. Nothing thrown escapes.
void resolve_inflated();

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

// Reads the running game's GhostType and EvidenceType enums whole, every value and its name, into `out`
// for content numbering (sdk/runtime/game_enums.h). Returns why they cannot be read, or empty. Main
// thread, after resolve.
std::string read_content_enums(content::GameEnums& out);

// An entry as the host has it, or null when it is unknown or unresolved. Host code only: it includes
// GameAssembly's exports.
const PmGameEntry* entry(const char* key);

// An entry as the host contract gives it to mods: as entry, but null for an export or a native site,
// which only the host may use. A mod that requires one is therefore never started.
const PmGameEntry* mod_entry(const char* key);

// The Il2CppClass* an object made for `key` is of: a resolved Type entry's class, or a resolved Inflated entry's
// generic instance (List<UInt32> for List<UInt32>.ctor). Null for any other key or one unresolved. Main thread.
void* instance_class(const char* key);

}  // namespace pm::gamemap
