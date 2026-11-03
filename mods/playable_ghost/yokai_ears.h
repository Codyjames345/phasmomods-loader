#pragma once

#include <vector>

#include "held.h"

// The Yokai player's hearing in a hunt (perceive::yokai_hears) applied to the investigators' voices on
// the ghost player's client, through the game API alone. Each voice's positional tap (PlayerVoice.localTap,
// the tap that plays where its player stands) is muted while the Yokai's hunt would not hear its player.
// The radio's voices (PlayerVoice.globalTap) are muted for the ghost player anyway (senses.h). Main thread.
namespace pm::yokai_ears {

// Mutes the positional voice of each investigator the Yokai `ghost`'s hunt would not hear now and unmutes
// those it would; adds each source it muted to `muted`, once.
void hear(void* ghost, std::vector<api::Held>& muted);
// Unmutes every source in `muted` that is still alive, and empties it.
void unmute(std::vector<api::Held>& muted);

}  // namespace pm::yokai_ears
