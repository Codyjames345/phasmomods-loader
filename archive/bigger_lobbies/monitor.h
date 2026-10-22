#pragma once

// The truck's sanity monitor (HeartRateData) for eight players. Its four player panels sit in a grid;
// four more are copied from its last, each in its own colour, and the game's lists of panels, names
// and readings grow to eight, so it fills them by itself. The panels keep their size: four show at a
// time and the monitor turns to players 5-8 and back every few seconds. Main thread.
namespace pg::lobbies::monitor {

bool resolve();
void tick();
void teardown();

}  // namespace pg::lobbies::monitor
