#pragma once

#include "score.h"

// The ghost player's contract reward (see payout.h). At the end of a contract the ghost player's client reads the
// game's own main objective reward and difficulty reward multiplier; once the deception score is final (outcome.h)
// that gives the ghost's amount.
//
// The money: the reward screen's own money total becomes the ghost's amount once its rows are built
// (RewardManager.SetTotals), so the total it shows is what RewardManager.Results saves, once, through the game's own
// save, and the ghost player gets no investigator money. The experience: SingleplayerProfile.AddXPAndFunds pays the
// ghost's (sdk/internals/contract.md, "Where the reward screen's money and experience go").
//
// The pay rests on the final score, so a reward screen that comes before it waits: its results do not start
// (RewardManager.StartResults) and its Skip and Next do nothing until the score is final, a few seconds after the
// contract's end at most. A late guess therefore never underpays the ghost.
//
// The score's popup shows as soon as the score is final, with the money when it is paid then; a reward screen that
// pays later shows the score with the money again, and a contract with no reward screen still shows the score. The
// screen's own challenge progress (RewardManager.Results and EventRewards, through
// ChallengesController.ChangeChallengeProgression) is not given to the ghost player, as their contract end skips the
// challenge checks, and its seasonal event score is neither shown nor sent (RewardManager.EventRow). The screen's Steam
// achievements are left to the game. Main thread only.
namespace pm::reward {

bool create_hooks();  // before hooks::enable_all
void contract_ended();  // ghost player's client, before leaving the level
void settled(const score::Tally& final);  // ghost player's client, once the score is final
void contract_started();  // a payout not taken by the last reward screen is dropped
void teardown();  // the mod is stopping: results still waiting run as the game's own

}  // namespace pm::reward
