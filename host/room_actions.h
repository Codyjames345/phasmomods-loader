#pragma once

// The game's own steps for the join guard (host/join_guard.h). Main thread only.
namespace pg::room_actions {

bool resolve();
bool lobby_open();     // the lobby's screen (ServerManager) is up; looked for at most once a second
// Each true when the game's step ran; false (and logged) when it could not, so it can be asked again.
bool leave_lobby();    // ServerManager.LeaveServer: the lobby's own Leave
bool kick(int actor);  // ServerManager.KickPlayer with that player's ID card (the host)

}  // namespace pg::room_actions
