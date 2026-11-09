#include <windows.h>

#include "log.h"
#include "mod_glue.h"

// Emotes as a mod of the suite (core/mod_api.h): a wheel of eight dances on a key the player rebinds in the game's own
// keybinds menu; each plays on the player's own body with its song, seen and heard by every player, heard by the ghost
// as the player talking, with an eased third-person view on screen
// (docs/superpowers/specs/2026-11-10-emotes-design.md). The loader decides that every player in the lobby runs it; this
// mod keeps no such check of its own.
namespace {

constexpr const char* kId = "phasmomods.emotes";
constexpr PmModInfo kInfo{PMMOD_API_VERSION, kId, "Emotes", 7, 0};

bool g_running = false;

bool start(const PmHostApi* api) { return pm::mod::bind(api, &kInfo); }

void tick() {}

}  // namespace

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("Emotes", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pmmod_tick() { pm::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pmmod_stop() { pm::mod::guarded_stop(g_running, [] {}); }
