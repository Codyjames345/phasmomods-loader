#pragma once

#include <windows.h>

#include "Transform.h"
#include "drag_rule.h"
#include "game.h"
#include "held.h"

// A thing a ghost carries off by itself and sets down somewhere else, on the host: the Kikimora hiding
// equipment and the Draugr taking its things back. The host takes the thing over through its PhotonView,
// as a player picking it up does: PhotonView.RequestOwnership only asks every client, and the thing is the
// host's once that request comes back; moved before then, the player who still owns it would sync it
// straight back. Then it sets the thing down: its transform, then its Rigidbody's position, its motion
// stopped, so every client sees it move. A dragged thing (the Draugr's) instead travels there along the
// floor (drag_rule.h), kinematic on the way, at a walking pace. Main thread.
namespace pm::custom::carry {

// How often a ghost that carries things off by itself does so: a random gap in this range, in
// milliseconds (the Kikimora's hide cadence, which the Draugr shares).
constexpr ULONGLONG kMinGapMs = 25000, kMaxGapMs = 45000;
constexpr ULONGLONG kOwnershipWaitMs = 2000;  // how long a taken thing waits for the takeover to land
constexpr float kFloorReach = 2.0f;           // metres from a thing to the floor below it on the navigation mesh

class Carry {
 public:
  // Whether the thing may still be moved (the Kikimora's: it still lies loose; the Draugr's: nobody
  // carries it and no hunt has begun).
  using Still = bool (*)(void* item);
  enum class Step { Idle, Waiting, Moving, Set, Dropped };

  bool busy() const { return item_.get() != nullptr; }
  /** Takes `item` (a PhotonObjectInteract) to set it down at `spot`, asking for it unless it is the host's already. False while it carries another. */
  bool take(void* item, phasmo::Vector3 spot, ULONGLONG now);
  /** As take, but once it is the host's the thing is dragged to `spot` along the floor at `speed` metres a second. */
  bool take_dragging(void* item, phasmo::Vector3 spot, ULONGLONG now, float speed);
  /**
   * Every frame on the host: once the thing is the host's, sets it down (Set) or drags it a step (Moving, then
   * Set on its spot). Leaves it where it is, falling again, when it is gone, `still` says it may not be moved
   * or the takeover has not landed in time (Dropped).
   */
  Step step(ULONGLONG now, Still still);
  void reset();

 private:
  void start_drag(void* item);
  void release(void* item);

  api::Held item_;
  phasmo::Vector3 spot_{};
  ULONGLONG deadline_ = 0;
  float speed_ = 0;  // metres a second when dragged; 0: set down at once
  bool owned_ = false;
  drag::Route route_;
  float travelled_ = 0;
  ULONGLONG last_ = 0;
  bool wasKinematic_ = false;
  api::Held path_;  // the NavMeshPath routes are worked out into, made once
};

}  // namespace pm::custom::carry
