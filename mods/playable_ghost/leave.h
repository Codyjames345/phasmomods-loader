#pragma once

// Leaving the contract and quitting the game from the ghost player's pause menu (the game's own, in
// their kept journal, src/journal.h). PauseMenuController.Leave and Quit read the local investigator,
// which the ghost player does not have, so while their investigator is out of the contract the two
// buttons run these instead:
//  - Leave: the game's own leave. LoadingController.LoadScene to the main menu, then
//    PhotonNetwork.Disconnect, as PauseMenuController.Leave does.
//  - Quit: Application.Quit, as PauseMenuController.Quit does.
// Both give the ghost player their rig and camera back first. Main thread only.
//
// Closing the game: the runtime's shutdown (il2cpp_shutdown) waits for Photon's receive thread, which
// ends only when its socket closes. Photon is disconnected first and the pending receive cancelled.
namespace pg::leave {

bool resolve();
bool create_hooks();  // before hooks::enable_all

// Every frame: finishes a leave once the main menu has begun loading.
void tick_background();
bool leaving();  // the ghost player has left the contract and its level is still unloading

}  // namespace pg::leave
