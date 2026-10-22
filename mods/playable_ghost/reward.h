#pragma once

// The ghost player's contract reward (see payout.h). At the end of a contract the ghost player's
// client reads the game's own main objective reward and difficulty reward multiplier; on the
// reward screen the game's own payout (SingleplayerProfile.AddXPAndFunds) then pays the ghost's
// amount instead of the investigator one, and a popup says what it was for. Main thread only.
namespace pg::reward {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void contract_ended(bool everyInvestigatorDied);  // ghost player's client, before leaving the level
void contract_started();  // a payout not taken by the last reward screen is dropped

}  // namespace pg::reward
