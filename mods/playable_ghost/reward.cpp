#include "reward.h"

#include <cstdio>
#include <string>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "notice.h"
#include "payout.h"

namespace pg::reward {
namespace {

// GameAssembly build 25811160 (verified at load).
constexpr uintptr_t kAddXpAndFundsRva = 0x141E640;  // SingleplayerProfile.AddXPAndFunds(int xp, int funds)
constexpr uintptr_t kRewardMultiplierRva = 0x1FC30F0;  // Difficulty.GetRewardMultiplier()
constexpr size_t kLevelValuesDifficulty = 0x38;
constexpr size_t kLevelValuesMainObjective = 0x50;  // the main objective entry
constexpr size_t kObjectiveValues = 0x10;           // entry: ObjectiveValues
constexpr size_t kObjectiveMoney = 0x30;            // ObjectiveValues.moneyReward
constexpr unsigned kPopupMs = 8000;

struct Api {
  il2cpp::Class* levelValues = nullptr;
  float (*rewardMultiplier)(void*, const void*) = nullptr;
} api;

using AddXpAndFunds_t = void (*)(void*, int, int, const void*);
AddXpAndFunds_t g_addXpAndFunds = nullptr;

struct Pending {
  bool valid = false;
  bool won = false;
  payout::Result result;
  float multiplier = 0;
} g_pending;

void detour_add_xp_and_funds(void* profile, int xp, int funds, const void* method) {
  hooks::Guard guard;
  if (!g_pending.valid) {
    g_addXpAndFunds(profile, xp, funds, method);
    return;
  }
  const Pending p = g_pending;
  g_pending = Pending{};
  g_addXpAndFunds(profile, p.result.xp, p.result.money, method);
  PG_LOG("reward: ghost payout $%d, %d XP (won %d, difficulty x%.2f)", p.result.money, p.result.xp, p.won,
         p.multiplier);
  char body[160];
  if (p.won) {
    std::snprintf(body, sizeof body, "$%d and %d XP (x%.2f for a x%.2f difficulty)", p.result.money, p.result.xp,
                  p.result.scale, p.multiplier);
    notice::show("The ghost claimed every investigator", body, kPopupMs);
  } else {
    notice::show("The investigators survived the ghost", "No payout this contract", kPopupMs);
  }
}

}  // namespace

bool resolve() {
  api.levelValues = il2cpp::cls("Assembly-CSharp.dll", "", "LevelValues");
  api.rewardMultiplier = reinterpret_cast<decltype(api.rewardMultiplier)>(game::game_assembly_address(kRewardMultiplierRva));
  const bool ok = api.levelValues && api.rewardMultiplier;
  PG_LOG("reward: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kAddXpAndFundsRva), reinterpret_cast<void*>(&detour_add_xp_and_funds),
                       reinterpret_cast<void**>(&g_addXpAndFunds), "SingleplayerProfile.AddXPAndFunds");
}

void contract_started() {
  if (g_pending.valid) PG_LOG("reward: unclaimed ghost payout dropped");
  g_pending = Pending{};
}

void contract_ended(bool everyInvestigatorDied) {
  void* values = il2cpp::static_field_by_type(api.levelValues, "LevelValues");
  void* difficulty = values ? il2cpp::field<void*>(values, kLevelValuesDifficulty) : nullptr;
  void* entry = values ? il2cpp::field<void*>(values, kLevelValuesMainObjective) : nullptr;
  void* objective = entry ? il2cpp::field<void*>(entry, kObjectiveValues) : nullptr;
  const int mainReward = objective ? il2cpp::field<int>(objective, kObjectiveMoney) : 0;
  const float multiplier = difficulty ? api.rewardMultiplier(difficulty, nullptr) : 0.0f;
  g_pending.valid = true;
  g_pending.won = everyInvestigatorDied;
  g_pending.multiplier = multiplier;
  g_pending.result = payout::ghost(everyInvestigatorDied, mainReward, multiplier);
  PG_LOG("reward: contract over (won %d, main objective $%d, difficulty x%.2f)", everyInvestigatorDied, mainReward,
         multiplier);
}

}  // namespace pg::reward
