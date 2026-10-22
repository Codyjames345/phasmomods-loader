#pragma once

#include <string>

#include "session.h"

// E1 and E3 on the ghost player's own client: the host's marks (perceive.h) drawn where they are on
// screen, the Kormos's near blindness in a hunt (the view darkens; only what it hears shows), the
// Yokai's hunting deafness (investigators' voices farther than 2.5 m fall silent, as its vanilla
// hearing does), and the alert line. Main thread.
namespace pg::perceive_view {

bool resolve();
// Every frame while the ghost player plays the ghost. camera: their eyes; ghost: the replicated
// GhostAI; shown: false while a menu covers the view (alerts still arrive and wait). Returns the alert
// line for the HUD (empty for none).
std::string tick(session::Session& s, void* camera, void* ghost, bool shown);
void hide();  // destroys the markers and gives back any muted voices

}  // namespace pg::perceive_view
