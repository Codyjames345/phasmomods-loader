#pragma once

#include <cstddef>
#include <cstdint>

#include "moments.h"

// The host records the contract twice a second (moments.h Frame: the ghost, its state and floor,
// every investigator's position and death) and the level's room outlines; crucifix burns are caught
// where the game handles them. When the contract ends, the host finds its top moments and sends the
// replay to every client running this mod; each shows it in the lobby (view.h). Main thread.
namespace pg::replay::recorder {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick();
void receive(int sender, const uint8_t* data, size_t size);
bool take(Replay& out);  // a replay to show, once
void reset();

}  // namespace pg::replay::recorder
