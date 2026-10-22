#pragma once

#include <algorithm>
#include <cmath>

// The ghost player's payout for a contract. Pure arithmetic, tested offline.
//
// The ghost is paid only when every investigator died. The amount is the contract's own main
// objective reward (what investigators earn for naming the ghost) scaled inversely to the
// difficulty's reward multiplier: a harder contract gives investigators more and the ghost less,
// because the ghost wins more easily there. A multiplier of 0 (a custom difficulty that pays
// nothing) pays the ghost nothing too, and the scale is capped so near-zero multipliers cannot be
// farmed. Experience equals the money paid.
namespace pg::payout {

constexpr float kReferenceMultiplier = 1.0f;  // the multiplier at which ghost and objective pay the same
constexpr float kMaxGhostScale = 3.0f;

struct Result {
  int money = 0;
  int xp = 0;
  float scale = 0;  // applied to the main objective reward
};

inline Result ghost(bool everyInvestigatorDied, int mainObjectiveReward, float difficultyMultiplier) {
  Result r;
  if (!everyInvestigatorDied || mainObjectiveReward <= 0 || !(difficultyMultiplier > 0)) return r;
  r.scale = std::min(kMaxGhostScale, kReferenceMultiplier / difficultyMultiplier);
  r.money = static_cast<int>(std::lround(mainObjectiveReward * r.scale));
  r.xp = r.money;
  return r;
}

}  // namespace pg::payout
