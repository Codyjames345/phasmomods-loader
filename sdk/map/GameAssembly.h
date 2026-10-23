#pragma once

// GameAssembly.dll's own exports: the IL2CPP runtime's API. Only the host uses them, through
// pg::api::Export (sdk/api/game_api.h, defined only in host code), so this header has no phasmo::
// wrappers. They resolve from the module's export table, in the game and offline alike.

#include "entry.h"

namespace pg::map {

/// The IL2CPP runtime's shutdown, which the game's quit calls. It waits for every thread attached to
/// the runtime, Photon's receive thread among them, so Playable Ghost hooks it to disconnect Photon
/// and cancel that thread's pending receive first (mods/playable_ghost/leave.h). Inferred: the IL2CPP
/// runtime's API and host/main.cpp's note.
inline constexpr pg::sdk::EntryDesc GameAssembly_il2cpp_shutdown{
    .key = "GameAssembly.il2cpp_shutdown", .kind = pg::sdk::Kind::Export, .name = "il2cpp_shutdown"};

}  // namespace pg::map
