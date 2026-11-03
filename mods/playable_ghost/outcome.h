#pragma once

#include "session.h"

// The deception score (score.h), which also sets the ghost's pay (payout.h). Each investigator's client tells
// the ghost player its journal guess and whether its investigator is dead, whenever either changes. The ghost
// player's client keeps everyone who is an investigator during the contract, and sees who has left from the
// room's own player list. A few seconds after the contract ends (so the last guesses can arrive), it counts the
// score, hands it to the reward (reward.h), whose popup shows it at once and with the money once paid, and
// sends it to the investigators, who see the score with no money. Main thread.
namespace pm::outcome {

void tick(session::Session& s, bool inContract);  // every frame
void contract_over();                              // the ghost player's client, as the contract ends

}  // namespace pm::outcome
