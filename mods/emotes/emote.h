#pragma once

#include <cstddef>
#include <cstdint>

// Playing, sharing and ending emotes in the game (emote_session.h decides; this carries it out): the wheel's key, the
// clip on a player's body, its song from the body, the third-person view or the shown body, the body's facing, the
// ghost hearing the song as talking, and the messages to and from every other client. Main thread.
namespace pm::emotes::play {

/// Loads the bundle, the songs' loudness and the key; false (logged) when the mod cannot run in this build.
bool start();

/// Each frame.
void tick();

/// A message on the mod's channel; `sender` is the loader's word for who sent it.
void receive(int sender, const uint8_t* data, size_t size);

/// Ends every emote and frees what the mod made (the mod stopping).
void stop();

}  // namespace pm::emotes::play
