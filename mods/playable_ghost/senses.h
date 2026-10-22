#pragma once

#include "session.h"

// What the ghost player hears and says. Main thread only.
//  - Their microphone is muted through Vivox for the whole contract (with no investigator to carry
//    it, their voice would otherwise reach everyone), and any unmute by the game is refused.
//  - While the ghost is being chosen they hear nothing (the truck's conversation is not theirs).
//  - Once the ghost exists they hear from the ghost's head (the listener is on their camera, which
//    sits there), so investigators' positional voices reach them with the game's own falloff.
//    Walkie-talkie (global) voice taps are muted for them; a ghost cannot listen to the radio.
//  - Dead investigators are silent for them, as for any living investigator (the game's
//    VoiceVolume.Update decides that from the local investigator, which their client has none of).
namespace pg::senses {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s, void* ghost);
void teardown();

}  // namespace pg::senses
