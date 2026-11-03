#include "hunt.h"

#include "GhostHuntState.h"
#include "LevelRoom.h"
#include "Player.h"
#include "runtime.h"

namespace pm::custom::hunt {

// The game's test reads, in this order: the player's room (throwing for none), the room's kind, the
// player's death and, for a living player, the killed list (throwing for none), and returns
// `alive && !killed.Contains(player) && kind != outsideMap`.
bool may_go_for(void* hunt, void* player) {
  if (!hunt || !player) return false;
  void* room = phasmo::Player::currentRoom.get(player);
  const api::List killed{phasmo::GhostHuntState::killed.get(hunt)};
  if (!room || !killed.raw) return false;
  return !phasmo::Player::isDead.get(player) && !killed.contains(player) &&
         phasmo::LevelRoom::kind.get(room) != phasmo::RoomKind::outsideMap();
}

bool begun(void* hunt) { return hunt && phasmo::GhostHuntState::startDelay.get(hunt) < 0; }

void record_seen(void* hunt, void* player) {
  const api::List seen{hunt ? phasmo::GhostHuntState::seen.get(hunt) : nullptr};
  if (seen.raw && player && !seen.contains(player)) seen.add(player);
}

namespace {
bool g_ownSight = false;  // an own_sees call is running
}  // namespace

bool own_sees(void* hunt, void* player) {
  struct Own {  // sets the flag for the call and restores it on return, or when the game's test throws
    bool outer = g_ownSight;
    Own() { g_ownSight = true; }
    ~Own() { g_ownSight = outer; }
  } own;
  return phasmo::GhostHuntState::Sees(hunt, player, false);
}

bool own_sight() { return g_ownSight; }

void clear_own_sight() { g_ownSight = false; }

}  // namespace pm::custom::hunt
