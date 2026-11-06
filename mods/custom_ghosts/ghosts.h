#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include "text_table.h"

// The custom ghosts and the rules that make each one identifiable. Pure data and arithmetic, kept free
// of game calls so it can be tested offline.
//
// Each custom ghost goes to the content layer (sdk/api/content.h; registration.h) under its id,
// "phasmomods.custom-ghosts.<name in lower case, spaces as _>", with its journal text and evidence. The host makes
// it a ghost type of the game's while content is active: it numbers it after the game's own types, and
// the type roll, evidence, the journal, the players' guess and the results name and judge it by that
// number, while its AI follows no vanilla type's rules; only its own rules, attached to its id, set it
// apart. That number is the host's (content::type_of(id)) and may differ from one lobby to the next, so
// the mod's own code names a ghost by its Type below, which is never a game number.
namespace pm::custom {

// The evidence the ghosts give, by content id: the game's own (core/mod_api.h, PmGhostDef) and Spectral
// Flame, which the Spectral Flame mod registers (mods/spectral_flame/contract.h).
inline constexpr const char* kEmf = "emf";
inline constexpr const char* kSpiritBox = "spirit_box";
inline constexpr const char* kUltraviolet = "ultraviolet";
inline constexpr const char* kOrbs = "orbs";
inline constexpr const char* kWriting = "writing";
inline constexpr const char* kFreezing = "freezing";
inline constexpr const char* kDots = "dots";
inline constexpr const char* kSpectralFlame = "phasmomods.spectral-flame.flame";

// The mod's own name for each of its ghosts; 0 is none. Never a game type number.
enum Type : int {
  kEcho = 1, kDullahan, kKikimora, kDraugr, kBlackShuck,
  // The flame ghosts: they carry Spectral Flame (docs/superpowers/specs/2026-10-20-flame-ghosts-design.md).
  kLithobolia, kGremlin, kBrownie, kBogeyman, kLethe, kMesmer, kKnell, kAsterion, kTempest, kHydra,
  // A person in a bedsheet, not a ghost: no evidence, no activity (docs/superpowers/specs/2026-11-04-squatter-design.md).
  kSquatter
};

struct Ghost {
  Type type;
  const char* name;  // its English name, which its id is made from; players read the table's (title_key)
  const char* evidence[3];  // content evidence ids
  const char* description;  // the journal page: what it is (each text field is a key of the mod's table)
  const char* strength;
  const char* weakness;
  const char* ability;             // what a player controlling it may do at will (null: nothing)
  const char* abilityDescription;
  const char* flameLook = nullptr;  // how its Spectral Flame looks ("green"); null: the evidence's own
  // Its last evidence is never the one a difficulty withholds, as the Goryo's D.O.T.S. (the content
  // layer's PM_GHOST_KEEPS_LAST_EVIDENCE); on zero evidence none is given.
  bool keepsLastEvidence = false;
  // Blind in a hunt (the content layer's PM_GHOST_BLIND_IN_HUNTS): a player controlling it sees only its
  // player view (the Black Shuck's scent) while it hunts.
  bool blindInHunts = false;
  // Never on the Mimic's copy list (the content layer's PM_GHOST_NO_MIMIC_COPY): a Mimic never copies it.
  bool noMimicCopy = false;
};

inline constexpr Ghost kGhosts[] = {
    {kEcho, "Echo", {kSpiritBox, kFreezing, kDots},
     i18n::key("ghost.echo.description"), i18n::key("ghost.echo.strength"), i18n::key("ghost.echo.weakness"),
     i18n::key("ghost.echo.ability"), i18n::key("ghost.echo.about")},
    {kDullahan, "Dullahan", {kEmf, kOrbs, kWriting},
     i18n::key("ghost.dullahan.description"), i18n::key("ghost.dullahan.strength"), i18n::key("ghost.dullahan.weakness"),
     nullptr, nullptr},
    {kKikimora, "Kikimora", {kUltraviolet, kWriting, kSpectralFlame},
     i18n::key("ghost.kikimora.description"), i18n::key("ghost.kikimora.strength"), i18n::key("ghost.kikimora.weakness"),
     i18n::key("ghost.kikimora.ability"), i18n::key("ghost.kikimora.about")},
    {kDraugr, "Draugr", {kEmf, kSpectralFlame, kFreezing},
     i18n::key("ghost.draugr.description"), i18n::key("ghost.draugr.strength"), i18n::key("ghost.draugr.weakness"),
     nullptr, nullptr, nullptr, true},
    {kBlackShuck, "Black Shuck", {kUltraviolet, kFreezing, kDots},
     i18n::key("ghost.black_shuck.description"), i18n::key("ghost.black_shuck.strength"), i18n::key("ghost.black_shuck.weakness"),
     nullptr, nullptr, nullptr, false, true},
    {kLithobolia, "Lithobolia", {kSpiritBox, kFreezing, kSpectralFlame},
     i18n::key("ghost.lithobolia.description"), i18n::key("ghost.lithobolia.strength"), i18n::key("ghost.lithobolia.weakness"),
     i18n::key("ghost.lithobolia.ability"), i18n::key("ghost.lithobolia.about")},
    {kGremlin, "Gremlin", {kEmf, kDots, kSpectralFlame},
     i18n::key("ghost.gremlin.description"), i18n::key("ghost.gremlin.strength"), i18n::key("ghost.gremlin.weakness"),
     i18n::key("ghost.gremlin.ability"), i18n::key("ghost.gremlin.about")},
    {kBrownie, "Brownie", {kEmf, kWriting, kSpectralFlame},
     i18n::key("ghost.brownie.description"), i18n::key("ghost.brownie.strength"), i18n::key("ghost.brownie.weakness"),
     nullptr, nullptr},
    {kBogeyman, "Bogeyman", {kUltraviolet, kOrbs, kSpectralFlame},
     i18n::key("ghost.bogeyman.description"), i18n::key("ghost.bogeyman.strength"), i18n::key("ghost.bogeyman.weakness"),
     nullptr, nullptr},
    {kLethe, "Lethe", {kOrbs, kFreezing, kSpectralFlame},
     i18n::key("ghost.lethe.description"), i18n::key("ghost.lethe.strength"), i18n::key("ghost.lethe.weakness"),
     nullptr, nullptr},
    {kMesmer, "Mesmer", {kSpiritBox, kDots, kSpectralFlame},
     i18n::key("ghost.mesmer.description"), i18n::key("ghost.mesmer.strength"), i18n::key("ghost.mesmer.weakness"),
     nullptr, nullptr},
    {kKnell, "Knell", {kSpiritBox, kOrbs, kSpectralFlame},
     i18n::key("ghost.knell.description"), i18n::key("ghost.knell.strength"), i18n::key("ghost.knell.weakness"),
     nullptr, nullptr},
    {kAsterion, "Asterion", {kWriting, kDots, kSpectralFlame},
     i18n::key("ghost.asterion.description"), i18n::key("ghost.asterion.strength"), i18n::key("ghost.asterion.weakness"),
     nullptr, nullptr},
    {kTempest, "Tempest", {kSpiritBox, kUltraviolet, kSpectralFlame},
     i18n::key("ghost.tempest.description"), i18n::key("ghost.tempest.strength"), i18n::key("ghost.tempest.weakness"),
     nullptr, nullptr},
    {kHydra, "Hydra", {kOrbs, kDots, kSpectralFlame},
     i18n::key("ghost.hydra.description"), i18n::key("ghost.hydra.strength"), i18n::key("ghost.hydra.weakness"),
     nullptr, nullptr, "green"},
    // No evidence at all: it is told apart by sight and sound, being visible and talking (the spec's "Rules this
    // ghost breaks on purpose"). Its interactions, events and hunts are refused in restraint.cpp. A Mimic never
    // copies it (user, 2026-11-06).
    {kSquatter, "Squatter", {nullptr, nullptr, nullptr},
     i18n::key("ghost.squatter.description"), i18n::key("ghost.squatter.strength"), i18n::key("ghost.squatter.weakness"),
     nullptr, nullptr, nullptr, false, false, true}

};
constexpr int kGhostCount = static_cast<int>(sizeof kGhosts / sizeof kGhosts[0]);

inline const Ghost* find(int type) {
  for (const Ghost& g : kGhosts)
    if (g.type == type) return &g;
  return nullptr;
}

/** The ghost's content id: the mod's id, a dot, and its name in lower case, spaces as _ ("phasmomods.custom-ghosts.black_shuck"). */
/** The table key of the ghost `g`'s name as players read it; its id comes from Ghost::name, which never changes. */
inline const char* title_key(const Ghost& g) {
  switch (g.type) {
    case kEcho: return i18n::key("ghost.echo.name");
    case kDullahan: return i18n::key("ghost.dullahan.name");
    case kKikimora: return i18n::key("ghost.kikimora.name");
    case kDraugr: return i18n::key("ghost.draugr.name");
    case kBlackShuck: return i18n::key("ghost.black_shuck.name");
    case kLithobolia: return i18n::key("ghost.lithobolia.name");
    case kGremlin: return i18n::key("ghost.gremlin.name");
    case kBrownie: return i18n::key("ghost.brownie.name");
    case kBogeyman: return i18n::key("ghost.bogeyman.name");
    case kLethe: return i18n::key("ghost.lethe.name");
    case kMesmer: return i18n::key("ghost.mesmer.name");
    case kKnell: return i18n::key("ghost.knell.name");
    case kAsterion: return i18n::key("ghost.asterion.name");
    case kTempest: return i18n::key("ghost.tempest.name");
    case kHydra: return i18n::key("ghost.hydra.name");
    case kSquatter: return i18n::key("ghost.squatter.name");
  }
  return "";
}

inline std::string id_of(const Ghost& g) {
  std::string id = "phasmomods.custom-ghosts.";
  for (const char* c = g.name; *c; ++c)
    id += *c == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
  return id;
}

/** The ghost whose content id is `id`, or null. */
inline const Ghost* find_id(std::string_view id) {
  for (const Ghost& g : kGhosts)
    if (id_of(g) == id) return &g;
  return nullptr;
}

// Dullahan: fast on straights, slow to turn. Its body turns at kDullahanTurn degrees a second, and its
// speed falls with the angle between where it faces and where it must go.
constexpr float kDullahanTurn = 45.0f;        // degrees per second (a vanilla ghost turns far faster)
constexpr float kDullahanStraight = 2.0f;     // times the game's base speed when facing its path
constexpr float kDullahanCornerFloor = 0.25f; // times the game's base speed when facing away from its path

/**
 * The Dullahan's own hunt base for the game's base speed `base` (speed_rule.h: before the difficulty's and the
 * other multipliers) and the angle (degrees) between facing and path. Not capped.
 */
inline float dullahan_speed(float base, float angleDegrees) {
  const float a = std::clamp(angleDegrees, 0.0f, 180.0f) * 3.14159265f / 180.0f;
  const float alignment = std::max(0.0f, std::cos(a));
  const float factor = kDullahanCornerFloor + (kDullahanStraight - kDullahanCornerFloor) * alignment * alignment;
  return base * factor;
}

// Draugr: strong near its room, weak away from it; hunts early once something leaves its room.
constexpr float kDraugrRoomSpeed = 2.8f;     // metres per second in its room, before the game's multipliers
constexpr float kDraugrSlope = 0.15f;        // metres per second lost per metre from its room
constexpr float kDraugrFloor = 0.3f;         // metres per second at the slowest, below the Deogen's crawl
constexpr int kDraugrAngeredBonus = 15;      // hunt check insanity bonus: hunts from 65% sanity

/** The Draugr's hunt base at a distance (metres) from its favourite room. */
inline float draugr_speed(float distance) {
  return std::clamp(kDraugrRoomSpeed - kDraugrSlope * distance, kDraugrFloor, kDraugrRoomSpeed);
}

}  // namespace pm::custom
