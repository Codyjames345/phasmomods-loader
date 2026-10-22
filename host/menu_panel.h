#pragma once

#include <string>
#include <vector>

#include "il2cpp.h"
#include "ui.h"

// The mod's popups in the main menu, drawn as the game's own error popup: a copy of ErrorManager's
// ErrorParent (the black-out, the taped title, the pinned lined paper and the game's button) beside
// the original, preserving the board's native transform and using its canvas camera. The copy's own
// button scripts and localisers are switched off (they would drive the game's error queue and put
// the game's words back), and clicks are read by the mod. Main thread only.
namespace pg::menu_panel {

// One of the mod's copies of the menu's taped buttons: its Image, where clicks are read, and the
// game's own colour animator (Doozy's UISelectableColorAnimator, left running on the copy), played
// as the pointer enters and leaves so the copy lights up as the game's tapes do.
struct Control {
  il2cpp::Held image;
  il2cpp::Held animator;  // null when the copy has none
  bool over = false;      // the pointer was on it last frame
};

// Held (il2cpp.h): everything a panel keeps between frames.
struct Row {
  il2cpp::Held hit;          // full-width row hit rectangle
  il2cpp::Held name;         // TextMeshProUGUI in the mod-name column
  il2cpp::Held description;  // TextMeshProUGUI in the description column
  il2cpp::Held checkmark;    // native checkbox graphic, active when selected
};

struct Panel {
  il2cpp::Held root;   // the copy (Transform)
  il2cpp::Held title;  // TextMeshProUGUI on the tape
  il2cpp::Held body;   // TextMeshProUGUI on the paper
  il2cpp::Held paper;  // the paper's RectTransform (Background), for rows
  float rowFontSize = 0;   // native body size before content-dependent auto-sizing
  std::vector<Control> buttons;
  std::vector<il2cpp::Held> buttonTexts;  // TextMeshProUGUI
  std::vector<Row> rows;
};

bool resolve();
/** True only while the active main-menu board is focused through its UI camera. */
bool main_menu_visible();
/** Why main_menu_visible() is false, for the log (a fixed string), or null while it is true. */
const char* why_menu_hidden();
/** True while the game shows its own main-menu buttons (Singleplayer, Multiplayer), focused or not. */
bool menu_buttons_shown();

// A popup with `title`, `body` (wrapped rich text at a fixed readable size) and one button per
// label, left to right along the bottom. False (and nothing left behind) when the menu's popup
// cannot be copied.
bool open(Panel& p, const std::string& title, const std::string& body, const std::vector<std::string>& buttons);
void close(Panel& p);
bool is_open(const Panel& p);
// Open and drawn: the copy active in the scene and the canvas it sits on switched on.
bool drawn(const Panel& p);

// Every frame while open: closes on lost menu focus, lights up the button under the pointer, and
// returns the index of the button clicked this frame, or -1.
int tick(Panel& p);

// Native checkbox graphics with aligned mod-name and description columns above a status footer.
bool add_row(Panel& p, int index, int count, const std::string& name, const std::string& description);
void set_row_checked(Panel& p, size_t index, bool checked);
int row_clicked(const Panel& p);
void set_text(void* text, const std::string& value);

// For the mod's other copies of the menu's own controls (the Mods button): the copy's scripts that
// would act for the game switched off, the camera the menu is seen through, and a control that
// lights up while the pointer is over it and reports a click this frame.
void make_inert(void* root);
void* menu_camera();
Control control_of(void* root);  // the Image and colour animator on root's GameObject
bool press(Control& c);

// A one-button message ("OK"), shown until it is clicked. Replaces any message still showing.
void message(const std::string& title, const std::string& body);
void tick_message();

}  // namespace pg::menu_panel
