#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

// What the ghost player perceives beyond their own eyes, as their type's vanilla ghost does, and the
// things done to the ghost that its vanilla code reacts to. Pure (no game calls), testable offline.
//
// Marks (E1): points the host reads from the game's own state for the ghost's type and sends to the
// ghost player's screen. Nothing the ghost's type would not know: the Kormos hears footsteps within
// its hearing distances, the Deogen knows where everyone is in a hunt, the Dayan feels the nearest
// investigator in its sight moving or holding still, the Raiju senses the electronics its sensor
// detects, and the Black Shuck smells the scent trail it would follow.
//
// Alerts (E3): a line for things done to the ghost (its name spoken, the Ouija board or spirit box
// answering, a cursed object used on it, a crucifix burned, salt stepped in, being smudged or
// summoned), with a direction and distance when they happen somewhere. No timers, no sanity.
namespace pg::perceive {

enum MarkKind : uint8_t {
  kStepWalk = 1,    // Kormos: footsteps it hears, by how the investigator moves
  kStepSprint = 2,
  kStepCreep = 3,
  kPrey = 4,        // Deogen: an investigator, during a hunt
  kMover = 5,       // Dayan: the investigator it watches, moving (empowers it)
  kStill = 6,       // Dayan: the investigator it watches, holding still (weakens it)
  kCharge = 7,      // Raiju: electronics its sensor detects
  kScent = 8,       // Black Shuck: a point on the scent trail
};
constexpr int kMaxMarks = 32;

struct Mark {
  uint8_t kind = 0;
  float x = 0, y = 0, z = 0;
  bool operator==(const Mark&) const = default;
};

enum AlertKind : uint8_t {
  kName = 1,       // its name was spoken (the host's reaction to it)
  kSpiritBox = 2,  // the spirit box answered for it
  kOuija = 3,      // the Ouija board answered (text: the answer)
  kMusicBox = 4,
  kSummoned = 5,   // the summoning circle took it
  kTarot = 6,      // arg: the card (TarotCards' enum)
  kVoodoo = 7,
  kMonkeyPaw = 8,
  kMirror = 9,
  kCrucifix = 10,
  kSalt = 11,
  kSmudged = 12,
};
constexpr uint64_t kAlertMs = 4000;    // how long an alert stays on screen
constexpr uint64_t kRepeatMs = 2500;   // the same kind again within this is the same alert
constexpr size_t kMaxAlertText = 40;

// TarotCards' card enum (build 25811160); only these four act on the ghost.
enum TarotCard : uint8_t { kTower = 2, kDevil = 3, kDeath = 4, kHermit = 5 };
inline bool tarot_affects_ghost(int card) { return card >= kTower && card <= kHermit; }

struct Alert {
  uint8_t kind = 0;
  uint8_t arg = 0;
  bool located = false;  // x, y, z say where it happened
  float x = 0, y = 0, z = 0;
  std::string text;      // kOuija: the answer, at most kMaxAlertText bytes
  bool operator==(const Alert&) const = default;
};

/** What the alert says, without where. */
inline std::string alert_text(const Alert& a) {
  switch (a.kind) {
    case kName: return "Someone spoke your name";
    case kSpiritBox: return "The spirit box spoke for you";
    case kOuija: return a.text.empty() ? "The Ouija board spoke for you" : "The Ouija board answered: \"" + a.text + "\"";
    case kMusicBox: return "Music is calling you";
    case kSummoned: return "You are being summoned";
    case kTarot:
      switch (a.arg) {
        case kTower: return "The Tower card: you grow restless";
        case kDevil: return "The Devil card: you are made to appear";
        case kDeath: return "The Death card: you are made to hunt";
        case kHermit: return "The Hermit card: you are sent home";
        default: return "A tarot card was drawn";
      }
    case kVoodoo: return "A pin was pushed into the voodoo doll";
    case kMonkeyPaw: return "A wish was made on the monkey paw";
    case kMirror: return "They are watching your room in the mirror";
    case kCrucifix: return "A crucifix stopped you";
    case kSalt: return "You walked through salt";
    case kSmudged: return "You have been smudged";
    default: return {};
  }
}

/**
 * Where a point lies from the ghost: "ahead", "behind", "to your left" or "to your right", and how far.
 * yawDegrees is the way the ghost looks (0 = +z, 90 = +x), as Unity measures it.
 */
inline std::string bearing(float gx, float gz, float yawDegrees, float tx, float tz) {
  const float dx = tx - gx, dz = tz - gz;
  const float distance = std::sqrt(dx * dx + dz * dz);
  char buf[48];
  if (distance < 1.5f) return "right here";
  const float toTarget = std::atan2(dx, dz) * 180.0f / 3.14159265f;
  float rel = std::fmod(toTarget - yawDegrees + 540.0f, 360.0f) - 180.0f;  // -180..180, positive: right
  const char* side = std::fabs(rel) <= 45.0f ? "ahead" : std::fabs(rel) >= 135.0f ? "behind" : rel > 0 ? "to your right" : "to your left";
  std::snprintf(buf, sizeof buf, "%s, %d m", side, static_cast<int>(distance + 0.5f));
  return buf;
}

/** Drops an alert of a kind already shown within kRepeatMs (an RPC seen here and relayed by the host). */
class Recent {
 public:
  bool fresh(uint8_t kind, uint64_t now) {
    auto it = last_.find(kind);
    if (it != last_.end() && now - it->second < kRepeatMs) return false;
    last_[kind] = now;
    return true;
  }
  void clear() { last_.clear(); }

 private:
  std::map<uint8_t, uint64_t> last_;
};

}  // namespace pg::perceive
