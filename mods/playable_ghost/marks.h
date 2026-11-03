#pragma once

#include "session.h"

// E1, host side: the marks of perceive.h for the ghost's type, read from the game's own state five
// times a second and sent to the ghost player (session SENSE). Main thread.
namespace pm::marks {

bool resolve();
// Every frame on the host. ghost: the GhostAI while a player controls it (null otherwise);
// behaviourType: the type whose rules its AI follows. A registered ghost's marks are its definition's
// player view (sdk/api/content.h player_view), the Black Shuck's scent for one.
void tick(session::Session& s, void* ghost, int behaviourType);

}  // namespace pm::marks
