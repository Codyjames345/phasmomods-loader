#pragma once

#include "tells.h"

// The Shade's tells, kept free of game calls so they can be tested offline. "Very shy", it "will stop
// all paranormal activity if there are people nearby": when it shows itself and a player stares at it,
// it is gone at once; with two or more players near it does nothing at all (vanilla already holds it
// back with anyone in its room); and in a hunt it hesitates when it sees players together.
namespace pg::tells {

constexpr float kShadeGazeCone = 15.0f;     // degrees either side of where a player faces
constexpr float kShadeGazeRange = 20.0f;    // metres
constexpr float kShadeGazeSeconds = 0.4f;   // of staring before it vanishes
constexpr float kShadeCompanyRange = 5.0f;  // metres: two living players this near, through walls, hold it back
constexpr int kShadeCompany = 2;
constexpr float kShadeGroupRange = 2.5f;    // metres from its prey a second player counts as with them
constexpr float kShadeFalterSeconds = 1.5f;
constexpr float kShadeFalterCooldown = 15.0f;

/** A player staring at it while it shows itself outside a hunt. */
struct Gaze {
  float looked = 0;
  bool gone = false;
  /** One frame; true on the frame it vanishes. */
  bool step(bool appeared, bool lookedAt, float dt) {
    if (!appeared) {
      looked = 0;
      gone = false;
      return false;
    }
    if (gone) return false;
    if (!lookedAt) {
      looked = 0;
      return false;
    }
    looked += dt;
    if (looked < kShadeGazeSeconds) return false;
    gone = true;
    return true;
  }
};

inline bool crowded(int livingWithinRange) { return livingWithinRange >= kShadeCompany; }

/** Its hesitation in a hunt at the sight of its prey with another player beside them. */
struct Falter {
  Timer hold;
  float cooldown = 0;
  /** One frame; true on the frame it falters. */
  bool step(bool hunting, bool groupInSight, float dt) {
    cooldown -= dt;
    if (!hunting) {
      hold.stop();
      return false;
    }
    if (hold.running()) {
      hold.step(dt);
      return false;
    }
    if (!groupInSight || cooldown > 0) return false;
    hold.start(kShadeFalterSeconds);
    cooldown = kShadeFalterCooldown;
    return true;
  }
  bool holding() const { return hold.running(); }
};

}  // namespace pg::tells
