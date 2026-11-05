#pragma once

// Content: the ghosts and evidence a mod adds to the game, through the host contract's content
// functions (core/mod_api.h, version 2.2). A mod registers its content in pmmod_start; the host
// numbers it, puts it into the game and keeps it out of saves and away from players without it.
// Mods never choose a number: a ghost or evidence is named by its stable id, "<mod id>.<name>", and
// its number is asked for when it is needed (type_of, evidence_of), since it is known only while
// content is active or while a contract that loaded with content holds it, and may differ from one
// lobby to the next.
//
//   pm::api::content::EvidenceDef flame;
//   flame.id = "phasmomods.spectral-flame.flame";
//   flame.name = {"en", "Spectral Flame"};
//   flame.journalLine = {"en", "Watch a placed candle or lantern burn blue"};
//   pm::api::content::register_evidence(flame);
//
// Content is active only in a private room, on the room's host and on a client whose content and mods equal the host's
// (sdk/README.md, "The host's list"; core/mod_api.h describes the lobby rule). That is the one answer for every mod, held from a
// contract's load until it ends: a mod's content, its rules and its journal lines all wait on it, so a
// mod never checks the other players itself. While content is active in the lobby, the lobby's journals
// show the custom ghosts and evidence as a contract's do. Threading: Unity's main thread, like the rest
// of the game API. Before pm::api::bind every function answers as inactive: 0, -1, empty or false.

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
  Text() = default;
  /// The text `text` in the language `lang`.
  Text(std::string lang, std::string text) { add(std::move(lang), std::move(text)); }
  /// Adds the text in another language, and returns this text for the next.
  Text& add(std::string lang, std::string text) {
    items_.emplace_back(std::move(lang), std::move(text));
    return *this;
  }
  /// Whether it holds no language at all.
  bool empty() const { return items_.empty(); }
  /// The C view of it, valid until this text is changed, copied or destroyed.
  PmTextSet c_set() const;

 private:
  std::vector<std::pair<std::string, std::string>> items_;
  mutable std::vector<PmText> view_;
};

/// A custom ghost, as it is registered (PmGhostDef in core/mod_api.h says what each member means).
struct GhostDef {
  std::string id;  // "<mod id>.<name>", stable across versions
  Text name, description, strength, weakness;
  std::vector<Text> tells;               // one line each
  std::array<std::string, 3> evidence;  // vanilla ("emf" ... "dots") or custom evidence ids; "" for none (all three may be)
  Text abilityName, abilityDescription;  // both empty: no ability
  void* user = nullptr;                  // handed to every callback
  bool (*useAbility)(void* user) = nullptr;
  int32_t (*hudState)(void* user) = nullptr;
  bool (*heldBack)(int32_t kind, void* user) = nullptr;
  uint32_t (*playerView)(float* xyz, uint32_t max, void* user) = nullptr;
  const char* (*evidenceLook)(const char* evidenceId, void* user) = nullptr;
  uint32_t flags = 0;  // PM_GHOST_* bits (PM_GHOST_KEEPS_LAST_EVIDENCE, PM_GHOST_BLIND_IN_HUNTS)
  // Its one target, to which the host holds each of its hunts as the game holds its Banshee's, for its AI
  // and a player controlling it alike: never dead nor one of the `excludedCount` players at `excluded` (null
  // and 0 for none), or null. Asked at each hunt's start and wherever the game gives a Banshee a new target
  // (core/mod_api.h).
  void* (*target)(const void* const* excluded, uint32_t excludedCount, void* user) = nullptr;
  const char* (*hudLine)(int32_t state, void* user) = nullptr;  // its player's HUD line for a hudState; null none
  // Host only: its rules' limit on the direction (dx, dz) a player steers it in from `at` (x, y, z), which it
  // may shorten, turn or set to 0; null for none (core/mod_api.h).
  void (*playerMove)(const float* at, float* dx, float* dz, void* user) = nullptr;
  // Host only, once for each interaction a player has paid for and chosen, just before it happens: true when
  // its own rules lose this one, as they lose a share of its AI's attempts; it stays paid for. May roll
  // (core/mod_api.h).
  bool (*interactionLost)(void* user) = nullptr;
  /// Host only, at the game's roll of the contract's ghost type, just before it draws: false keeps the ghost out
  /// of that roll, as a weight of 0 does (a ghost that cannot run now); null or true leaves the game's weight. A
  /// Custom difficulty that names its type still gets it (core/mod_api.h, PmGhostDef::rollable). Main thread, on
  /// the master, with `user`.
  bool (*rollable)(void* user) = nullptr;

  /// The C definition, valid until this definition is changed, copied or destroyed. The host copies
  /// what it needs when it is registered.
  const PmGhostDef& c_def() const;

 private:
  mutable PmGhostDef def_{};
  mutable std::vector<PmTextSet> tellSets_;
};

/// A custom evidence, as it is registered (PmEvidenceDef in core/mod_api.h).
struct EvidenceDef {
  std::string id;      // "<mod id>.<name>", stable across versions
  Text name;           // its name: on its journal row and on the pages of the ghosts that give it
  Text journalLine;    // the line under its name on its journal row, saying how it is found

  /// The C definition, valid until this definition is changed, copied or destroyed.
  const PmEvidenceDef& c_def() const;

 private:
  mutable PmEvidenceDef def_{};
};

/// A ghost type's rules (PmGhostRules in core/mod_api.h says when each runs). Every callback is optional
/// and runs on the main thread with `user`.
struct GhostRules {
  void* user = nullptr;
  void (*start)(void* user) = nullptr;
  void (*tick)(void* user) = nullptr;
  void (*stop)(void* user) = nullptr;
  bool (*huntSpeed)(float* base, float* losScale, void* user) = nullptr;
  bool (*heldBack)(int32_t kind, void* user) = nullptr;

  /// The C rules, valid until these rules are changed, copied or destroyed. The host copies them.
  const PmGhostRules& c_rules() const;

 private:
  mutable PmGhostRules rules_{};
};

/// Registers the ghost `def`, from pmmod_start only. Returns its handle, greater than 0, or 0 when the
/// host refused it (outside pmmod_start, a repeated or malformed id, text without English or over the
/// limits in core/mod_api.h; the log says which) and before bind. A custom evidence it gives may be
/// registered by any mod that starts, before or after it; once every mod has started, a ghost whose
/// evidence nobody registered is dropped (logged).
int32_t register_ghost(const GhostDef& def);
/// Registers the evidence `def` likewise, from pmmod_start only.
int32_t register_evidence(const EvidenceDef& def);

/// The game type number of the ghost `ghostId` while content is active, and while a contract that
/// loaded with content active holds it (until it ends, whatever the room's content state does), else
/// -1. A vanilla id ("vanilla.mare") answers its type whenever the game API is bound.
int32_t type_of(std::string_view ghostId);
/// The id of the ghost type `type`: "vanilla.<name>" for a vanilla type (None is no ghost), a
/// custom ghost's id while content is active or a contract holds it (as type_of), else empty.
std::string id_of(int32_t type);
/// The game value of the evidence `evidenceId`: the game's own for a vanilla id ("emf" is 1), a
/// custom evidence's while content is active or a contract holds it (as type_of), else -1.
int32_t evidence_of(std::string_view evidenceId);
/// Whether custom content is in play on this client: false outside a private room, and until this client is the host or
/// matches it. A joiner's mods never change this for anyone else (sdk/README.md, "The host's list").
bool active();
/// Whether a player controls the contract's ghost (the Playable Ghost mod); false without it.
bool player_controlled();
/// Whether this client's running mods equal the host's list, in a private room; held through a contract. The host gates
/// content, rules and journal lines on it already, and a mod has no need of it: it stays so 2.x mods load. False before
/// bind.
bool room_agrees();

/// Attaches `rules` to the ghost `ghostId`, a custom ghost's id or a vanilla one ("vanilla.mare"),
/// from pmmod_start only. They run while the contract's ghost acts as that type (its own type, or a
/// Mimic's copy of it) and this client's mods match the host's list, an answer that holds from the contract's load until
/// it ends, so rules never start partway through a contract. Returns a handle greater than 0, or 0 when the host refused them (logged: core/mod_api.h,
/// ghost_rules) and before bind.
int32_t attach_rules(std::string_view ghostId, const GhostRules& rules);
/// The type the contract's ghost acts as on this client (a Mimic's copy, custom ones included), or -1
/// outside a contract, before the ghost spawns and before bind.
int32_t acting_as();
/// The contract's ghost's identity (what the journal judges), or -1 outside a contract, before the
/// ghost spawns and before bind.
int32_t identity();

/// Adds `lines` to the journal page of the vanilla ghost `vanillaGhostId` ("vanilla.mare"), after the
/// page's own text, from pmmod_start only; each line is text in several languages, English required.
/// Several mods may add lines to one ghost. They show in a private room only, in the lobby's journals
/// and a contract's alike, and only while this client's mods match the host's list (held from a contract's load until it
/// ends), in the player's language when
/// the line has it, else English. Returns a handle greater than 0, or 0 when the host refused them
/// (logged: core/mod_api.h, journal_lines) and before bind.
int32_t add_journal_lines(std::string_view vanillaGhostId, const std::vector<Text>& lines);

/// How the contract's ghost changes the look of the evidence `evidenceId` (its GhostDef::evidenceLook,
/// e.g. "green" for the Hydra's Spectral Flame), asked by the ghost's identity: a Mimic copying a ghost
/// keeps the Mimic's look. Empty for the evidence's own look: no ghost, a game ghost, a ghost without
/// evidenceLook, and before bind. A contract that holds custom ghosts answers for its ghost until it
/// ends, whatever the room's content state does meanwhile.
std::string evidence_look(std::string_view evidenceId);

// The contract's ghost as a player controls it (Playable Ghost), for any registered ghost: each asks the
// definition (GhostDef) of the type the ghost acts as on this client (acting_as), a custom type a contract
// that loaded with content holds or one of the active content. A game type, no ghost, a callback the
// definition leaves null, and a call before bind answer as inactive: false, empty, -1, 0 or null.

/// Host only: uses the ghost's ability (GhostDef::useAbility); false when it has none or could not.
bool use_ability();
/// The ghost's ability name and description, in the player's language when it has it, else English;
/// empty for a ghost without an ability.
std::string ability_name();
std::string ability_description();
/// Host only: the ghost's state as its player feels it (GhostDef::hudState), 0 to 255, or -1.
int32_t hud_state();
/// The HUD line for the state `state` that hud_state answered on the host (GhostDef::hudLine), asked on
/// the client of the player controlling the ghost; empty for none.
std::string hud_line(int32_t state);
/// Whether the ghost may not act now as `kind` (0: an interaction, 1: an event or its ability, 2: a hunt, 3: its
/// ability alone, 4: the fuse box): its
/// definition's heldBack, or the heldBack of any rules running for its type, a vanilla type's included
/// (attach_rules).
bool held_back(int32_t kind);
/// The points only the ghost's player sees (GhostDef::playerView) into `xyz`, room for `max`; how many
/// it wrote.
uint32_t player_view(float* xyz, uint32_t max);
/// The one player the ghost's hunts are held to now (GhostDef::target), never
/// one of the `excludedCount` players at `excluded` (null and 0 for none), or null for no restriction.
void* target(const void* const* excluded = nullptr, uint32_t excludedCount = 0);
/// Host only: the ghost's own limit (GhostDef::playerMove) on the direction (`dx`, `dz`) a player steers it
/// in from `at` (x, y, z): it may shorten, turn or zero it; unchanged for no limit.
void player_move(const float at[3], float& dx, float& dz);
/// Host only: whether the ghost's own rules lose this interaction, which a player has paid for and chosen
/// (GhostDef::interactionLost). Ask once per interaction: the answer may be a roll.
bool interaction_lost();
/// Whether a player controlling the ghost takes the actions of the game's idle roll running now (the Playable
/// Ghost mod), so a rule on the AI's attempts stands aside, whichever mod's hook runs first; false without it.
bool roll_taken();
/// The ghost's flags (GhostDef::flags, PM_GHOST_* bits): PM_GHOST_BLIND_IN_HUNTS, say.
uint32_t flags();

}  // namespace pm::api::content
