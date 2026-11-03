#pragma once

#include <string>

// A centred popup in the game's font: a title, one line of text and a dark panel, for the investigator who tries to open
// the truck before the ghost player has chosen the ghost. Main thread only.
// PM_UI_TODO: a mod-drawn window, against the vanilla-UI rule. The game has no popup mapped for a contract, so it stays until
// the author chooses a game surface for it (FINDINGS.md 31). The payout and score moved to the game's own popup.
namespace pm::notice {

// The truck stays shut: `title` and `body` are the caller's text (the mod's string table), `body` for `secondsLeft`.
void show_wait(unsigned secondsLeft, const std::string& title, const std::string& body);
// Every frame in a contract, with the body for `secondsLeft`; 0 once the truck may open.
void tick_wait(unsigned secondsLeft, const std::string& body);
void tick();                           // every frame: hides the popup when its time is up
void hide();

}  // namespace pm::notice
