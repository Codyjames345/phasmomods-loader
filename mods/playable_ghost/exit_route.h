#pragma once

#include <climits>
#include <vector>

// Which way the contract's end (ExitLevel.Exit or GameController.Exit, src/contract_end.cpp) goes on the ghost player's client, kept
// free of game calls so it can be tested offline. The game's own body reads this client's
// investigator, which the ghost player does not have while they play the ghost (src/body.h):
//  - A call the game's own sender check refuses goes to the game's body, which refuses it, as vanilla
//    does, before it reads anything. That refusal flags the sender (sdk/systems/photon.md). The mod sends
//    neither end, so a refused one here is another client's.
//  - The path vanilla takes while LevelValues.eventsBlocked is set (it leaves to the main menu) goes to
//    the game's body too, once the investigator is back.
//  - Vanilla's other path pays the ghost, then runs the game's body as the investigator too, without
//    its mission and challenge checks.
namespace pm::exit_route {

// What both ends' own check of their RPC's sender reads (decompiled).
struct Sender {
  bool inRoom = false;      // PhotonNetwork.get_InRoom
  bool offline = false;     // PhotonNetwork.get_OfflineMode
  bool readable = false;    // the sender, PhotonNetwork.get_PlayerList and each player in it are non-null
  int actor = 0;            // the sender's actor number
  std::vector<int> actors;  // the actor number of each player in PhotonNetwork.get_PlayerList
};

// Whether the check lets the call through. Outside a room, or offline, it lets every call through. In
// a room it lets through only the room's first actor: the lowest actor number among the room's
// players. Where it meets a null it throws: false here, so the game's body meets the null itself.
inline bool accepted(const Sender& s) {
  if (!s.inRoom || s.offline) return true;
  if (!s.readable) return false;
  int first = INT_MAX;
  for (int a : s.actors)
    if (a < first) first = a;
  return s.actor == first;
}

enum class Route {
  Game,                // the game's own body, as vanilla runs it
  GameAsInvestigator,  // the ghost player's investigator comes back, then the game's own body runs
  GhostPlayer,         // the ghost's payout, then the same, without the mission and challenge checks
};

// steppedOut: the ghost player plays the ghost on this client (body.cpp's step-out). accepted: as
// above. eventsBlocked: LevelValues.eventsBlocked, which picks ExitLevel.Exit's path; GameController.Exit
// has none for it, so false there.
inline Route route(bool steppedOut, bool accepted, bool eventsBlocked) {
  if (!steppedOut || !accepted) return Route::Game;
  return eventsBlocked ? Route::GameAsInvestigator : Route::GhostPlayer;
}

}  // namespace pm::exit_route
