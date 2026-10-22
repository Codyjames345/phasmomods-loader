#pragma once

#include <cstdint>

// Tables of C function pointers that cooperating mods publish to each other through the host
// (PgHostApi::register_service and ::service). A table stays valid for the process lifetime; its
// version changes whenever its layout does. Any mod may be absent: callers check for null.
extern "C" {

// "playable-ghost": what the Playable Ghost mod knows about who controls the ghost.
#define PG_SERVICE_PLAYABLE_GHOST "playable-ghost.v1"
struct PgPlayableGhostService {
  uint32_t version;  // 2
  // A player controls the ghost in this contract (the native AI only acts when they are restless).
  bool (*ghostControlled)(void);
  // Version 2. The Photon actor of the player who plays (or will play) the ghost; 0 for none.
  int32_t (*ghostActor)(void);
};

// "spectral-flame": whether the Spectral Flame evidence (evidence value 8) can be given, so a mod
// adding ghost types that carry it only does so when every player can find it.
#define PG_SERVICE_SPECTRAL_FLAME "spectral-flame.v1"
struct PgSpectralFlameService {
  uint32_t version;  // 1
  // This is a private room in which every client runs Spectral Flame (the flames, their sync and the
  // journal's evidence row work for everyone).
  bool (*ready)(void);
};

// "custom-ghosts": the custom ghost types' own abilities, for a player controlling one.
#define PG_SERVICE_CUSTOM_GHOSTS "custom-ghosts.v1"
struct PgCustomGhostsService {
  uint32_t version;  // 4
  // The ability of a ghost type, by its GhostType; null when it has none (or is not a custom type).
  const char* (*abilityName)(int type);
  const char* (*abilityDescription)(int type);
  // Host only: use the custom ghost haunting this contract's ability now; false if it could not.
  bool (*useAbility)(int type);
  // Version 2. Host only: the custom ghost haunting this contract's own state as its player feels it,
  // 0..255 (the Echo: 1 while dormant; the Dullahan: its speed in percent of the game's base while hunting, else 0; the
  // Draugr: 1 while something taken from its room is outside it; the Kikimora: 1 while the lights are
  // on in its room); -1 for none.
  int (*sense)(int type);
  // Version 3. Host only: the custom ghost's own rules keep it from acting now (kind 0: interact with
  // something, 1: cause an event), so a player controlling it may not either.
  bool (*heldBack)(int type, int kind);
  // Version 4. Host only: the freshest points of the scent trails the Black Shuck follows in a hunt, as
  // x, y, z triples in `xyz` (room for maxPoints); returns how many were written.
  int (*scent)(float* xyz, int maxPoints);
};

// "ghost-tells": the new tells of six vanilla ghosts, for a player controlling one of them.
#define PG_SERVICE_GHOST_TELLS "ghost-tells.v1"
struct PgGhostTellsService {
  uint32_t version;  // 1
  // Host only: the tells' rules keep the ghost haunting this contract from acting now (kind 0:
  // interact with something, the fuse box included; 1: cause an event or use its ability; 2: hunt), so a
  // player controlling it may not either.
  bool (*heldBack)(int kind);
};

}
