#pragma once

#include "bindings.h"
#include "session.h"

// The ghost player's choices on their own journal (src/journal.h), held open in front of them over a
// slow panorama of the chosen room (src/panorama.h), so the truck and the investigators are not seen.
//
// The journal opens on its evidence spread. The right-hand page is the journal's own ghost page,
// where the chosen ghost is ticked and those this contract does not allow are crossed out. The
// left-hand page is the truck's map (src/mapview.h): a room is chosen by clicking it, and the arrows
// change floor.
//  - At the start of a contract, while the host holds the spawn: ghost type and favourite room.
//    "Begin the haunting" sends both; with less than two seconds left whatever is chosen is sent as it
//    is, and anything unchosen is left to the native rolls. Escape switches to the journal's pause
//    menu and back. Without a journal or map it falls back to two plain lists driven by the movement
//    keys and Interact.
//  - After a native favourite-room change, while the host allows another room: the Journal key opens
//    the map page and any room can be made the favourite room instead.
// Main thread only.
namespace pg::picker {

struct Keys {
  bindings::Key up, down, left, right, confirm;
};

struct Input {
  bool focused = false;         // the game window has focus
  bool pausePressed = false;    // the Pause binding went down this frame
  bool journalPressed = false;  // the Journal binding went down this frame
};

// Call every frame while the local player is the ghost player with their camera. Returns true while
// the picker is shown (and while the pause menu opened from it is): it then owns the ghost player's
// input, this frame's key presses and the cursor.
bool tick(session::Session& s, const Keys& keys, const Input& in);
void hide();

}  // namespace pg::picker
