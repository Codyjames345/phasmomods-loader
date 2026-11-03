#pragma once

#include <string>

// The game's own steps for the join guard (host/join_guard.h). Main thread only. An entry that did not resolve leaves each
// answering false, so the guard does not take a step that never ran for one that did.
namespace pm::room_actions {

// Whether this build gives everything the join guard needs: a leave, a kick, and what it reads to know which room this is, who
// is in it and whether a contract is up. Without it the lobby's rules could not hold, so the host starts no mod. False
// before the host's bind.
bool available();
// What available() lacks, for the log: the keys of the entries that did not resolve, separated by ", "; empty when
// available.
std::string missing();
// The lobby's screen (ServerManager, looked for at most once a second) is up and no contract's load has begun: the game
// stops Photon's queue as a contract starts, while this scene is still up (sdk/systems/photon.md).
bool lobby_open();
// Each true when the game's step ran; false (and logged) when it could not, so it can be asked again.
bool leave_lobby();    // ServerManager.LeaveServer: the lobby's own Leave
// Whether `button` is the lobby's Start button (ServerManager.startGameButton). False when no lobby is found, or it cannot be read.
bool is_start_button(void* button);
// The name the room shows for the player `actor`, as they chose it; empty when it cannot be read.
std::string player_name(int actor);
// ServerManager.KickPlayer with that player's ID card (the host). The game's method can do nothing without a word (see its
// map note), so true proves nothing about the player: the join guard waits to see them leave the room.
bool kick(int actor);

}  // namespace pm::room_actions
