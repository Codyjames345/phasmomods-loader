#pragma once

#include <vector>

#include "ghostbook.h"
#include "il2cpp.h"

// The ghost player keeps the game's own journal. It hangs off their investigator's networked body
// (PCPlayer/NetworkedJournal: the 3D book Journal_anim, its world-space pages canvas
// Journal2.0Canvas with JournalController and PauseMenuController, and the evidence and ghost pages
// with their sync controllers). The body stays in the contract, parked in the truck and out of play
// (src/body.h), so the journal stays on it: it goes with the body as an investigator's does, and
// the game's own handler shows each investigator's evidence and ghost choices in it, as in every
// Player's journal. While open it is placed in front of the ghost player's camera.
//
// Nothing of it reaches the network while the ghost player has it (the other clients' copies are on
// the hidden body in the truck, where a page turn would be heard):
//  - Its PhotonViews stop synchronising.
//  - They no longer count as this client's (PhotonView.IsMine off). The journal's own code then runs
//    every page turn and tab change locally instead of sending it as an RPC, which it does for
//    views that are not this client's.
//  - Any RPC still sent from them is dropped.
// release() undoes all of it, and the mod's own pages, for an investigator who takes the body back.
// Opening and closing are the game's own steps run locally: JournalController.StateChangeSync's (it
// stops early for a client without a Player, which the ghost player is) and the investigator rig's
// LocalJournalController's (the rig is switched off for the ghost player), including the pages
// canvas's move to the PlayerView layer that lets the game's HUD passes draw it over the book. The
// evidence tab is view only, and the pause menu's Leave and Quit are handled by
// src/leave.cpp. While open, the book is held where the game holds an investigator's. Main thread
// only.
namespace pg::journal {

struct GhostButton {
  int type = -1;         // GhostType
  il2cpp::Held rect;     // the button's transform, for hit tests
  il2cpp::Held ticked;   // Image shown when chosen (ThreeStateButton.tickedImage)
  il2cpp::Held crossed;  // Image shown when crossed out (ThreeStateButton.crossedOutImage)
  il2cpp::Held button;   // its UnityEngine.UI.Button (the journal's hover highlight)
};

// The evidence spread taken over by the mod. The left (evidence) page's contents are hidden and
// `page` covers it. The right page is the game's ghost page, whose buttons the mod may mark.
struct Spread {
  il2cpp::Held page;         // an empty page over the evidence page (RectTransform)
  il2cpp::Held title;        // the ghost page's title text (transform), to copy for headings
  il2cpp::Held description;  // its "We've decided it's a:" text (transform), to copy for body text
  il2cpp::Held ghostTitle;   // the ghost page's heading, for the mod's text (TextMeshProUGUI)
  il2cpp::Held ghostLine;    // the line under it, for the mod's text (TextMeshProUGUI)
  std::vector<GhostButton> ghosts;
};

bool resolve();
bool create_hooks();  // before hooks::enable_all

void keep(void* player);  // the investigator's Player, as it is parked out of play
void release();           // give the journal back to the investigator's body as the game left it
bool kept();
void* root();  // the kept journal's root transform (NetworkedJournal), or null

void open(bool pause);  // on the pause menu (pause) or where it was last left; ends the spread
void close();
bool is_open();
bool on_pause();  // open on the pause menu

// Every frame while the ghost player has their camera: holds the open journal in front of it and
// keeps the journal's views local.
void tick(void* camera);


// Opens the journal on the evidence spread for the mod's own page (the game's clicks switched off),
// or returns null. end_spread puts the evidence page and the ghost buttons back as they were.
const Spread* begin_spread();
void end_spread();
bool spread_active();  // false once anything (opening the pause menu, closing) has ended it

// Show `state` (0 empty, 1 ticked, 2 crossed out) on a ghost button as ThreeStateButton.SetState
// draws it, without raising the state-change event that drives the journal's network sync.
void set_state(const GhostButton& b, int state);

// The ghost player's versions of the investigators' pages (Home, Items, Overview, the evidence spread's
// headings), written over them when the journal is kept. Call while it is open; unchanged text is
// not rewritten.
void write(const ghostbook::Book& book);
// The ghost button under the pointer (null for none) gets the journal's own hover highlight, as the
// button's pointer enter and exit draw it.
void hover(const GhostButton* b);

// A copy of one of the journal's text objects (its font and style) under parent, with its localiser
// switched off so the mod's own text stays. Returns the TextMeshProUGUI component.
void* copy_text(void* sourceTransform, void* parent);

}  // namespace pg::journal
