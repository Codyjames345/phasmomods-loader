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
// stopped, so every client sees it move. A dragged thing (the Draugr's) is instead held by a top corner and
// pulled with physics along the floor's route (drag_rule.h), at a walking pace, swinging and bumping as it
// comes, and set on its spot at the end. Main thread.
namespace pm::custom::carry {

// How often a ghost that carries things off by itself does so: a random gap in this range, in
// milliseconds (the Kikimora's hide cadence, which the Draugr shares).
constexpr ULONGLONG kMinGapMs = 25000, kMaxGapMs = 45000;
constexpr ULONGLONG kOwnershipWaitMs = 2000;  // how long a taken thing waits for the takeover to land
constexpr float kFloorReach = 2.0f;           // metres from a thing to the floor below it on the navigation mesh
constexpr ULONGLONG kDragSpareMs = 15000;     // a drag gives up this long after its walk should have ended

class Carry {
 public:
  // Whether the thing may still be moved (the Kikimora's: it still lies loose; the Draugr's: nobody
  // carries it and no hunt has begun).
  using Still = bool (*)(void* item);
  enum class Step { Idle, Waiting, Moving, Set, Dropped };

  bool busy() const { return item_.get() != nullptr; }
  /** The thing it carries now, or null. */
  void* item() const { return item_.get(); }
  /** Takes `item` (a PhotonObjectInteract) to set it down at `spot`, asking for it unless it is the host's already. False while it carries another. */
  bool take(void* item, phasmo::Vector3 spot, ULONGLONG now);
  /** As take, but once it is the host's the thing is dragged to `spot` along the floor at `speed` metres a second. */
  bool take_dragging(void* item, phasmo::Vector3 spot, ULONGLONG now, float speed);
  /**
   * Every frame on the host: once the thing is the host's, sets it down (Set) or drags it a step (Moving, then
   * Set on its spot). Leaves it where it is, under physics, when it is gone, `still` says it may not be moved,
   * the takeover has not landed in time or the drag has gone on too long (Dropped).
   */
  Step step(ULONGLONG now, Still still);
  void reset();

 private:
  void start_drag(void* item, ULONGLONG now);

  api::Held item_;
  phasmo::Vector3 spot_{};
  ULONGLONG deadline_ = 0;
  float speed_ = 0;  // metres a second when dragged; 0: set down at once
  bool owned_ = false;
  drag::Route route_;           // the held corner's route, at its height above the floor
  phasmo::Vector3 grabLocal_{}; // the held corner, in the thing's own space
  float travelled_ = 0;
  ULONGLONG last_ = 0;
  bool wasKinematic_ = false;
  api::Held path_;  // the NavMeshPath routes are worked out into, made once
};

}  // namespace pm::custom::carry
