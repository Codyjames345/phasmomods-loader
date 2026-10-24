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
#define PGMOD_API_MINOR 1u
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
  // Added in version 2.1.
  PG_ENTRY_ICALL,       // a Unity engine function the runtime registered by name, with its code
  PG_ENTRY_EXPORT,      // a function GameAssembly.dll exports; host only, game_entry never gives one
  PG_ENTRY_SCENE,       // a GameObject by its path, found through scene_find
  PG_ENTRY_LAYER,       // a layer, with its index in `value`
  PG_ENTRY_ANIMATOR_PARAM,  // an Animator parameter by its name, checked through animator_param
  PG_ENTRY_NATIVE_SITE,     // a place in UnityPlayer.dll's code, its address in `code`; see patch_site
};

// A game entry as the host resolved it in the running game. The host keeps it for the process
// lifetime. Members a kind does not use are zero. Every pointer is the runtime's metadata or code,
// never a managed object.
struct PgGameEntry {
  uint32_t kind;       // PgEntryKind
  void* klass;         // Il2CppClass*: the type itself, or the type that declares the member
  const void* method;  // MethodInfo* of a method, passed as a call's last argument
  // A method's code (its canonical body), a code entry's address, an icall's or export's code, or a
  // native site's address in UnityPlayer.dll.
  void* code;
  // A method's same behaviour at other addresses (the obfuscator's twins), besides `code`.
  void* const* twins;
  uint32_t twinCount;
  // A field's offset from the start of the object, including the object header; a static field's
  // from the start of its class's static data.
  int32_t offset;
  void* field;    // FieldInfo* of a field, static or not
  int64_t value;  // an enum value's number, or a layer's index (0 to 31)
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
  // stay valid for the process lifetime. A name is published once: registering a name already
  // registered is refused (logged) and the first table stays. service returns null for a name
  // nobody registered.
  void (*register_service)(const char* name, const void* table);
  const void* (*service)(const char* name);

  // Added in version 2.0. Mods reach these through the game API (sdk/api/game_api.h).

  // The game entry `key` names (its map key, e.g. "GhostAI.ChangeState"), or null when the map has
  // no such entry, it did not resolve in this game build, or it is a GameAssembly export (only the
  // host may use one, so a mod that requires an export is never started).
  const PgGameEntry* (*game_entry)(const char* key);
  // A MinHook detour on the method entry `key` names, not yet enabled (enable_hooks enables it).
  // Without `allTwins` it is hook_body on body 0, the canonical body. With `allTwins` the one
  // `detour` is hooked on every body, each in that body's own chain, and `original` receives body
  // 0's: a call that entered through a twin runs this detour and then continues on the canonical
  // body's chain, skipping the detours older than this one on the twin and running those on the
  // canonical body instead. Twins are the same behaviour, so the game is unaffected, but mods may
  // be: use hook_body (version 2.1), which keeps every call on its own body's chain. Nothing is
  // hooked unless every body is. False when the entry is unknown, unresolved or not a method, or when
  // the host cannot hook it (it logs that).
  bool (*hook_entry)(const char* key, void* detour, void** original, const char* name, bool allTwins);
  // Stores the managed reference `value` into the managed object `obj` at `offset` through the
  // runtime's GC write barrier: the only correct way to write a reference field.
  void (*set_reference)(void* obj, int32_t offset, void* value);
  // How many calls of the method entry `key` names (any twin) are running on the calling thread, as
  // the host's hook on it counts them; 0 when the host does not track that entry.
  int32_t (*inside_entry)(const char* key);

  // Added in version 2.1: the runtime services. Only the host knows the runtime's object layouts;
  // mods reach managed strings, arrays, lists and objects through these (sdk/api/runtime.h). Every
  // one runs on the main thread, never throws, and answers null, 0 or false for a null argument and
  // for a key that is unknown, unresolved or of the wrong kind. `typeKey` and `methodKey` are map
  // keys of a Type and a Method entry. The services that read a managed object check its kind
  // first: one given an object of the wrong kind reads nothing from it, logs that once, and answers
  // 0 or null.

  // A new managed string (System.String) holding the UTF-8 text `utf8`.
  void* (*string_new)(const char* utf8);
  // Copies the managed string `str` (a System.String) into `out` as NUL-terminated UTF-8, `size`
  // bytes including the NUL, cut at a character boundary (a surrogate pair is one character; an
  // unpaired one becomes U+FFFD). Returns the whole string's length in UTF-8 bytes without the NUL,
  // so a caller can size its buffer. A null `out` is taken as `size` 0. A null `str` is the empty
  // string; anything that is not a System.String answers 0 (logged once).
  size_t (*string_utf8)(const void* str, char* out, size_t size);
  // The number of elements of the managed array `arr` (any T[]); anything else answers 0 (logged
  // once).
  size_t (*array_length)(const void* arr);
  // The address of element 0 of the managed array `arr`; elements follow at their type's size.
  // Anything that is not an array answers null (logged once). The address is valid only until the
  // next managed call: never store it.
  void* (*array_data)(void* arr);
  // A new managed array of `length` elements of the type `elementTypeKey` names, zeroed.
  void* (*array_new)(const char* elementTypeKey, size_t length);
  // The number of items in the managed System.Collections.Generic.List<T> `list`; anything else
  // answers 0 (logged once).
  int32_t (*list_count)(const void* list);
  // The managed array behind the List<T> `list` (its capacity may exceed list_count); anything else
  // answers null (logged once). The List replaces this array when it grows, so it is valid only
  // until the next managed call: take it again after any game call, and never store it.
  void* (*list_items)(const void* list);
  // The System.Type object of the type `typeKey` names (C#'s typeof).
  void* (*type_object)(const char* typeKey);
  // A new managed object of the class `typeKey` names, its fields zeroed. No constructor runs.
  void* (*object_new)(const char* typeKey);
  // Whether the managed object `obj` is of the type `typeKey` names or of a class derived from it
  // (following the class's parents).
  bool (*is_instance)(const void* obj, const char* typeKey);
  // UnityEngine.Object's implicit bool: false for null and for an engine object that was destroyed.
  bool (*alive)(const void* unityObject);
  // The method the managed object `obj` runs for the method entry `methodKey` (a C# virtual call):
  // the override in obj's class, or the declared method when nothing overrides it or the method is
  // not virtual. Null when `obj` is null or not of the entry's declaring type. The result is a
  // MethodInfo*, passed as the call's last argument, and its first member is the address of its
  // code.
  const void* (*virtual_method)(const void* obj, const char* methodKey);
  // A strong GC handle on the managed object `obj` (0 for null): the runtime keeps the object while
  // native code keeps a pointer to it. Release it when the pointer is dropped.
  uint32_t (*gc_hold)(void* obj);
  // Releases a handle from gc_hold; 0 does nothing.
  void (*gc_release)(uint32_t handle);
  // Logs a caught managed exception (the first member of the thrown Il2CppExceptionWrapper): its
  // class, message and stack trace, each line beginning with `where`.
  void (*log_exception)(const char* where, const void* exception);

  // Also added in version 2.1: static fields. `key` is the map key of a Field entry flagged static
  // (kStatic). Reading or writing a static field first runs its class's static constructor when that
  // has not run yet, as the game's own first use would, so call these only from code that runs while
  // the game plays (a tick, a detour), on the main thread; the host never touches a static field
  // while it resolves the map at startup. Each answers false without reading or writing the field
  // for a null pointer, for a key that is unknown, unresolved or not a static field, when `size` is
  // not the size of the field's value (a pointer's for a managed reference; this is checked before
  // the class is initialised, so a refused call runs no static constructor), and when the class's
  // initialisation throws (logged once per key).

  // Copies the static field's value, `size` bytes, into `out`. For a managed reference `out`
  // receives the pointer.
  bool (*static_get)(const char* key, void* out, size_t size);
  // Stores the `size` bytes at `in` into the static field. For a managed reference `in` points at
  // the pointer, which is stored through the runtime's GC write barrier.
  bool (*static_set)(const char* key, const void* in, size_t size);

  // Also added in version 2.1: hooks per body. A method's bodies are its canonical body (index 0)
  // and then each distinct twin (PgGameEntry::twins, less any folded onto an earlier body), in order.
  // Each body has its own chain: the newest detour on a body runs first for the calls that enter
  // through that body, and its `original` reaches the next detour of that body's chain, or the body
  // itself. A detour that serves several bodies must therefore be a separate function per body, so it
  // knows which body's `original` to call (the game API's hook<M> makes one per body).

  // A MinHook detour on body `bodyIndex` of the method entry `key`, not yet enabled (enable_hooks
  // enables it), chained on that body alone. False when the entry is unknown, unresolved or not a
  // method, when `bodyIndex` is not below body_count, or when the host cannot hook it (it logs that).
  bool (*hook_body)(const char* key, uint32_t bodyIndex, void* detour, void** original, const char* name);
  // How many bodies the method entry `key` has: 1 for a method without twins; 0 when the entry is
  // unknown, unresolved or not a method.
  uint32_t (*body_count)(const char* key);

  // Also added in version 2.1: scene and asset facts, the entries of kind PG_ENTRY_SCENE and
  // PG_ENTRY_ANIMATOR_PARAM (a layer is resolved when the host activates, and game_entry gives its
  // index). Main thread, while the game plays. A fact that is missing where it is used is logged once
  // per scene load, and the call answers null or false, so the feature that needs it stays off.

  // The Transform at the Scene entry `key`'s path under the Transform `root`, as Transform.Find finds
  // it. One answer is kept per entry, for the last `root`, until the next scene load: the same `root`
  // costs no search, another one (or the object found being destroyed) searches again. Null when the
  // entry is unknown, unresolved or not a Scene entry, when `root` is null (at once, the kept answer
  // untouched), destroyed or not a Transform, and when nothing is at the path.
  void* (*scene_find)(const char* key, void* root);
  // Whether the Animator `animator` has the parameter the AnimatorParam entry `key` names, read from
  // the Animator's own parameter list the first time each Animator is asked (and again after a scene
  // load); its hash (Animator.StringToHash) into `hash` when it has. False, `hash` untouched, when the
  // entry is unknown, unresolved or not an AnimatorParam entry, when `animator` is null, destroyed or
  // not an Animator, or when it lacks the parameter.
  bool (*animator_param)(const char* key, const void* animator, int32_t* hash);

  // Also added in version 2.1: native sites, the entries of kind PG_ENTRY_NATIVE_SITE.

  // Patches the native site `key` names, on the main thread while the mod starts. When the
  // `expectedSize` bytes there equal `expected` (5 to 32 bytes, whole instructions), the host copies
  // the `stubSize` bytes of `stub` to the start of a page of its own within rel32 range of the site,
  // follows them with a jump back to the instruction after the replaced bytes, and writes a 5-byte
  // `jmp` to the page over the site, the rest of the replaced bytes as NOPs. The stub must perform
  // whatever of the replaced instructions it still needs, and position-independent code is required:
  // it runs at the page's address. It resumes the game by running off its own end, or by jumping to
  // page + stubSize. A stub that keeps data must place it after a jump that ends its code (to
  // page + stubSize, or wherever it means to go), never where the code could run off into it. `page`
  // receives the stub's first byte: the page stays readable, writable and executable for the process
  // lifetime and is never freed (a thread may still be running the stub), so the mod may keep the
  // stub's data there and write it from any thread. The patch belongs to the
  // mod like its hooks: the site's own bytes are written back when the mod fails to start and when
  // the mods restart (the page stays); a restore the host has to refuse leaves the patch in place,
  // and the host tries again at the next restart of the mods and at shutdown. Every patch takes a new page, one 64 KB reservation, so patching
  // again after a rollback takes another, and no page is ever freed. The host stops every other thread
  // of the process while it writes or restores the site, and refuses (nothing written) when one keeps
  // stopping inside the replaced bytes. False, `page` null and nothing written, when the entry is
  // unknown, unresolved or not a native site, when the bytes there differ from `expected` (another
  // game build), when `expected` is not whole instructions or not 5 to 32 bytes, when the stub is
  // empty or longer than 4082 bytes (a page less the jump back), when the site is already patched or
  // hooked, when no page can be had within rel32 range, or when the write fails; the host logs which.
  bool (*patch_site)(const char* key, const void* expected, size_t expectedSize, const void* stub, size_t stubSize,
                     void** page);
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
// entries the mod uses that are not marked optional. It points `keys` at an array the mod keeps,
// unchanged, for as long as its DLL stays loaded, and returns its length. Callable at any time. The
// host does not start a mod while any of these did not resolve, and logs which. A mod without this
// export requires no game entry.
typedef size_t (*PgModRequiresFn)(const char* const** keys);
}
