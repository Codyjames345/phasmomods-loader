#include "content_link.h"

#include <string>

#include "content.h"

namespace pm::content_link {
namespace content = api::content;

const ability::Ability* own_ability(int behaviourType) {
  if (const ability::Ability* a = ability::of(content::id_of(behaviourType))) return a;
  static ability::Ability own;  // the definition's text, in the player's language
  own = {content::ability_name(), content::ability_description()};
  return own.name.empty() ? nullptr : &own;
}

bool definition_ability(int behaviourType) {
  return !ability::of(content::id_of(behaviourType)) && !content::ability_name().empty();
}

bool use_definition_ability() { return content::use_ability(); }

bool held_back(int kind) { return content::held_back(kind); }

}  // namespace pm::content_link
