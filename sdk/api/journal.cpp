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
  const auto net = phasmo::Network::get_Instance();
  const auto player = net && alive(net.raw()) ? phasmo::Network::localPlayer.get(net) : nullptr;
  const auto journal = player && alive(player.raw()) ? phasmo::Player::journal.get(player) : nullptr;
  if (!journal || !alive(journal.raw())) return std::nullopt;
  return phasmo::JournalController::open.get(journal);
}

}  // namespace pm::api::journal
