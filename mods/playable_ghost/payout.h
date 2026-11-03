#pragma once

#include <algorithm>
#include <cmath>

// The ghost player's payout for a contract. Pure arithmetic, tested offline.
//
// The ghost is paid full pay x points / investigators, where points are the deception score (score.h):
// equal shares, one point per investigator at most. Once no investigator is left, by any mix of deaths and
// quits, that is full pay; investigators who all survive and name the ghost pay nothing.
//
// Full pay is the contract's own main objective reward (what investigators earn for naming the ghost)
// scaled inversely to the difficulty's reward multiplier: a harder contract gives investigators more and
// the ghost less, because the ghost wins more easily there. A multiplier of 0 (a custom difficulty that
// pays nothing) pays the ghost nothing too, and the scale is capped so near-zero multipliers cannot be
// farmed. Experience equals the money paid.
namespace pm::payout {

constexpr float kReferenceMultiplier = 1.0f;  // the multiplier at which ghost and objective pay the same
constexpr float kMaxGhostScale = 3.0f;

struct Result {
  int money = 0;
  int xp = 0;
  float scale = 0;  // applied to the main objective reward for full pay
};

inline Result ghost(int points, int investigators, int mainObjectiveReward, float difficultyMultiplier) {
  Result r;
  if (points <= 0 || investigators <= 0 || mainObjectiveReward <= 0 || !(difficultyMultiplier > 0)) return r;
  r.scale = std::min(kMaxGhostScale, kReferenceMultiplier / difficultyMultiplier);
  const double full = static_cast<double>(std::lround(mainObjectiveReward * r.scale));
  r.money = static_cast<int>(std::lround(full * std::min(points, investigators) / investigators));
  r.xp = r.money;
  return r;
}

}  // namespace pm::payout
