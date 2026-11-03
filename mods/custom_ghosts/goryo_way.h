#pragma once

#include <cstdint>
#include <type_traits>

#include "GhostAI.h"
#include "GhostInfo.h"
#include "GhostTypes.h"

// The Draugr keeps its room the Goryo way (behaviour.cpp hooks GhostWanderState.PickDestination). The
// wander's destination picker skips its long-roam roll for a ghost whose behaviour type is the Goryo's,
// so that ghost's wander radius stays 3 and its wander never changes its favourite room. For the call of
// the picker alone the Draugr's behaviour type reads as the Goryo's; nothing the picker calls reads
// GhostInfo, and with the radius at 3 the picker's other Goryo test is never reached, so only the roll
// differs (sdk/systems/ghost-ai.md). Main thread, on the master.
namespace pm::custom::goryo_way {

/**
 * Runs `pick` (the game's own picker) with `ghost`'s behaviour type (GhostInfo.behaviourType) reading
 * as the Goryo's, and puts the ghost's own back after, also when `pick` throws. Runs `pick` untouched
 * when the ghost has no GhostInfo or the Goryo's number or the field did not resolve.
 */
template <class Pick>
auto pick_as_goryo(void* ghost, Pick pick) -> decltype(pick()) {
  using Goryo = std::remove_cvref_t<decltype(phasmo::GhostType::Goryo)>;
  using BehaviourType = std::remove_cvref_t<decltype(phasmo::GhostInfo::behaviourType)>;
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  if (!info || !Goryo::use::resolved() || !BehaviourType::use::resolved()) return pick();
  struct PutBack {
    void* info;
    int32_t own;
    ~PutBack() { phasmo::GhostInfo::behaviourType.set(info, own); }
  } putBack{info, phasmo::GhostInfo::behaviourType.get(info)};
  phasmo::GhostInfo::behaviourType.set(info, phasmo::GhostType::Goryo());
  return pick();
}

}  // namespace pm::custom::goryo_way
