#pragma once

// Unity's Playables, for playing an AnimationClip over an Animator the game already drives (sdk/api/player_anim.h).
// Not obfuscated. Their managed wrappers take structs, so the map holds the engine functions behind them by their
// icall names, which resolve only in the running game. A graph, a playable handle and an output handle are each a
// pointer and a version (phasmo::PlayableGraph and friends below), passed by address. How the investigator's body is
// animated, and why a second graph is how a clip plays over it, is in sdk/systems/players.md, "The body".

#include <cstdint>

#include "entry.h"
#include "game_api.h"
#include "UnityObject.h"

namespace pm::map {

inline constexpr const char* Playables_two[] = {"a", "b"};
inline constexpr const char* Playables_three[] = {"a", "b", "c"};
inline constexpr const char* Playables_one[] = {"a"};

/// Unity's animation clip (UnityEngine.AnimationModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimationClip{.key = "AnimationClip", .name = "AnimationClip",
                                                  .assembly = "UnityEngine.AnimationModule", .ns = "UnityEngine",
                                                  .base = &UnityObject};

/// A clip's length in seconds: the engine function behind AnimationClip.length. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimationClip_get_length{.key = "AnimationClip.get_length",
                                                             .kind = pm::sdk::Kind::Icall,
                                                             .name = "UnityEngine.AnimationClip::get_length"};

/// Whether a clip loops (its import setting Loop Time): the engine function behind Motion.isLooping, which an
/// AnimationClip inherits. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Motion_get_isLooping{.key = "Motion.get_isLooping", .kind = pm::sdk::Kind::Icall,
                                                         .name = "UnityEngine.Motion::get_isLooping"};

/// A new graph by its name, written to the PlayableGraph given. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableGraph_Create_Injected{
    .key = "PlayableGraph.Create_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableGraph::Create_Injected", .params = Playables_two};

/// Destroys a graph, its playables and its outputs. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableGraph_Destroy_Injected{
    .key = "PlayableGraph.Destroy_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableGraph::Destroy_Injected", .params = Playables_one};

/// Starts a graph: from then on it updates each frame by its time update mode. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableGraph_Play_Injected{
    .key = "PlayableGraph.Play_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableGraph::Play_Injected", .params = Playables_one};

/// Whether a graph still exists. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableGraph_IsValid_Injected{
    .key = "PlayableGraph.IsValid_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableGraph::IsValid_Injected", .params = Playables_one};

/// Sets how a graph's time advances, a DirectorUpdateMode number (1, GameTime: with Time.deltaTime, scaled). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableGraph_SetTimeUpdateMode_Injected{
    .key = "PlayableGraph.SetTimeUpdateMode_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableGraph::SetTimeUpdateMode_Injected", .params = Playables_two};

/// A new animation output in a graph by its name, written to the PlayableOutputHandle given; whether it was made.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimationPlayableGraphExtensions_InternalCreateAnimationOutput{
    .key = "AnimationPlayableGraphExtensions.InternalCreateAnimationOutput", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Animations.AnimationPlayableGraphExtensions::InternalCreateAnimationOutput",
    .params = Playables_three};

/// Points an animation output at an Animator. Several outputs on one Animator are applied in their sorting order, each
/// blended over what came before it by its weight; the Animator's own controller comes first. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc AnimationPlayableOutput_InternalSetTarget{
    .key = "AnimationPlayableOutput.InternalSetTarget", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Animations.AnimationPlayableOutput::InternalSetTarget", .params = Playables_two};

/// Sets an animation output's sorting order among the outputs on its Animator: a higher one is applied later.
/// Animation Rigging's RigBuilder uses 1000 for its own. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimationPlayableOutputExtensions_InternalSetSortingOrder_Injected{
    .key = "AnimationPlayableOutputExtensions.InternalSetSortingOrder_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Experimental.Animations.AnimationPlayableOutputExtensions::InternalSetSortingOrder_Injected",
    .params = Playables_two};

/// A new clip playable in a graph, written to the PlayableHandle given; whether it was made. It loops as the clip does.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AnimationClipPlayable_CreateHandleInternal_Injected{
    .key = "AnimationClipPlayable.CreateHandleInternal_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Animations.AnimationClipPlayable::CreateHandleInternal_Injected", .params = Playables_three};

/// Whether a humanoid clip playable applies foot IK, as an Animator state's Foot IK does. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc AnimationClipPlayable_SetApplyFootIKInternal{
    .key = "AnimationClipPlayable.SetApplyFootIKInternal", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Animations.AnimationClipPlayable::SetApplyFootIKInternal", .params = Playables_two};

/// Makes a playable the source of an output, from the playable's output port given. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableOutputHandle_SetSourcePlayable_Injected{
    .key = "PlayableOutputHandle.SetSourcePlayable_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableOutputHandle::SetSourcePlayable_Injected", .params = Playables_three};

/// Sets an output's weight, 0 to 1: how much of it is blended over the outputs before it. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc PlayableOutputHandle_SetWeight_Injected{
    .key = "PlayableOutputHandle.SetWeight_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableOutputHandle::SetWeight_Injected", .params = Playables_two};

/// A playable's local time in seconds, a double. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableHandle_GetTime_Injected{
    .key = "PlayableHandle.GetTime_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableHandle::GetTime_Injected", .params = Playables_one};

/// Sets a playable's local time in seconds. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc PlayableHandle_SetTime_Injected{
    .key = "PlayableHandle.SetTime_Injected", .kind = pm::sdk::Kind::Icall, .flags = pm::sdk::kStatic,
    .name = "UnityEngine.Playables.PlayableHandle::SetTime_Injected", .params = Playables_two};

}  // namespace pm::map

/// @file
/// @brief The game API for Unity's Playables (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve logs once and returns its type's default.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// A playable graph: the engine's pointer and a version, as Unity lays out the struct.
struct PlayableGraph {
  void* handle = nullptr;  ///< the engine's pointer
  uint32_t version = 0;    ///< the engine's version, which a destroyed one no longer matches

  /// Create_Injected(name, &graph). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableGraph_Create_Injected, void(pm::api::Object, PlayableGraph*)> Create{};
  /// Destroy_Injected(&graph). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableGraph_Destroy_Injected, void(PlayableGraph*)> Destroy{};
  /// Play_Injected(&graph). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableGraph_Play_Injected, void(PlayableGraph*)> Play{};
  /// IsValid_Injected(&graph). Unresolved: false.
  static constexpr pm::api::Icall<pm::map::PlayableGraph_IsValid_Injected, bool(PlayableGraph*)> IsValid{};
  /// SetTimeUpdateMode_Injected(&graph, mode). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableGraph_SetTimeUpdateMode_Injected, void(PlayableGraph*, int32_t)>
      SetTimeUpdateMode{};
};

/// A playable in a graph: the engine's pointer and a version.
struct PlayableHandle {
  void* handle = nullptr;  ///< the engine's pointer
  uint32_t version = 0;    ///< the engine's version, which a destroyed one no longer matches

  /// GetTime_Injected(&playable): its local time in seconds. Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::PlayableHandle_GetTime_Injected, double(PlayableHandle*)> GetTime{};
  /// SetTime_Injected(&playable, seconds). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableHandle_SetTime_Injected, void(PlayableHandle*, double)> SetTime{};
};

/// An output of a graph: the engine's pointer and a version.
struct PlayableOutputHandle {
  void* handle = nullptr;  ///< the engine's pointer
  uint32_t version = 0;    ///< the engine's version, which a destroyed one no longer matches

  /// SetSourcePlayable_Injected(&output, &playable, port). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableOutputHandle_SetSourcePlayable_Injected,
                                  void(PlayableOutputHandle*, PlayableHandle*, int32_t)>
      SetSourcePlayable{};
  /// SetWeight_Injected(&output, weight). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::PlayableOutputHandle_SetWeight_Injected, void(PlayableOutputHandle*, float)>
      SetWeight{};
};

/// Animation outputs and clip playables.
struct AnimationPlayables {
  /// InternalCreateAnimationOutput(&graph, name, &output): whether it was made. Unresolved: false.
  static constexpr pm::api::Icall<pm::map::AnimationPlayableGraphExtensions_InternalCreateAnimationOutput,
                                  bool(PlayableGraph*, pm::api::Object, PlayableOutputHandle*)>
      CreateOutput{};
  /// InternalSetTarget(&output, animator). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::AnimationPlayableOutput_InternalSetTarget,
                                  void(PlayableOutputHandle*, pm::api::Object)>
      SetTarget{};
  /// InternalSetSortingOrder_Injected(&output, order). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::AnimationPlayableOutputExtensions_InternalSetSortingOrder_Injected,
                                  void(PlayableOutputHandle*, int32_t)>
      SetSortingOrder{};
  /// CreateHandleInternal_Injected(&graph, clip, &playable): whether it was made. Unresolved: false.
  static constexpr pm::api::Icall<pm::map::AnimationClipPlayable_CreateHandleInternal_Injected,
                                  bool(PlayableGraph*, pm::api::Object, PlayableHandle*)>
      CreateClipPlayable{};
  /// SetApplyFootIKInternal(&playable, on). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::AnimationClipPlayable_SetApplyFootIKInternal, void(PlayableHandle*, bool)>
      SetApplyFootIK{};
};

/// An animation clip.
struct AnimationClip {
  /// The type: type_object() for AssetBundle.LoadAsset_Internal. Unresolved: null.
  static constexpr pm::api::Type<pm::map::AnimationClip> type{};
  /// get_length(clip): seconds. Unresolved: 0.
  static constexpr pm::api::Icall<pm::map::AnimationClip_get_length, float(pm::api::Object)> get_length{};
  /// get_isLooping(clip): its Loop Time. Unresolved: false.
  static constexpr pm::api::Icall<pm::map::Motion_get_isLooping, bool(pm::api::Object)> get_isLooping{};
};

/// @}
}  // namespace phasmo
