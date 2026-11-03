// Offline test of the ghost player's payout (mods/playable_ghost/reward.cpp): the contract's main objective
// reward is read through LevelValues.instance, mainObjective, Objective.values and ObjectiveValues.moneyReward,
// and the reward screen's own AddXPAndFunds pays the ghost's share instead. The game is the fake backend, with
// its fields at offsets of the test's own; notice.cpp is modelled here.
// Run: build\reward_test.exe
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "Difficulty.h"
#include "Rewards.h"
#include "check.h"
#include "text_fixture.h"
#include "fake_game.h"
#include "hooks.h"

#include "../mods/playable_ghost/reward.cpp"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct Object {
  void* klass = nullptr;
  alignas(8) unsigned char fields[0x40]{};
};
Object values, difficulty, objective, objectiveValues, profile;
void* valuesStatic = &values;  // LevelValues.instance's storage
float multiplier = 0.5f;
int paidXp = -1, paidFunds = -1, pays = 0;
std::string lastNotice, lastBody;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

template <class T>
void put(Object& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

float reward_multiplier(void* d, const void*) { return d == &difficulty ? multiplier : -1.0f; }
void add_xp_and_funds(void* p, int32_t xp, int32_t funds, const void*) {
  CHECK(p == &profile);
  paidXp = xp;
  paidFunds = funds;
  ++pays;
}

// The reward screen's results as the game runs them: each progresses a challenge through the controller's
// method, which the mod's hook sees first. `progressed` counts what reached the game.
int progressed = 0;
void call_hooked(const char* key, void* self, int32_t type, int32_t amount);
void challenge_progress(void*, int32_t, int32_t, const void*) { ++progressed; }
void results(void* machine, const void*) { call_hooked("ChallengesController.ChangeChallengeProgression", machine, 0x16, 100); }

// The reward screen as the game runs it, from the decompile: RewardManager.StartResults runs the results' first step,
// which builds the rows (RewardManager.BuildRows, adding to the totals), then saves the money total once. The rows are
// worth $700 and 90 XP here. `shown` is where the shown total's count ends.
Object screen;
int starts = 0, saved = 0, saves = 0, shown = 0, screenMoney = 0, screenXp = 0, skips = 0, nexts = 0;
void hooked_build_rows(void* s);
void set_totals_body(void* s, int32_t money, int32_t xp, const void*) {
  CHECK(s == &screen);
  screenMoney = shown = money;
  screenXp = xp;
}
void build_rows_body(void*, const void*) {
  screenMoney = shown = 700;
  screenXp = 90;
}
void start_results_body(void* s, const void*) {
  CHECK(s == &screen);
  ++starts;
  hooked_build_rows(s);
  saved += screenMoney;
  ++saves;
}
int eventRows = 0;
phasmo::RewardEventRow event_row_body(void*, const void*) {
  ++eventRows;
  phasmo::RewardEventRow row;
  row.words[0] = 7;  // filled: the score the results would send
  return row;
}
void skip_body(void*, const void*) { ++skips; }
void next_body(void*, const void*) { ++nexts; }
void event_results(void* machine, const void*) { call_hooked("ChallengesController.ChangeChallengeProgression", machine, 0x27, 1); }

/** Gives every entry reward.cpp uses, and binds through a host that records each hook's detour. */
void give_entries() {
  fake::reset();
  fake::static_field(map::LevelValues_instance, &valuesStatic, sizeof valuesStatic, true);
  fake::field(map::LevelValues_difficulty, 0x10);
  fake::field(map::LevelValues_mainObjective, 0x18);
  fake::field(map::Objective_values, 0x10);
  fake::field(map::ObjectiveValues_moneyReward, 0x10);
  fake::method(map::Difficulty_GetRewardMultiplier, reinterpret_cast<void*>(&reward_multiplier));
  fake::method(map::SingleplayerProfile_AddXPAndFunds, reinterpret_cast<void*>(&add_xp_and_funds));
  fake::method(map::RewardManager_Results_MoveNext, reinterpret_cast<void*>(&results));
  fake::method(map::RewardManager_EventRewards_MoveNext, reinterpret_cast<void*>(&event_results));
  fake::method(map::ChallengesController_ChangeChallengeProgression, reinterpret_cast<void*>(&challenge_progress));
  fake::method(map::RewardManager_StartResults, reinterpret_cast<void*>(&start_results_body));
  fake::method(map::RewardManager_BuildRows, reinterpret_cast<void*>(&build_rows_body));
  fake::method(map::RewardManager_SetTotals, reinterpret_cast<void*>(&set_totals_body));
  fake::method(map::RewardManager_EventRow, reinterpret_cast<void*>(&event_row_body));
  fake::method(map::RewardManager_Skip, reinterpret_cast<void*>(&skip_body));
  fake::method(map::RewardManager_Next, reinterpret_cast<void*>(&next_body));
  put<void*>(values, 0x10, &difficulty);
  put<void*>(values, 0x18, &objective);
  put<void*>(objective, 0x10, &objectiveValues);
  put<int32_t>(objectiveValues, 0x10, 1000);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::reward::create_hooks());
}

/** The reward screen pays `xp` and `funds` through the game's call, which our hook sees first. */
void reward_screen(int32_t xp, int32_t funds) {
  const auto hooked = detours.find("SingleplayerProfile.AddXPAndFunds");
  CHECK(hooked != detours.end());
  if (hooked == detours.end()) return;
  reinterpret_cast<void (*)(void*, int32_t, int32_t, const void*)>(hooked->second)(&profile, xp, funds, nullptr);
}

void call_hooked(const char* key, void* self, int32_t type, int32_t amount) {
  const auto hooked = detours.find(key);
  CHECK(hooked != detours.end());
  if (hooked == detours.end()) return;
  reinterpret_cast<void (*)(void*, int32_t, int32_t, const void*)>(hooked->second)(self, type, amount, nullptr);
}

/** The game runs one step of a state machine through the hooked MoveNext `key`. */
void run_machine(const char* key) {
  const auto hooked = detours.find(key);
  CHECK(hooked != detours.end());
  if (hooked == detours.end()) return;
  reinterpret_cast<void (*)(void*, const void*)>(hooked->second)(&values, nullptr);
}

/** The game calls one of the reward screen's hooked methods, with no arguments but the screen. */
void screen_calls(const char* key) {
  const auto hooked = detours.find(key);
  CHECK(hooked != detours.end());
  if (hooked == detours.end()) return;
  reinterpret_cast<void (*)(void*, const void*)>(hooked->second)(&screen, nullptr);
}
void hooked_build_rows(void*) { screen_calls("RewardManager.BuildRows"); }
void reset_screen() { starts = saved = saves = shown = screenMoney = screenXp = skips = nexts = 0; }

/** Challenge progress that does not come from the reward screen's results. */
void progress_elsewhere() { call_hooked("ChallengesController.ChangeChallengeProgression", &objective, 0x16, 100); }
}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
// The score's popup: the game's own, through the host contract's show_message (core/mod_glue.h).
namespace pm::mod {
void show_message(const std::string& title, const std::string& body) {
  lastNotice = title;
  lastBody = body;
}
}  // namespace pm::mod

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  give_entries();
  using pm::score::Tally;

  // No contract of the ghost player's: the reward screen pays what the game says. A score still shows, with no pay.
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(pays == 0 && lastNotice == "Your deception score: 3 of 3" && lastBody == "3 killed, 0 misled");
  lastNotice.clear();
  reward_screen(5, 7);
  CHECK(pays == 1 && paidXp == 5 && paidFunds == 7 && lastNotice.empty());

  // Every point: the main objective's $1000 at a x0.5 difficulty pays the ghost twice that, once.
  pm::reward::contract_started();
  pm::reward::contract_ended();
  pm::reward::settled(Tally{2, 0, 1, 3});
  CHECK(pays == 1);  // the score alone pays nothing: the reward screen does
  CHECK(lastNotice == "Your deception score: 3 of 3");  // but it shows at once, with no money
  CHECK(lastBody == "2 killed, 0 misled, 1 quit");
  reward_screen(5, 7);
  CHECK(pays == 2 && paidXp == 2000 && paidFunds == 2000);
  CHECK(lastNotice == "Your deception score: 3 of 3");  // one popup, the score and the money together
  CHECK(lastBody == "$2000 and 2000 XP: 2 killed, 0 misled, 1 quit");
  reward_screen(5, 7);
  CHECK(pays == 3 && paidXp == 5 && paidFunds == 7);

  // A share of the points is that share of the pay.
  pm::reward::contract_ended();
  pm::reward::settled(Tally{1, 1, 0, 3});
  reward_screen(5, 7);
  CHECK(pays == 4 && paidXp == 1333 && paidFunds == 1333);
  CHECK(lastNotice == "Your deception score: 2 of 3");
  CHECK(lastBody == "$1333 and 1333 XP: 1 killed, 1 misled");

  // Every investigator named the ghost and lived: nothing for the ghost, and still the one popup.
  pm::reward::contract_ended();
  pm::reward::settled(Tally{0, 0, 0, 3});
  reward_screen(5, 7);
  CHECK(pays == 5 && paidXp == 0 && paidFunds == 0);
  CHECK(lastBody == "$0 and 0 XP: 0 killed, 0 misled");

  // The reward screen comes first: nothing is paid on a score that may yet change. The game's call waits, and
  // runs once with the final amount, so a late guess costs the ghost nothing.
  lastNotice.clear();
  pm::reward::contract_ended();
  reward_screen(5, 7);
  CHECK(pays == 5 && lastNotice.empty());
  reward_screen(5, 7);  // the game's other caller: still waiting, still nothing paid
  CHECK(pays == 5);
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(pays == 6 && paidXp == 2000 && paidFunds == 2000);
  CHECK(lastNotice == "Your deception score: 3 of 3");
  pm::reward::settled(Tally{0, 0, 0, 3});  // and only once
  CHECK(pays == 6);
  reward_screen(5, 7);
  CHECK(pays == 7 && paidXp == 5 && paidFunds == 7);

  // The reward screen's profile is gone by the time the score settles: nothing is paid, and the score still shows.
  lastNotice.clear();
  pm::reward::contract_ended();
  reward_screen(5, 7);
  CHECK(pays == 7 && lastNotice.empty());
  fake::destroy(&profile);
  pm::reward::settled(Tally{2, 1, 0, 3});
  CHECK(pays == 7 && lastNotice == "Your deception score: 3 of 3" && lastBody == "2 killed, 1 misled");
  reward_screen(5, 7);  // and nothing is left waiting: a later call is the game's own
  CHECK(pays == 8 && paidXp == 5 && paidFunds == 7);
  give_entries();  // the destroyed profile is alive again

  // A payout no reward screen took is dropped when the next contract starts, waiting or not; its score showed.
  lastNotice.clear();
  pm::reward::contract_ended();
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(lastNotice == "Your deception score: 3 of 3" && lastBody == "3 killed, 0 misled");
  pm::reward::contract_started();
  reward_screen(5, 7);
  CHECK(pays == 9 && paidXp == 5 && paidFunds == 7);
  pm::reward::contract_ended();
  reward_screen(5, 7);
  pm::reward::contract_started();
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(pays == 9);

  // A contract with no LevelValues reads no reward and pays nothing.
  valuesStatic = nullptr;
  pm::reward::contract_ended();
  pm::reward::settled(Tally{3, 0, 0, 3});
  reward_screen(5, 7);
  CHECK(pays == 10 && paidXp == 0 && paidFunds == 0);

  // The reward screen's own challenge progress: the game's in a contract of an investigator, none of it for the
  // ghost player after their contract, from both parts of the results, and the game's again at the next contract.
  valuesStatic = &values;
  pm::reward::contract_started();
  progressed = 0;
  run_machine("RewardManager.Results.MoveNext");
  run_machine("RewardManager.EventRewards.MoveNext");
  CHECK(progressed == 2);
  pm::reward::contract_ended();
  run_machine("RewardManager.Results.MoveNext");
  run_machine("RewardManager.EventRewards.MoveNext");
  CHECK(progressed == 2);
  progress_elsewhere();  // progress from anywhere but the results stays the game's
  CHECK(progressed == 3);
  run_machine("RewardManager.Results.MoveNext");  // and the results' end lets it through again
  progress_elsewhere();
  CHECK(progressed == 4);
  pm::reward::contract_started();
  run_machine("RewardManager.Results.MoveNext");
  CHECK(progressed == 5);

  // An investigator's reward screen: the game's own total is what shows and what is saved.
  give_entries();
  pm::reward::contract_started();
  reset_screen();
  screen_calls("RewardManager.StartResults");
  CHECK(starts == 1 && saves == 1 && saved == 700 && shown == 700 && screenXp == 90);

  // The ghost player's, with the score already final: the total is the ghost's, shown and saved alike, once. Every
  // point of the $1000 main objective at x0.5 is $2000.
  pm::reward::contract_ended();
  pm::reward::settled(Tally{3, 0, 0, 3});
  reset_screen();
  screen_calls("RewardManager.StartResults");
  CHECK(starts == 1 && saves == 1 && saved == 2000 && shown == 2000);
  CHECK(screenMoney == 2000 && screenXp == 2000);
  // The screen's own experience call pays the ghost's experience, and the money stays what was saved.
  const int paysBefore = pays;
  reward_screen(90, 2000);
  CHECK(pays == paysBefore + 1 && paidXp == 2000 && saved == 2000 && saves == 1);

  // The screen comes before the final score: nothing starts and nothing is saved until it is in, and then once.
  pm::reward::contract_ended();
  reset_screen();
  screen_calls("RewardManager.StartResults");
  CHECK(starts == 0 && saves == 0);
  pm::reward::settled(Tally{1, 0, 0, 2});  // half the points: $1000
  CHECK(starts == 1 && saves == 1 && saved == 1000 && shown == 1000);
  pm::reward::settled(Tally{2, 0, 0, 2});  // and only once
  CHECK(starts == 1 && saves == 1 && saved == 1000);

  // A fast skip: Skip and Next do nothing while the score is not in, and are the game's own once it is.
  pm::reward::contract_ended();
  reset_screen();
  screen_calls("RewardManager.Skip");
  screen_calls("RewardManager.Next");
  CHECK(skips == 0 && nexts == 0 && saves == 0);
  screen_calls("RewardManager.StartResults");
  screen_calls("RewardManager.Skip");
  CHECK(skips == 0 && starts == 0);
  pm::reward::settled(Tally{2, 0, 0, 2});
  CHECK(starts == 1 && saved == 2000);
  screen_calls("RewardManager.Skip");
  screen_calls("RewardManager.Next");
  CHECK(skips == 1 && nexts == 1 && saves == 1);

  // A second contract as an investigator: the game's own total again, buttons and all.
  reward_screen(90, 700);  // the last screen's pay
  pm::reward::contract_started();
  reset_screen();
  screen_calls("RewardManager.Skip");
  screen_calls("RewardManager.StartResults");
  CHECK(skips == 1 && saved == 700 && shown == 700);
  // A second contract as the ghost player: its own amount, not the last one's. Half of $1000 at x0.5 is $1000.
  pm::reward::contract_ended();
  pm::reward::settled(Tally{1, 0, 0, 2});
  reset_screen();
  screen_calls("RewardManager.StartResults");
  CHECK(saved == 1000 && shown == 1000 && saves == 1);

  // The screen that waited is gone when the score comes: nothing runs.
  pm::reward::contract_ended();
  reset_screen();
  screen_calls("RewardManager.StartResults");
  fake::destroy(&screen);
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(starts == 0 && saves == 0);
  give_entries();

  // A next contract drops a screen still waiting.
  pm::reward::contract_ended();
  reset_screen();
  screen_calls("RewardManager.StartResults");
  pm::reward::contract_started();
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(starts == 0 && saves == 0);
  pm::reward::teardown();  // nothing left to run, even at the mod's stop
  CHECK(starts == 0 && saves == 0);

  // The seasonal event's score: the game's for an investigator; for the ghost player the empty answer of a contract
  // with no event, from their contract's end to the next contract, and the game's method does not run.
  const auto event_row = [] {
    const auto hooked = detours.find("RewardManager.EventRow");
    CHECK(hooked != detours.end());
    return reinterpret_cast<phasmo::RewardEventRow (*)(void*, const void*)>(hooked->second)(&screen, nullptr);
  };
  pm::reward::contract_started();
  eventRows = 0;
  CHECK(event_row().words[0] == 7 && eventRows == 1);
  pm::reward::contract_ended();
  const phasmo::RewardEventRow none = event_row();
  CHECK(none.words[0] == 0 && none.words[1] == 0 && none.words[2] == 0 && none.words[3] == 0 && eventRows == 1);
  pm::reward::settled(Tally{3, 0, 0, 3});
  CHECK(event_row().words[0] == 0 && eventRows == 1);  // the score being final changes nothing
  pm::reward::contract_started();
  CHECK(event_row().words[0] == 7 && eventRows == 2);

  // The mod stops while the results wait: they run as the game's own, and the screen is not stuck.
  pm::reward::contract_ended();
  reset_screen();
  screen_calls("RewardManager.StartResults");
  CHECK(starts == 0);
  pm::reward::teardown();
  CHECK(starts == 1 && saves == 1 && saved == 700);
  return g_failures ? 1 : 0;
}
