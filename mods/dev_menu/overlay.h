#pragma once

// The dev menu's read-outs that stay on screen while the menu is closed: the debug HUD (lines of text in the
// top-left corner) and ESP (labels drawn over the game's picture at the ghost and at the level's rooms). Both
// are the mod's own text on one canvas of their own, made when first switched on and destroyed on stop. Every
// value is read from the game each frame through actions.h; nothing here calls a game method that changes
// anything. Main thread.
namespace pm::devmenu::overlay {

/** Each frame, menu open or not: shows or hides the HUD and the ESP labels as the switches in
 *  actions::switches() say (debugHud, espGhost, espRooms), and refreshes what is shown. */
void tick();

/** Destroys the canvas and forgets every label (pmmod_stop, and a scene change that took the canvas). */
void destroy();

}  // namespace pm::devmenu::overlay
