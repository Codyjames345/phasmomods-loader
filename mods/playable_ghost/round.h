#pragma once

#include <string>

#include "session.h"

// One contract with a ghost player, run through the game's own ghost setup. Main thread only.
//
// Host: when the contract starts the native ghost spawn (GhostController, after every player has
// loaded) is held back while the ghost player picks a type and favourite room, for at most
// kPickMs. The spawn then runs natively with only the weighted type roll and the favourite-room
// roll answered by the ghost player's choice (each only if the native roll could have produced
// it); anything not chosen is left to the native rolls. Native favourite-room changes happen as in
// vanilla and then open a short window in which the ghost player may move the favourite room to
// the room the ghost is in instead.
//
// Everyone: while the host holds the spawn the truck's door stays shut. The host refuses the
// truck's own door button (LiftButton) and the investigator who pressed it is told to wait. The
// house's main door is the game's own and untouched.
namespace pg::round {

constexpr unsigned kPickMs = 30000;
constexpr unsigned kRoomClaimMs = 20000;

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick(session::Session& s);
void teardown();

// Level and ghost data for the ghost player's UI. Indices are into the level's favourite-room list.
int room_count();
void* room(int index);  // the LevelRoom, or null
std::string room_name(int index);
int current_ghost_room();  // the room the ghost is in, or -1
int favourite_room();      // the ghost's favourite room, or -1
bool ghost_in_favourite_room();  // the room the ghost is in is its favourite room (any room, candidate or not)
std::string ghost_type_name(int type);

}  // namespace pg::round
