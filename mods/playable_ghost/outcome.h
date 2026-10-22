#pragma once

#include "session.h"

// The deception score (score.h). Each investigator's client tells the ghost player its journal guess
// and whether its investigator is dead, whenever either changes. A few seconds after the contract ends
// (so the last guesses can arrive), the ghost player's client counts the score, shows it and sends it
// to the investigators, who see it too. Main thread.
namespace pg::outcome {

void tick(session::Session& s, bool inContract);  // every frame
void contract_over();                              // the ghost player's client, as the contract ends

}  // namespace pg::outcome
