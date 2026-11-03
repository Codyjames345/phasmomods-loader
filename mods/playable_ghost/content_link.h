#pragma once

#include "ability.h"

// What the ghost's own definition says about playing it, for any ghost: the game's types from what
// Playable Ghost read of the game's code (ability.h, typestate.h), a registered ghost through the content
// layer (sdk/api/content.h), by the type the contract's ghost acts as, never by a type number. Main
// thread.
namespace pm::content_link {

constexpr int kAbilityAlone = 3;  // held_back's kind for the ghost's ability alone (core/mod_api.h, heldBack)
constexpr int kFuseBox = 4;       // held_back's kind for the fuse box

// The ability the ghost has of its own: the game's, by the type whose rules its AI follows
// (`behaviourType`), else the definition's of the type it acts as; null when it has none. The text stays
// valid until the next call.
const ability::Ability* own_ability(int behaviourType);
// Host: uses the definition's ability (a registered ghost's); true if it happened. The game's own
// abilities run through the game's ability state instead.
bool use_definition_ability();
// Whether the ghost's own ability is its definition's (a registered ghost's), not the game's.
bool definition_ability(int behaviourType);
// Whether the ghost's own rules keep it from acting now as `kind` (0 an interaction, never the fuse box; 1 an
// event or its ability; 2 a hunt; kAbilityAlone; kFuseBox): its definition's or any rules attached to its type, a
// vanilla type's included (Ghost Tells' tells). The game's own fuse box rules (the Jinn, the Hantu) are tether.h's.
bool held_back(int kind);

}  // namespace pm::content_link
