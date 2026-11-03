#pragma once

// Leaving the contract and quitting the game from the ghost player's pause menu (the game's own, in
// their kept journal, src/journal.h). PauseMenuController.Leave and Quit expect a whole investigator,
// so while the ghost player's investigator is out of the contract the two buttons first give them
// their rig, camera and body back (body::teardown), then run the game's own Leave or Quit. Only if the
// investigator did not come back (Network or the Player gone) does the mod finish without them:
//  - Leave: the steps of the game's leave coroutine. LoadingController.LoadScene to the main menu,
//    then PhotonNetwork.Disconnect once the loading screen is ready.
//  - Quit: Application.Quit, the game's last step.
// Main thread only.
namespace pm::leave {

bool create_hooks();  // before hooks::enable_all

// Every frame: finishes a leave without an investigator once the main menu has begun loading, and ends
// leaving() once the level is gone.
void tick_background();
// The game's own end of the contract is taking the ghost player out of the level, to the lobby or the main
// menu (src/contract_end.cpp).
void left();
bool leaving();  // the ghost player has left the contract and its level is still unloading

}  // namespace pm::leave
