#pragma once

#include <cstddef>
#include <cstdint>

/// @file
/// @brief The host contract: the C interface between ModHost.dll and each mod DLL.
///
/// Every module links its own static C runtime, so only plain pointers, integers and C strings cross
/// it, and nothing thrown may cross it: a mod catches its own exceptions. Every call in both
/// directions is made on Unity's main thread, except `log`, which any thread may call.
///
/// Compatibility is versioned like a library. Within a major version the contract only grows:
/// PmHostApi and every struct here gain members at the end and nothing is removed or changed, so a
/// host accepts a mod built against its own major version and the same or an older minor version. A
/// new major version refuses every older mod, and the host names those that need rebuilding.
///
/// A mod built on the game API (sdk/api) reaches the game through its wrappers and seldom needs this
/// file directly: it gives the exports (pmmod_info, pmmod_start, pmmod_tick, pmmod_stop) and
/// PmHostApi, which sdk/api/content.h and the game API call for the mod.
/// @ingroup content
extern "C" {

/// @addtogroup content
/// @{

/// The host contract's major version this header describes. A host runs only mods built for its own
/// major version.
/// @since Contract 2.0
#define PMMOD_API_MAJOR 2u
/// The host contract's minor version this header describes. Within a major version a host runs mods
/// built for its own minor version or an older one.
/// @since Contract 2.0
#define PMMOD_API_MINOR 7u
/// Major in the high 16 bits, minor in the low 16. Version 1 (the first contract) had no minor.
/// @since Contract 2.0
#define PMMOD_API_VERSION ((PMMOD_API_MAJOR << 16) | PMMOD_API_MINOR)

/// @brief Whether a host of version `host` runs a mod built against version `mod`: the same major
/// version and a minor version no newer than the host's.
/// @param host The host's PMMOD_API_VERSION.
/// @param mod The PMMOD_API_VERSION the mod was built against (PmModInfo::apiVersion).
/// @return True when the host runs the mod.
/// @pre Any thread.
/// @since Contract 2.0
/// @code
/// if (!pm_api_compatible(api->version, PMMOD_API_VERSION)) return false;
/// @endcode
inline bool pm_api_compatible(uint32_t host, uint32_t mod) {
  return (host >> 16) == (mod >> 16) && (mod & 0xFFFFu) <= (host & 0xFFFFu);  // pm:not-game-data: the minor version
}

/// What a game entry (an entry of the game map, sdk/map) names in the game: PmGameEntry::kind.
/// @since Contract 2.0
enum PmEntryKind : uint32_t {
  PM_ENTRY_TYPE = 1,  ///< a class, struct or enum
  PM_ENTRY_METHOD,  ///< a method, with its code
  PM_ENTRY_FIELD,  ///< a field, with its offset
  PM_ENTRY_ENUM_VALUE,  ///< one value of an enum
  PM_ENTRY_CODE,  ///< an address inside a method's code
  // Added in version 2.1.
  PM_ENTRY_ICALL,  ///< a Unity engine function the runtime registered by name, with its code (since 2.1)
  PM_ENTRY_EXPORT,  ///< a function GameAssembly.dll exports; host only, game_entry never gives one (since 2.1)
  PM_ENTRY_SCENE,  ///< a GameObject by its path, found through scene_find (since 2.1)
  PM_ENTRY_LAYER,  ///< a layer, with its index in `value` (since 2.1)
  PM_ENTRY_ANIMATOR_PARAM,  ///< an Animator parameter by its name, checked through animator_param (since 2.1)
  PM_ENTRY_NATIVE_SITE,  ///< a place in UnityPlayer.dll's code; host only, game_entry never gives one, steer_agent (since 2.1)
  // Added in version 2.2.
  PM_ENTRY_SHADER_PROPERTY,  ///< a shader property by its name, with its id (Shader.PropertyToID) in `value` (since 2.2)
};

/// A game entry as the host resolved it in the running game. The host keeps it for the process
/// lifetime. Members a kind does not use are zero. Every pointer is the runtime's metadata or code,
/// never a managed object. game_entry gives one; the game API's wrappers read it for the mod.
/// @since Contract 2.0
struct PmGameEntry {
  uint32_t kind;  ///< PmEntryKind
  void* klass;  ///< Il2CppClass*: the type itself, or the type that declares the member
  const void* method;  ///< MethodInfo* of a method, passed as a call's last argument
  /// A method's code (its canonical body), a code entry's address, an icall's or export's code, or a
  /// native site's address in UnityPlayer.dll.
  void* code;
  /// A method's same behaviour at other addresses (the obfuscator's twins), besides `code`.
  void* const* twins;
  uint32_t twinCount;  ///< how many addresses `twins` holds; 0 for a method without twins
  /// A field's offset from the start of the object, including the object header; a static field's
  /// from the start of its class's static data.
  int32_t offset;
  void* field;  ///< FieldInfo* of a field, static or not
  int64_t value;  ///< an enum value's number, a layer's index (0 to 31) or a shader property's id
};

/// Who a message is sent to: the `to` of PmHostApi::send.
/// @since Contract 1.0
enum PmTo : int32_t {
  PM_TO_OTHERS = 0,  ///< every other verified player in the room
  PM_TO_MASTER = 1,  ///< the room's master client
  PM_TO_ACTOR = 2,   ///< the one player whose Photon actor number is send's `actor`
};

/// The largest payload PmHostApi::send takes, in bytes. Mod payloads ride on Photon event 197 behind a
/// one-byte channel; channel 0 is the host's own.
/// @since Contract 1.0
constexpr size_t PM_MAX_MESSAGE = 511;

/// A mod's receiver of its channel's messages, set with PmHostApi::set_receiver. `sender` is the
/// sending client's Photon actor number, taken from the event, never the payload; `data` holds `size`
/// bytes, valid during the call only. Main thread.
/// @since Contract 1.0
typedef void (*PmReceiver)(int32_t sender, const uint8_t* data, size_t size);

/// One piece of text in one language. Neither member may be null.
/// @since Contract 2.2
typedef struct PmText {
  const char* lang;  ///< a language code of the game's own, e.g. "en" (GetLanguageAbbreviationText, in lower case)
  const char* text;  ///< UTF-8, at most 64 KB
} PmText;
/// A piece of text in several languages (at most 64), each named once. English ("en") is required
/// wherever text is given; a language the game asks for and the set lacks falls back to English.
/// @since Contract 2.2
typedef struct PmTextSet {
  const PmText* items;  ///< the texts, one per language
  uint32_t count;       ///< how many `items` holds
} PmTextSet;

/// A custom ghost, registered with PmHostApi::ghost_register (sdk/api/content.h wraps it as
/// pm::api::content::GhostDef). The host copies everything a definition holds when it is registered,
/// except `user` and the callbacks, which must stay valid while the mod runs. It begins with its own
/// size, so it can grow at the end like PmHostApi: the host reads no member past `size`.
/// @since Contract 2.2
typedef struct PmGhostDef {
  uint32_t size;  ///< sizeof(PmGhostDef)
  const char* id;  ///< "<mod id>.<name>", stable across versions; "vanilla." is the game's own
  PmTextSet name;         ///< its name, with English
  PmTextSet description;  ///< its journal page's description, with English
  PmTextSet strength;     ///< its journal page's strength, with English
  PmTextSet weakness;     ///< its journal page's weakness, with English
  const PmTextSet* tells;  ///< its tells (at most 64), one line each, each with English
  uint32_t tellCount;      ///< how many `tells` holds
  /// The evidence it gives: a vanilla evidence id ("emf" "spirit_box" "ultraviolet" "orbs" "writing"
  /// "freezing" "dots", and for evidence a game update adds, its EvidenceType name in lower case), or the
  /// id of a custom evidence any started mod registers. Null or "" is an empty slot; no evidence twice. All
  /// three may be empty, for a ghost that gives none: its GhostData then holds the game's own "no evidence"
  /// (EvidenceType.None) in each slot, the game's pick gives only that, and the journal's filter crosses it out
  /// once any evidence is found (sdk/systems/evidence.md, "A ghost that gives no evidence").
  const char* evidence[3];
  /// Its ability's name, for a player who controls it. With abilityDescription's count 0 too: no ability;
  /// otherwise both with English.
  PmTextSet abilityName;
  /// Its ability's description, for a player who controls it; see abilityName.
  PmTextSet abilityDescription;
  void* user;  ///< handed to every callback below
  /// Each callback is optional (null) and runs on the main thread.
  bool (*useAbility)(void* user);  ///< host only: use the ability now; false if it could not
  /// Host only: its state as its player feels it, 0 to 255, sent to the player's client, where hudLine
  /// (below) turns it into the HUD's line; -1 none.
  int32_t (*hudState)(void* user);
  /// Its own rules forbid acting now (kind 0: an interaction, 1: an event or its ability, 2: a hunt, 3: its
  /// ability alone, for a rule that silences it while its events may still happen, 4: the fuse box, which
  /// is no interaction unless its rules say so), so a player controlling it may not either. The game's own rules on
  /// the fuse box (the Jinn, the Hantu) are the game's and need no answer here.
  bool (*heldBack)(int32_t kind, void* user);
  /// Points (x, y, z triples, room for `max`) only the ghost player sees; returns how many it wrote.
  uint32_t (*playerView)(float* xyz, uint32_t max, void* user);
  /// How this ghost changes the look of the evidence `evidenceId` (e.g. "green"); null for the
  /// evidence's own look. The evidence's mod reads it.
  const char* (*evidenceLook)(const char* evidenceId, void* user);
  /// Added in version 2.2 after evidenceLook (a definition of an earlier size reads it as 0): what sets
  /// the ghost apart in the game's own rules, PM_GHOST_* bits.
  uint32_t flags;
  /// Added in version 2.2 after flags (an earlier size reads it as null): its one target (a Player), or
  /// null for no restriction: the host holds each of its hunts to that player as the game holds its
  /// Banshee's, whoever steers it, its AI or a player controlling it; never a dead player nor one of the
  /// `excludedCount` Players at `excluded` (null and 0 for none). A hunt asks it at its start with nobody
  /// excluded; again wherever the game gives the Banshee a new target (GhostAI.SetNewBansheeTarget: a
  /// kill, the Hanged Man card), excluding the player about to die (not dead yet); and again, excluding
  /// nobody, when its target leaves the game. An answer of null keeps the target, as the game does when
  /// nobody else qualifies. Between those it keeps the answer: a player it names mid-hunt binds from the
  /// next hunt or re-target. Main thread; asked on the host alone, which runs the hunt and fills the HUD
  /// status of a player controlling it.
  void* (*target)(const void* const* excluded, uint32_t excludedCount, void* user);
  /// Added in version 2.2 after target (an earlier size reads it as null): the line its player's
  /// HUD shows for the state `state` that hudState answered on the host, asked on that player's client:
  /// UTF-8 text in the game's rich text, valid until the next call; null or "" for none.
  const char* (*hudLine)(int32_t state, void* user);
  /// Added in version 2.2 after hudLine (an earlier size reads it as null): host only, every frame a player
  /// steers it: its own rules' limit on where they may steer. `at` is the ghost's position (x, y, z) and
  /// (`dx`, `dz`) the horizontal direction they steer in, its length at most 1, which it may shorten, turn or
  /// set to 0 (holding the ghost still); it leaves them as they are for no limit. Main thread.
  void (*playerMove)(const float* at, float* dx, float* dz, void* user);
  /// Added in version 2.2 after playerMove (an earlier size reads it as null): host only, once for each
  /// interaction a player controlling it has paid for and chosen, just before it happens: true when its own
  /// rules lose this one, as they lose a share of its AI's attempts. It then does not happen and stays paid
  /// for. Asked once per interaction, so it may roll. Main thread.
  bool (*interactionLost)(void* user);
} PmGhostDef;

/// PmGhostDef.flags: its third evidence is never the one a difficulty withholds, as for the game's
/// Hantu, Goryo, Obake, Moroi and Deogen (the game's evidence pick draws the evidence it removes from all
/// but the last); on a difficulty that gives none, none is given.
#define PM_GHOST_KEEPS_LAST_EVIDENCE 1u
/// PmGhostDef.flags: blind in a hunt, as its own hunt rules have it: a player controlling it sees nothing
/// of the house while it hunts but its playerView, as Playable Ghost darkens a hunting Kormos's view.
#define PM_GHOST_BLIND_IN_HUNTS 2u
/// PmGhostDef.flags, added in version 2.3: never on the Mimic's copy list, so a Mimic never copies it, whether it may
/// be rolled or not. The host leaves it out when it adds the contract's custom ghosts to the list the game built
/// (GhostController.BuildMimicCopies); the game's own types and every other custom ghost are added as before. A
/// host of version 2.2 does not know the bit and adds the ghost like any other.
#define PM_GHOST_NO_MIMIC_COPY 4u
/// PmGhostDef.flags, added in version 2.3: no player ever plays it. A mod that lets a player be the ghost (Playable
/// Ghost) never offers it, never rolls it for a contract with a ghost player and never keeps it from a pick; it asks
/// content_flags_of. Its AI haunts as usual in every other contract. The host itself does nothing with the bit.
#define PM_GHOST_NOT_PLAYABLE 8u

/// A custom evidence, registered with PmHostApi::evidence_register (sdk/api/content.h wraps it as
/// pm::api::content::EvidenceDef). The host copies it when it is registered. It begins with its own
/// size, so it can grow at the end: the host reads no member past `size`.
/// @since Contract 2.2
typedef struct PmEvidenceDef {
  uint32_t size;  ///< sizeof(PmEvidenceDef)
  const char* id;  ///< "<mod id>.<name>", stable across versions
  /// Its name, with English: on its row of the journal's evidence page and on the pages of the ghosts
  /// that give it, as the game names its own.
  PmTextSet name;
  /// The rest of its row's text, with English: the line under its name saying how it is found, as the
  /// game's rows have ("Getting a reading of 5 on an EMF Reader").
  PmTextSet journalLine;
} PmEvidenceDef;

/// A ghost type's rules, attached by ghost_rules. The host runs them for the contract's ghost while it
/// acts as that type: its own type, or for a Mimic the type it has copied (content_acting_as). Each
/// callback is optional (null) and runs on the main thread with `user`.
/// @since Contract 2.2
typedef struct PmGhostRules {
  uint32_t size;  ///< sizeof(PmGhostRules)
  void* user;  ///< handed to every callback below; must stay valid while the mod runs
  void (*start)(void* user);  ///< the ghost begins acting as the type, on every client
  void (*tick)(void* user);  ///< every frame while it does, after start
  void (*stop)(void* user);  ///< it stops: another type, the contract's end, content withdrawn, the mods stopping
  /// Every frame of a hunt, on the host (which runs the ghost's AI), before the game sets the hunt's
  /// speed. `base` holds the game's own base speed for this hunt in metres a second and `losScale` its
  /// line-of-sight speed-up (1 to 1.65, growing while the ghost sees its prey); change them and return
  /// true to have the host write them as the hunt's inputs (set losScale to 1 to hold the speed-up off).
  /// The game then applies the multipliers every type shares (difficulty, Blood Moon) once. Return
  /// false for the game's own. Several rules on one type each see the values the one before left.
  bool (*huntSpeed)(float* base, float* losScale, void* user);
  /// The type's own rules forbid acting now, by the kinds of PmGhostDef.heldBack; for a vanilla type (Ghost
  /// Tells' tells) as well as a custom one. A
  /// player controlling the ghost asks it through content_held_back.
  bool (*heldBack)(int32_t kind, void* user);
} PmGhostRules;

/// A content pack made for the calling mod (PmHostApi::content_packs): a mod folder of data alone whose manifest names
/// that mod in "contentPackFor" (sdk/README.md, "Content packs"). The host copies a pack for each game process as it
/// copies a mod, and every text and path here lasts until the mods next stop.
/// @since Contract 2.6
typedef struct PmContentPack {
  uint32_t size;          ///< the bytes of this struct the host filled: the caller's item size, at most the host's sizeof(PmContentPack)
  const char* id;         ///< the pack's mod id, "<author>.<name>" (UTF-8)
  const char* name;       ///< its manifest's name, as the Mods panel shows it (UTF-8)
  const char* version;    ///< its manifest's version, MAJOR.MINOR.PATCH
  const wchar_t* folder;  ///< the copy of its assets folder for this game process; read-only
  const wchar_t* text;    ///< the copy of its i18n/default.json for this game process (core/text_table.h); may not exist
} PmContentPack;

/// PmContentPack's size as contract 2.6 defines it, the smallest entry PmHostApi::content_packs fills: a mod built for
/// 2.6 passes it as its item size, and the host fills that many bytes of each entry however the struct grows later.
/// @since Contract 2.6
constexpr uint32_t PM_CONTENT_PACK_SIZE_2_6 = static_cast<uint32_t>(offsetof(PmContentPack, text) + sizeof(const wchar_t*));

/// One entry of a managed Dictionary<K, int> (the host contract's dictionary_int_entries).
/// @since Contract 2.2
struct PmIntEntry {
  int32_t key;    ///< the entry's key, a 4-byte value such as an enum's number
  int32_t value;  ///< the entry's value
};

/// How much a line of the shared log matters: the `level` of PmHostApi::log_level, as SMAPI's LogLevel. mods.log shows
/// each line's level before its text, five letters wide ("WARN ").
/// @since Contract 2.7
enum PmLogLevel : uint32_t {
  PM_LOG_TRACE = 0,  ///< step-by-step detail for tracking down a problem
  PM_LOG_DEBUG,      ///< detail a mod's author reads while making the mod
  PM_LOG_INFO,       ///< what happened, for anyone reading the log: the level of PmHostApi::log
  PM_LOG_WARN,       ///< something went wrong, and the mod carries on
  PM_LOG_ERROR,      ///< something failed: a feature, or the whole mod, is off
  PM_LOG_ALERT,      ///< something the player should know about
};

/// What PmModEntry::flags says of an installed mod.
/// @since Contract 2.5
enum PmModEntryFlags : uint32_t {
  /// It runs on this client now (mod_started answers the same).
  PM_MOD_ENTRY_STARTED = 1,
  /// Its manifest marks it developerOnly (offered only with launcher.ini developer=1).
  PM_MOD_ENTRY_DEVELOPER_ONLY = 2,
  /// Its manifest says it records voice (recordsVoice).
  PM_MOD_ENTRY_RECORDS_VOICE = 4,
};

/// One installed mod, as the host contract's mod_entry gives it (SMAPI's IModRegistry.GetAll is the precedent). The
/// strings are UTF-8 and the host's own: they last until the mods are next listed afresh (a start of the mods) or the
/// next mod_entry for the same index, whichever comes first; copy them to keep them. Grows only at the end: set `size`
/// before asking, and the host fills only what fits in it.
/// @since Contract 2.5
struct PmModEntry {
  uint32_t size;          ///< set by the mod to sizeof(PmModEntry) before it asks
  const char* id;         ///< the manifest's id, `<author>.<mod>`
  const char* name;       ///< the manifest's name, the Mods panel's
  const char* version;    ///< the manifest's version; empty when an old-format mod gave none
  const char* author;     ///< the manifest's author; empty for an old-format mod
  uint32_t flags;         ///< PmModEntryFlags bits
  const char* offReason;  ///< why the last start left it off or refused it, the Mods panel's words; empty while it runs
};

/// What the host gives a mod: the services of the host contract, passed to pmmod_start. The host keeps
/// it for the process lifetime, so a mod may keep the pointer. Members only grow at the end; check `version` before using a
/// member added after the version a mod needs. A mod built on the game API reaches most of these
/// through its wrappers (sdk/api).
/// @since Contract 1.0
struct PmHostApi {
  /// PMMOD_API_VERSION
  /// @since Contract 1.0
  uint32_t version;
  /// HMODULE, already verified against the audited build
  /// @since Contract 1.0
  void* gameAssembly;
  /// HMODULE, already verified against the audited build
  /// @since Contract 1.0
  void* unityPlayer;
  /// The mods' data folder, `%LOCALAPPDATA%\PhasmoMods`, which exists.
  /// @since Contract 1.0
  const wchar_t* dataDirectory;

  /// One line into the shared log (the mod names itself in it), at the info level; log_level (version 2.7) takes a
  /// level. Any thread.
  /// @since Contract 1.0
  void (*log)(const char* line);
  /// A MinHook detour through the host's single MinHook instance, not yet enabled. A target that is
  /// already hooked (by the host or another mod) is chained: the newest detour runs first and its
  /// `original` reaches the previous one. Since 2.3 a hook that cannot be made or switched on during a mod's
  /// pmmod_start keeps that mod off, whatever pmmod_start returns, as every host call that fails then does
  /// (hook_entry, hook_body, enable_hooks, register_service): the host stops the mod, removes its hooks and
  /// content, and logs why.
  /// @since Contract 1.0
  bool (*create_hook)(void* target, void* detour, void** original, const char* name);
  /// enable every hook created so far
  /// @since Contract 1.0
  bool (*enable_hooks)(void);
  /// A message on the mod's own channel: the one it declared, or, when it declared 0, the one derived from its id
  /// (see PmModInfo::channel). At most PM_MAX_MESSAGE bytes. `actor` is used with PM_TO_ACTOR.
  /// @since Contract 1.0
  bool (*send)(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor);
  /// Null stops receiving. A message reaches it only from a verified player, on a client that matches the host; anything else
  /// is dropped, and a send goes only to verified players (sdk/README.md, "The host's list").
  /// @since Contract 1.0
  void (*set_receiver)(uint8_t channel, PmReceiver receiver);
  /// Whether the client of `actor` in this room is verified and the host's list has the mod `modId`: a verified player's mods
  /// equal that list (sdk/README.md, "The host's list"). Kept so 2.x mods load. A mod has no need of it: the loader
  /// keeps every player in a contract to the same mods, and a mod does no matching of its own.
  /// @since Contract 1.0
  /// @deprecated Deprecated in contract 2.2: a mod has no need of it, since the loader keeps every player in a contract to the host's list. It stays so mods built for 2.x load.
  bool (*peer_has_mod)(int32_t actor, const char* modId);
  /// Named tables of C function pointers that cooperating mods publish for each other. A table must
  /// stay valid for the process lifetime. A name is published once: registering a name already
  /// registered is refused (logged) and the first table stays; during a mod's pmmod_start that keeps the mod
  /// off (since 2.3). service returns null for a name nobody registered.
  /// @since Contract 1.0
  void (*register_service)(const char* name, const void* table);
  /// The table a cooperating mod published under `name` with register_service, or null when nobody
  /// registered that name.
  /// @since Contract 1.0
  const void* (*service)(const char* name);

  /// @name Game entries and hooks
  /// Added in version 2.0. Mods reach these through the game API (sdk/api/game_api.h).
  /// @since Contract 2.0
  /// @{

  /// The game entry `key` names (its map key, e.g. "GhostAI.ChangeState"), or null when the map has
  /// no such entry, it did not resolve in this game build, or it is a GameAssembly export or a native
  /// site in UnityPlayer.dll (only the host may use one, so a mod that requires one is never started).
  /// @since Contract 2.0
  const PmGameEntry* (*game_entry)(const char* key);
  /// A MinHook detour on the method entry `key` names, not yet enabled (enable_hooks enables it).
  /// Without `allTwins` it is hook_body on body 0, the canonical body. With `allTwins` the one
  /// `detour` is hooked on every body, each in that body's own chain, and `original` receives body
  /// 0's: a call that entered through a twin runs this detour and then continues on the canonical
  /// body's chain, skipping the detours older than this one on the twin and running those on the
  /// canonical body instead. Twins are the same behaviour, so the game is unaffected, but mods may
  /// be: use hook_body (version 2.1), which keeps every call on its own body's chain. Nothing is
  /// hooked unless every body is. False when the entry is unknown, unresolved or not a method, or when
  /// the host cannot hook it (it logs that).
  /// @since Contract 2.0
  bool (*hook_entry)(const char* key, void* detour, void** original, const char* name, bool allTwins);
  /// Stores the managed reference `value` into the managed object `obj` at `offset` through the
  /// runtime's GC write barrier: the only correct way to write a reference field.
  /// @since Contract 2.0
  void (*set_reference)(void* obj, int32_t offset, void* value);
  /// How many calls of the method entry `key` names (any twin) are running on the calling thread, as
  /// the host's hook on it counts them; 0 when the host does not track that entry.
  /// @since Contract 2.0
  int32_t (*inside_entry)(const char* key);

  /// @}
  /// @name Runtime services
  /// Added in version 2.1: the runtime services. Only the host knows the runtime's object layouts;
  /// mods reach managed strings, arrays, lists and objects through these (sdk/api/runtime.h). Every
  /// one runs on the main thread, never throws, and answers null, 0 or false for a null argument and
  /// for a key that is unknown, unresolved or of the wrong kind. `typeKey` and `methodKey` are map
  /// keys of a Type and a Method entry. The services that read a managed object check its kind
  /// first: one given an object of the wrong kind reads nothing from it, logs that once, and answers
  /// 0 or null.
  /// @since Contract 2.1
  /// @{

  /// A new managed string (System.String) holding the UTF-8 text `utf8`.
  /// @since Contract 2.1
  void* (*string_new)(const char* utf8);
  /// Copies the managed string `str` (a System.String) into `out` as NUL-terminated UTF-8, `size`
  /// bytes including the NUL, cut at a character boundary (a surrogate pair is one character; an
  /// unpaired one becomes U+FFFD). Returns the whole string's length in UTF-8 bytes without the NUL,
  /// so a caller can size its buffer. A null `out` is taken as `size` 0. A null `str` is the empty
  /// string; anything that is not a System.String answers 0 (logged once).
  /// @since Contract 2.1
  size_t (*string_utf8)(const void* str, char* out, size_t size);
  /// The number of elements of the managed array `arr` (any T[]); anything else answers 0 (logged
  /// once).
  /// @since Contract 2.1
  size_t (*array_length)(const void* arr);
  /// The address of element 0 of the managed array `arr`; elements follow at their type's size.
  /// Anything that is not an array answers null (logged once). The address is valid only until the
  /// next managed call: never store it.
  /// @since Contract 2.1
  void* (*array_data)(void* arr);
  /// A new managed array of `length` elements of the type `elementTypeKey` names, zeroed.
  /// @since Contract 2.1
  void* (*array_new)(const char* elementTypeKey, size_t length);
  /// The number of items in the managed System.Collections.Generic.List<T> `list`; anything else
  /// answers 0 (logged once).
  /// @since Contract 2.1
  int32_t (*list_count)(const void* list);
  /// The managed array behind the List<T> `list` (its capacity may exceed list_count); anything else
  /// answers null (logged once). The List replaces this array when it grows, so it is valid only
  /// until the next managed call: take it again after any game call, and never store it.
  /// @since Contract 2.1
  void* (*list_items)(const void* list);
  /// The System.Type object of the type `typeKey` names (C#'s typeof).
  /// @since Contract 2.1
  void* (*type_object)(const char* typeKey);
  /// A new managed object of the class `typeKey` names, its fields zeroed. No constructor runs. `typeKey` is a Type
  /// entry's key, or (from Contract 2.5) an Inflated entry's, which makes an object of its generic instance (a
  /// List<UInt32> for List<UInt32>.ctor); an older host answers null for that.
  /// @since Contract 2.1
  void* (*object_new)(const char* typeKey);
  /// Whether the managed object `obj` is of the type `typeKey` names or of a class derived from it
  /// (following the class's parents).
  /// @since Contract 2.1
  bool (*is_instance)(const void* obj, const char* typeKey);
  /// UnityEngine.Object's implicit bool: false for null and for an engine object that was destroyed.
  /// @since Contract 2.1
  bool (*alive)(const void* unityObject);
  /// The method the managed object `obj` runs for the method entry `methodKey` (a C# virtual call):
  /// the override in obj's class, or the declared method when nothing overrides it or the method is
  /// not virtual. Null when `obj` is null or not of the entry's declaring type. The result is a
  /// MethodInfo*, passed as the call's last argument, and its first member is the address of its
  /// code.
  /// @since Contract 2.1
  const void* (*virtual_method)(const void* obj, const char* methodKey);
  /// A strong GC handle on the managed object `obj` (0 for null): the runtime keeps the object while
  /// native code keeps a pointer to it. Release it when the pointer is dropped.
  /// @since Contract 2.1
  uint32_t (*gc_hold)(void* obj);
  /// Releases a handle from gc_hold; 0 does nothing.
  /// @since Contract 2.1
  void (*gc_release)(uint32_t handle);
  /// Logs a caught managed exception (the first member of the thrown Il2CppExceptionWrapper): its
  /// class, message and stack trace, each line beginning with `where`.
  /// @since Contract 2.1
  void (*log_exception)(const char* where, const void* exception);

  /// @}
  /// @name Static fields
  /// Also added in version 2.1: static fields. `key` is the map key of a Field entry flagged static
  /// (kStatic). Reading or writing a static field first runs its class's static constructor when that
  /// has not run yet, as the game's own first use would, so call these only from code that runs while
  /// the game plays (a tick, a detour), on the main thread; the host never touches a static field
  /// while it resolves the map at startup. Each answers false without reading or writing the field
  /// for a null pointer, for a key that is unknown, unresolved or not a static field, when `size` is
  /// not the size of the field's value (a pointer's for a managed reference; this is checked before
  /// the class is initialised, so a refused call runs no static constructor), and when the class's
  /// initialisation throws (logged once per key).
  /// @since Contract 2.1
  /// @{

  /// Copies the static field's value, `size` bytes, into `out`. For a managed reference `out`
  /// receives the pointer.
  /// @since Contract 2.1
  bool (*static_get)(const char* key, void* out, size_t size);  // pm:not-game-data: the service, not a read
  /// Stores the `size` bytes at `in` into the static field. For a managed reference `in` points at
  /// the pointer, which is stored through the runtime's GC write barrier.
  /// @since Contract 2.1
  bool (*static_set)(const char* key, const void* in, size_t size);

  /// @}
  /// @name Hooks per body
  /// Also added in version 2.1: hooks per body. A method's bodies are its canonical body (index 0)
  /// and then each distinct twin (PmGameEntry::twins, less any folded onto an earlier body), in order.
  /// Each body has its own chain: the newest detour on a body runs first for the calls that enter
  /// through that body, and its `original` reaches the next detour of that body's chain, or the body
  /// itself. A detour that serves several bodies must therefore be a separate function per body, so it
  /// knows which body's `original` to call (the game API's hook<M> makes one per body).
  /// @since Contract 2.1
  /// @{

  /// A MinHook detour on body `bodyIndex` of the method entry `key`, not yet enabled (enable_hooks
  /// enables it), chained on that body alone. False when the entry is unknown, unresolved or not a
  /// method, when `bodyIndex` is not below body_count, or when the host cannot hook it (it logs that).
  /// @since Contract 2.1
  bool (*hook_body)(const char* key, uint32_t bodyIndex, void* detour, void** original, const char* name);
  /// How many bodies the method entry `key` has: 1 for a method without twins; 0 when the entry is
  /// unknown, unresolved or not a method.
  /// @since Contract 2.1
  uint32_t (*body_count)(const char* key);

  /// @}
  /// @name Scene and asset facts
  /// Also added in version 2.1: scene and asset facts, the entries of kind PM_ENTRY_SCENE and
  /// PM_ENTRY_ANIMATOR_PARAM (a layer is resolved when the host activates, and game_entry gives its
  /// index; so is a shader property since version 2.2, PM_ENTRY_SHADER_PROPERTY, with its id). Main
  /// thread, while the game plays. A fact that is missing where it is used is logged once per scene
  /// load, and the call answers null or false, so the feature that needs it stays off.
  /// @since Contract 2.1
  /// @{

  /// The Transform at the Scene entry `key`'s path under the Transform `root`, as Transform.Find finds
  /// it. One answer is kept per entry, for the last `root`, until the next scene load: the same `root`
  /// costs no search, another one (or the object found being destroyed) searches again. Null when the
  /// entry is unknown, unresolved or not a Scene entry, when `root` is null (at once, the kept answer
  /// untouched), destroyed or not a Transform, and when nothing is at the path.
  /// @since Contract 2.1
  void* (*scene_find)(const char* key, void* root);
  /// Whether the Animator `animator` has the parameter the AnimatorParam entry `key` names, read from
  /// the Animator's own parameter list the first time each Animator is asked (and again after a scene
  /// load); its hash (Animator.StringToHash) into `hash` when it has. False, `hash` untouched, when the
  /// entry is unknown, unresolved or not an AnimatorParam entry, when `animator` is null, destroyed or
  /// not an Animator, or when it lacks the parameter.
  /// @since Contract 2.1
  bool (*animator_param)(const char* key, const void* animator, int32_t* hash);

  /// @}
  /// @name Content
  /// Added in version 2.2: content (sdk/api/content.h). Main thread. A mod registers content only from
  /// its pmmod_start; registration only records a definition. Once every mod has started the host
  /// settles the content and puts it into the game. A ghost whose custom evidence no started mod registered is
  /// dropped, and so are rules on a custom ghost no started mod registered; since 2.3 the mod that registered either
  /// stays off for that start, logged, and the mods start again without it, so nothing is dropped from a mod that runs. Mods never choose a number: the host numbers custom ghost types from
  /// one past the game's largest ghost type, and custom evidence from one past its largest evidence
  /// (both read from the game at startup: 31 and 8 in the audited build), each in byte order of its id,
  /// over the content the room's host has. Content is active only in a private room, on the host, or on a client
  /// whose host has sent a content manifest (its ghost and evidence ids, each with a hash of its
  /// definition, on the host's own channel as players join) equal to this client's (a host not heard from yet
  /// keeps it off for up to 10 s, then it is off). Another player's content changes nothing for anyone: the loader
  /// removes whoever cannot match, and no contract starts until they are gone. So in a contract every
  /// client holds the same content and reaches the same numbers. Content also stays off when the game's
  /// ghost type and evidence enums cannot be read (logged at startup). Vanilla ghost types and evidence
  /// answer whenever those enums were read. The content goes when the mods stop or restart, and a
  /// mod's own content when its pmmod_start fails. The host keeps off a mod whose pmmod_start registered anything it
  /// refused, whatever pmmod_start returns: it stops the mod, removes its hooks and content, and logs why, so a mod
  /// need not check what a registration returned (since 2.3).
  /// @since Contract 2.2
  /// @{

  /// Records the ghost `def` and returns its handle (greater than 0), or 0 when it is refused: a call
  /// outside pmmod_start, a null `def`, a `size` smaller than version 2.2's, an id that is missing, has
  /// no '.', is longer than 255 bytes, begins with "vanilla." or is already registered, text without
  /// English (or with a null or repeated language, more than 64 languages, or a text over 64 KB), more
  /// than 64 tells, or an evidence that is neither vanilla nor "<mod id>.<name>", or is listed twice, and
  /// (since 2.3) any ghost while custom content is off on this client: the game's enums unreadable, or a game
  /// entry or hook the host's content needs missing. The log names the ghost and why, and the mod stays off.
  /// @since Contract 2.2
  int32_t (*ghost_register)(const PmGhostDef* def);
  /// Records the evidence `def` like ghost_register: its handle, or 0 (logged) for a call outside
  /// pmmod_start, a null `def`, a short `size`, a bad or repeated id, text refused as above, or (since 2.3)
  /// custom content off on this client; then the mod stays off.
  /// @since Contract 2.2
  int32_t (*evidence_register)(const PmEvidenceDef* def);
  /// The game type number of the ghost `ghostId` while content is active, and while a contract that
  /// loaded with content active holds it, until that contract ends, whatever the room's content state
  /// does meanwhile; else -1. A vanilla id ("vanilla.mare") answers its type whenever the game's enums
  /// were read; null or an unknown id answers -1.
  /// @since Contract 2.2
  int32_t (*content_type)(const char* ghostId);
  /// The id of the ghost type `type`: "vanilla.<its enum value's name in lower case>" for a vanilla type
  /// (for example "vanilla.mare" for 6; None is no ghost, and a value without a plain ASCII name has no
  /// id), a custom ghost's id while content is active or a contract holds it (as content_type), else
  /// null. The text stays valid until the content changes (the mods stop or restart).
  /// @since Contract 2.2
  const char* (*content_ghost_id)(int32_t type);
  /// The game value of the evidence `evidenceId`: the game's own for a vanilla id ("emf" is 1)
  /// whenever the game's enums were read, a custom evidence's number while content is active or a
  /// contract holds it (as content_type), else -1.
  /// @since Contract 2.2
  int32_t (*content_evidence)(const char* evidenceId);
  /// Whether custom content is in play: this client registered some and is the host or matches it (above). False
  /// outside a private room.
  /// @since Contract 2.2
  bool (*content_active)(void);
  /// Whether a player controls the contract's ghost, as the Playable Ghost mod reports it
  /// (playable-ghost.v1); false without it.
  /// @since Contract 2.2
  bool (*content_player_controlled)(void);

  /// @}
  /// @name Ghost types in the game
  /// Also added in version 2.2: ghost types in the game. While content is active the host puts each
  /// custom ghost into the game as a ghost type of its own: a GhostData in the contract's
  /// GhostCollection in its alphabetical place (the roll, the evidence given and the journal read
  /// it), its name for the game's name key, the custom type as the ghost's identity and the game's
  /// "no type" as its AI's behaviour type on every client, and the custom types in the list the Mimic
  /// copies from. Whether a contract has them is decided when it loads (content active then) and
  /// holds until it ends; they leave at its end and when the mods stop. In the lobby they follow the
  /// room: while content is active there the lists that offer ghost types (the difficulty menu's, the
  /// lobby journals' collection) get them, and lose them when it stops being active. Main thread.
  /// @since Contract 2.2
  /// @{

  /// Attaches `rules` (copied) to the ghost `ghostId`: a custom ghost's id or a vanilla one
  /// ("vanilla.mare"). Several mods may attach rules to one id; they run in the order attached. Returns
  /// a handle greater than 0, or 0 when refused (logged with the id and why): a call outside
  /// pmmod_start, a null id or `rules`, a `size` smaller than version 2.2's, an id that is neither
  /// "<mod id>.<name>" nor a vanilla id of the game's enums. Rules on a custom id that no started mod
  /// registered are dropped once every mod has started, and (since 2.3) the mod that attached them stays off, logged. A custom ghost's rules run in a
  /// contract that loaded while content was active, for the whole contract; a vanilla type's whenever
  /// the contract's ghost acts as it. No rules run outside a private room, nor while this client's mods do
  /// not match the host's list (sdk/runtime/mod_agreement.h). That answer holds from the contract's load until it ends, so a
  /// contract's rules never start partway through, and content stays pending while it is no.
  /// @since Contract 2.2
  int32_t (*ghost_rules)(const char* ghostId, const PmGhostRules* rules);
  /// The type the contract's ghost acts as on this client: its own type, or for a Mimic the type it has
  /// copied, a custom one included; -1 outside a contract or before the ghost spawns. The Mimic's copy
  /// happens on the host, which sends it to every client on its own channel; until it arrives a client
  /// answers from its own GhostInfo.
  /// @since Contract 2.2
  int32_t (*content_acting_as)(void);
  /// The contract's ghost's identity, the type the journal judges and the results name (a custom
  /// ghost's type, or the Mimic's while it copies); -1 outside a contract or before the ghost spawns.
  /// @since Contract 2.2
  int32_t (*content_identity)(void);

  /// @}
  /// @name The journal
  /// Also added in version 2.2: the journal. On every client's journals, other players' books
  /// included, in a contract and in the lobby alike (a contract's custom ghosts, else the lobby's while
  /// content is active there, so players read a custom ghost's page before and after a contract; when
  /// that stops, each journal is put back as the game made it), the host gives each custom ghost a
  /// guess button on the evidence spread's ghost page (a copy of the game's own, crossed out by its
  /// evidence filter and synced by its own controller; the grid's rows shortened so every ghost fits),
  /// an entry in the Ghosts tab's list in its alphabetical place, and a page of its own after the
  /// book's last page:
  /// its name, its description with its strength and weakness, its ability when it has one, and its
  /// tells, laid out as the game's own pages are. Pages are numbered in the order of the ghosts' type
  /// numbers, so they are the same on every client. Texts are in the player's language when the ghost
  /// has it, else English, and follow the game's language switch. Main thread.
  /// @since Contract 2.2
  /// @{

  /// Adds `count` lines to the journal page of the vanilla ghost `vanillaGhostId` ("vanilla.mare"),
  /// each in several languages (English required), after the page's own text. Several mods may add
  /// lines to one ghost; they show in the order added. They show in a private room only, in the lobby's
  /// journals and a contract's alike. Returns a
  /// handle greater than 0, or 0 when refused (logged with the id and why): a call outside
  /// pmmod_start, an id that is not one of the game's vanilla ghosts, null or no lines, more than 64
  /// lines, or a line refused as ghost_register refuses text. Like ghost_rules' rules, the lines show
  /// only while this client's mods match the host's list (sdk/runtime/mod_agreement.h), held from a contract's load until it
  /// ends; the host writes the pages again when that answer changes.
  /// @since Contract 2.2
  int32_t (*journal_lines)(const char* vanillaGhostId, const PmTextSet* lines, uint32_t count);

  /// @}
  /// @name Custom evidence in the game
  /// Also added in version 2.2: custom evidence in the game. A custom ghost gives a custom evidence it
  /// lists through the game's own evidence pick: its number stands in the ghost's GhostData, so a
  /// difficulty that gives fewer evidence withholds it exactly as it withholds the game's own, and the
  /// game sends what it gives to every client. On every client's journals, whenever the custom ghosts
  /// they show give a custom evidence (a contract's, or the lobby's while content is active there),
  /// that evidence gets a copy of the game's Spirit Box row after the
  /// game's rows on the evidence page: its name (PmEvidenceDef::name) and, under it, the line saying how
  /// it is found (PmEvidenceDef::journalLine), in the player's language when the evidence has it, else
  /// English; it is marked, synced to the other players and filters the ghost buttons as the game's own
  /// rows do, and a custom ghost's page names it as the game's pages name theirs. What reveals the
  /// evidence in the world stays its mod's job. Main thread.
  /// @since Contract 2.2
  /// @{

  /// How the contract's ghost changes the look of the evidence `evidenceId`: what its definition's
  /// evidenceLook answers (PmGhostDef), asked of the ghost by its identity (content_identity), so a
  /// Mimic that copies a ghost keeps the Mimic's look. Null for the evidence's own look: no ghost, a
  /// game ghost, a ghost without evidenceLook or one that answers null, and a null `evidenceId`. A
  /// contract that holds custom ghosts answers for its ghost until it ends. The text is the ghost mod's;
  /// copy it before the next call.
  /// @since Contract 2.2
  const char* (*content_evidence_look)(const char* evidenceId);

  /// Also added in version 2.2, after content_evidence_look: adds the managed reference `item` (null
  /// allowed) at the end of the List<T> `list`, through the Add of the list's own class, as the game's
  /// code adds to its lists; the List may replace its backing array. False, touching nothing, for a null
  /// `list`, an object that is not a List (logged once) or a List whose Add cannot be run. An item the
  /// List's element type does not take is the caller's error, as in C#. Main thread.
  /// @since Contract 2.2
  bool (*list_add)(void* list, void* item);

  /// Also added in version 2.2, after list_add: copies `src`'s settings onto `dst`, two objects of the
  /// class the Type entry `typeKey` resolved to (or of classes derived from it): every instance field
  /// that class itself declares as public and of a value type (a number, a bool, an enum or a struct),
  /// the settings Unity's inspector shows for a component of the class. A struct is copied whole, and
  /// each managed reference inside it is stored through the GC write barrier. Fields the class
  /// inherits, and its fields of a reference type, are left as they are. False, copying nothing, for an
  /// unknown or unresolved key, a null object, an object not of the class, or a setting whose type the
  /// runtime cannot say (logged once): a copy is whole or not made. Main thread.
  /// @since Contract 2.2
  bool (*copy_settings)(const char* typeKey, void* dst, const void* src);

  /// Also added in version 2.2, after copy_settings: the managed references `obj` holds in every
  /// instance field that the class the Type entry `typeKey` resolved to declares itself of the type the
  /// Type entry `fieldTypeKey` resolved to (with `arrayOf`, of an array of it), in the order the class
  /// lays them out, the first `max` of them (by layout, among all of them) into `out`. Returns how many
  /// such fields the class declares, which may be more than `max`; 0 for an unknown or unresolved key, a null `obj`, or an object not of
  /// the class. A class may declare several fields of one type that nothing in the game's code tells
  /// apart; this reads all of them without naming any. Main thread.
  /// @since Contract 2.2
  uint32_t (*fields_of_type)(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                             uint32_t max);

  /// @}
  /// @name A ghost a player controls
  /// Also added in version 2.2, after fields_of_type: the contract's ghost as a player controls it, for
  /// any registered ghost. Each asks the definition (PmGhostDef) of the type the ghost acts as on this
  /// client (content_acting_as): a custom type a contract that loaded with content holds, or one of the
  /// active content. A game type, no ghost, or a callback the definition leaves null answers as below.
  /// Main thread.
  /// @since Contract 2.2
  /// @{

  /// Host only: runs the definition's useAbility; false when it has none or could not.
  /// @since Contract 2.2
  bool (*content_use_ability)(void);
  /// The definition's ability name, or with `description` its description, in the player's language
  /// when it has it, else English; null for a ghost without an ability. Valid until the content changes.
  /// @since Contract 2.2
  const char* (*content_ability_text)(bool description);
  /// Host only: the definition's hudState, 0 to 255, or -1.
  /// @since Contract 2.2
  int32_t (*content_hud_state)(void);
  /// The definition's hudLine for `state`; null for none. Copy it before the next call.
  /// @since Contract 2.2
  const char* (*content_hud_line)(int32_t state);
  /// Whether the ghost may not act now as `kind` (as PmGhostDef.heldBack):
  /// the definition's heldBack, or the heldBack of any rules running for the type (ghost_rules; a
  /// vanilla type's too). False with no ghost.
  /// @since Contract 2.2
  bool (*content_held_back)(int32_t kind);
  /// The definition's playerView into `xyz` (room for `max` points); how many it wrote, 0 for none.
  /// @since Contract 2.2
  uint32_t (*content_player_view)(float* xyz, uint32_t max);
  /// The definition's target: the one player the ghost's hunts are held to now, never
  /// one of the `excludedCount` players at `excluded` (null and 0 for none), or null.
  /// @since Contract 2.2
  void* (*content_target)(const void* const* excluded, uint32_t excludedCount);
  /// The definition's flags (PM_GHOST_* bits); 0 with no definition.
  /// @since Contract 2.2
  uint32_t (*content_flags)(void);

  /// Also added in version 2.2, after content_flags: whether this client's running mods equal the host's list, in a private
  /// room (sdk/runtime/mod_agreement.h); held from a contract's load until it ends. It is the one answer the host gates the
  /// mods' content, rules and journal lines on. Kept so 2.x mods load: a mod has no need of it, since the loader removes
  /// or switches off whoever does not match, and no unverified player reaches a contract. Main thread.
  /// @since Contract 2.2
  /// @deprecated Deprecated in contract 2.2: a mod has no need of it, since the loader keeps every player in a contract to the host's list. It stays so mods built for 2.x load.
  bool (*room_agrees)(void);

  /// @}
  /// @name More runtime services
  /// Also added in version 2.2, after room_agrees: more runtime services (sdk/api/runtime.h), which read
  /// what they need from the runtime's metadata. Main thread; each answers 0 or false, touching nothing,
  /// for null and for an object of another kind (logged once).
  /// @since Contract 2.2
  /// @{

  /// The number of items in the managed collection `collection`, a System.Collections.Generic List<T>,
  /// HashSet<T>, Dictionary<K,V>, Queue<T> or Stack<T>, through its own get_Count, found by name on its
  /// class (a HashSet's count is not a List's).
  /// @since Contract 2.2
  int32_t (*collection_count)(const void* collection);
  /// The bytes one element of the managed array `arr` takes as the runtime lays it out: a value type's own
  /// (unboxed) size, as a List<T> of structs holds them, or a reference's.
  /// @since Contract 2.2
  size_t (*array_element_size)(const void* arr);
  /// Removes the item at `index` from the List<T> `list` through the RemoveAt of the list's own class, as
  /// the game's code removes from its lists; false for an index out of range or a RemoveAt that cannot be
  /// run.
  /// @since Contract 2.2
  bool (*list_remove_at)(void* list, int32_t index);

  /// Also added in version 2.2, after list_remove_at: host only, the definition's playerMove on the
  /// direction (`dx`, `dz`) a player steers the ghost in from `at` (x, y, z); it leaves them as they are
  /// without a definition or the callback. Main thread.
  /// @since Contract 2.2
  void (*content_player_move)(const float* at, float* dx, float* dz);
  /// Also added in version 2.2, after content_player_move: host only, the definition's interactionLost for an
  /// interaction a player has paid for and chosen; false without a definition or the callback. Asked once
  /// per interaction. Main thread.
  /// @since Contract 2.2
  bool (*content_interaction_lost)(void);
  /// Also added in version 2.2, after content_interaction_lost: whether a player controlling the ghost takes
  /// the actions of the game's idle roll running now, so a mod's rule on the AI's attempts stands aside
  /// whichever mod's hook runs first; as the Playable Ghost mod reports it (playable-ghost.v1, version 3),
  /// false without it. Main thread.
  /// @since Contract 2.2
  bool (*content_roll_taken)(void);

  /// Also added in version 2.2, after content_roll_taken: replaces the direction Unity's crowd walks the
  /// NavMeshAgent `agent` (a managed reference) in, until the host opens the next frame, on this client
  /// only: nothing is sent. (`dx`, `dz`) is a world direction of length at most 1, and (0, 0) holds the
  /// agent still as it decelerates. The agent's destination, speed, acceleration and avoidance stay the
  /// game's. A mod keeps an agent steered by asking in each pmmod_tick; a frame without an ask leaves
  /// the game's own direction. One agent at a time: the last ask of a frame counts, and a direction
  /// that is not one (longer than 1, not finite) ends the frame's steering until a valid ask after it.
  /// A null or destroyed `agent`, or an object that is not a NavMeshAgent, changes nothing. True when the
  /// host can steer, so a null `agent` asks only that; false when it cannot in this game build (the host
  /// logs why), and nothing was changed. The first ask makes the effect ready in the game's code; after a
  /// refusal the host tries again only when the mods start afresh. The game entries this needs are the
  /// host's own, and a mod requires none of them. Main thread, a null `agent` from pmmod_start, an agent
  /// from pmmod_tick.
  /// @since Contract 2.2
  bool (*steer_agent)(void* agent, float dx, float dz);

  /// Also added in version 2.2, after steer_agent: unwinds the running GhostController.Spawn, as a managed
  /// exception does, back to the mod's C++ catch around its call of it, so a mod can hold the setup back at
  /// its type roll, before it writes or sends anything. It does not return. Outside a running spawn it does
  /// nothing and returns (logged once). Main thread, from a hook the spawn reaches.
  /// @since Contract 2.2
  void (*abandon_ghost_spawn)(void);
  /// Also added in version 2.2, after abandon_ghost_spawn: the entries of the managed Dictionary<K, int>
  /// `dict`, whose K is a 4-byte value such as an enum, as the dictionary's own array keeps them: at most
  /// `max` into `out`, in that order, free slots left out. Returns how many it wrote; 0 for null and for an
  /// object of another kind (logged once). Main thread.
  /// @since Contract 2.2
  int32_t (*dictionary_int_entries)(const void* dict, PmIntEntry* out, int32_t max);
  /// Also added in version 2.2, after dictionary_int_entries: whether the mod `modId` (its manifest id) has started on this
  /// client and runs now. For an optional dependency (a manifest dependency with "required": false), asked from the
  /// mod's pmmod_start onwards. The host starts a mod after the dependencies it names, but one below this mod's
  /// minimumVersion, or on an edge that would close a cycle, gets no place in the order and may start after this mod, so
  /// do not keep the answer from pmmod_start: ask when it is needed. It says nothing of the other mod's version, nor of
  /// other players' mods (the loader keeps them to the host's list). False for null and for a mod not installed, off, not started
  /// yet or already stopped. Main thread.
  /// @since Contract 2.2
  bool (*mod_started)(const char* modId);
  /// Also added in version 2.2, after mod_started: a one-button message in the game's own popup on the menu's and the lobby's
  /// board, with `title` and `body` (UTF-8, the game's rich text). It waits until the board is in view, however long that takes,
  /// so one asked for during a contract shows once the lobby is back; it shows until it is clicked. It replaces any message
  /// showing or waiting, the host's own included, so keep it for news a player needs. Nothing shows in a build whose popup
  /// cannot be copied (logged once). Null `title` or `body`: nothing. The loader draws no window of its own (vanilla UI only).
  /// Main thread.
  /// @since Contract 2.2
  void (*show_message)(const char* title, const char* body);
  /// Added in version 2.3, after show_message: the flags (PmGhostDef.flags, PM_GHOST_* bits) of the custom ghost whose
  /// game value is `type`, while content is active or a contract holds it; 0 for a vanilla type, an unknown one and
  /// while no content is active. Read-only. Main thread.
  /// @since Contract 2.3
  uint32_t (*content_flags_of)(int32_t type);
  /// Added in version 2.3, after content_flags_of: the name the manifest of the installed mod `modId` gives it (UTF-8),
  /// the name the Mods panel shows the player, for a heading of the mod's own in a game menu. Null for null and for a mod
  /// not installed. The text lasts until the mods next start afresh: copy it to keep it. Main thread.
  /// @since Contract 2.3
  const char* (*mod_name)(const char* modId);
  /// Added in version 2.4, after mod_name: a UnityAction (the game's delegate type for a no-argument UI event),
  /// made the way the game makes its own listeners: bound to `target`, which must be a managed object (a
  /// UnityEvent skips a listener whose target is a destroyed Unity object; key your own state off the `target`
  /// the listener is called with), and calling `listener` when invoked. `listener` runs inside the game's
  /// managed call on the main thread: it must not throw. Null, making nothing, when the game's UnityAction
  /// constructor or Invoke did not resolve or `target` or `listener` is null. Keep the action alive as long as
  /// the event should call it; the mod adds it to the event with `phasmo::UnityEvent::AddListener`. Main thread.
  /// @since Contract 2.4
  void* (*unity_action_new)(void* target, void (*listener)(void* target));
  /// Added in version 2.5, after unity_action_new: the installed mod at `index` (0 up), in the host's own order, into
  /// `out`, whose `size` the mod sets first; the host fills only the members that fit in it and leaves `size` as it was.
  /// Every installed mod is listed, running or not, the asking mod included. Read-only: it changes nothing, and says
  /// nothing of other players' mods. False, writing nothing, past the last mod, for a null `out` or a `size` too small
  /// for `id`. Main thread.
  /// @since Contract 2.5
  bool (*mod_entry)(int32_t index, PmModEntry* out);
  /// @}
  /// @name Content packs
  /// Added in version 2.6, after mod_entry: the content packs made for a mod (PmContentPack).
  /// @since Contract 2.6
  /// @{

  /// Fills `out` with the content packs made for the mod `modId` that this start of the mods starts: each pack enabled in
  /// the Mods panel whose dependencies are met, whose framework is `modId`, and which the framework has not reported
  /// failed. A pack starts just after its framework, so a framework asks for its packs from its own pmmod_start and
  /// finds them there. In id order. Each entry is `itemSize` bytes (the caller's sizeof(PmContentPack)); the host
  /// writes at most that much of each, and sets its `size`. Returns how many packs there are, which may be more than
  /// `max`. 0 for a null `modId` or `itemSize` smaller than version 2.6's struct. Main thread.
  /// @since Contract 2.6
  uint32_t (*content_packs)(const char* modId, PmContentPack* out, uint32_t max, uint32_t itemSize);
  /// From a framework's pmmod_start: the content pack `packId` cannot be used (its data is broken, say), for `why`
  /// (UTF-8, for the Mods panel). The pack does not start in this start, its Mods panel row says why, and so it is not in
  /// the lobby's list of mods. Outside a pmmod_start, or for a pack that already started, it is logged and does nothing.
  /// Main thread.
  /// @since Contract 2.6
  void (*content_pack_failed)(const char* packId, const char* why);
  /// @}
  /// @name Log levels
  /// Added in version 2.7, after content_pack_failed: a level for each line of the shared log.
  /// @since Contract 2.7
  /// @{

  /// One line into the shared log at `level` (PmLogLevel), as SMAPI's Monitor.Log: mods.log shows the level before the
  /// line. log writes at PM_LOG_INFO. A level past PM_LOG_ALERT is written as info; a null `line` writes nothing. A mod
  /// built on core/log.h reaches it through PM_LOG_AT, which falls back to log, with the level's word in the text, on
  /// an older host. Any thread, like log.
  /// @since Contract 2.7
  void (*log_level)(uint32_t level, const char* line);
  /// @}
};

/// What a mod declares of itself in PmModInfo::flags.
/// @since Contract 2.0
enum PmModFlags : uint32_t {
  /// Keeps other players' speech in memory during a contract. The manifest's recordsVoice must say so too: the notice reads
  /// that, before the DLL loads, and the host keeps off a DLL that sets this under a manifest that does not.
  PM_MOD_RECORDS_VOICE = 1,
  /// Reserved: a mod built for contract 2.0 to 2.2 may set it, and it still loads. The host ignores it, because every mod is
  /// required by every player in a lobby (host/join_guard.h). A new mod leaves it clear.
  PM_MOD_VANILLA_PEERS = 2,
};

/// What a mod says of itself: pmmod_info returns it, and the host reads it when it loads the DLL.
/// @since Contract 1.0
struct PmModInfo {
  uint32_t apiVersion;  ///< PMMOD_API_VERSION the mod was built against
  const char* id;  ///< `<author>.<mod>`, as in its manifest.json, e.g. "phasmomods.playable-ghost"
  const char* name;  ///< for the log; the manifest's name is what the player reads
  /// its own network channel. Declare 0: the loader gives the mod one from 64 to 255, derived from a stable
  /// hash of its id (core/mod_ids.h hashed_channel), so every client agrees on it without negotiating; a mod
  /// declaring 64 to 255 is refused. 1 to 63 are reserved: only the loader's own bundled mods, whose id author
  /// is the project's, may name one, and they keep their fixed numbers. The loader settles channels across every
  /// planned mod before any of them starts: every mod on a channel two or more mods claim is refused, three or
  /// more included, each with a message naming the others. A mod already running keeps its channel: a newcomer
  /// that lands on it is refused, naming it.
  uint8_t channel;
  uint32_t flags;  ///< PmModFlags
};

// The exports each mod DLL provides. The host loads the DLL when the mod first starts, after the player agreed to the
// notice, never before; it reads these then. Do no work in DllMain. A loaded DLL stays mapped until the game exits, even
// if the host refuses it, because unloading one is not safe.

/// The export "pmmod_info": the mod's PmModInfo. The host keeps the pointer and reads it again later, so it points
/// at data that lives as long as the DLL (a static). Callable at any time.
/// @since Contract 1.0
typedef const PmModInfo* (*PmModInfoFn)(void);
/// The export "pmmod_start": called once, after the player agreed to the notice, with the host's PmHostApi, on the
/// main thread. Register content and hooks here. Returning false leaves the mod inactive, and the host withdraws
/// the content it registered.
/// @since Contract 1.0
typedef bool (*PmModStartFn)(const PmHostApi*);
/// The export "pmmod_tick": called every frame once the mod started, on the main thread.
/// @since Contract 1.0
typedef void (*PmModTickFn)(void);
/// The export "pmmod_stop": called once, when the host stops the mods, on the main thread.
/// @since Contract 1.0
typedef void (*PmModStopFn)(void);
/// The export "pmmod_requires" (optional; the game API's glue exports it): the keys of the game
/// entries the mod uses that are not marked optional. It points `keys` at an array the mod keeps,
/// unchanged, for as long as its DLL stays loaded, and returns its length. Callable at any time. The
/// host does not start a mod while any of these did not resolve, and logs which. A mod without this
/// export requires no game entry.
/// @since Contract 2.0
typedef size_t (*PmModRequiresFn)(const char* const** keys);

/// @}
}
