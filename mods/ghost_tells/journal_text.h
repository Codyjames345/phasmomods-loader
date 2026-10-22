#pragma once

#include <string_view>

#include "tells.h"

// What each told ghost's journal page says about its new tells. The game's page text for a ghost is
// the localisation key "Journal2_<Type>" (its description, then its Strength and Weakness lines);
// journal.cpp adds one more line after them. English only.
namespace pg::tells {

/** The behaviour type whose journal description `key` is, for the six told ghosts; -1 otherwise. */
inline int journal_type(std::u16string_view key) {
  constexpr std::u16string_view kPrefix = u"Journal2_";
  if (key.size() <= kPrefix.size() || key.substr(0, kPrefix.size()) != kPrefix) return -1;
  const std::u16string_view name = key.substr(kPrefix.size());
  if (name == u"Spirit") return kSpirit;
  if (name == u"Mare") return kMare;
  if (name == u"Shade") return kShade;
  if (name == u"Yurei") return kYurei;
  if (name == u"Goryo") return kGoryo;
  if (name == u"Onryo") return kOnryo;
  return -1;
}

/** The line naming a told ghost's new tells, after "Tells: "; null for any other type. */
inline const char* tell_line(int type) {
  switch (type) {
    case kSpirit: return "incense stops it dead in a hunt and silences it for a minute; it knocks when you say its name.";
    case kMare:
      return "it puts the lights out before it shows itself, leaves the rooms it hunted through dark, and often switches off a "
             "light you have just turned on.";
    case kShade:
      return "it vanishes the moment you stare at it, does nothing with two of you nearby, and hesitates when it sees you "
             "together.";
    case kYurei:
      return "smudged, it shuts itself in; the door its ability slams stays locked for a moment; a door left open in its room "
             "draws its ability.";
    case kGoryo:
      return "a video camera can catch it standing in its room while nobody is near, where it also acts more; it heads home "
             "when it loses you far from there.";
    case kOnryo:
      return "it snuffs a flame lit near it, every flame gutters as its count of snuffed flames rises, and a flame it snuffed "
             "will not relight for a while.";
    default: return nullptr;
  }
}

}  // namespace pg::tells
