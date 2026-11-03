#pragma once

#include <cstddef>
#include <cstdint>

// The contract between ModHost.dll and each mod DLL. Every module links its own static C runtime,
// so only plain pointers, integers and C strings cross it, and nothing thrown may cross it: a mod
// catches its own exceptions. Every call in both directions is made on Unity's main thread, except
// `log`, which any thread may call.
//
// Compatibility is versioned like a library. Within a major version the contract only grows:
// PmHostApi and every struct here gain members at the end and nothing is removed or changed, so a
// host accepts a mod built against its own major version and the same or an older minor version. A
// new major version refuses every older mod, and the host names those that need rebuilding.
extern "C" {

#define PMMOD_API_MAJOR 2u
#define PMMOD_API_MINOR 2u
// Major in the high 16 bits, minor in the low 16. Version 1 (the first contract) had no minor.
#define PMMOD_API_VERSION ((PMMOD_API_MAJOR << 16) | PMMOD_API_MINOR)

// Whether a host of version `host` runs a mod built against version `mod`: the same major version
// and a minor version no newer than the host's. Callable from any thread.
inline bool pm_api_compatible(uint32_t host, uint32_t mod) {
  return (host >> 16) == (mod >> 16) && (mod & 0xFFFFu) <= (host & 0xFFFFu);  // pm:not-game-data: the minor version
}

// What a game entry (an entry of the game map, sdk/map) names in the game.
enum PmEntryKind : uint32_t {
  PM_ENTRY_TYPE = 1,    // a class, struct or enum
  PM_ENTRY_METHOD,      // a method, with its code
  PM_ENTRY_FIELD,       // a field, with its offset
  PM_ENTRY_ENUM_VALUE,  // one value of an enum
  PM_ENTRY_CODE,        // an address inside a method's code
  // Added in version 2.1.
  PM_ENTRY_ICALL,       // a Unity engine function the runtime registered by name, with its code
  PM_ENTRY_EXPORT,      // a function GameAssembly.dll exports; host only, game_entry never gives one
  PM_ENTRY_SCENE,       // a GameObject by its path, found through scene_find
  PM_ENTRY_LAYER,       // a layer, with its index in `value`
  PM_ENTRY_ANIMATOR_PARAM,  // an Animator parameter by its name, checked through animator_param
  PM_ENTRY_NATIVE_SITE,     // a place in UnityPlayer.dll's code; host only, game_entry never gives one (steer_agent)
  // Added in version 2.2.
  PM_ENTRY_SHADER_PROPERTY,  // a shader property by its name, with its id (Shader.PropertyToID) in `value`
};

// A game entry as the host resolved it in the running game. The host keeps it for the process
// lifetime. Members a kind does not use are zero. Every pointer is the runtime's metadata or code,
// never a managed object.
struct PmGameEntry {
  uint32_t kind;       // PmEntryKind
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
  int64_t value;  // an enum value's number, a layer's index (0 to 31) or a shader property's id
};

// Who a message is sent to.
enum PmTo : int32_t { PM_TO_OTHERS = 0, PM_TO_MASTER = 1, PM_TO_ACTOR = 2 };

// Mod payloads ride on Photon event 197 behind a one-byte channel; channel 0 is the host's own.
constexpr size_t PM_MAX_MESSAGE = 511;

// `sender` is the sending client's Photon actor number, taken from the event, never the payload.
typedef void (*PmReceiver)(int32_t sender, const uint8_t* data, size_t size);

// Added in version 2.2: content, the ghosts and evidence mods add to the game (sdk/api/content.h).
// The host copies everything a definition holds when it is registered, except `user` and the
// callbacks, which must stay valid while the mod runs. Each definition begins with its own size
// (sizeof), so it can grow at the end like PmHostApi: the host reads no member past `size`.

// One piece of text in one language. `lang` is a language code of the game's own (e.g. "en"); `text`
// is UTF-8, at most 64 KB. Neither may be null.
typedef struct PmText {
  const char* lang;
  const char* text;
} PmText;
// A piece of text in several languages (at most 64), each named once. English ("en") is required
// wherever text is given; a language the game asks for and the set lacks falls back to English.
typedef struct PmTextSet {
  const PmText* items;
  uint32_t count;
} PmTextSet;

// A custom ghost.
typedef struct PmGhostDef {
  uint32_t size;   // sizeof(PmGhostDef)
  const char* id;  // "<mod id>.<name>", stable across versions; "vanilla." is the game's own
  PmTextSet name, description, strength, weakness;  // its name and journal page, each with English
  const PmTextSet* tells;  // its tells (at most 64), one line each, each with English
  uint32_t tellCount;
  // The evidence it gives: a vanilla evidence id ("emf" "spirit_box" "ultraviolet" "orbs" "writing"
  // "freezing" "dots", and for evidence a game update adds, its EvidenceType name in lower case), or the
  // id of a custom evidence any started mod registers. Null or "" is an empty slot; no evidence twice.
  const char* evidence[3];
  // Its ability for a player who controls it. Both count 0: no ability; otherwise both with English.
  PmTextSet abilityName, abilityDescription;
  void* user;  // handed to every callback below
  // Each callback is optional (null) and runs on the main thread.
  bool (*useAbility)(void* user);  // host only: use the ability now; false if it could not
  // Host only: its state as its player feels it, 0 to 255, sent to the player's client, where hudLine
  // (below) turns it into the HUD's line; -1 none.
  int32_t (*hudState)(void* user);
  // Its own rules forbid acting now (kind 0: an interaction, 1: an event or its ability, 2: a hunt, 3: its
  // ability alone, for a rule that silences it while its events may still happen, 4: the fuse box, which
  // is no interaction unless its rules say so), so a player controlling it may not either. The game's own rules on
  // the fuse box (the Jinn, the Hantu) are the game's and need no answer here.
  bool (*heldBack)(int32_t kind, void* user);
  // Points (x, y, z triples, room for `max`) only the ghost player sees; returns how many it wrote.
  uint32_t (*playerView)(float* xyz, uint32_t max, void* user);
  // How this ghost changes the look of the evidence `evidenceId` (e.g. "green"); null for the
  // evidence's own look. The evidence's mod reads it.
  const char* (*evidenceLook)(const char* evidenceId, void* user);
  // Added in version 2.2 after evidenceLook (a definition of an earlier size reads it as 0): what sets
  // the ghost apart in the game's own rules, PM_GHOST_* bits.
  uint32_t flags;
  // Added in version 2.2 after flags (an earlier size reads it as null): its one target (a Player), or
  // null for no restriction: the host holds each of its hunts to that player as the game holds its
  // Banshee's, whoever steers it, its AI or a player controlling it; never a dead player nor one of the
  // `excludedCount` Players at `excluded` (null and 0 for none). A hunt asks it at its start with nobody
  // excluded; again wherever the game gives the Banshee a new target (GhostAI.SetNewBansheeTarget: a
  // kill, the Hanged Man card), excluding the player about to die (not dead yet); and again, excluding
  // nobody, when its target leaves the game. An answer of null keeps the target, as the game does when
  // nobody else qualifies. Between those it keeps the answer: a player it names mid-hunt binds from the
  // next hunt or re-target. Main thread; asked on the host alone, which runs the hunt and fills the HUD
  // status of a player controlling it.
  void* (*target)(const void* const* excluded, uint32_t excludedCount, void* user);
  // Added in version 2.2 after target (an earlier size reads it as null): the line its player's
  // HUD shows for the state `state` that hudState answered on the host, asked on that player's client:
  // UTF-8 text in the game's rich text, valid until the next call; null or "" for none.
  const char* (*hudLine)(int32_t state, void* user);
  // Added in version 2.2 after hudLine (an earlier size reads it as null): host only, every frame a player
  // steers it: its own rules' limit on where they may steer. `at` is the ghost's position (x, y, z) and
  // (`dx`, `dz`) the horizontal direction they steer in, its length at most 1, which it may shorten, turn or
  // set to 0 (holding the ghost still); it leaves them as they are for no limit. Main thread.
  void (*playerMove)(const float* at, float* dx, float* dz, void* user);
  // Added in version 2.2 after playerMove (an earlier size reads it as null): host only, once for each
  // interaction a player controlling it has paid for and chosen, just before it happens: true when its own
  // rules lose this one, as they lose a share of its AI's attempts. It then does not happen and stays paid
  // for. Asked once per interaction, so it may roll. Main thread.
  bool (*interactionLost)(void* user);
} PmGhostDef;

// PmGhostDef.flags: its third evidence is never the one a difficulty withholds, as for the game's
// Hantu, Goryo, Obake, Moroi and Deogen (the game's evidence pick draws the evidence it removes from all
// but the last); on a difficulty that gives none, none is given.
#define PM_GHOST_KEEPS_LAST_EVIDENCE 1u
// PmGhostDef.flags: blind in a hunt, as its own hunt rules have it: a player controlling it sees nothing
// of the house while it hunts but its playerView, as Playable Ghost darkens a hunting Kormos's view.
#define PM_GHOST_BLIND_IN_HUNTS 2u

// A custom evidence.
typedef struct PmEvidenceDef {
  uint32_t size;          // sizeof(PmEvidenceDef)
  const char* id;         // "<mod id>.<name>", stable across versions
  // Its name, with English: on its row of the journal's evidence page and on the pages of the ghosts
  // that give it, as the game names its own.
  PmTextSet name;
  // The rest of its row's text, with English: the line under its name saying how it is found, as the
  // game's rows have ("Getting a reading of 5 on an EMF Reader").
  PmTextSet journalLine;
} PmEvidenceDef;

// A ghost type's rules, attached by ghost_rules. The host runs them for the contract's ghost while it
// acts as that type: its own type, or for a Mimic the type it has copied (content_acting_as). Each
// callback is optional (null) and runs on the main thread with `user`.
typedef struct PmGhostRules {
  uint32_t size;  // sizeof(PmGhostRules)
  void* user;
  void (*start)(void* user);  // the ghost begins acting as the type, on every client
  void (*tick)(void* user);   // every frame while it does, after start
  void (*stop)(void* user);   // it stops: another type, the contract's end, content withdrawn, the mods stopping
  // Every frame of a hunt, on the host (which runs the ghost's AI), before the game sets the hunt's
  // speed. `base` holds the game's own base speed for this hunt in metres a second and `losScale` its
  // line-of-sight speed-up (1 to 1.65, growing while the ghost sees its prey); change them and return
  // true to have the host write them as the hunt's inputs (set losScale to 1 to hold the speed-up off).
  // The game then applies the multipliers every type shares (difficulty, Blood Moon) once. Return
  // false for the game's own. Several rules on one type each see the values the one before left.
  bool (*huntSpeed)(float* base, float* losScale, void* user);
  // The type's own rules forbid acting now, by the kinds of PmGhostDef.heldBack; for a vanilla type (Ghost
  // Tells' tells) as well as a custom one. A
  // player controlling the ghost asks it through content_held_back.
  bool (*heldBack)(int32_t kind, void* user);
} PmGhostRules;

// One entry of a managed Dictionary<K, int> (the host contract's dictionary_int_entries).
struct PmIntEntry {
  int32_t key;
  int32_t value;
};

struct PmHostApi {
  uint32_t version;              // PMMOD_API_VERSION
  void* gameAssembly;            // HMODULE, already verified against the audited build
  void* unityPlayer;             // HMODULE, already verified against the audited build
  const wchar_t* dataDirectory;  // %LOCALAPPDATA%\PhasmoMods, which exists

  void (*log)(const char* line);  // one line into the shared log (the mod names itself in it)
  // A MinHook detour through the host's single MinHook instance, not yet enabled. A target that is
  // already hooked (by the host or another mod) is chained: the newest detour runs first and its
  // `original` reaches the previous one.
  bool (*create_hook)(void* target, void* detour, void** original, const char* name);
  bool (*enable_hooks)(void);  // enable every hook created so far
  // A message on the mod's own channel (PmModInfo::channel; at most PM_MAX_MESSAGE bytes). `actor` is
  // used with PM_TO_ACTOR.
  bool (*send)(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor);
  // Null stops receiving. A message reaches it only from a verified player, on a client that matches the host; anything else
  // is dropped, and a send goes only to verified players (sdk/README.md, "The host's list").
  void (*set_receiver)(uint8_t channel, PmReceiver receiver);
  // Whether the client of `actor` in this room is verified and the host's list has the mod `modId`: a verified player's mods
  // equal that list (sdk/README.md, "The host's list"). Kept so 2.x mods load. A mod has no need of it: the loader
  // keeps every player in a contract to the same mods, and a mod does no matching of its own.
  bool (*peer_has_mod)(int32_t actor, const char* modId);
  // Named tables of C function pointers that cooperating mods publish for each other. A table must
  // stay valid for the process lifetime. A name is published once: registering a name already
  // registered is refused (logged) and the first table stays. service returns null for a name
  // nobody registered.
  void (*register_service)(const char* name, const void* table);
  const void* (*service)(const char* name);

  // Added in version 2.0. Mods reach these through the game API (sdk/api/game_api.h).

  // The game entry `key` names (its map key, e.g. "GhostAI.ChangeState"), or null when the map has
  // no such entry, it did not resolve in this game build, or it is a GameAssembly export or a native
  // site in UnityPlayer.dll (only the host may use one, so a mod that requires one is never started).
  const PmGameEntry* (*game_entry)(const char* key);
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
  bool (*static_get)(const char* key, void* out, size_t size);  // pm:not-game-data: the service, not a read
  // Stores the `size` bytes at `in` into the static field. For a managed reference `in` points at
  // the pointer, which is stored through the runtime's GC write barrier.
  bool (*static_set)(const char* key, const void* in, size_t size);

  // Also added in version 2.1: hooks per body. A method's bodies are its canonical body (index 0)
  // and then each distinct twin (PmGameEntry::twins, less any folded onto an earlier body), in order.
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

  // Also added in version 2.1: scene and asset facts, the entries of kind PM_ENTRY_SCENE and
  // PM_ENTRY_ANIMATOR_PARAM (a layer is resolved when the host activates, and game_entry gives its
  // index; so is a shader property since version 2.2, PM_ENTRY_SHADER_PROPERTY, with its id). Main
  // thread, while the game plays. A fact that is missing where it is used is logged once per scene
  // load, and the call answers null or false, so the feature that needs it stays off.

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

  // Added in version 2.2: content (sdk/api/content.h). Main thread. A mod registers content only from
  // its pmmod_start; registration only records a definition. Once every mod has started the host
  // settles the content (a ghost whose custom evidence no started mod registered is dropped, logged)
  // and puts it into the game. Mods never choose a number: the host numbers custom ghost types from
  // one past the game's largest ghost type, and custom evidence from one past its largest evidence
  // (both read from the game at startup: 31 and 8 in the audited build), each in byte order of its id,
  // over the content the room's host has. Content is active only in a private room, on the host, or on a client
  // whose host has sent a content manifest (its ghost and evidence ids, each with a hash of its
  // definition, on the host's own channel as players join) equal to this client's (a host not heard from yet
  // keeps it off for up to 10 s, then it is off). Another player's content changes nothing for anyone: the loader
  // removes whoever cannot match, and no contract starts until they are gone. So in a contract every
  // client holds the same content and reaches the same numbers. Content also stays off when the game's
  // ghost type and evidence enums cannot be read (logged at startup). Vanilla ghost types and evidence
  // answer whenever those enums were read. The content goes when the mods stop or restart, and a
  // mod's own content when its pmmod_start fails.

  // Records the ghost `def` and returns its handle (greater than 0), or 0 when it is refused: a call
  // outside pmmod_start, a null `def`, a `size` smaller than version 2.2's, an id that is missing, has
  // no '.', is longer than 255 bytes, begins with "vanilla." or is already registered, text without
  // English (or with a null or repeated language, more than 64 languages, or a text over 64 KB), more
  // than 64 tells, or an evidence that is neither vanilla nor "<mod id>.<name>", or is listed twice.
  // The log names the ghost and why.
  int32_t (*ghost_register)(const PmGhostDef* def);
  // Records the evidence `def` like ghost_register: its handle, or 0 (logged) for a call outside
  // pmmod_start, a null `def`, a short `size`, a bad or repeated id, or text refused as above.
  int32_t (*evidence_register)(const PmEvidenceDef* def);
  // The game type number of the ghost `ghostId` while content is active, and while a contract that
  // loaded with content active holds it, until that contract ends, whatever the room's content state
  // does meanwhile; else -1. A vanilla id ("vanilla.mare") answers its type whenever the game's enums
  // were read; null or an unknown id answers -1.
  int32_t (*content_type)(const char* ghostId);
  // The id of the ghost type `type`: "vanilla.<its enum value's name in lower case>" for a vanilla type
  // (for example "vanilla.mare" for 6; None is no ghost, and a value without a plain ASCII name has no
  // id), a custom ghost's id while content is active or a contract holds it (as content_type), else
  // null. The text stays valid until the content changes (the mods stop or restart).
  const char* (*content_ghost_id)(int32_t type);
  // The game value of the evidence `evidenceId`: the game's own for a vanilla id ("emf" is 1)
  // whenever the game's enums were read, a custom evidence's number while content is active or a
  // contract holds it (as content_type), else -1.
  int32_t (*content_evidence)(const char* evidenceId);
  // Whether custom content is in play: this client registered some and is the host or matches it (above). False
  // outside a private room.
  bool (*content_active)(void);
  // Whether a player controls the contract's ghost, as the Playable Ghost mod reports it
  // (playable-ghost.v1); false without it.
  bool (*content_player_controlled)(void);

  // Also added in version 2.2: ghost types in the game. While content is active the host puts each
  // custom ghost into the game as a ghost type of its own: a GhostData in the contract's
  // GhostCollection in its alphabetical place (the roll, the evidence given and the journal read
  // it), its name for the game's name key, the custom type as the ghost's identity and the game's
  // "no type" as its AI's behaviour type on every client, and the custom types in the list the Mimic
  // copies from. Whether a contract has them is decided when it loads (content active then) and
  // holds until it ends; they leave at its end and when the mods stop. In the lobby they follow the
  // room: while content is active there the lists that offer ghost types (the difficulty menu's, the
  // lobby journals' collection) get them, and lose them when it stops being active. Main thread.

  // Attaches `rules` (copied) to the ghost `ghostId`: a custom ghost's id or a vanilla one
  // ("vanilla.mare"). Several mods may attach rules to one id; they run in the order attached. Returns
  // a handle greater than 0, or 0 when refused (logged with the id and why): a call outside
  // pmmod_start, a null id or `rules`, a `size` smaller than version 2.2's, an id that is neither
  // "<mod id>.<name>" nor a vanilla id of the game's enums. Rules on a custom id that no started mod
  // registered are dropped once every mod has started (logged). A custom ghost's rules run in a
  // contract that loaded while content was active, for the whole contract; a vanilla type's whenever
  // the contract's ghost acts as it. No rules run outside a private room, nor while this client's mods do
  // not match the host's list (sdk/runtime/mod_agreement.h). That answer holds from the contract's load until it ends, so a
  // contract's rules never start partway through, and content stays pending while it is no.
  int32_t (*ghost_rules)(const char* ghostId, const PmGhostRules* rules);
  // The type the contract's ghost acts as on this client: its own type, or for a Mimic the type it has
  // copied, a custom one included; -1 outside a contract or before the ghost spawns. The Mimic's copy
  // happens on the host, which sends it to every client on its own channel; until it arrives a client
  // answers from its own GhostInfo.
  int32_t (*content_acting_as)(void);
  // The contract's ghost's identity, the type the journal judges and the results name (a custom
  // ghost's type, or the Mimic's while it copies); -1 outside a contract or before the ghost spawns.
  int32_t (*content_identity)(void);

  // Also added in version 2.2: the journal. On every client's journals, other players' books
  // included, in a contract and in the lobby alike (a contract's custom ghosts, else the lobby's while
  // content is active there, so players read a custom ghost's page before and after a contract; when
  // that stops, each journal is put back as the game made it), the host gives each custom ghost a
  // guess button on the evidence spread's ghost page (a copy of the game's own, crossed out by its
  // evidence filter and synced by its own controller; the grid's rows shortened so every ghost fits),
  // an entry in the Ghosts tab's list in its alphabetical place, and a page of its own after the
  // book's last page:
  // its name, its description with its strength and weakness, its ability when it has one, and its
  // tells, laid out as the game's own pages are. Pages are numbered in the order of the ghosts' type
  // numbers, so they are the same on every client. Texts are in the player's language when the ghost
  // has it, else English, and follow the game's language switch. Main thread.

  // Adds `count` lines to the journal page of the vanilla ghost `vanillaGhostId` ("vanilla.mare"),
  // each in several languages (English required), after the page's own text. Several mods may add
  // lines to one ghost; they show in the order added. They show in a private room only, in the lobby's
  // journals and a contract's alike. Returns a
  // handle greater than 0, or 0 when refused (logged with the id and why): a call outside
  // pmmod_start, an id that is not one of the game's vanilla ghosts, null or no lines, more than 64
  // lines, or a line refused as ghost_register refuses text. Like ghost_rules' rules, the lines show
  // only while this client's mods match the host's list (sdk/runtime/mod_agreement.h), held from a contract's load until it
  // ends; the host writes the pages again when that answer changes.
  int32_t (*journal_lines)(const char* vanillaGhostId, const PmTextSet* lines, uint32_t count);

  // Also added in version 2.2: custom evidence in the game. A custom ghost gives a custom evidence it
  // lists through the game's own evidence pick: its number stands in the ghost's GhostData, so a
  // difficulty that gives fewer evidence withholds it exactly as it withholds the game's own, and the
  // game sends what it gives to every client. On every client's journals, whenever the custom ghosts
  // they show give a custom evidence (a contract's, or the lobby's while content is active there),
  // that evidence gets a copy of the game's Spirit Box row after the
  // game's rows on the evidence page: its name (PmEvidenceDef::name) and, under it, the line saying how
  // it is found (PmEvidenceDef::journalLine), in the player's language when the evidence has it, else
  // English; it is marked, synced to the other players and filters the ghost buttons as the game's own
  // rows do, and a custom ghost's page names it as the game's pages name theirs. What reveals the
  // evidence in the world stays its mod's job. Main thread.

  // How the contract's ghost changes the look of the evidence `evidenceId`: what its definition's
  // evidenceLook answers (PmGhostDef), asked of the ghost by its identity (content_identity), so a
  // Mimic that copies a ghost keeps the Mimic's look. Null for the evidence's own look: no ghost, a
  // game ghost, a ghost without evidenceLook or one that answers null, and a null `evidenceId`. A
  // contract that holds custom ghosts answers for its ghost until it ends. The text is the ghost mod's;
  // copy it before the next call.
  const char* (*content_evidence_look)(const char* evidenceId);

  // Also added in version 2.2, after content_evidence_look: adds the managed reference `item` (null
  // allowed) at the end of the List<T> `list`, through the Add of the list's own class, as the game's
  // code adds to its lists; the List may replace its backing array. False, touching nothing, for a null
  // `list`, an object that is not a List (logged once) or a List whose Add cannot be run. An item the
  // List's element type does not take is the caller's error, as in C#. Main thread.
  bool (*list_add)(void* list, void* item);

  // Also added in version 2.2, after list_add: copies `src`'s settings onto `dst`, two objects of the
  // class the Type entry `typeKey` resolved to (or of classes derived from it): every instance field
  // that class itself declares as public and of a value type (a number, a bool, an enum or a struct),
  // the settings Unity's inspector shows for a component of the class. A struct is copied whole, and
  // each managed reference inside it is stored through the GC write barrier. Fields the class
  // inherits, and its fields of a reference type, are left as they are. False, copying nothing, for an
  // unknown or unresolved key, a null object, an object not of the class, or a setting whose type the
  // runtime cannot say (logged once): a copy is whole or not made. Main thread.
  bool (*copy_settings)(const char* typeKey, void* dst, const void* src);

  // Also added in version 2.2, after copy_settings: the managed references `obj` holds in every
  // instance field that the class the Type entry `typeKey` resolved to declares itself of the type the
  // Type entry `fieldTypeKey` resolved to (with `arrayOf`, of an array of it), in the order the class
  // lays them out, the first `max` of them (by layout, among all of them) into `out`. Returns how many
  // such fields the class declares, which may be more than `max`; 0 for an unknown or unresolved key, a null `obj`, or an object not of
  // the class. A class may declare several fields of one type that nothing in the game's code tells
  // apart; this reads all of them without naming any. Main thread.
  uint32_t (*fields_of_type)(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                             uint32_t max);

  // Also added in version 2.2, after fields_of_type: the contract's ghost as a player controls it, for
  // any registered ghost. Each asks the definition (PmGhostDef) of the type the ghost acts as on this
  // client (content_acting_as): a custom type a contract that loaded with content holds, or one of the
  // active content. A game type, no ghost, or a callback the definition leaves null answers as below.
  // Main thread.

  // Host only: runs the definition's useAbility; false when it has none or could not.
  bool (*content_use_ability)(void);
  // The definition's ability name, or with `description` its description, in the player's language
  // when it has it, else English; null for a ghost without an ability. Valid until the content changes.
  const char* (*content_ability_text)(bool description);
  // Host only: the definition's hudState, 0 to 255, or -1.
  int32_t (*content_hud_state)(void);
  // The definition's hudLine for `state`; null for none. Copy it before the next call.
  const char* (*content_hud_line)(int32_t state);
  // Whether the ghost may not act now as `kind` (as PmGhostDef.heldBack):
  // the definition's heldBack, or the heldBack of any rules running for the type (ghost_rules; a
  // vanilla type's too). False with no ghost.
  bool (*content_held_back)(int32_t kind);
  // The definition's playerView into `xyz` (room for `max` points); how many it wrote, 0 for none.
  uint32_t (*content_player_view)(float* xyz, uint32_t max);
  // The definition's target: the one player the ghost's hunts are held to now, never
  // one of the `excludedCount` players at `excluded` (null and 0 for none), or null.
  void* (*content_target)(const void* const* excluded, uint32_t excludedCount);
  // The definition's flags (PM_GHOST_* bits); 0 with no definition.
  uint32_t (*content_flags)(void);

  // Also added in version 2.2, after content_flags: whether this client's running mods equal the host's list, in a private
  // room (sdk/runtime/mod_agreement.h); held from a contract's load until it ends. It is the one answer the host gates the
  // mods' content, rules and journal lines on. Kept so 2.x mods load: a mod has no need of it, since the loader removes
  // or switches off whoever does not match, and no unverified player reaches a contract. Main thread.
  bool (*room_agrees)(void);

  // Also added in version 2.2, after room_agrees: more runtime services (sdk/api/runtime.h), which read
  // what they need from the runtime's metadata. Main thread; each answers 0 or false, touching nothing,
  // for null and for an object of another kind (logged once).

  // The number of items in the managed collection `collection`, a System.Collections.Generic List<T>,
  // HashSet<T>, Dictionary<K,V>, Queue<T> or Stack<T>, through its own get_Count, found by name on its
  // class (a HashSet's count is not a List's).
  int32_t (*collection_count)(const void* collection);
  // The bytes one element of the managed array `arr` takes as the runtime lays it out: a value type's own
  // (unboxed) size, as a List<T> of structs holds them, or a reference's.
  size_t (*array_element_size)(const void* arr);
  // Removes the item at `index` from the List<T> `list` through the RemoveAt of the list's own class, as
  // the game's code removes from its lists; false for an index out of range or a RemoveAt that cannot be
  // run.
  bool (*list_remove_at)(void* list, int32_t index);

  // Also added in version 2.2, after list_remove_at: host only, the definition's playerMove on the
  // direction (`dx`, `dz`) a player steers the ghost in from `at` (x, y, z); it leaves them as they are
  // without a definition or the callback. Main thread.
  void (*content_player_move)(const float* at, float* dx, float* dz);
  // Also added in version 2.2, after content_player_move: host only, the definition's interactionLost for an
  // interaction a player has paid for and chosen; false without a definition or the callback. Asked once
  // per interaction. Main thread.
  bool (*content_interaction_lost)(void);
  // Also added in version 2.2, after content_interaction_lost: whether a player controlling the ghost takes
  // the actions of the game's idle roll running now, so a mod's rule on the AI's attempts stands aside
  // whichever mod's hook runs first; as the Playable Ghost mod reports it (playable-ghost.v1, version 3),
  // false without it. Main thread.
  bool (*content_roll_taken)(void);

  // Also added in version 2.2, after content_roll_taken: replaces the direction Unity's crowd walks the
  // NavMeshAgent `agent` (a managed reference) in, until the host opens the next frame, on this client
  // only: nothing is sent. (`dx`, `dz`) is a world direction of length at most 1, and (0, 0) holds the
  // agent still as it decelerates. The agent's destination, speed, acceleration and avoidance stay the
  // game's. A mod keeps an agent steered by asking in each pmmod_tick; a frame without an ask leaves
  // the game's own direction. One agent at a time: the last ask of a frame counts, and a direction
  // that is not one (longer than 1, not finite) ends the frame's steering until a valid ask after it.
  // A null or destroyed `agent`, or an object that is not a NavMeshAgent, changes nothing. True when the
  // host can steer, so a null `agent` asks only that; false when it cannot in this game build (the host
  // logs why), and nothing was changed. The first ask makes the effect ready in the game's code; after a
  // refusal the host tries again only when the mods start afresh. The game entries this needs are the
  // host's own, and a mod requires none of them. Main thread, a null `agent` from pmmod_start, an agent
  // from pmmod_tick.
  bool (*steer_agent)(void* agent, float dx, float dz);

  // Also added in version 2.2, after steer_agent: unwinds the running GhostController.Spawn, as a managed
  // exception does, back to the mod's C++ catch around its call of it, so a mod can hold the setup back at
  // its type roll, before it writes or sends anything. It does not return. Outside a running spawn it does
  // nothing and returns (logged once). Main thread, from a hook the spawn reaches.
  void (*abandon_ghost_spawn)(void);
  // Also added in version 2.2, after abandon_ghost_spawn: the entries of the managed Dictionary<K, int>
  // `dict`, whose K is a 4-byte value such as an enum, as the dictionary's own array keeps them: at most
  // `max` into `out`, in that order, free slots left out. Returns how many it wrote; 0 for null and for an
  // object of another kind (logged once). Main thread.
  int32_t (*dictionary_int_entries)(const void* dict, PmIntEntry* out, int32_t max);
  // Also added in version 2.2, after dictionary_int_entries: whether the mod `modId` (its manifest id) has started on this
  // client and runs now. For an optional dependency (a manifest dependency with "required": false), asked from the
  // mod's pmmod_start onwards. The host starts a mod after the dependencies it names, but one below this mod's
  // minimumVersion, or on an edge that would close a cycle, gets no place in the order and may start after this mod, so
  // do not keep the answer from pmmod_start: ask when it is needed. It says nothing of the other mod's version, nor of
  // other players' mods (the loader keeps them to the host's list). False for null and for a mod not installed, off, not started
  // yet or already stopped. Main thread.
  bool (*mod_started)(const char* modId);
  // Also added in version 2.2, after mod_started: a one-button message in the game's own popup on the menu's and the lobby's
  // board, with `title` and `body` (UTF-8, the game's rich text). It waits until the board is in view, however long that takes,
  // so one asked for during a contract shows once the lobby is back; it shows until it is clicked. It replaces any message
  // showing or waiting, the host's own included, so keep it for news a player needs. Nothing shows in a build whose popup
  // cannot be copied (logged once). Null `title` or `body`: nothing. The loader draws no window of its own (vanilla UI only).
  // Main thread.
  void (*show_message)(const char* title, const char* body);
};

enum PmModFlags : uint32_t {
  // Keeps other players' speech in memory during a contract. The manifest's recordsVoice must say so too: the notice reads
  // that, before the DLL loads, and the host keeps off a DLL that sets this under a manifest that does not.
  PM_MOD_RECORDS_VOICE = 1,
  // Reserved: a mod built for contract 2.0 to 2.2 may set it, and it still loads. The host ignores it, because every mod is
  // required by every player in a lobby (host/join_guard.h). A new mod leaves it clear.
  PM_MOD_VANILLA_PEERS = 2,
};

struct PmModInfo {
  uint32_t apiVersion;  // PMMOD_API_VERSION the mod was built against
  const char* id;       // <author>.<mod>, as in its manifest.json, e.g. "phasmomods.playable-ghost"
  const char* name;     // for the log; the manifest's name is what the player reads
  uint8_t channel;      // its own network channel, 1..255, unique in the suite
  uint32_t flags;       // PmModFlags
};

// The exports each mod DLL provides. The host loads the DLL when the mod first starts, after the player agreed to the
// notice, never before; it reads these then. Do no work in DllMain. A loaded DLL stays mapped until the game exits, even
// if the host refuses it, because unloading one is not safe.
typedef const PmModInfo* (*PmModInfoFn)(void);  // "pmmod_info": callable at any time
typedef bool (*PmModStartFn)(const PmHostApi*);  // "pmmod_start": once, after consent; false leaves it inactive
typedef void (*PmModTickFn)(void);               // "pmmod_tick": every frame once started
typedef void (*PmModStopFn)(void);               // "pmmod_stop": once, when the host stops
// "pmmod_requires" (optional, version 2 onwards; the game API's glue exports it): the keys of the game
// entries the mod uses that are not marked optional. It points `keys` at an array the mod keeps,
// unchanged, for as long as its DLL stays loaded, and returns its length. Callable at any time. The
// host does not start a mod while any of these did not resolve, and logs which. A mod without this
// export requires no game entry.
typedef size_t (*PmModRequiresFn)(const char* const** keys);
}
