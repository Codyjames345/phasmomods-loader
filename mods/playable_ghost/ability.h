#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "text_table.h"

// The ghost types with an ability of their own: the ones GhostAI's ability entry (state 15, build
// 25811160) starts a state for. Every other type's ability roll does nothing. What each one does is
// read from that state's code. A type is named by its content id ("vanilla.wraith"), so its number is
// the game's own (sdk/api/content.h type_of). A custom ghost's ability is its definition's, read
// through the content layer (content_link.h). Kept free of game calls so it can be tested offline.
namespace pm::ability {

struct Ability {
  std::string name;
  std::string description;
};

inline const Ability* of(std::string_view ghostId) {
  // One per type, its text read from the mod's table on each call, so it is the table's as it stands now.
  static Ability kWraith, kPhantom, kPoltergeist, kJinn, kDemon, kYurei, kObake;
  const auto with = [](Ability& a, std::string name, std::string description) {
    a = {std::move(name), std::move(description)};
    return &a;
  };
  if (ghostId == "vanilla.wraith") return with(kWraith, i18n::text("ability.wraith"), i18n::text("ability.wraith.about"));
  if (ghostId == "vanilla.phantom") return with(kPhantom, i18n::text("ability.phantom"), i18n::text("ability.phantom.about"));
  if (ghostId == "vanilla.poltergeist")
    return with(kPoltergeist, i18n::text("ability.poltergeist"), i18n::text("ability.poltergeist.about"));
  if (ghostId == "vanilla.jinn") return with(kJinn, i18n::text("ability.jinn"), i18n::text("ability.jinn.about"));
  if (ghostId == "vanilla.demon") return with(kDemon, i18n::text("ability.demon"), i18n::text("ability.demon.about"));
  if (ghostId == "vanilla.yurei") return with(kYurei, i18n::text("ability.yurei"), i18n::text("ability.yurei.about"));
  if (ghostId == "vanilla.obake") return with(kObake, i18n::text("ability.obake"), i18n::text("ability.obake.about"));
  return nullptr;
}

// The Jinn's ability state leaves at once while the power is off.
inline bool needs_power(std::string_view ghostId) { return ghostId == "vanilla.jinn"; }

}  // namespace pm::ability
