#pragma once

#include <string>

#include "ability.h"
#include "session.h"

// The ghost player's HUD: the investigator's own crosshair and a bottom-centre strip in the game's
// font. A restlessness bar, the actions with the player's own bound keys and how many of each the
// native roll has given (dimmed when unavailable; the ability by its name, and only for a type that
// has one; the fuse box by what it would do), the name of the interactable object under the ghost's
// gaze, a one-line notice (favourite-room changes, movement) and the investigators' average sanity
// with a mark where hunts begin (red once it is low enough). Main thread only.
namespace pg::hud {

struct View {
  session::Status status;
  const char* aimed = nullptr;  // interactable under the ghost's gaze, or null
  std::string interactKey, primaryKey, secondaryKey, fuseKey, crouchKey;
  std::string notice;  // empty when there is nothing to tell the ghost player
  std::string ghostType, favouriteRoom;  // what the ghost is (empty until known)
  const ability::Ability* ability = nullptr;  // its own ability, if its type has one
  std::string senses;  // the ghost's own state (typestate.h), under its type and room
  std::string alert;   // something done to the ghost (perceive.h), top centre; empty for none
};

void show(const View& view);  // creates the HUD on first use
void hide();                  // destroys it

}  // namespace pg::hud
