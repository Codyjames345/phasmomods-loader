#pragma once

#include <cstddef>
#include <cstdint>

// The contract between ModHost.dll and each mod DLL. Every module links its own static C runtime,
// so only plain pointers, integers and C strings cross it, and nothing thrown may cross it: a mod
// catches its own exceptions. Every call in both directions is made on Unity's main thread, except
// `log`, which any thread may call.
extern "C" {

#define PGMOD_API_VERSION 1u

// Who a message is sent to.
enum PgTo : int32_t { PG_TO_OTHERS = 0, PG_TO_MASTER = 1, PG_TO_ACTOR = 2 };

// Mod payloads ride on Photon event 197 behind a one-byte channel; channel 0 is the host's own.
constexpr size_t PG_MAX_MESSAGE = 511;

// `sender` is the sending client's Photon actor number, taken from the event, never the payload.
typedef void (*PgReceiver)(int32_t sender, const uint8_t* data, size_t size);

struct PgHostApi {
  uint32_t version;              // PGMOD_API_VERSION
  void* gameAssembly;            // HMODULE, already verified against the audited build
  void* unityPlayer;             // HMODULE, already verified against the audited build
  const wchar_t* dataDirectory;  // %LOCALAPPDATA%\PhasmoMods, which exists

  void (*log)(const char* line);  // one line into the shared log (the mod names itself in it)
  // A MinHook detour through the host's single MinHook instance, not yet enabled. A target that is
  // already hooked (by the host or another mod) is chained: the newest detour runs first and its
  // `original` reaches the previous one.
  bool (*create_hook)(void* target, void* detour, void** original, const char* name);
  bool (*enable_hooks)(void);  // enable every hook created so far
  // A message on the mod's own channel (PgModInfo::channel; at most PG_MAX_MESSAGE bytes). `actor` is
  // used with PG_TO_ACTOR.
  bool (*send)(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor);
  void (*set_receiver)(uint8_t channel, PgReceiver receiver);  // null stops receiving
  // Whether the client of `actor` in this room runs the mod `modId` (its player having agreed to
  // the notice). The local actor answers for this client.
  bool (*peer_has_mod)(int32_t actor, const char* modId);
  // Named tables of C function pointers that cooperating mods publish for each other. A table must
  // stay valid for the process lifetime. service returns null for a name nobody registered.
  void (*register_service)(const char* name, const void* table);
  const void* (*service)(const char* name);
};

enum PgModFlags : uint32_t {
  PG_MOD_RECORDS_VOICE = 1,  // keeps other players' speech in memory during a contract
  // Players without this mod can share its lobby: it falls back to the vanilla game for them. A
  // modded host removes players without the mod unless every mod it runs has this flag.
  PG_MOD_VANILLA_PEERS = 2,
};

struct PgModInfo {
  uint32_t apiVersion;  // PGMOD_API_VERSION the mod was built against
  const char* id;       // stable, lower-case, e.g. "playable-ghost"
  const char* name;     // shown to the player, e.g. "Playable Ghost"
  uint8_t channel;      // its own network channel, 1..255, unique in the suite
  uint32_t flags;       // PgModFlags
};

// The exports each mod DLL provides.
typedef const PgModInfo* (*PgModInfoFn)(void);  // "pgmod_info": callable at any time
typedef bool (*PgModStartFn)(const PgHostApi*);  // "pgmod_start": once, after consent; false leaves it inactive
typedef void (*PgModTickFn)(void);               // "pgmod_tick": every frame once started
typedef void (*PgModStopFn)(void);               // "pgmod_stop": once, when the host stops
}
