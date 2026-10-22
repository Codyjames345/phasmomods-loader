#pragma once

#include "session.h"

// E1, host side: the marks of perceive.h for the ghost's type, read from the game's own state five
// times a second and sent to the ghost player (session SENSE). Main thread.
namespace pg::marks {

bool resolve();
// Every frame on the host. ghost: the GhostAI while a player controls it (null otherwise);
// behaviourType: the type whose rules its AI follows; identity: what it is (a custom ghost's type).
void tick(session::Session& s, void* ghost, int behaviourType, int identity);

}  // namespace pg::marks
