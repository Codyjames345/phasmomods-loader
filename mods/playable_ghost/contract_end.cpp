#include "contract_end.h"

#include "Difficulty.h"
#include "GameController.h"
#include "Photon.h"
#include "body.h"
#include "exit_route.h"
#include "held.h"
#include "leave.h"
#include "log.h"
#include "outcome.h"
#include "reward.h"
#include "runtime.h"

namespace pm::contract_end {
namespace {

void* live(void* instance) { return api::alive(instance) ? instance : nullptr; }

// What the game's own check of the call's sender reads, in both ends (src/exit_route.h).
exit_route::Sender exit_sender(void* info) {
  exit_route::Sender s;
  s.inRoom = phasmo::PhotonNetwork::get_InRoom();
  s.offline = phasmo::PhotonNetwork::get_OfflineMode();
  if (!s.inRoom || s.offline) return s;
  void* sender = info ? phasmo::PhotonMessageInfo::Sender.get(info) : nullptr;
  void* players = phasmo::PhotonNetwork::get_PlayerList();
  s.readable = sender && players;
  const api::RefArray list{players};
  for (size_t i = 0, n = list.size(); i < n; ++i) {
    void* p = list[i];
    if (!p) s.readable = false;
    else s.actors.push_back(phasmo::PhotonPlayer::get_ActorNumber(p));
  }
  s.actor = sender ? phasmo::PhotonPlayer::get_ActorNumber(sender) : 0;
  return s;
}

// The ghost's payout and deception score, in that order: the score, once final, sets the pay. Every end
// comes here, the one the game takes when every investigator is gone by any mix of deaths and quits too.
void pay() {
  reward::contract_ended();
  outcome::contract_over();
}

// The end for a client whose investigator did not come back, where the game's body would throw:
// LevelValues.completed, and the game's own way back to the lobby.
void back_to_lobby() {
  if (void* values = live(phasmo::LevelValues::instance.get())) phasmo::LevelValues::completed.set(values, true);
  if (void* gc = live(phasmo::GameController::instance.get())) phasmo::GameController::LoadBackToMenu(gc);
}

// The ghost player's run of the game's body skips only these two checks, so the mission and challenge
// progress an investigator earns does not reach them. An exception out of the game's body lowers the flag.
bool g_skipChecks = false;
struct SkipChecks {
  SkipChecks() { g_skipChecks = true; }
  ~SkipChecks() { g_skipChecks = false; }
};

void detour_check_missions(api::Original<phasmo::ExitLevel::CheckMissions> original) {
  if (!g_skipChecks) original();
}

void detour_check_challenges(api::Original<phasmo::ExitLevel::CheckChallenges> original) {
  if (!g_skipChecks) original();
}

// GameController.Exit writes the death status (GameplaySettings.PlayerDied) on every client, alive or not.
// The ghost player never died, so their run of it leaves the status as it found it, as the truck's end does
// for a living investigator: the reward screen then shows them no death row or halved totals and returns
// their committed equipment (sdk/systems/contract.md). Put back however the game's body ends.
class KeepDeathStatus {
 public:
  KeepDeathStatus() : settings_(phasmo::GameplaySettings::get()) {
    if (settings_) was_ = phasmo::GameplaySettings::PlayerDied.get(settings_);
  }
  ~KeepDeathStatus() {
    if (settings_ && phasmo::GameplaySettings::PlayerDied.get(settings_) != was_) {
      phasmo::GameplaySettings::PlayerDied.set(settings_, was_);
      PM_LOG("contract end: no death status for the ghost player");
    }
  }
  KeepDeathStatus(const KeepDeathStatus&) = delete;
  KeepDeathStatus& operator=(const KeepDeathStatus&) = delete;

 private:
  api::Held settings_;
  bool was_ = false;
};

// Either end's call on a ghost player's client. eventsBlocked: the path ExitLevel.Exit takes while it is set.
// keepDeathStatus: GameController.Exit's, which writes the death status for everyone.
template <typename Original>
void end_contract(Original original, void* self, void* info, bool eventsBlocked, bool keepDeathStatus) {
  switch (exit_route::route(true, exit_route::accepted(exit_sender(info)), eventsBlocked)) {
    case exit_route::Route::Game:
      original(self, info);
      return;
    case exit_route::Route::GameAsInvestigator: {
      body::teardown();
      if (body::investigator_back()) {
        PM_LOG("contract end: the game's own way to the main menu");
        leave::left();
        original(self, info);
        return;
      }
      PM_LOG("contract end: no investigator came back; the ghost player's own end instead");
      pay();
      back_to_lobby();
      return;
    }
    case exit_route::Route::GhostPlayer: {
      pay();
      body::teardown();
      if (!body::investigator_back()) {
        PM_LOG("contract end: no investigator came back; back to the lobby without the game's bookkeeping");
        back_to_lobby();
        return;
      }
      // The game's body keeps its own order; the two checks do nothing in it (src/contract_end.h).
      leave::left();
      const SkipChecks skip;
      if (keepDeathStatus) {
        const KeepDeathStatus keep;
        original(self, info);
      } else {
        original(self, info);
      }
      return;
    }
  }
}

// ExitLevel.Exit runs on every client when the truck leaves. Only a ghost player's client reads
// anything here; every other client goes straight to the game's body.
void detour_exit(api::Original<phasmo::ExitLevel::Exit> original, void* exitLevel, void* info) {
  if (!body::stepped_out()) {
    original(exitLevel, info);
    return;
  }
  void* values = live(phasmo::LevelValues::instance.get());
  end_contract(original, exitLevel, info, values && phasmo::LevelValues::eventsBlocked.get(values), false);
}

// GameController.Exit runs on every client when every player has died. It has no path of its own for
// LevelValues.eventsBlocked.
void detour_game_exit(api::Original<phasmo::GameController::Exit> original, void* controller, void* info) {
  if (!body::stepped_out()) {
    original(controller, info);
    return;
  }
  end_contract(original, controller, info, false, true);
}

}  // namespace

bool create_hooks() {
  // The canonical bodies, as the raw hook by address was.
  return api::hook<phasmo::ExitLevel::Exit>(&detour_exit, "ExitLevel.Exit", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::GameController::Exit>(&detour_game_exit, "GameController.Exit", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::ExitLevel::CheckMissions>(&detour_check_missions, "ExitLevel.CheckMissions",
                                                     api::Twins::CanonicalOnly) &&
         api::hook<phasmo::ExitLevel::CheckChallenges>(&detour_check_challenges, "ExitLevel.CheckChallenges",
                                                       api::Twins::CanonicalOnly);
}

}  // namespace pm::contract_end
