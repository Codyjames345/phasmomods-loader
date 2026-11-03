#include "reward.h"

#include <string>

#include "Difficulty.h"
#include "GameController.h"
#include "Rewards.h"
#include "held.h"
#include "log.h"
#include "mod_glue.h"
#include "payout.h"
#include "runtime.h"

namespace pm::reward {
namespace {


using phasmo::LevelValues;

struct Pending {
  bool valid = false;
  int mainReward = 0;
  float multiplier = 0;
  bool settled = false;
  bool totalsSet = false;  // the reward screen's money total is the ghost's
  score::Tally tally;
} g_pending;
api::Held g_waiting;  // the reward screen came before the final score: its profile, whose call waits for it
api::Held g_heldScreen;  // the reward screen whose results wait for the final score
bool g_buttonsLogged = false;

// The score is not final yet: outcome.cpp settles it a few seconds after the contract's end, whatever else happens.
bool waiting_for_score() { return g_pending.valid && !g_pending.settled; }

// The ghost player's reward screen gives them none of its own challenge progress, as their contract end skips
// the challenge checks (contract_end.h). Set from the ghost player's contract end to the next contract; the
// progress is dropped only while the screen's own results run, so challenge progress anywhere else is the game's.
bool g_ghostResults = false;
int g_inResults = 0;
bool g_droppedLogged = false;

struct InResults {
  InResults() { ++g_inResults; }
  ~InResults() { --g_inResults; }
  InResults(const InResults&) = delete;
  InResults& operator=(const InResults&) = delete;
};

// Results that have not started save nothing, so they wait for the final score here. The first step cannot wait
// itself: its state machine lives on this method's stack until its first await.
void detour_start_results(api::Original<phasmo::RewardManager::StartResults> original, void* screen) {
  if (waiting_for_score()) {
    g_heldScreen = screen;
    PM_LOG("reward: the reward screen's results wait for the final score");
    return;
  }
  original(screen);
}

// The ghost player's money is the screen's own total, set by the screen's own method once its rows are built, so the
// total it shows is the one the results save straight after this returns.
void detour_build_rows(api::Original<phasmo::RewardManager::BuildRows> original, void* screen) {
  original(screen);
  if (!g_pending.valid || !g_pending.settled || g_pending.totalsSet) return;
  g_pending.totalsSet = true;
  const payout::Result r = payout::ghost(g_pending.tally.points(), g_pending.tally.investigators, g_pending.mainReward,
                                         g_pending.multiplier);
  phasmo::RewardManager::SetTotals(screen, r.money, r.xp);
  PM_LOG("reward: the reward screen's total is the ghost's: $%d, %d XP", r.money, r.xp);
}

// The ghost player earns no seasonal event score: the game's own answer for a contract with no event, so there is no
// row, nothing goes to the game's server and the results take the path they take for any such contract.
phasmo::RewardEventRow detour_event_row(api::Original<phasmo::RewardManager::EventRow> original, void* screen) {
  if (!g_ghostResults) return original(screen);
  PM_LOG("reward: the reward screen sends no seasonal event score for the ghost player");
  return phasmo::RewardEventRow{};
}

// While the results wait the screen's buttons do nothing: a press now would end a screen that has saved nothing.
bool button_waits() {
  if (!waiting_for_score()) return false;
  if (!g_buttonsLogged) PM_LOG("reward: a reward screen button waits for the final score");
  g_buttonsLogged = true;
  return true;
}

void detour_skip(api::Original<phasmo::RewardManager::Skip> original, void* screen) {
  if (!button_waits()) original(screen);
}

void detour_next(api::Original<phasmo::RewardManager::Next> original, void* screen) {
  if (!button_waits()) original(screen);
}

void detour_results(api::Original<phasmo::RewardResults::MoveNext> original, void* machine) {
  const InResults in;
  original(machine);
}

void detour_event_results(api::Original<phasmo::RewardEventResults::MoveNext> original, void* machine) {
  const InResults in;
  original(machine);
}

void detour_challenge_progress(api::Original<phasmo::ChallengesController::ChangeChallengeProgression> original,
                               void* controller, int32_t type, int32_t amount) {
  if (g_ghostResults && g_inResults > 0) {
    if (!g_droppedLogged) PM_LOG("reward: the reward screen's own challenge progress is not the ghost player's");
    g_droppedLogged = true;
    return;
  }
  original(controller, type, amount);
}

payout::Result amount(const Pending& p) {
  return payout::ghost(p.tally.points(), p.tally.investigators, p.mainReward, p.multiplier);
}

void announce(const Pending& p, const payout::Result& r) {
  PM_LOG("reward: ghost payout $%d, %d XP (%d of %d, main objective $%d, difficulty x%.2f)", r.money, r.xp,
         p.tally.points(), p.tally.investigators, p.mainReward, p.multiplier);
  mod::show_message(score::headline(p.tally, true),
                    i18n::text("reward.paid", {{"money", std::to_string(r.money)}, {"xp", std::to_string(r.xp)}, {"details", score::details(p.tally)}}));
}

// The score alone, for a settle that pays nothing at that moment.
void show_score(const score::Tally& t) { mod::show_message(score::headline(t, true), score::details(t)); }

void detour_add_xp_and_funds(api::Original<phasmo::SingleplayerProfile::AddXPAndFunds> original, void* profile,
                             int32_t xp, int32_t funds) {
  if (!g_pending.valid) {
    original(profile, xp, funds);
    return;
  }
  if (!g_pending.settled) {
    g_waiting = profile;
    PM_LOG("reward: the reward screen came before the final score; its payout waits for it");
    return;
  }
  const Pending p = g_pending;
  g_pending = Pending{};
  const payout::Result r = amount(p);
  original(profile, r.xp, r.money);
  announce(p, r);
}

}  // namespace

bool create_hooks() {
  return api::hook<phasmo::SingleplayerProfile::AddXPAndFunds>(&detour_add_xp_and_funds,
                                                               "SingleplayerProfile.AddXPAndFunds",
                                                               api::Twins::CanonicalOnly) &&
         api::hook<phasmo::RewardResults::MoveNext>(&detour_results, "RewardManager.Results.MoveNext",
                                                    api::Twins::CanonicalOnly) &&
         api::hook<phasmo::RewardEventResults::MoveNext>(&detour_event_results,
                                                         "RewardManager.EventRewards.MoveNext",
                                                         api::Twins::CanonicalOnly) &&
         api::hook<phasmo::ChallengesController::ChangeChallengeProgression>(
             &detour_challenge_progress, "ChallengesController.ChangeChallengeProgression",
             api::Twins::CanonicalOnly) &&
         // Every body: the one RewardManager.Awake listens with is a twin, not the canonical one.
         api::hook<phasmo::RewardManager::StartResults>(&detour_start_results, "RewardManager.StartResults",
                                                        api::Twins::All) &&
         api::hook<phasmo::RewardManager::BuildRows>(&detour_build_rows, "RewardManager.BuildRows",
                                                     api::Twins::CanonicalOnly) &&
         api::hook<phasmo::RewardManager::EventRow>(&detour_event_row, "RewardManager.EventRow",
                                                    api::Twins::CanonicalOnly) &&
         api::hook<phasmo::RewardManager::Skip>(&detour_skip, "RewardManager.Skip", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::RewardManager::Next>(&detour_next, "RewardManager.Next", api::Twins::CanonicalOnly);
}

void contract_started() {
  if (g_pending.valid) PM_LOG("reward: unclaimed ghost payout dropped");
  g_pending = Pending{};
  g_waiting = nullptr;
  g_heldScreen = nullptr;
  g_ghostResults = false;
}

void contract_ended() {
  void* values = LevelValues::instance.get();
  void* difficulty = values ? LevelValues::difficulty.get(values) : nullptr;
  void* entry = values ? LevelValues::mainObjective.get(values) : nullptr;
  void* objective = entry ? phasmo::Objective::values.get(entry) : nullptr;
  g_pending = Pending{};
  g_waiting = nullptr;
  g_heldScreen = nullptr;
  g_ghostResults = true;
  g_droppedLogged = false;
  g_buttonsLogged = false;
  g_pending.valid = true;
  g_pending.mainReward = objective ? phasmo::ObjectiveValues::moneyReward.get(objective) : 0;
  g_pending.multiplier = difficulty ? phasmo::Difficulty::GetRewardMultiplier(difficulty) : 0.0f;
  PM_LOG("reward: contract over (main objective $%d, difficulty x%.2f)", g_pending.mainReward, g_pending.multiplier);
}

void settled(const score::Tally& final) {
  if (!g_pending.valid) {
    show_score(final);
    return;
  }
  g_pending.settled = true;
  g_pending.tally = final;
  // The results that waited run now, with the final amount.
  void* screen = g_heldScreen.get();
  g_heldScreen = nullptr;
  if (screen && api::alive(screen)) phasmo::RewardManager::StartResults(screen);
  void* profile = g_waiting.get();
  if (!profile) {  // the reward screen's call pays it when it comes, if one comes: the score shows now
    show_score(final);
    return;
  }
  const Pending p = g_pending;
  g_pending = Pending{};  // so the call below is the plain game's, past the hook
  if (api::alive(profile)) {
    const payout::Result r = amount(p);
    phasmo::SingleplayerProfile::AddXPAndFunds(profile, r.xp, r.money);
    announce(p, r);
  } else {
    PM_LOG("reward: the reward screen's profile went before the final score; ghost payout dropped");
    show_score(p.tally);
  }
  g_waiting = nullptr;
}

void teardown() {
  // The mod is stopping: results still waiting are the game's own again, so the screen is not left stuck.
  void* screen = g_heldScreen.get();
  g_pending = Pending{};
  g_waiting = nullptr;
  g_heldScreen = nullptr;
  g_ghostResults = false;
  if (screen && api::alive(screen)) phasmo::RewardManager::StartResults(screen);
}

}  // namespace pm::reward
