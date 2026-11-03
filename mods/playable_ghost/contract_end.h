#pragma once

// The contract's end on the ghost player's client. A contract ends in one of two RPCs: ExitLevel.Exit
// when the truck leaves, GameController.Exit when every player has died. Each one's native body reads
// this client's investigator, which the ghost player does not have while they play the ghost
// (src/body.h), so their investigator comes back first and the game's own body then runs.
// src/exit_route.h says which way each call goes. On the way that ends the contract normally the game
// keeps its bookkeeping for the player (stats sync, mission status, objective counts, journal close) and
// only its mission and challenge checks are skipped, with the death status GameController.Exit writes for
// everyone: the ghost player never died. The ghost's payout (src/reward.h) replaces what they would earn.
// Main thread only.
namespace pm::contract_end {

bool create_hooks();  // before hooks::enable_all

}  // namespace pm::contract_end
