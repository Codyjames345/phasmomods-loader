#include "journal.h"

#include "JournalController.h"
#include "Player.h"
#include "game_api.h"
#include "runtime.h"

namespace pm::api::journal {

std::optional<bool> open() {
  if (!all_resolved<phasmo::Network::get_Instance, phasmo::Network::localPlayer, phasmo::Player::journal,
                    phasmo::JournalController::open>())
    return std::nullopt;
  void* net = phasmo::Network::get_Instance();
  void* player = net && alive(net) ? phasmo::Network::localPlayer.get(net) : nullptr;
  void* journal = player && alive(player) ? phasmo::Player::journal.get(player) : nullptr;
  if (!journal || !alive(journal)) return std::nullopt;
  return phasmo::JournalController::open.get(journal);
}

}  // namespace pm::api::journal
