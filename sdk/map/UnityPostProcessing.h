#pragma once

// The Universal Render Pipeline's camera data and volumes: a mod's own camera that renders as the player's does, and a
// screen blur and vignette of a mod's own (sdk/api/third_person.h, sdk/api/screen_effects.h). The game renders with
// URP (Unity.RenderPipelines.Universal.Runtime). Not obfuscated, so every entry is by real name.

#include <cstdint>

#include "entry.h"
#include "game_api.h"
#include "UnityObject.h"

namespace pm::map {

inline constexpr const char* PostProcessing_bool[] = {"bool"};
inline constexpr const char* PostProcessing_LayerMask[] = {"LayerMask"};
inline constexpr const char* PostProcessing_Type[] = {"Type"};
inline constexpr const char* PostProcessing_Type_bool[] = {"Type", "bool"};

/// URP's settings of one camera (Unity.RenderPipelines.Universal.Runtime), a component beside the Camera. Inferred:
/// URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData{
    .key = "UniversalAdditionalCameraData", .name = "UniversalAdditionalCameraData",
    .assembly = "Unity.RenderPipelines.Universal.Runtime", .ns = "UnityEngine.Rendering.Universal",
                                                                  .base = &MonoBehaviour};

/// Whether the camera renders post-processing (the volumes' effects). Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData_get_renderPostProcessing{
    .key = "UniversalAdditionalCameraData.get_renderPostProcessing", .kind = pm::sdk::Kind::Method,
    .owner = "UniversalAdditionalCameraData", .name = "get_renderPostProcessing"};

/// Sets whether the camera renders post-processing. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData_set_renderPostProcessing{
    .key = "UniversalAdditionalCameraData.set_renderPostProcessing", .kind = pm::sdk::Kind::Method,
    .owner = "UniversalAdditionalCameraData", .name = "set_renderPostProcessing", .params = PostProcessing_bool};

/// The layers whose volumes affect the camera, a LayerMask (an int). Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData_get_volumeLayerMask{
    .key = "UniversalAdditionalCameraData.get_volumeLayerMask", .kind = pm::sdk::Kind::Method,
    .owner = "UniversalAdditionalCameraData", .name = "get_volumeLayerMask"};

/// Sets the layers whose volumes affect the camera. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData_set_volumeLayerMask{
    .key = "UniversalAdditionalCameraData.set_volumeLayerMask", .kind = pm::sdk::Kind::Method,
    .owner = "UniversalAdditionalCameraData", .name = "set_volumeLayerMask", .params = PostProcessing_LayerMask};

/// Unity's ScriptableObject (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc ScriptableObject{.key = "ScriptableObject", .name = "ScriptableObject",
                                                     .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine",
                                                     .base = &UnityObject};

/// A new instance of the ScriptableObject System.Type given. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc ScriptableObject_CreateInstance{
    .key = "ScriptableObject.CreateInstance", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "ScriptableObject", .name = "CreateInstance", .params = PostProcessing_Type};

/// A volume (Unity.RenderPipelines.Core.Runtime): a component whose profile's effects apply to the cameras whose volume
/// layer mask holds its GameObject's layer. Inferred: the render pipeline's documented API.
inline constexpr pm::sdk::EntryDesc Volume{.key = "Volume", .name = "Volume",
                                           .assembly = "Unity.RenderPipelines.Core.Runtime",
                                           .ns = "UnityEngine.Rendering", .base = &MonoBehaviour};

/// Whether the volume applies everywhere rather than inside its colliders. Inferred: the pipeline's source.
inline constexpr pm::sdk::EntryDesc Volume_m_IsGlobal{.key = "Volume.m_IsGlobal", .kind = pm::sdk::Kind::Field,
                                                      .owner = "Volume", .name = "m_IsGlobal"};

/// The volume's priority among volumes: a higher one is applied over a lower. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc Volume_priority{.key = "Volume.priority", .kind = pm::sdk::Kind::Field,
                                                    .owner = "Volume", .name = "priority"};

/// How much of the volume applies, 0 to 1. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc Volume_weight{.key = "Volume.weight", .kind = pm::sdk::Kind::Field,
                                                  .owner = "Volume", .name = "weight"};

/// The volume's profile, a VolumeProfile shared as an asset. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc Volume_sharedProfile{.key = "Volume.sharedProfile", .kind = pm::sdk::Kind::Field,
                                                         .owner = "Volume", .name = "sharedProfile"};

/// A volume's effects, a ScriptableObject. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeProfile{.key = "VolumeProfile", .name = "VolumeProfile",
                                                  .assembly = "Unity.RenderPipelines.Core.Runtime",
                                                  .ns = "UnityEngine.Rendering", .base = &ScriptableObject};

/// Adds an effect of the System.Type given (a VolumeComponent) and returns it; with `overrides` every parameter is
/// marked as set. The non-generic overload. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeProfile_Add{.key = "VolumeProfile.Add", .kind = pm::sdk::Kind::Method,
                                                      .owner = "VolumeProfile", .name = "Add",
                                                      .params = PostProcessing_Type_bool};

// A parameter's value. VolumeParameter<T>.set_value has no code in this build for float or DepthOfFieldMode (the
// running game reported both instances without code, 2026-11-10), so a
// mod sets a value with the parameter's own constructor, run again on the parameter in place: the constructor stores
// the value, the range and the override and nothing else, and the profile keeps the same object.

inline constexpr const char* PostProcessing_ClampedFloat_ctor[] = {"float", "float", "float", "bool"};
inline constexpr const char* PostProcessing_MinFloat_ctor[] = {"float", "float", "bool"};
inline constexpr const char* PostProcessing_DepthOfFieldMode_ctor[] = {"@DepthOfFieldMode", "bool"};

/// A float parameter held to a range (Unity.RenderPipelines.Core.Runtime): the vignette's intensity and smoothness, the
/// blur's radius. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc ClampedFloatParameter{.key = "ClampedFloatParameter",
                                                          .name = "ClampedFloatParameter",
                                                          .assembly = "Unity.RenderPipelines.Core.Runtime",
                                                          .ns = "UnityEngine.Rendering"};

/// ClampedFloatParameter(value, min, max, overrideState): the value held to min..max. It stores the value, min, max and
/// the override and nothing else (its base sets the value and calls set_overrideState). Decompiled.
inline constexpr pm::sdk::EntryDesc ClampedFloatParameter_ctor{
    .key = "ClampedFloatParameter.ctor", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ClampedFloatParameter",
    .name = ".ctor", .params = PostProcessing_ClampedFloat_ctor};

/// The parameter's lowest value. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc ClampedFloatParameter_min{.key = "ClampedFloatParameter.min",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "ClampedFloatParameter", .name = "min"};

/// The parameter's highest value. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc ClampedFloatParameter_max{.key = "ClampedFloatParameter.max",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "ClampedFloatParameter", .name = "max"};

/// A float parameter with a lowest value (Unity.RenderPipelines.Core.Runtime): the blur's start and end. Inferred: the
/// pipeline's documented API.
inline constexpr pm::sdk::EntryDesc MinFloatParameter{.key = "MinFloatParameter", .name = "MinFloatParameter",
                                                      .assembly = "Unity.RenderPipelines.Core.Runtime",
                                                      .ns = "UnityEngine.Rendering"};

/// MinFloatParameter(value, min, overrideState): it stores the value, min and the override and nothing else. Decompiled.
inline constexpr pm::sdk::EntryDesc MinFloatParameter_ctor{.key = "MinFloatParameter.ctor",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "MinFloatParameter",
                                                           .name = ".ctor", .params = PostProcessing_MinFloat_ctor};

/// The parameter's lowest value. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc MinFloatParameter_min{.key = "MinFloatParameter.min", .kind = pm::sdk::Kind::Field,
                                                          .owner = "MinFloatParameter", .name = "min"};

/// URP's depth-of-field modes (Unity.RenderPipelines.Universal.Runtime). Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfFieldMode{.key = "DepthOfFieldMode", .name = "DepthOfFieldMode",
                                                     .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                                     .ns = "UnityEngine.Rendering.Universal"};

/// Gaussian: a cheap blur of what lies beyond a start distance. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfFieldMode_Gaussian{.key = "DepthOfFieldMode.Gaussian",
                                                              .kind = pm::sdk::Kind::EnumValue,
                                                              .owner = "DepthOfFieldMode", .name = "Gaussian"};

/// URP's depth-of-field mode parameter (Unity.RenderPipelines.Universal.Runtime). Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfFieldModeParameter{.key = "DepthOfFieldModeParameter",
                                                              .name = "DepthOfFieldModeParameter",
                                                              .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                                              .ns = "UnityEngine.Rendering.Universal"};

/// DepthOfFieldModeParameter(value, overrideState): it stores the mode and the override and nothing else. Decompiled.
inline constexpr pm::sdk::EntryDesc DepthOfFieldModeParameter_ctor{
    .key = "DepthOfFieldModeParameter.ctor", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DepthOfFieldModeParameter",
    .name = ".ctor", .params = PostProcessing_DepthOfFieldMode_ctor};

/// URP's vignette effect, a VolumeComponent. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette{.key = "Vignette", .name = "Vignette",
                                             .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                             .ns = "UnityEngine.Rendering.Universal", .base = &ScriptableObject};

/// How dark the edges are, 0 to 1, a ClampedFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette_intensity{.key = "Vignette.intensity", .kind = pm::sdk::Kind::Field,
                                                       .owner = "Vignette", .name = "intensity"};

/// How soft the edge is, 0.01 to 1, a ClampedFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette_smoothness{.key = "Vignette.smoothness", .kind = pm::sdk::Kind::Field,
                                                        .owner = "Vignette", .name = "smoothness"};

/// URP's depth-of-field effect, a VolumeComponent. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField{.key = "DepthOfField", .name = "DepthOfField",
                                                 .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                                 .ns = "UnityEngine.Rendering.Universal", .base = &ScriptableObject};

/// The mode, a DepthOfFieldModeParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField_mode{.key = "DepthOfField.mode", .kind = pm::sdk::Kind::Field,
                                                      .owner = "DepthOfField", .name = "mode"};

/// Where the Gaussian blur starts, metres from the camera, a MinFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField_gaussianStart{.key = "DepthOfField.gaussianStart",
                                                               .kind = pm::sdk::Kind::Field, .owner = "DepthOfField",
                                                               .name = "gaussianStart"};

/// Where the Gaussian blur is full, metres, a MinFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField_gaussianEnd{.key = "DepthOfField.gaussianEnd",
                                                             .kind = pm::sdk::Kind::Field, .owner = "DepthOfField",
                                                             .name = "gaussianEnd"};

/// How wide the Gaussian blur is at most, 0.5 to 1.5, a ClampedFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField_gaussianMaxRadius{.key = "DepthOfField.gaussianMaxRadius",
                                                                   .kind = pm::sdk::Kind::Field,
                                                                   .owner = "DepthOfField", .name = "gaussianMaxRadius"};

}  // namespace pm::map

/// @file
/// @brief The game API for URP's camera data and volumes (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve logs once and returns its type's default.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// URP's settings of one camera.
struct UniversalAdditionalCameraData {
  /// The type: type_object() for GetComponent and AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::UniversalAdditionalCameraData> type{};
  /// get_renderPostProcessing(data). Unresolved: false.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_get_renderPostProcessing, bool(pm::api::Ref<pm::map::UniversalAdditionalCameraData>)>
      get_renderPostProcessing{};
  /// set_renderPostProcessing(data, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_set_renderPostProcessing, void(pm::api::Ref<pm::map::UniversalAdditionalCameraData>, bool)>
      set_renderPostProcessing{};
  /// get_volumeLayerMask(data): a layer mask. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_get_volumeLayerMask, int32_t(pm::api::Ref<pm::map::UniversalAdditionalCameraData>)>
      get_volumeLayerMask{};
  /// set_volumeLayerMask(data, mask). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_set_volumeLayerMask, void(pm::api::Ref<pm::map::UniversalAdditionalCameraData>, int32_t)>
      set_volumeLayerMask{};
};

/// Unity's ScriptableObject.
struct ScriptableObject {
  /// CreateInstance(type): a new one of the System.Type given. Unresolved: null.
  static constexpr pm::api::Method<pm::map::ScriptableObject_CreateInstance, pm::api::Object(pm::api::Object)> CreateInstance{};
};

/// A render volume.
struct Volume {
  /// The type: type_object() for AddComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Volume> type{};
  /// Whether it applies everywhere: m_IsGlobal.set(volume, on). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::Volume_m_IsGlobal, bool> m_IsGlobal{};
  /// Its priority: priority.set(volume, p). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::Volume_priority, float> priority{};
  /// How much applies: weight.set(volume, w). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::Volume_weight, float> weight{};
  /// Its VolumeProfile: sharedProfile.set(volume, profile). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::Volume_sharedProfile, pm::api::Object> sharedProfile{};
};

/// A volume's effects.
struct VolumeProfile {
  /// The type: type_object() for ScriptableObject::CreateInstance. Unresolved: null.
  static constexpr pm::api::Type<pm::map::VolumeProfile> type{};
  /// Add(profile, componentType, overrides): the new effect. Unresolved: null.
  static constexpr pm::api::Method<pm::map::VolumeProfile_Add, pm::api::Object(pm::api::Ref<pm::map::VolumeProfile>, pm::api::Object, bool)> Add{};
};

/// A float parameter held to a range. Set a value by running its constructor on it again (the map's note says why).
struct ClampedFloatParameter {
  /// ctor(parameter, value, min, max, overrideState). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ClampedFloatParameter_ctor, void(pm::api::Ref<pm::map::ClampedFloatParameter>, float, float, float, bool)> ctor{};
  /// Its lowest value: min.get(parameter). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::ClampedFloatParameter_min, float> min{};
  /// Its highest value: max.get(parameter). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::ClampedFloatParameter_max, float> max{};
};

/// A float parameter with a lowest value.
struct MinFloatParameter {
  /// ctor(parameter, value, min, overrideState). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MinFloatParameter_ctor, void(pm::api::Ref<pm::map::MinFloatParameter>, float, float, bool)> ctor{};
  /// Its lowest value: min.get(parameter). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::MinFloatParameter_min, float> min{};
};

/// URP's depth-of-field mode parameter.
struct DepthOfFieldModeParameter {
  /// ctor(parameter, mode, overrideState): mode a DepthOfFieldMode number. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::DepthOfFieldModeParameter_ctor, void(pm::api::Ref<pm::map::DepthOfFieldModeParameter>, int32_t, bool)> ctor{};
};

/// URP's depth-of-field modes.
namespace DepthOfFieldMode {
/// Gaussian(): the value's number. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DepthOfFieldMode_Gaussian> Gaussian{};
}  // namespace DepthOfFieldMode

/// URP's vignette.
struct Vignette {
  /// The type: type_object() for VolumeProfile::Add. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Vignette> type{};
  /// Its intensity parameter: intensity.get(vignette). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Vignette_intensity, pm::api::Object> intensity{};
  /// Its smoothness parameter: smoothness.get(vignette). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Vignette_smoothness, pm::api::Object> smoothness{};
};

/// URP's depth of field.
struct DepthOfField {
  /// The type: type_object() for VolumeProfile::Add. Unresolved: null.
  static constexpr pm::api::Type<pm::map::DepthOfField> type{};
  /// Its mode parameter: mode.get(dof). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_mode, pm::api::Object> mode{};
  /// Its Gaussian start parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianStart, pm::api::Object> gaussianStart{};
  /// Its Gaussian end parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianEnd, pm::api::Object> gaussianEnd{};
  /// Its Gaussian maximum radius parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianMaxRadius, pm::api::Object> gaussianMaxRadius{};
};

/// @}
}  // namespace phasmo
