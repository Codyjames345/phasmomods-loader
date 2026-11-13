#pragma once

// The dev menu on screen: one overlay canvas with the sections down the left and the section's rows on the
// right, built with the shared ui:: helpers (the user's call, 2026-11-10: plain rows, no vanilla widget
// dressing for the menu itself). A row's click asks menu_rule.h's refuse() first and shows the reason
// rather than working around a game guard. Main thread; the mod drives it from pmmod_tick.
namespace pm::devmenu::menu {

bool is_open();
/** Opens: builds the rows if needed, marks the dev session on (progression suppression starts), pauses the
 *  game when that switch is on, and frees the cursor. */
void open();
/** Closes: hides the rows, restores the timeScale the pause saved, and takes the cursor back. */
void close();
/** Each frame while open: rereads every value from the game and takes one row click. */
void tick();
/** Throws the canvas away (pmmod_stop). */
void destroy();

}  // namespace pm::devmenu::menu
