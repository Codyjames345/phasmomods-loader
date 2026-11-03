#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "text_table.h"

// The ghost player's deception score: a point for every investigator who was killed, who survived but named
// the wrong ghost (or none), or who quit mid-contract. One point per investigator at most. The ghost's pay is
// its share of these points (payout.h). Pure logic, kept free of game calls so it can be tested offline.
namespace pm::score {

constexpr uint8_t kNoGuess = 0xFF;

struct Guess {
  uint8_t type = kNoGuess;  // the GhostType chosen in their journal
  bool dead = false;
  bool operator==(const Guess&) const = default;
};

struct Tally {
  int killed = 0, misled = 0, quit = 0, investigators = 0;
  int points() const { return killed + misled + quit; }
  bool operator==(const Tally&) const = default;
};

/**
 * The score. `investigators` is everyone who was one during the contract, so one who quit before their first
 * guess still counts. `present` is who was still in the room as the contract ended: a last message saying
 * alive does not keep a leaver from counting as quit. `guesses` is each one's last message, by actor. A death
 * reported before the quit counts as the death: the order changes the line, never the points.
 */
inline Tally tally(const std::set<int>& investigators, const std::vector<int>& present,
                   const std::map<int, Guess>& guesses, int actualType) {
  Tally t;
  for (const int actor : investigators) {
    ++t.investigators;
    const auto heard = guesses.find(actor);
    const Guess g = heard == guesses.end() ? Guess{} : heard->second;
    if (g.dead) ++t.killed;
    else if (std::find(present.begin(), present.end(), actor) == present.end()) ++t.quit;
    else if (g.type != actualType) ++t.misled;
  }
  return t;
}

/** The popup's headline, for the ghost player (yours) or the investigators. */
inline std::string headline(const Tally& t, bool yours) {
  return yours ? i18n::text("score.yours", {{"points", std::to_string(t.points())}, {"of", std::to_string(t.investigators)}})
               : i18n::text("score.ghosts", {{"points", std::to_string(t.points())}, {"of", std::to_string(t.investigators)}});
}

/** What the points were for: quitters are named only when there are some. */
inline std::string details(const Tally& t) {
  if (t.quit > 0)
    return i18n::text("score.details-quit",
                      {{"killed", std::to_string(t.killed)}, {"misled", std::to_string(t.misled)}, {"quit", std::to_string(t.quit)}});
  return i18n::text("score.details", {{"killed", std::to_string(t.killed)}, {"misled", std::to_string(t.misled)}});
}

}  // namespace pm::score
