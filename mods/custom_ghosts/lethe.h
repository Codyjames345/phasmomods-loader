#pragma once

#include <cstddef>
#include <cstdint>

// The Lethe (lethe_rule.h) in the game.
//  - Deja vu: the host records each appearance it makes (GhostAI.Appear / UnAppear: where, facing,
//    which form, how long) and plays each once more a few minutes later, exactly so.
//  - It forgets its prey: in a hunt, the player it still looks for after losing sight of them (the hunt's
//    last player, +0x58) is dropped once the hunt's own sight test has failed for them for a second
//    (lethe_rule.h LetheMemory), so it stops searching where it lost them.
//  - Media fades: the host times every filled photo, video and sound slot of the journal; a slot nobody
//    looked at (any client with the journal's media pages open says so) fades after three minutes on
//    every client, through the game's own removal (JournalMedia.RemoveMedia). Main thread.
namespace pg::custom::lethe {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pg::custom::lethe
