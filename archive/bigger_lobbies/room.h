#pragma once

// Eight players in a private room. The host's room takes eight (the game asks Photon for four when it
// creates the room and sets four again back in the lobby; its lock to the players already in when a
// contract starts is kept), and every client's per-player colours grow from four to eight. Main thread.
namespace pg::lobbies::room {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick();

}  // namespace pg::lobbies::room
