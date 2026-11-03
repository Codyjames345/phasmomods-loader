#pragma once

#include <windows.h>

#include "Transform.h"
#include "game.h"
#include "held.h"

// A thing a ghost carries off by itself and sets down somewhere else, on the host: the Kikimora hiding
// equipment and the Draugr taking its things back. The host takes the thing over through its PhotonView,
// as a player picking it up does: PhotonView.RequestOwnership only asks every client, and the thing is the
// host's once that request comes back; moved before then, the player who still owns it would sync it
// straight back. Then it sets the thing down: its transform, then its Rigidbody's position, its motion
// stopped, so every client sees it move. Main thread.
namespace pm::custom::carry {

// How often a ghost that carries things off by itself does so: a random gap in this range, in
// milliseconds (the Kikimora's hide cadence, which the Draugr shares).
constexpr ULONGLONG kMinGapMs = 25000, kMaxGapMs = 45000;
constexpr ULONGLONG kOwnershipWaitMs = 2000;  // how long a taken thing waits for the takeover to land

class Carry {
 public:
  // Whether the thing may still be moved (the Kikimora's: it still lies loose; the Draugr's: nobody
  // carries it).
  using Still = bool (*)(void* item);
  enum class Step { Idle, Waiting, Set, Dropped };

  bool busy() const { return item_.get() != nullptr; }
  /** Takes `item` (a PhotonObjectInteract) to set it down at `spot`, asking for it unless it is the host's already. False while it carries another. */
  bool take(void* item, phasmo::Vector3 spot, ULONGLONG now);
  /**
   * Every frame on the host: sets the thing down once it is the host's (Set); leaves it where it is when
   * it is gone, `still` says it may not be moved or the takeover has not landed in time (Dropped).
   */
  Step step(ULONGLONG now, Still still);
  void reset();

 private:
  api::Held item_;
  phasmo::Vector3 spot_{};
  ULONGLONG deadline_ = 0;
};

}  // namespace pm::custom::carry
