#pragma once

#include <cstddef>
#include <cstdint>

// The contract between ModHost.dll and each mod DLL. Every module links its own static C runtime,
// so only plain pointers, integers and C strings cross it, and nothing thrown may cross it: a mod
// catches its own exceptions. Every call in both directions is made on Unity's main thread, except
// `log`, which any thread may call.
//
// Compatibility is versioned like a library. Within a major version the contract only grows:
// PgHostApi and every struct here gain members at the end and nothing is removed or changed, so a
// host accepts a mod built against its own major version and the same or an older minor version. A
// new major version refuses every older mod, and the host names those that need rebuilding.
extern "C" {

#define PGMOD_API_MAJOR 2u
#define PGMOD_API_MINOR 0u
// Major in the high 16 bits, minor in the low 16. Version 1 (the first contract) had no minor.
#define PGMOD_API_VERSION ((PGMOD_API_MAJOR << 16) | PGMOD_API_MINOR)

// Whether a host of version `host` runs a mod built against version `mod`: the same major version
// and a minor version no newer than the host's. Callable from any thread.
inline bool pg_api_compatible(uint32_t host, uint32_t mod) {
  return (host >> 16) == (mod >> 16) && (mod & 0xFFFFu) <= (host & 0xFFFFu);
}

// What a game entry (an entry of the game map, sdk/map) names in the game.
enum PgEntryKind : uint32_t {
  PG_ENTRY_TYPE = 1,    // a class, struct or enum
  PG_ENTRY_METHOD,      // a method, with its code
  PG_ENTRY_FIELD,       // a field, with its offset
  PG_ENTRY_ENUM_VALUE,  // one value of an enum
  PG_ENTRY_CODE,        // an address inside a method's code
};

// A game entry as the host resolved it in the running game. The host keeps it for the process
// lifetime. Members a kind does not use are zero. Every pointer is the runtime's metadata or code,
// never a managed object.
struct PgGameEntry {
  uint32_t kind;       // PgEntryKind
  void* klass;         // Il2CppClass*: the type itself, or the type that declares the member
  const void* method;  // MethodInfo* of a method, passed as a call's last argument
  void* code;          // a method's code (its canonical body), or a code entry's address
  // A method's same behaviour at other addresses (the obfuscator's twins), besides `code`.
  void* const* twins;
  uint32_t twinCount;
  int32_t offset;  // a field's offset from the start of the object, including the object header
  void* field;     // FieldInfo* of a field
  int64_t value;   // an enum value's number
};

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

  // Added in version 2.0. Mods reach these through the game API (sdk/api/game_api.h).

  // The game entry `key` names (its map key, e.g. "GhostAI.ChangeState"), or null when the map has
  // no such entry or it did not resolve in this game build.
  const PgGameEntry* (*game_entry)(const char* key);
  // A MinHook detour on the method entry `key` names, not yet enabled (enable_hooks enables it), and
  // chained like create_hook. `original` reaches the canonical body. With `allTwins` every twin of
  // the method is hooked as well, so the detour runs whichever body the game calls; otherwise only
  // the canonical body. False when the entry is unknown, unresolved or not a method, or when the
  // host cannot hook entries yet (it logs that).
  bool (*hook_entry)(const char* key, void* detour, void** original, const char* name, bool allTwins);
  // Stores the managed reference `value` into the managed object `obj` at `offset` through the
  // runtime's GC write barrier: the only correct way to write a reference field.
  void (*set_reference)(void* obj, int32_t offset, void* value);
  // How many calls of the method entry `key` names (any twin) are running on the calling thread, as
  // the host's hook on it counts them; 0 when the host does not track that entry.
  int32_t (*inside_entry)(const char* key);
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
// "pgmod_requires" (optional, version 2 onwards; the game API's glue exports it): the keys of the game
// entries the mod uses that are not marked optional. It points `keys` at an array the mod keeps for
// its lifetime and returns its length. Callable at any time. The host does not start a mod while any
// of these did not resolve, and logs which. A mod without this export requires no game entry.
typedef size_t (*PgModRequiresFn)(const char* const** keys);
}
