#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "tells.h"

// The Spirit's tells, kept free of game calls so they can be tested offline. Its journal says it "can be
// temporarily stopped by burning incense": incense in a hunt stops it where it stands for a moment;
// incense outside a hunt silences it for a minute (its vanilla three minutes without a hunt are
// unchanged); and, the most human of the ghosts, it knocks when a player near it says its name.
namespace pg::tells {

constexpr float kSpiritStopSeconds = 3.0f;    // standing still after incense reaches it in a hunt
constexpr float kSpiritQuietSeconds = 60.0f;  // no interactions, events or ability after incense outside a hunt
constexpr float kSpiritHearRange = 10.0f;     // metres from the ghost, on its floor, a name is heard from
constexpr float kSpiritAnswerCooldown = 20.0f;
constexpr float kSpiritAnswerMinDelay = 1.0f, kSpiritAnswerMaxDelay = 3.0f;  // seconds from the name to the knock

/** Incense in a hunt (GhostAI +0xF8 becomes set): it stops for kSpiritStopSeconds. */
struct IncenseStop {
  Edge smudge;
  Timer hold;
  /** One frame: `smudged` the hunt's smudge is set, `hunting` it hunts. Returns whether it stands still now. */
  bool step(bool smudged, bool hunting, float dt) {
    const bool landed = smudge.rose(smudged);
    if (!hunting) {
      hold.stop();
      return false;
    }
    if (landed) {
      hold.start(kSpiritStopSeconds);
      return true;
    }
    return hold.step(dt);
  }
};

/** Incense outside a hunt (GhostAI +0xF5 becomes set while it does not hunt): a minute of silence. */
struct Quiet {
  Edge incense;
  Timer left;
  bool step(bool incenseActive, bool hunting, float dt) {
    if (incense.rose(incenseActive) && !hunting) left.start(kSpiritQuietSeconds);
    left.step(dt);
    return quiet();
  }
  bool quiet() const { return left.running(); }
};

/** The lower-case words of a phrase, split at anything but a letter or digit. */
inline std::vector<std::string> words_of(std::string_view text) {
  std::vector<std::string> out;
  std::string word;
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u) || u >= 0x80) {
      word += static_cast<char>(std::tolower(u));
    } else if (!word.empty()) {
      out.push_back(std::move(word));
      word.clear();
    }
  }
  if (!word.empty()) out.push_back(std::move(word));
  return out;
}

/** Whether a recognised phrase says the ghost's name: its whole name, or its first name, as words. */
inline bool says_name(std::string_view phrase, std::string_view name) {
  const std::vector<std::string> said = words_of(phrase), named = words_of(name);
  if (said.empty() || named.empty()) return false;
  for (size_t i = 0; i < said.size(); ++i)
    if (said[i] == named[0]) return true;  // the whole name begins with the first name, so this covers both
  return false;
}

/** The knock answering its name: once, a moment later, at most once every kSpiritAnswerCooldown seconds. */
struct Answer {
  float cooldown = 0;
  float due = -1;  // seconds until the knock, -1 for none
  /** A player near it said its name; `allowed` it may answer now; `delayRoll01` in [0, 1). True when it will knock. */
  bool hear(bool allowed, float delayRoll01) {
    if (!allowed || cooldown > 0) return false;
    due = kSpiritAnswerMinDelay + (kSpiritAnswerMaxDelay - kSpiritAnswerMinDelay) * delayRoll01;
    cooldown = kSpiritAnswerCooldown;
    return true;
  }
  /** One frame; true on the frame it knocks. A pending knock is dropped the moment it may not answer. */
  bool step(bool allowed, float dt) {
    cooldown -= dt;
    if (due < 0) return false;
    if (!allowed) {
      due = -1;
      return false;
    }
    due -= dt;
    if (due > 0) return false;
    due = -1;
    return true;
  }
};

}  // namespace pg::tells
