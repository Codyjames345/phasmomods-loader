#pragma once

#include <string>
#include <vector>

#include "il2cpp.h"
#include "ui.h"

// The mod's popups in the main menu, drawn as the game's own error popup: a copy of ErrorManager's
// ErrorParent (the black-out, the taped title, the pinned lined paper and the game's button) beside
// the original, preserving the board's native transform and using its canvas camera. The copy's
// localisers and error manager are switched off (they would put the game's words back and drive the
// game's error queue). Its buttons keep the game's own button working, with the game's listeners taken
// off and the loader's put on (sdk/systems/menus.md): the game drives their hover, press and colour, and
// its own input clicks them. Main thread only.
namespace pm::menu_panel {

// One of the loader's copies of a game button: the copy's Doozy UIButton, the target of the loader's listener on its
// click event. The listener only counts the click; clicked() takes it.
struct Control {
  il2cpp::Held button;
};

// Held (il2cpp.h): everything a panel keeps between frames.
struct Row {
  il2cpp::Held toggle;       // a copy of the lobby paper's Toggle, the row's control; its texts are its children
  il2cpp::Held name;         // TextMeshProUGUI in the mod-name column
  il2cpp::Held description;  // TextMeshProUGUI in the description column
  bool shownOn = false;      // what the host last set the toggle to
  bool locked = false;       // never takes clicks: a mod that stays off whatever the player chooses
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
  bool anyPaper = false;  // shown over whichever paper of the corkboard is up, not only the main menu's (message)
};

// What this build lacks, in the log: at activation, once the host's game API is bound. Without the main menu's entries the
// notice cannot be shown and answers Declined; without the popup's, the same.
void report();
// Whether the host can read the main menu at all: its board, camera focus and the menu's buttons. Without it the menu never
// reads as in view, so the consent notice could never be shown or answered: it answers Declined (host/consent.h).
bool menu_available();
// Whether the loader can put a listener of its own on a game event: Unity's UnityEvent.AddListener and UnityAction.
// Without it no control of the loader's can take a click.
bool listeners_available();
// Whether a copy of the menu's popup or tape can be made safe and clickable: every entry the copy and its scripts use
// resolved, so a copy cannot act for the game, and its Doozy button can take the loader's listener.
bool can_copy();
// Whether the Mods panel's rows can be made: copies of the lobby paper's Toggle, each with a new change event.
bool can_copy_rows();
/** True only while the active main-menu board is focused through its UI camera. */
bool main_menu_visible();
/** Why main_menu_visible() is false, for the log (a fixed string), or null while it is true. */
const char* why_menu_hidden();
/** True while the game shows its own main-menu buttons (Singleplayer, Multiplayer), focused or not. */
bool menu_buttons_shown();

// Why open() made no popup, for its caller and the log. `lasting`: this game build cannot make one, and asking again
// makes none: an entry a copy needs did not resolve, or the game's popup or its button lacks what a copy needs.
// Otherwise it may work later in this run: the main menu is out of view, or the game's popup is not in the scene, or a
// copy of it could not be made, this time. `why` is a fixed string.
struct OpenFailure {
  bool lasting = false;
  const char* why = nullptr;
  bool noPopup = false;  // a message found no popup in this scene: it searches less often while that lasts
};

// A popup with `title`, `body` (wrapped rich text at a fixed readable size) and one button per
// label, left to right along the bottom. False (and nothing left behind) when the menu's popup
// cannot be copied, with why in `failure` when it is given.
bool open(Panel& p, const std::string& title, const std::string& body, const std::vector<std::string>& buttons,
          OpenFailure* failure = nullptr);
// Lets the open panel's body shrink to fit its paper, down to a floor, so a long text cannot run over the buttons.
void fit_body(const Panel& p);
void close(Panel& p);
bool is_open(const Panel& p);
// Open and drawn: the copy active in the scene and the canvas it sits on switched on.
bool drawn(const Panel& p);

// Every frame while open: closes on lost menu focus and returns the index of the button the game
// clicked since the last tick, or -1.
int tick(Panel& p);

// Rows of the lobby paper's checkbox (a copy of its Toggle) with aligned mod-name and description
// columns, above a status footer. A locked row is greyed, as the game greys a checkbox it does not let you click, and
// stays so.
bool add_row(Panel& p, int index, int count, const std::string& name, const std::string& description, bool locked = false);
// Sets the row's toggle without raising its change event.
void set_row_checked(Panel& p, size_t index, bool checked);
// Whether the rows' toggles take clicks (Selectable.set_interactable; the game greys one that does not). A locked row
// never does.
void set_rows_interactable(Panel& p, bool on);
// The first row whose toggle the game flipped since the host last set it, or -1. The host sets it back
// (set_row_checked) to what it then holds.
int row_clicked(const Panel& p);
void set_text(void* text, const std::string& value);

// For the mod's other copies of the menu's own controls (the Mods button; can_copy first): the copy's scripts that
// would act for the game switched off (its buttons too, until clickable() wires one), and the camera the menu is seen
// through.
void make_inert(void* root);
void* menu_camera();
// Keeps the copy's Doozy UIButton on root's GameObject working as the game's: every behaviour gets a new, empty
// UnityEvent and loses its runners (the game's listeners), the click behaviour's event gets the loader's listener,
// and the button is switched on. False when any step cannot be made: this switches on no button, and the listeners
// it took off stay off, so whatever the copy's button then does runs nothing of the game's.
bool clickable(Control& c, void* root);

// Whether the game clicked the control since the last call; takes every click counted.
bool clicked(Control& c);
// Takes the loader's listener's count for the control away, and the control's button with it.
void release(Control& c);

// A one-button message ("OK") in the game's popup, on the corkboard (sdk/systems/menus.md): over the lobby's paper as over
// the main menu's. It waits until the board is focused, however long that takes, hides while the board is not, and comes
// back until it is clicked. While the scene has no popup (a contract) it looks less and less often, at least every 16 s, so
// it can show that long after the lobby is back. Replaces any message showing or waiting. A build that cannot make the
// popup, or a popup that throws while it is built, drops it with a log line: it is only news, and the loader draws no
// window of its own instead.
void message(const std::string& title, const std::string& body);
// The same for a mod's message: it never takes the place of the host's, such as a lobby's removal list or a start's wait. It waits
// behind one, and shows once that is gone. Behind or in front, the newest mod message replaces an older one.
void mod_message(const std::string& title, const std::string& body);
// Takes back the message showing or waiting: one that no longer holds, as the room it spoke of has been left. A mod's message
// behind it stays, and comes next.
void drop_message();
void tick_message();  // every frame
const Panel& shown_message();  // the message's panel: is_open says whether it shows now

}  // namespace pm::menu_panel
