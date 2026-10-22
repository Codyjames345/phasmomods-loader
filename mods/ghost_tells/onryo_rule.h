#pragma once

#include <algorithm>
#include <map>

// The Onryo's tells, kept free of game calls so they can be tested offline. It "fears any form of fire"
// and "extinguishing a flame can cause an Onryo to attack": a flame lit near it is often snuffed at once
// (the game's own ghost blow-out, which counts toward its third-flame hunt); every flame in the house
// gutters as its count of blown-out flames rises and burns low while the count stands at two; and a
// flame it blew out will not catch again for a while. Light and size only, never colour, so nothing here
// looks like Spectral Flame.
namespace pg::tells {

constexpr float kOnryoReach = 4.0f;       // metres: its vanilla flame range (and on its floor)
constexpr int kOnryoSnuffOneIn = 2;
constexpr float kOnryoSnuffMin = 1.0f, kOnryoSnuffMax = 2.0f;  // seconds from the lighting to the snuff
constexpr float kOnryoSpite = 15.0f;      // seconds a flame it blew out will not catch
constexpr float kGutterLow = 0.3f;        // a flame's light at the bottom of its gutter
constexpr float kGutterSeconds = 0.6f;    // the gutter's recovery
constexpr float kBurnLowLight = 0.6f;     // every flame's light while its count stands at two
constexpr float kBurnLowScale = 0.75f;    // and its flame's size

/** Seconds until a freshly lit flame near it is snuffed, or -1 when it stays lit; both rolls in [0, 1). */
inline float snuff_delay(float chanceRoll, float delayRoll) {
  if (chanceRoll * kOnryoSnuffOneIn >= 1.0f) return -1.0f;
  return kOnryoSnuffMin + (kOnryoSnuffMax - kOnryoSnuffMin) * delayRoll;
}

/** How every lit flame burns, from the Onryo's count of blown-out flames. */
struct Flicker {
  int counted = 0;
  float since = 1e9f;  // seconds since the count last rose
  void count(int n) {
    if (n > counted) since = 0;
    counted = n;
  }
  void step(float dt) { since += dt; }
  float light() const {
    const float steady = counted >= 2 ? kBurnLowLight : 1.0f;
    const float gutter = kGutterLow + (1.0f - kGutterLow) * std::min(since / kGutterSeconds, 1.0f);
    return std::min(steady, gutter);
  }
  float scale() const { return counted >= 2 ? kBurnLowScale : 1.0f; }
  void reset() {
    counted = 0;
    since = 1e9f;
  }
};

/** The flames it blew out that will not catch yet, by PhotonView id. */
struct Spite {
  std::map<int, float> left;
  void mark(int view, float seconds) { left[view] = seconds; }
  bool spited(int view) const {
    const auto it = left.find(view);
    return it != left.end() && it->second > 0;
  }
  void step(float dt) {
    for (auto it = left.begin(); it != left.end();) {
      it->second -= dt;
      it = it->second > 0 ? std::next(it) : left.erase(it);
    }
  }
  void clear() { left.clear(); }
};

}  // namespace pg::tells
