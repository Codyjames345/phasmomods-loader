#pragma once

#include <string>

#include "consent_text.h"

// The notice that every player answers before any mod starts, drawn as the game's own popup over the
// main menu (host/menu_panel.h), and only so: the loader draws no window of its own. It waits for
// main-menu focus with no time limit, and hides when focus is lost. Its buttons are copies of the
// popup's Doozy button, clicked by the game's input.
// It answers Declined, logged, so the game runs without mods, when it could never be shown or answered:
// at begin, for a build that gives the host no listener on a button, no way to read the main menu or no
// way to copy its popup; and while the menu is in view, when open() says the failure lasts (the popup or
// its button lacks what a copy needs), when open() throws, which also takes away a copy it half built,
// or after 10 tries a second apart, counted only with the menu in view, that found no popup in the scene.
// It blocks the game's own UI beneath it until it is answered. "I agree" is remembered against the notice's
// fingerprint, which covers the game build `game`, in `rememberedPath`; "Play without mods" is not remembered, so the
// next start asks again. On a build whose hash could not be read nothing is remembered, so every start asks.
// Main thread only.
namespace pm::consent {

enum class Answer { Pending, Agreed, Declined };

void begin(const Notice& notice, const build::Hashes& game, const std::wstring& rememberedPath);
Answer tick();  // every frame: shows the notice (again, after a scene change) until it is answered
void hide();
// Why the answer is Declined when the player did not click it (the notice could not be shown), for the Mods panel; empty
// when it is not (host/mods_panel.h tick_off).
std::string off_cause();

}  // namespace pm::consent
