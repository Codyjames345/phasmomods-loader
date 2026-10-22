#pragma once

#include <charconv>
#include <map>
#include <string>
#include <string_view>

#include "ghosts.h"

// The custom difficulty's Ghost Type choice for the custom ghosts, kept free of game calls so it can
// be tested offline (difficulty.h has the game's side).
namespace pg::custom {

constexpr int kNoGhostType = 24;  // GhostType.None: the option's "Random"

/** Custom 1..3 are Difficulty.difficulty 6..8 (Difficulty.IsCustom). */
inline bool custom_preset(int preset) { return preset >= 6 && preset <= 8; }

// The custom ghost each custom difficulty preset forces. The game's save never holds one, so this
// is where a preset's custom ghost is kept between sessions.
class Picks {
 public:
  /** The custom ghost `preset` forces, or kNoGhostType. */
  int of(int preset) const {
    auto it = picks_.find(preset);
    return it == picks_.end() ? kNoGhostType : it->second;
  }

  /** The player chose `type` for `preset`: a custom ghost is remembered, anything else forgets
   * it. True when what is remembered changed. */
  bool choose(int preset, int type) {
    if (!custom_preset(preset)) return false;
    if (is_custom(type)) {
      const bool changed = of(preset) != type;
      picks_[preset] = type;
      return changed;
    }
    return picks_.erase(preset) > 0;
  }

  /** One "preset=type" line each. */
  std::string text() const {
    std::string out;
    for (const auto& [preset, type] : picks_) out += std::to_string(preset) + "=" + std::to_string(type) + "\n";
    return out;
  }

  /** Picks from text(); any other line is ignored. */
  static Picks parse(std::string_view text) {
    Picks picks;
    while (!text.empty()) {
      const size_t end = text.find('\n');
      std::string_view line = text.substr(0, end);
      text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
      if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
      const size_t eq = line.find('=');
      int preset = 0, type = 0;
      if (eq == std::string_view::npos || !whole_number(line.substr(0, eq), preset) || !whole_number(line.substr(eq + 1), type))
        continue;
      if (is_custom(type)) picks.choose(preset, type);
    }
    return picks;
  }

 private:
  static bool whole_number(std::string_view s, int& out) {
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), out);
    return !s.empty() && error == std::errc() && end == s.data() + s.size();
  }

  std::map<int, int> picks_;
};

/** The ghost type a Difficulty may carry out of the game: the save file never holds a custom ghost,
 * and the lobby sync only to a room where everyone runs the custom ghosts. */
inline int outbound_type(int type, bool toSave, bool everyone) {
  return is_custom(type) && (toSave || !everyone) ? kNoGhostType : type;
}

/** The Ghost Type option's number of choices: Random, then every ghost in its collection. */
inline int ghost_choices(int collectionSize) { return collectionSize + 1; }

/** The option's index once it has `count` choices: Random when it no longer fits. */
inline int ghost_index(int index, int count) { return index >= 0 && index < count ? index : 0; }

}  // namespace pg::custom
