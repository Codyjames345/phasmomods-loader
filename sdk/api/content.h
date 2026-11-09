#pragma once

/// @file
/// @brief Content: the ghosts and evidence a mod adds to the game, through the host contract's
/// content functions (core/mod_api.h, version 2.2).
///
/// A mod registers its content in pmmod_start; the host numbers it, puts it into the game and keeps it
/// out of saves and away from players without it. Mods never choose a number: a ghost or evidence is
/// named by its stable id, `"<mod id>.<name>"`, and its number is asked for when it is needed
/// (type_of, evidence_of), since it is known only while content is active or while a contract that
/// loaded with content holds it, and may differ from one lobby to the next.
///
/// @code
/// pm::api::content::EvidenceDef flame;
/// flame.id = "phasmomods.spectral-flame.flame";
/// flame.name = {"en", "Spectral Flame"};
/// flame.journalLine = {"en", "Watch a placed candle or lantern burn blue"};
/// pm::api::content::register_evidence(flame);
/// @endcode
///
/// Content is active only in a private room, on the room's host and on a client whose content and
/// mods equal the host's (sdk/README.md, "The host's list"; core/mod_api.h describes the lobby rule).
/// That is the one answer for every mod, held from a contract's load until it ends: a mod's content,
/// its rules and its journal lines all wait on it, so a mod never checks the other players itself.
/// While content is active in the lobby, the lobby's journals show the custom ghosts and evidence as a
/// contract's do. Threading: Unity's main thread, like the rest of the game API. Before pm::api::bind
/// every function answers as inactive: 0, -1, empty or false.
/// @ingroup content
/// @since Contract 2.2

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mod_api.h"

namespace pm::api::content {

/// A piece of text in several languages, keyed by the game's language codes. English ("en") is
/// required wherever text is given; a language the game asks for and the text lacks shows English.
class Text {
 public:
  /// An empty text, in no language.
  Text() = default;
  /// The text `text` in the language `lang`.
  /// @param lang A language code of the game's own, such as "en" (see PmText::lang).
  /// @param text UTF-8, at most 64 KB.
  Text(std::string lang, std::string text) { add(std::move(lang), std::move(text)); }
  /// Adds the text in another language, and returns this text for the next.
  /// @param lang A language code of the game's own, named once in a text.
  /// @param text UTF-8, at most 64 KB.
  /// @return This text.
  /// @code
  /// // Both strings come from the mod's own string table (docs/player-text.md).
  /// pm::api::content::Text name = pm::api::content::Text("en", english).add(code, translated);
  /// @endcode
  Text& add(std::string lang, std::string text) {
    items_.emplace_back(std::move(lang), std::move(text));
    return *this;
  }
  /// Whether it holds no language at all.
  /// @return True for a text with no language.
  bool empty() const { return items_.empty(); }
  /// The C view of it, valid until this text is changed, copied or destroyed.
  /// @return The PmTextSet the host contract takes.
  PmTextSet c_set() const;

 private:
  std::vector<std::pair<std::string, std::string>> items_;
  mutable std::vector<PmText> view_;
};

/// A custom ghost, as it is registered with register_ghost. Each member is the C++ form of the member
/// of the same name in PmGhostDef (core/mod_api.h), whose note says in full when the host calls it and
/// on which client. Every callback is optional (null) and runs on the main thread with `user`.
/// @see adding-a-ghost.md, the guide that builds sdk/examples/example_ghost.
struct GhostDef {
  std::string id;  ///< `"<mod id>.<name>"`, stable across versions (PmGhostDef::id)
  Text name;         ///< its name, with English (PmGhostDef::name)
  Text description;  ///< its journal page's description, with English
  Text strength;     ///< its journal page's strength, with English
  Text weakness;     ///< its journal page's weakness, with English
  std::vector<Text> tells;  ///< its tells, one journal line each, at most 64 (PmGhostDef::tells)
  /// Its three evidence: vanilla ids ("emf", "spirit_box", "ultraviolet", "orbs", "writing", "freezing",
  /// "dots") or custom evidence ids; "" for an empty slot, and all three may be (PmGhostDef::evidence).
  std::array<std::string, 3> evidence;
  Text abilityName;         ///< its ability's name for a player who controls it; both empty: no ability
  Text abilityDescription;  ///< its ability's description; both empty: no ability
  void* user = nullptr;     ///< handed to every callback
  /// Host only: uses the ability now; false when it could not (PmGhostDef::useAbility).
  bool (*useAbility)(void* user) = nullptr;
  /// Host only: its state as its player feels it, 0 to 255, or -1 for none (PmGhostDef::hudState).
  int32_t (*hudState)(void* user) = nullptr;
  /// Its own rules forbid acting now as `kind`, the kinds PmGhostDef::heldBack lists.
  bool (*heldBack)(int32_t kind, void* user) = nullptr;
  /// Points (x, y, z triples, room for `max`) only the ghost player sees; returns how many it wrote
  /// (PmGhostDef::playerView).
  uint32_t (*playerView)(float* xyz, uint32_t max, void* user) = nullptr;
  /// How it changes the look of the evidence `evidenceId` (for example "green"), or null for the
  /// evidence's own look (PmGhostDef::evidenceLook).
  const char* (*evidenceLook)(const char* evidenceId, void* user) = nullptr;
  /// What sets it apart in the game's own rules, PM_GHOST_* bits ORed together (core/mod_api.h): 0 for none.
  /// PM_GHOST_KEEPS_LAST_EVIDENCE: a difficulty never withholds its third evidence. PM_GHOST_BLIND_IN_HUNTS: a player
  /// controlling it sees only its playerView while it hunts. PM_GHOST_NO_MIMIC_COPY (since 2.3): a Mimic never copies
  /// it; the host leaves it off the Mimic's copy list. PM_GHOST_NOT_PLAYABLE (since 2.3): no player ever plays it; a
  /// mod that lets a player be the ghost (Playable Ghost) reads it with flags_of and never offers, rolls or keeps it
  /// for them. Read once, when it is registered.
  uint32_t flags = 0;
  /// Its one target, to which the host holds each of its hunts as the game holds its Banshee's, for its AI
  /// and a player controlling it alike: never dead nor one of the `excludedCount` players at `excluded` (null
  /// and 0 for none), or null. Asked at each hunt's start and wherever the game gives a Banshee a new target
  /// (PmGhostDef::target).
  void* (*target)(const void* const* excluded, uint32_t excludedCount, void* user) = nullptr;
  /// Its player's HUD line for a hudState, asked on that player's client; null for none (PmGhostDef::hudLine).
  const char* (*hudLine)(int32_t state, void* user) = nullptr;
  /// Host only: its rules' limit on the direction (dx, dz) a player steers it in from `at` (x, y, z), which it
  /// may shorten, turn or set to 0; null for none (PmGhostDef::playerMove).
  void (*playerMove)(const float* at, float* dx, float* dz, void* user) = nullptr;
  /// Host only, once for each interaction a player has paid for and chosen, just before it happens: true when
  /// its own rules lose this one, as they lose a share of its AI's attempts; it stays paid for. May roll
  /// (PmGhostDef::interactionLost).
  bool (*interactionLost)(void* user) = nullptr;

  /// The C definition, valid until this definition is changed, copied or destroyed. The host copies
  /// what it needs when it is registered.
  /// @return The PmGhostDef the host contract takes.
  const PmGhostDef& c_def() const;

 private:
  mutable PmGhostDef def_{};
  mutable std::vector<PmTextSet> tellSets_;
};

/// A custom evidence, as it is registered with register_evidence (PmEvidenceDef in core/mod_api.h).
/// @see adding-evidence.md, the guide with Spectral Flame as its worked case.
struct EvidenceDef {
  std::string id;    ///< `"<mod id>.<name>"`, stable across versions
  Text name;         ///< its name: on its journal row and on the pages of the ghosts that give it
  Text journalLine;  ///< the line under its name on its journal row, saying how it is found

  /// The C definition, valid until this definition is changed, copied or destroyed.
  /// @return The PmEvidenceDef the host contract takes.
  const PmEvidenceDef& c_def() const;

 private:
  mutable PmEvidenceDef def_{};
};

/// A ghost type's rules, attached with attach_rules (PmGhostRules in core/mod_api.h says when each
/// runs). Every callback is optional and runs on the main thread with `user`.
/// @see tells-for-vanilla-ghosts.md, the guide to rules on the game's own ghosts.
struct GhostRules {
  void* user = nullptr;  ///< handed to every callback
  /// The ghost begins acting as the type, on every client (PmGhostRules::start).
  void (*start)(void* user) = nullptr;
  /// Every frame while it acts as the type, after start, on every client (PmGhostRules::tick).
  void (*tick)(void* user) = nullptr;
  /// It stops: another type, the contract's end, content withdrawn, the mods stopping (PmGhostRules::stop).
  void (*stop)(void* user) = nullptr;
  /// Host only, every frame of a hunt before the game sets the hunt's speed: change `base` (metres a
  /// second) and `losScale` (1 to 1.65) and return true to have the host write them, or return false
  /// for the game's own (PmGhostRules::huntSpeed).
  bool (*huntSpeed)(float* base, float* losScale, void* user) = nullptr;
  /// The type's own rules forbid acting now as `kind`, the kinds PmGhostDef::heldBack lists.
  bool (*heldBack)(int32_t kind, void* user) = nullptr;

  /// The C rules, valid until these rules are changed, copied or destroyed. The host copies them.
  /// @return The PmGhostRules the host contract takes.
  const PmGhostRules& c_rules() const;

 private:
  mutable PmGhostRules rules_{};
};

/// Registers the ghost `def`, from pmmod_start only. Returns its handle, greater than 0, or 0 when the
/// host refused it (outside pmmod_start, a repeated or malformed id, text without English or over the
/// limits in core/mod_api.h, or custom content off on this client; the log says which) and before bind. The host keeps
/// off a mod whose start registered anything it refused, whatever the start returns, so checking the handle is
/// optional (since 2.3). A custom evidence it gives may be
/// registered by any mod that starts, before or after it; once every mod has started, a ghost whose
/// evidence nobody registered is dropped, and the mod that registered it stays off for that start (logged).
/// @param def The ghost. The host copies it; `user` and the callbacks must stay valid while the mod runs.
/// @return Its handle, greater than 0, or 0 when refused.
/// @pre Main thread, from pmmod_start.
/// @code
/// // From sdk/examples/example_ghost/ghost.h.
/// pm::api::content::GhostDef def;
/// def.id = "example.ghost.knell";
/// def.name = {"en", pm::i18n::text("ghost.knell.name")};
/// def.evidence = {"spirit_box", "orbs", "phasmomods.spectral-flame.flame"};
/// if (pm::api::content::register_ghost(def) <= 0) return false;
/// @endcode
int32_t register_ghost(const GhostDef& def);
/// Registers the evidence `def` likewise, from pmmod_start only. As for register_ghost, the host keeps off a mod whose
/// start registered anything it refused, so checking the handle is optional.
/// @param def The evidence; the host copies it.
/// @return Its handle, greater than 0, or 0 when refused (logged) and before bind.
/// @pre Main thread, from pmmod_start.
int32_t register_evidence(const EvidenceDef& def);

/// The game type number of the ghost `ghostId` while content is active, and while a contract that
/// loaded with content active holds it (until it ends, whatever the room's content state does), else
/// -1. A vanilla id ("vanilla.mare") answers its type whenever the game API is bound.
/// @param ghostId A ghost's id, custom or vanilla.
/// @return The GhostType number, the same on every client in a contract, or -1. Never store it past
///   the lobby: it may differ in the next.
int32_t type_of(std::string_view ghostId);
/// The id of the ghost type `type`: "vanilla.<name>" for a vanilla type (None is no ghost), a
/// custom ghost's id while content is active or a contract holds it (as type_of), else empty.
/// @param type A GhostType number, as the game or type_of gives it.
/// @return The ghost's id, or empty.
std::string id_of(int32_t type);
/// The game value of the evidence `evidenceId`: the game's own for a vanilla id ("emf" is 1), a
/// custom evidence's while content is active or a contract holds it (as type_of), else -1.
/// @param evidenceId An evidence's id, vanilla ("emf" ... "dots") or custom.
/// @return The EvidenceType number, or -1.
int32_t evidence_of(std::string_view evidenceId);
/// Whether custom content is in play on this client: false outside a private room, and until this client is the host or
/// matches it. A joiner's mods never change this for anyone else (sdk/README.md, "The host's list").
/// @return True while content is active on this client.
bool active();
/// Whether a player controls the contract's ghost (the Playable Ghost mod); false without it.
/// @return True while a player controls the ghost.
bool player_controlled();
/// Whether this client's running mods equal the host's list, in a private room; held through a contract. The host gates
/// content, rules and journal lines on it already, and a mod has no need of it: it stays so 2.x mods load. False before
/// bind.
/// @return True while this client's mods match the host's.
/// @deprecated Deprecated in contract 2.2: a mod has no need of it, since the loader keeps every player in a contract to the host's list. It stays so mods built for 2.x load.
bool room_agrees();

/// Attaches `rules` to the ghost `ghostId`, a custom ghost's id or a vanilla one ("vanilla.mare"),
/// from pmmod_start only. They run while the contract's ghost acts as that type (its own type, or a
/// Mimic's copy of it) and this client's mods match the host's list, an answer that holds from the contract's load until
/// it ends, so rules never start partway through a contract. Returns a handle greater than 0, or 0 when the host refused them (logged: core/mod_api.h,
/// ghost_rules) and before bind.
/// @param ghostId A custom ghost's id or a vanilla one.
/// @param rules The rules; the host copies them, and `user` and the callbacks must stay valid while
///   the mod runs.
/// @return A handle greater than 0, or 0 when refused.
/// @pre Main thread, from pmmod_start.
int32_t attach_rules(std::string_view ghostId, const GhostRules& rules);
/// The type the contract's ghost acts as on this client (a Mimic's copy, custom ones included), or -1
/// outside a contract, before the ghost spawns and before bind.
/// @return A GhostType number, or -1.
int32_t acting_as();
/// The contract's ghost's identity (what the journal judges), or -1 outside a contract, before the
/// ghost spawns and before bind.
/// @return A GhostType number, or -1.
int32_t identity();

/// Adds `lines` to the journal page of the vanilla ghost `vanillaGhostId` ("vanilla.mare"), after the
/// page's own text, from pmmod_start only; each line is text in several languages, English required.
/// Several mods may add lines to one ghost. They show in a private room only, in the lobby's journals
/// and a contract's alike, and only while this client's mods match the host's list (held from a contract's load until it
/// ends), in the player's language when
/// the line has it, else English. Returns a handle greater than 0, or 0 when the host refused them
/// (logged: core/mod_api.h, journal_lines) and before bind.
/// @param vanillaGhostId A vanilla ghost's id, such as "vanilla.mare".
/// @param lines The lines, at most 64, each with English.
/// @return A handle greater than 0, or 0 when refused.
/// @pre Main thread, from pmmod_start.
int32_t add_journal_lines(std::string_view vanillaGhostId, const std::vector<Text>& lines);

/// How the contract's ghost changes the look of the evidence `evidenceId` (its GhostDef::evidenceLook,
/// e.g. "green" for the Hydra's Spectral Flame), asked by the ghost's identity: a Mimic copying a ghost
/// keeps the Mimic's look. Empty for the evidence's own look: no ghost, a game ghost, a ghost without
/// evidenceLook, and before bind. A contract that holds custom ghosts answers for its ghost until it
/// ends, whatever the room's content state does meanwhile.
/// @param evidenceId An evidence's id.
/// @return The look's name, or empty.
std::string evidence_look(std::string_view evidenceId);

// The contract's ghost as a player controls it (Playable Ghost), for any registered ghost: each asks the
// definition (GhostDef) of the type the ghost acts as on this client (acting_as), a custom type a contract
// that loaded with content holds or one of the active content. A game type, no ghost, a callback the
// definition leaves null, and a call before bind answer as inactive: false, empty, -1, 0 or null.

/// Host only: uses the ghost's ability (GhostDef::useAbility); false when it has none or could not.
/// @return True when the ability was used.
bool use_ability();
/// The ghost's ability name, in the player's language when it has it, else English; empty for a ghost
/// without an ability.
/// @return The name, or empty.
std::string ability_name();
/// The ghost's ability description, in the player's language when it has it, else English; empty for
/// a ghost without an ability.
/// @return The description, or empty.
std::string ability_description();
/// Host only: the ghost's state as its player feels it (GhostDef::hudState), 0 to 255, or -1.
/// @return The state, or -1.
int32_t hud_state();
/// The HUD line for the state `state` that hud_state answered on the host (GhostDef::hudLine), asked on
/// the client of the player controlling the ghost; empty for none.
/// @param state A state hud_state answered.
/// @return The line, in the game's rich text, or empty.
std::string hud_line(int32_t state);
/// Whether the ghost may not act now as `kind` (0: an interaction, 1: an event or its ability, 2: a hunt, 3: its
/// ability alone, 4: the fuse box): its
/// definition's heldBack, or the heldBack of any rules running for its type, a vanilla type's included
/// (attach_rules).
/// @param kind The kind of act.
/// @return True when the ghost may not act so now.
bool held_back(int32_t kind);
/// The points only the ghost's player sees (GhostDef::playerView) into `xyz`, room for `max`; how many
/// it wrote.
/// @param xyz Room for `max` points, as x, y, z triples in world space.
/// @param max How many points `xyz` takes.
/// @return How many points were written.
uint32_t player_view(float* xyz, uint32_t max);
/// The one player the ghost's hunts are held to now (GhostDef::target), never
/// one of the `excludedCount` players at `excluded` (null and 0 for none), or null for no restriction.
/// @param excluded Players (Player objects) the answer may not be; null for none.
/// @param excludedCount How many `excluded` holds.
/// @return A Player, valid this frame, or null.
void* target(const void* const* excluded = nullptr, uint32_t excludedCount = 0);
/// Host only: the ghost's own limit (GhostDef::playerMove) on the direction (`dx`, `dz`) a player steers it
/// in from `at` (x, y, z): it may shorten, turn or zero it; unchanged for no limit.
/// @param at The ghost's position, world x, y, z in metres.
/// @param dx The direction's world x, length at most 1 with `dz`; changed in place.
/// @param dz The direction's world z; changed in place.
void player_move(const float at[3], float& dx, float& dz);
/// Host only: whether the ghost's own rules lose this interaction, which a player has paid for and chosen
/// (GhostDef::interactionLost). Ask once per interaction: the answer may be a roll.
/// @return True when the interaction is lost; it stays paid for.
bool interaction_lost();
/// Whether a player controlling the ghost takes the actions of the game's idle roll running now (the Playable
/// Ghost mod), so a rule on the AI's attempts stands aside, whichever mod's hook runs first; false without it.
/// @return True while a player takes the roll's actions.
bool roll_taken();
/// The ghost's flags (GhostDef::flags, PM_GHOST_* bits): PM_GHOST_BLIND_IN_HUNTS, say.
/// @return The bits, or 0.
uint32_t flags();
/// The flags (GhostDef::flags, PM_GHOST_* bits) of the custom ghost whose game value is `type` (a GhostType number),
/// while content is active or a contract holds it; 0 for a vanilla type, an unknown one and while no content is
/// active. A mod that lets a player be the ghost asks it for PM_GHOST_NOT_PLAYABLE before it offers or keeps a type.
/// Read-only, main thread.
/// @param type A GhostType number, the game's or a custom ghost's.
/// @return The bits, or 0.
/// @since Contract 2.3
uint32_t flags_of(int32_t type);

}  // namespace pm::api::content
