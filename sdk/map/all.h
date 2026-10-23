#pragma once

// Every map entry, in one list for the resolver. A new entry goes in its type's header and here;
// tests/map_test.cpp fails on one that is missing.

#include "Camera.h"
#include "GhostAI.h"

namespace pg::map {

inline constexpr const pg::sdk::EntryDesc* kAllEntries[] = {
    &GhostAI,
    &GhostState,
    &GhostState_idle,
    &GhostState_wander,
    &GhostState_hunting,
    &GhostAI_state,
    &GhostAI_agent,
    &GhostAI_Awake,
    &GhostAI_Hunting,
    &GhostAI_ChangeState,
    &GhostAI_UnAppear,
    &GhostAI_LookAtPlayer,
    &GhostAI_TemporarilyStopWander,
    &GhostAI_isHunting,
    &GhostAI_postHuntCooldown,
    &Camera,
    &Camera_set_fieldOfView,
    &Camera_get_cullingMask,
    &Camera_set_cullingMask,
    &Camera_get_nearClipPlane,
    &Camera_set_nearClipPlane,
    &Camera_get_farClipPlane,
    &Camera_set_farClipPlane,
};

}  // namespace pg::map
