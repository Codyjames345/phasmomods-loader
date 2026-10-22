#pragma once

#include "ability.h"

// The custom ghost types' own abilities, from the Custom Ghosts mod when it runs
// (core/services.h). Main thread.
namespace pg::custom_link {

// The ability a ghost type has of its own: the game's (by the type whose rules its AI follows) or a
// custom ghost's (by what it is); null when it has none.
const ability::Ability* own_ability(int behaviourType, int identity);
bool is_custom_ability(int behaviourType, int identity);  // the ability is a custom ghost's
bool use_custom_ability(int identity);                     // host: true if it happened
// Host: a custom ghost's own rules, or Ghost Tells' rules for one of its six vanilla ghosts, keep it
// from interacting (kind 0, the fuse box included), causing an event (kind 1) or hunting (kind 2, Ghost
// Tells only) now.
bool held_back(int identity, int kind);

}  // namespace pg::custom_link
