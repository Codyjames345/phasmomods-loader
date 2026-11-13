#pragma once

// The join prompt over a dancer of a synced emote (docs/superpowers/specs/2026-11-13-synced-emotes-design.md): the
// Interact button's glyph and "Join" beside it, on a small world-space canvas above the dancer's head that turns to
// face the viewer. The user's design, 2026-11-13. Only this client sees its own prompt.

#include <string>

namespace pm::emotes::join {

/// Draws the keycap's texture. False when it cannot be made (logged); the prompt then shows no glyph on keyboard.
bool load();
/// Lets the prompt and its texture go.
void unload();

/// Shows the prompt this frame above `head` (a Transform), facing `viewer` (a Transform). On a gamepad `padSprite` is
/// the game's own Sprite for the button; otherwise the keycap shows `keyName` (the keyboard key, "E"). A different head
/// moves the prompt to it.
void show(void* head, void* viewer, void* padSprite, const std::string& keyName);

/// Hides the prompt, if shown.
void hide();

}  // namespace pm::emotes::join
