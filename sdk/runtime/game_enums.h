#pragma once

// The game's GhostType and EvidenceType enums read whole from the resolved map: every value and its
// name, from the type the map's Type entry resolved to, and each enum's "none" value from its None
// entry. The host reads them from the running game's model at startup, the offline check from the
// dump's; content_ids.h's read_game_enums then numbers content from them. Header-only.

#include <string>
#include <string_view>

#include "content_ids.h"
#include "model.h"
#include "resolver.h"

namespace pm::content {

namespace detail {

// The resolved result of the entry `key`, or null when it is not in `resolution` or did not resolve.
inline const sdk::Result* resolved(const sdk::Resolution& resolution, std::string_view key) {
  for (const sdk::Result& r : resolution.results)
    if (r.desc && r.desc->key && r.desc->key == key) return r.status == sdk::Status::Resolved ? &r : nullptr;
  return nullptr;
}

// Every literal value, with its name, of the enum the resolved Type entry `key` names, into `out`.
// False when the entry did not resolve or names no enum in `model`.
inline bool enum_values(const sdk::GameModel& model, const sdk::Resolution& resolution, std::string_view key,
                        EnumValues& out) {
  out.clear();
  const sdk::Result* r = resolved(resolution, key);
  if (!r || r->type < 0 || static_cast<size_t>(r->type) >= model.types.size()) return false;
  const sdk::ModelType& t = model.types[static_cast<size_t>(r->type)];
  if (!t.isEnum) return false;
  for (const sdk::ModelField& f : t.fields)
    if (f.hasLiteral) out.emplace_back(f.name, f.literal);
  return true;
}

}  // namespace detail

/// Reads the map's GhostType and EvidenceType enums from `model` as `resolution` resolved them, into
/// `out` (content_ids.h). Returns why they cannot be read (GhostType, EvidenceType, GhostType.None or
/// EvidenceType.None did not resolve, or read_game_enums refused the values), or empty.
inline std::string read_game_enums(const sdk::GameModel& model, const sdk::Resolution& resolution, GameEnums& out) {
  out = {};
  EnumValues ghostTypes, evidenceTypes;
  if (!detail::enum_values(model, resolution, "GhostType", ghostTypes))
    return "the map's GhostType enum did not resolve";
  if (!detail::enum_values(model, resolution, "EvidenceType", evidenceTypes))
    return "the map's EvidenceType enum did not resolve";
  const sdk::Result* noGhost = detail::resolved(resolution, "GhostType.None");
  const sdk::Result* noEvidence = detail::resolved(resolution, "EvidenceType.None");
  if (!noGhost || !noEvidence) return "the map's GhostType.None or EvidenceType.None did not resolve";
  return read_game_enums(ghostTypes, evidenceTypes, noGhost->value, noEvidence->value, out);
}

}  // namespace pm::content
