#pragma once

#include "tells.h"

// The Goryo's tells, kept free of game calls so they can be tested offline. It "will usually only show
// itself on camera if there are no people nearby" and is "rarely seen far from its place of death": in
// its room, with nobody near and a video camera on it, it now and then stands visible to the camera
// alone, and it acts more there; in a hunt, having lost everyone far from its room, it heads home
// instead of searching. It never stops chasing anyone it detects, wherever they are.
namespace pg::tells {

constexpr float kGoryoAlone = 10.0f;        // metres: no living player this near
constexpr float kGoryoShowMin = 30.0f;      // seconds of those conditions between its camera showings
constexpr float kGoryoShowMax = 60.0f;
constexpr float kGoryoShowSeconds = 4.0f;
constexpr float kGoryoExtraEvery = 20.0f;   // seconds between its extra interactions on camera
constexpr float kGoryoFar = 12.0f;          // metres from its room: past this, losing its prey sends it home
constexpr float kGoryoHome = 3.0f;          // metres from its room: home again

/** Its camera-only figure: +1 on the frame it starts, -1 on the frame it stops. */
struct CameraShow {
  Timer show;
  float wait = -1;  // seconds of conditions before the next showing; -1 to be rolled
  int step(bool conditions, float dt, float roll01) {
    if (show.running()) {
      if (conditions && show.step(dt)) return 0;
      show.stop();
      wait = -1;
      return -1;
    }
    if (!conditions) return 0;
    if (wait < 0) wait = kGoryoShowMin + (kGoryoShowMax - kGoryoShowMin) * roll01;
    wait -= dt;
    if (wait > 0) return 0;
    wait = -1;
    show.start(kGoryoShowSeconds);
    return 1;
  }
  bool showing() const { return show.running(); }
  void reset() {
    show.stop();
    wait = -1;
  }
};

/** Its extra interaction attempts while a camera watches it alone: true every kGoryoExtraEvery seconds. */
struct Extra {
  float since = 0;
  bool step(bool conditions, float dt) {
    if (!conditions) {
      since = 0;
      return false;
    }
    since += dt;
    if (since < kGoryoExtraEvery) return false;
    since -= kGoryoExtraEvery;
    return true;
  }
};

/**
 * Whether, in a hunt, it heads for its room now: only once it neither has prey (the hunt state's target)
 * nor is searching for any (its search player, set where the prey was lost or heard).
 */
struct HomeSearch {
  bool going = false;
  bool step(bool hunting, bool hasPrey, bool searching, bool steered, float distanceToRoom) {
    if (!hunting || hasPrey || searching || steered) {
      going = false;
    } else if (going) {
      going = distanceToRoom > kGoryoHome;
    } else {
      going = distanceToRoom > kGoryoFar;
    }
    return going;
  }
};

}  // namespace pg::tells
