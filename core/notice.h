#pragma once

#include <string>

// A centred popup in the game's font: a title, one line of text and a dark panel. Used for the
// investigator who tries to open the truck before the ghost player has chosen the ghost, and for
// the ghost player's payout after a contract. Main thread only.
namespace pg::notice {

void show(const std::string& title, const std::string& body, unsigned ms);
void show_wait(unsigned secondsLeft);  // the truck stays shut; refreshes the countdown
void tick_wait(unsigned secondsLeft);  // every frame in a contract; 0 once the truck may open
void tick();                           // every frame: hides the popup when its time is up
void hide();

}  // namespace pg::notice
