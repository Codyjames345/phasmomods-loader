#pragma once

// The ghost's door, light and prop chances (GhostActivity.doorChance, .lightChance and
// .InteractWithARandomProp), through the game API: each draws its own roll of Random.RandomRangeInt(0, 100)
// before anything else in it draws one (roll.h Chance), and inside an aimed interaction that roll is
// answered 0, so the chain takes the aimed object whatever the type's chance (actions.cpp charges the
// interaction's cost instead). Main thread.
namespace pm::chances {

bool create_hooks();  // before hooks::enable_all
// A draw of Random.RandomRangeInt(min, max) that returned `r`: what the game gets, 0 for a chance's own
// roll while `aimed`, else `r`.
int draw(int min, int max, int r, bool aimed);

}  // namespace pm::chances
