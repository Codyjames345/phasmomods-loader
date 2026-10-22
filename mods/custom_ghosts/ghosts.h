#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

// The custom ghost types and the rules that make each one identifiable. Pure data and arithmetic,
// kept free of game calls so it can be tested offline.
//
// Each custom ghost is a new GhostType (31 and up, after the game's last type, Deildegast = 30) for
// everything that names or judges it: the type roll, evidence, the journal, the investigators' guess
// and the results. For the ghost's AI its behaviour type (GhostInfo +0x2C) is GhostType.None, so no
// vanilla type's rules apply to it; only its own rules, below, set it apart.
namespace pg::custom {

// Evidence values (the game's EvidenceType), and Spectral Flame, the evidence the Spectral Flame mod adds.
enum Evidence : int {
  kEmf = 1, kSpiritBox = 2, kUltraviolet = 3, kOrbs = 4, kWriting = 5, kFreezing = 6, kDots = 7, kSpectralFlame = 8
};

constexpr int kFirstType = 31;
constexpr int kBehaviourType = 24;  // GhostType.None: no vanilla type's rules
enum Type : int {
  kEcho = 31, kDullahan = 32, kKikimora = 33, kDraugr = 34, kBlackShuck = 35,
  // The flame ghosts: they carry Spectral Flame (docs/superpowers/specs/2026-10-20-flame-ghosts-design.md).
  kLithobolia = 36, kGremlin = 37, kBrownie = 38, kBogeyman = 39, kLethe = 40,
  kMesmer = 41, kKnell = 42, kAsterion = 43, kTempest = 44, kHydra = 45
};

struct Ghost {
  int type;
  const char* name;
  Evidence evidence[3];
  const char* description;  // the journal page: what it is
  const char* strength;
  const char* weakness;
  const char* ability;             // what a player controlling it may do at will (null: nothing)
  const char* abilityDescription;
};

inline constexpr Ghost kGhosts[] = {
    {kEcho, "Echo", {kSpiritBox, kFreezing, kDots},
     "A spirit with no voice of its own. It speaks only with the voices of the living, repeating what "
     "they have said to draw them close.",
     "An Echo can call out in the voice of anyone it has heard, from wherever it waits, and it repeats "
     "what players do in its room: the switch or door you just used is the next thing it touches.",
     "With nobody speaking near it for a while, an Echo falls quiet and rarely acts.",
     "Speak", "Call out with the last words an investigator said, in their own voice."},
    {kDullahan, "Dullahan", {kEmf, kOrbs, kWriting},
     "A headless rider that charges down whatever lies straight ahead of it. Legend says it cannot turn "
     "its mount easily.",
     "A Dullahan is very fast on long, straight runs.",
     "A Dullahan turns slowly and loses most of its speed at every corner.", nullptr, nullptr},
    {kKikimora, "Kikimora", {kUltraviolet, kWriting, kSpectralFlame},
     "A household spirit that lives behind the stove and cannot bear clutter. It tidies away whatever the "
     "living leave lying around, usually somewhere they will not think to look.",
     "A Kikimora carries off equipment left on the floor and hides it in another room.",
     "A Kikimora will not touch anything in a room with the lights on.",
     "Tidy away", "Carry off the nearest equipment lying in a dark room and hide it in another dark, empty room."},
    {kDraugr, "Draugr", {kEmf, kFreezing, kSpectralFlame},
     "The restless dead of the old barrows, which guard their resting place and everything in it. Blue fire "
     "burns over the mounds where they lie.",
     "Carry anything out of a Draugr's room and it may hunt while your sanity is still high.",
     "A Draugr never leaves its room for good, and in a hunt it slows to a crawl the further it strays from it.", nullptr, nullptr},
    {kBlackShuck, "Black Shuck", {kUltraviolet, kFreezing, kDots},
     "A phantom hound of the old roads. It follows the scent of the living, step for step.",
     "In a hunt a Black Shuck follows the exact path its prey walked, round every corner and into hiding places, though it cannot see or hear.",
     "Salt or running water breaks a Black Shuck's scent, and it loses anyone on the other side.", nullptr, nullptr},
    {kLithobolia, "Lithobolia", {kSpiritBox, kFreezing, kSpectralFlame},
     "Named for the stone-throwing devil that pelted a New Hampshire house in 1682. It does not throw "
     "things about; it throws them at you.",
     "A Lithobolia throws only at players it can see, and a hit costs more than a fright.",
     "With nobody in sight a Lithobolia throws nothing at all.",
     "Throw", "Throw the nearest loose object at a player in sight."},
    {kGremlin, "Gremlin", {kEmf, kDots, kSpectralFlame},
     "The saboteur airmen blamed for every failing engine. It cannot leave a working machine alone.",
     "A Gremlin switches off equipment that has been set down and left running.",
     "A Gremlin will not touch equipment someone is holding or standing beside.",
     "Short out", "Switch off the nearest placed equipment that nobody is holding or standing beside."},
    {kBrownie, "Brownie", {kEmf, kWriting, kSpectralFlame},
     "A household spirit of Scotland that works only while nobody is watching, and leaves the moment it "
     "is seen.",
     "A Brownie is very fast in a hunt while nobody looks at it, and its footsteps are hard to hear.",
     "A Brownie slows to less than a walk while anyone looks at it.", nullptr, nullptr},
    {kBogeyman, "Bogeyman", {kUltraviolet, kOrbs, kSpectralFlame},
     "The thing in the closet. It knows every place a frightened person hides. It always comes for the "
     "youngest.",
     "A Bogeyman's hunts begin from the closet or locker nearest the youngest player, and it goes for them "
     "first.",
     "A Bogeyman is slow in open rooms and loses sight of its prey there sooner.", nullptr, nullptr},
    {kLethe, "Lethe", {kOrbs, kFreezing, kSpectralFlame},
     "Named for the river of forgetting in the underworld. It has forgotten who it was, and makes the "
     "living forget too.",
     "Photos, videos and recordings of a Lethe fade from the journal a few minutes after they are taken, "
     "unless someone looks at them first.",
     "A Lethe forgets its prey: once it loses sight of someone in a hunt, it gives up the chase quickly.",
     nullptr, nullptr},
    {kMesmer, "Mesmer", {kSpiritBox, kDots, kSpectralFlame},
     "Named for the showman whose audiences saw whatever he wished. No two witnesses agree on what a Mesmer "
     "showed them.",
     "Each investigator sees a Mesmer differently, and the truck's sanity monitor shows the team saner than "
     "they are.",
     "A Mesmer cannot fool a camera: video, photos and the D.O.T.S. projector show it as it is.", nullptr, nullptr},
    {kKnell, "Knell", {kSpiritBox, kOrbs, kSpectralFlame},
     "The slow toll of a funeral bell. It moves to a beat, and the beat never changes.",
     "In a hunt a Knell surges to great speed on every other beat.",
     "Between surges a Knell crawls at a third of its speed; run on its slow beat.", nullptr, nullptr},
    {kAsterion, "Asterion", {kWriting, kDots, kSpectralFlame},
     "The Minotaur of the labyrinth, by its own name. It makes a maze of any house it hunts in.",
     "A closed door never stops an Asterion for good: it batters it down, and the door stays broken open.",
     "Battering a door down takes an Asterion a few seconds, and it can be heard far away.", nullptr, nullptr},
    {kTempest, "Tempest", {kSpiritBox, kUltraviolet, kSpectralFlame},
     "A storm spirit of the old weather lore. It moves with the lightning, and the wilder the night, the "
     "more often it strikes.",
     "In a hunt a Tempest cannot be seen, except when lightning strikes; then it is very fast.",
     "Between strikes a Tempest is slow, and the clearer the night, the rarer its lightning.", nullptr, nullptr},
    {kHydra, "Hydra", {kOrbs, kDots, kSpectralFlame},
     "The many-headed serpent: cut off one head and two grow back. Its flame burns green.",
     "Smudge a Hydra in a hunt and it splits into apparitions, only one of them real; smudge it again "
     "and there are more.",
     "A Hydra's false heads are harmless and make no sound as they walk.", nullptr, nullptr},
};
constexpr int kGhostCount = static_cast<int>(sizeof kGhosts / sizeof kGhosts[0]);

inline const Ghost* find(int type) {
  for (const Ghost& g : kGhosts)
    if (g.type == type) return &g;
  return nullptr;
}
inline bool is_custom(int type) { return find(type) != nullptr; }

/** The English name the journal shows for an evidence type. */
inline const char* evidence_name(int e) {
  switch (e) {
    case kEmf: return "EMF Level 5";
    case kSpiritBox: return "Spirit Box";
    case kUltraviolet: return "Ultraviolet";
    case kOrbs: return "Ghost Orb";
    case kWriting: return "Ghost Writing";
    case kFreezing: return "Freezing Temperatures";
    case kDots: return "D.O.T.S Projector";
    case kSpectralFlame: return "Spectral Flame";
    default: return "";
  }
}

/**
 * The localisation key the game builds for a ghost type's name ("Journal_" + GhostType.ToString() +
 * "Title"): a type with no enum name prints as its number, so custom types are "Journal_31Title".
 */
inline std::string title_key(int type) { return "Journal_" + std::to_string(type) + "Title"; }

/**
 * Whether a localisation key may name a custom type: "Journal_" and two digits and "Title", the
 * shape of every custom type's key (31 and up). A cheap check before the full comparison.
 */
inline bool maybe_title_key(std::u16string_view key) {
  constexpr std::u16string_view kPrefix = u"Journal_", kSuffix = u"Title";
  if (key.size() != kPrefix.size() + 2 + kSuffix.size() || key.substr(0, kPrefix.size()) != kPrefix ||
      key.substr(kPrefix.size() + 2) != kSuffix)
    return false;
  const char16_t a = key[kPrefix.size()], b = key[kPrefix.size() + 1];
  return a >= u'3' && a <= u'9' && b >= u'0' && b <= u'9';
}

/** The custom type a localisation key names, or 0. */
inline int type_of_title_key(const char* key) {
  if (!key) return 0;
  for (const Ghost& g : kGhosts)
    if (title_key(g.type) == key) return g.type;
  return 0;
}

/**
 * The name in a ghost type's title key ("Journal_TheTwinsTitle": "TheTwins"), empty for any other
 * key. A game ghost's is the name of its GhostData asset, which the game's collection is sorted by.
 */
inline std::string_view title_key_name(std::string_view key) {
  constexpr std::string_view kPrefix = "Journal_", kSuffix = "Title";
  if (key.size() <= kPrefix.size() + kSuffix.size() || key.substr(0, kPrefix.size()) != kPrefix ||
      key.substr(key.size() - kSuffix.size()) != kSuffix)
    return {};
  return key.substr(kPrefix.size(), key.size() - kPrefix.size() - kSuffix.size());
}

/** Whether ghost name `a` comes before `b` in the game's collection: alphabetically, ignoring case. */
inline bool sorts_before(std::string_view a, std::string_view b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
  });
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

}  // namespace pg::custom
