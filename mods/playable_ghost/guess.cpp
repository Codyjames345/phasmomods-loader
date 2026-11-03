#include "guess.h"

#include "GhostTypes.h"
#include "JournalController.h"
#include "Player.h"
#include "runtime.h"

namespace pm::guess {

score::Guess of(void* player) {
  score::Guess g;
  if (!player || !api::alive(player)) return g;
  g.dead = phasmo::Player::isDead.get(player);
  void* journal = phasmo::Player::journal.get(player);
  if (journal && api::alive(journal) && phasmo::JournalController::type.is(journal)) {
    const int32_t type = phasmo::JournalController::GetGhostType(journal);
    if (type != phasmo::GhostType::None() && type >= 0 && type < score::kNoGuess) g.type = static_cast<uint8_t>(type);
  }
  return g;
}

}  // namespace pm::guess
