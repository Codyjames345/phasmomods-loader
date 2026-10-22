#pragma once

#include <cstdint>
#include <map>
#include <string>

// The ghost player's deception score: a point for every investigator who died and for every one who
// survived but named the wrong ghost (or none). Money is not involved: only the end popup shows it.
// Pure logic, kept free of game calls so it can be tested offline.
namespace pg::score {

constexpr uint8_t kNoGuess = 0xFF;

struct Guess {
  uint8_t type = kNoGuess;  // the GhostType chosen in their journal
  bool dead = false;
  bool operator==(const Guess&) const = default;
};

struct Tally {
  int killed = 0, misled = 0, investigators = 0;
  int points() const { return killed + misled; }
  bool operator==(const Tally&) const = default;
};

/** The score from every investigator's last guess (keyed by actor) and what the ghost was. */
inline Tally tally(const std::map<int, Guess>& guesses, int actualType) {
  Tally t;
  for (const auto& [actor, g] : guesses) {
    ++t.investigators;
    if (g.dead) ++t.killed;
    else if (g.type != actualType) ++t.misled;
  }
  return t;
}

/** The popup's lines, for the ghost player (yours) or the investigators. */
inline std::string headline(const Tally& t, bool yours) {
  return (yours ? "Your deception score: " : "The ghost's deception score: ") + std::to_string(t.points()) + " of " +
         std::to_string(t.investigators);
}

inline std::string details(const Tally& t) {
  auto plural = [](int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); };
  return plural(t.killed, "investigator killed", "investigators killed") + ", " +
         plural(t.misled, "misled about the ghost", "misled about the ghost");
}

}  // namespace pg::score
