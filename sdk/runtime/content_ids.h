#pragma once

// The ids and numbers of the game's own ghost types and evidence, as content (core/mod_api.h, version
// 2.2) names them, read from the game's GhostType and EvidenceType enums. Nothing here is fixed to a
// game build: a ghost type's id is "vanilla." and its enum value's name in lower case ("vanilla.mare"),
// and custom ghost types and evidence are numbered from one past the enums' largest values, so a game
// update that adds a type or an evidence needs no change here. Header-only and pure, so the host, the
// offline check and the fake backend share it. Any thread.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pm::content {

inline constexpr std::string_view kVanillaPrefix = "vanilla.";

/// An enum's values as the game has them: (name, number).
using EnumValues = std::vector<std::pair<std::string, int64_t>>;

/// The game's ghost types and evidence as content names and numbers them.
struct GameEnums {
  std::vector<std::pair<int32_t, std::string>> ghosts;    // (type, "vanilla.<name>"); the none value left out
  std::vector<std::pair<int32_t, std::string>> evidence;  // (value, id: "emf" ... "dots", or the name)
  int32_t firstGhostType = -1;  // one past the largest GhostType value; -1 until read
  int32_t firstEvidence = -1;   // one past the largest EvidenceType value; -1 until read
  int32_t noGhostType = -1;     // GhostType's "none" value (the AI's behaviour type for a custom ghost)
  int32_t noEvidence = -1;      // EvidenceType's "none" value (an empty evidence slot)
  int64_t lowestGhostType = 0, lowestEvidence = 0;  // the enums' smallest values, for reports
  // Value names given no id: obfuscated, not plain ASCII, or naming the id of another value (a future
  // "Orbs" beside GhostOrb's "orbs", or two names alike but for case).
  std::vector<std::string> unusable;

  /// Whether the enums were read: content can be numbered.
  bool read() const { return firstGhostType >= 0 && firstEvidence >= 0; }
  /// The id of the vanilla ghost type `type`, or null (None, a value without a usable name, a number
  /// that is no vanilla type, or the enums unread).
  const char* ghost_id(int32_t type) const {
    for (const auto& [t, id] : ghosts)
      if (t == type) return id.c_str();
    return nullptr;
  }
  /// The type of the vanilla ghost `id`, or -1.
  int32_t ghost_type(std::string_view id) const {
    for (const auto& [t, name] : ghosts)
      if (name == id) return t;
    return -1;
  }
  /// The value of the vanilla evidence `id`, or -1.
  int32_t evidence_value(std::string_view id) const {
    for (const auto& [v, name] : evidence)
      if (name == id) return v;
    return -1;
  }
};

namespace detail {

// A value name that can be an id: an ASCII letter, then ASCII letters, digits and underscores. An
// obfuscated name is not, and is never guessed at.
inline bool usable_name(std::string_view name) {
  const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
  if (name.empty() || !letter(name[0])) return false;
  return std::all_of(name.begin(), name.end(),
                     [&](char c) { return letter(c) || (c >= '0' && c <= '9') || c == '_'; });
}

inline std::string lower(std::string_view name) {
  std::string out(name);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// The ids the content contract gives the game's evidence (core/mod_api.h, PmGhostDef::evidence),
// keyed by the EvidenceType value's name, never its number. They are the contract's own ids, so they
// stay fixed; any other evidence the game adds is named by its value's name in lower case.
inline constexpr std::pair<std::string_view, std::string_view> kEvidenceIds[] = {
    {"EMF", "emf"},           {"SpiritBox", "spirit_box"},  {"ultraViolet", "ultraviolet"}, {"GhostOrb", "orbs"},
    {"GhostWritingBook", "writing"}, {"Temperature", "freezing"}, {"DotsProjector", "dots"}};

}  // namespace detail

/// Reads the game's GhostType values `ghostTypes` and EvidenceType values `evidenceTypes` into `out`;
/// `noGhost` and `noEvidence` are the enums' own "none" values (the map's GhostType.None and
/// EvidenceType.None), which name no ghost or evidence and get no id. Returns why they cannot be read
/// (an enum without values, or a value outside 0 to 1,000,000, past which numbering would overflow),
/// or empty. A value whose name is not usable gets no id, and is listed in out.unusable; so does one
/// whose id another value already has, since content could not tell them apart: the evidence the
/// contract names keeps its id, and two other values that collide both lose theirs.
inline std::string read_game_enums(const EnumValues& ghostTypes, const EnumValues& evidenceTypes, int64_t noGhost,
                                   int64_t noEvidence, GameEnums& out) {
  out = {};
  if (ghostTypes.empty()) return "the game's GhostType enum has no values";
  if (evidenceTypes.empty()) return "the game's EvidenceType enum has no values";
  constexpr int64_t kLargest = 1000000;
  for (const EnumValues* values : {&ghostTypes, &evidenceTypes})
    for (const auto& [name, value] : *values)
      if (value < 0 || value > kLargest)
        return "the game's enum value " + name + " is " + std::to_string(value) + ", outside 0 to 1000000";
  GameEnums e;
  int64_t lastGhost = 0, lastEvidence = 0;
  e.lowestGhostType = ghostTypes.front().second;
  e.lowestEvidence = evidenceTypes.front().second;
  for (const auto& [name, value] : ghostTypes) {
    lastGhost = std::max(lastGhost, value);
    e.lowestGhostType = std::min(e.lowestGhostType, value);
    if (value == noGhost) continue;
    if (!detail::usable_name(name)) {
      e.unusable.push_back(name);
      continue;
    }
    e.ghosts.emplace_back(static_cast<int32_t>(value), std::string(kVanillaPrefix) + detail::lower(name));
  }
  for (const auto& [name, value] : evidenceTypes) {
    lastEvidence = std::max(lastEvidence, value);
    e.lowestEvidence = std::min(e.lowestEvidence, value);
    if (value == noEvidence) continue;
    if (!detail::usable_name(name)) {
      e.unusable.push_back(name);
      continue;
    }
    const auto known = std::find_if(std::begin(detail::kEvidenceIds), std::end(detail::kEvidenceIds),
                                    [&](const auto& k) { return k.first == name; });
    e.evidence.emplace_back(static_cast<int32_t>(value),
                            known != std::end(detail::kEvidenceIds) ? std::string(known->second) : detail::lower(name));
  }
  // An id two values share names neither for certain. The contract's own evidence ids win, since they
  // are keyed by the value's exact name.
  const auto drop_collisions = [&](std::vector<std::pair<int32_t, std::string>>& ids, const EnumValues& values,
                                   const auto& keeps) {
    std::vector<std::pair<int32_t, std::string>> kept;
    for (const auto& [value, id] : ids) {
      const auto same = std::count_if(ids.begin(), ids.end(), [&](const auto& other) { return other.second == id; });
      if (same == 1 || keeps(value)) {
        kept.emplace_back(value, id);
        continue;
      }
      for (const auto& [name, v] : values)
        if (v == value) e.unusable.push_back(name);
    }
    ids = std::move(kept);
  };
  const auto contract_evidence = [&](int32_t value) {
    for (const auto& [name, v] : evidenceTypes)
      if (v == value)
        return std::any_of(std::begin(detail::kEvidenceIds), std::end(detail::kEvidenceIds),
                           [&](const auto& k) { return k.first == name; });
    return false;
  };
  drop_collisions(e.ghosts, ghostTypes, [](int32_t) { return false; });
  drop_collisions(e.evidence, evidenceTypes, contract_evidence);
  e.firstGhostType = static_cast<int32_t>(lastGhost + 1);
  e.firstEvidence = static_cast<int32_t>(lastEvidence + 1);
  e.noGhostType = static_cast<int32_t>(noGhost);
  e.noEvidence = static_cast<int32_t>(noEvidence);
  out = std::move(e);
  return {};
}

}  // namespace pm::content
