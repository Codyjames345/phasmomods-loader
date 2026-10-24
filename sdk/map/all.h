#pragma once

// Every map entry, in one list for the resolver. A new entry goes in its type's header and here;
// tests/map_test.cpp fails on one that is missing.

#include "Animator.h"
#include "Camera.h"
#include "GameAssembly.h"
#include "GhostAI.h"
#include "LayerMask.h"
#include "LevelController.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Transform.h"
#include "UnityPlayer.h"

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
    &LevelController,
    &LevelController_instance,
    &Animator,
    &Animator_get_parameters,
    &Animator_StringToHash,
    &AnimatorControllerParameter,
    &AnimatorControllerParameter_m_Name,
    &Animator_SetBoolID,
    &Transform,
    &Transform_Find,
    &LayerMask,
    &LayerMask_NameToLayer,
    &SceneManager,
    &SceneManager_GetActiveScene,
    &Scene_JournalBookContent,
    &Layer_PlayerView,
    &GameAssembly_il2cpp_shutdown,
    &UnityPlayer_SteerSite,
    &UnityPlayer_GetInternalAgent,
};

}  // namespace pg::map
