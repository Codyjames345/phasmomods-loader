#pragma once

/// @file
/// @brief Whether this client's journal, and with it the pause screen, is open, as the game holds it.
///
/// The pause screen is a page of the player's journal (the journal holds the PauseMenuController, and
/// PauseMenuController.Resume closes the journal), so the journal's own flag, JournalController.open, is set while
/// either is open. open() reads it on this client's investigator (Network.localPlayer, Player.journal), so a mod knows
/// the player is in the journal or paused without guessing it from the cursor.
///
/// Main thread. Local only: each client holds its own journal.
/// @ingroup runtime
/// @since Contract 2.6

#include <optional>

namespace pm::api::journal {

/// Whether this client's journal or pause screen is open now (JournalController.open of the local investigator's
/// Player.journal).
/// @return The answer, or nothing with no local investigator or journal, or where an entry it reads did not resolve.
std::optional<bool> open();

}  // namespace pm::api::journal
