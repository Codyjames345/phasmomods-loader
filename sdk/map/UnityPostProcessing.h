#pragma once

// The Universal Render Pipeline's camera data and volumes: a mod's own camera that renders as the player's does, and a
// screen blur and vignette of a mod's own (sdk/api/third_person.h, sdk/api/screen_effects.h). The game renders with
// URP (Unity.RenderPipelines.Universal.Runtime). Not obfuscated, so every entry is by real name.

#include <cstdint>

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* PostProcessing_bool[] = {"bool"};
inline constexpr const char* PostProcessing_LayerMask[] = {"LayerMask"};
inline constexpr const char* PostProcessing_Type[] = {"Type"};
inline constexpr const char* PostProcessing_Type_bool[] = {"Type", "bool"};
inline constexpr const char* PostProcessing_T[] = {"T"};
inline constexpr const char* PostProcessing_Single[] = {"@Single"};
inline constexpr const char* PostProcessing_DepthOfFieldMode[] = {"@DepthOfFieldMode"};

/// URP's settings of one camera (Unity.RenderPipelines.Universal.Runtime), a component beside the Camera. Inferred:
/// URP's documented API.
inline constexpr pm::sdk::EntryDesc UniversalAdditionalCameraData{
    .key = "UniversalAdditionalCameraData", .name = "UniversalAdditionalCameraData",
    .assembly = "Unity.RenderPipelines.Universal.Runtime", .ns = "UnityEngine.Rendering.Universal"};

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
                                                     .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// A new instance of the ScriptableObject System.Type given. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc ScriptableObject_CreateInstance{
    .key = "ScriptableObject.CreateInstance", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "ScriptableObject", .name = "CreateInstance", .params = PostProcessing_Type};

/// A volume (Unity.RenderPipelines.Core.Runtime): a component whose profile's effects apply to the cameras whose volume
/// layer mask holds its GameObject's layer. Inferred: the render pipeline's documented API.
inline constexpr pm::sdk::EntryDesc Volume{.key = "Volume", .name = "Volume",
                                           .assembly = "Unity.RenderPipelines.Core.Runtime",
                                           .ns = "UnityEngine.Rendering"};

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
                                                  .ns = "UnityEngine.Rendering"};

/// Adds an effect of the System.Type given (a VolumeComponent) and returns it; with `overrides` every parameter is
/// marked as set. The non-generic overload. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeProfile_Add{.key = "VolumeProfile.Add", .kind = pm::sdk::Kind::Method,
                                                      .owner = "VolumeProfile", .name = "Add",
                                                      .params = PostProcessing_Type_bool};

// The name alone is shared with the non-generic VolumeParameter, its base; only this one holds a value of its parameter.
inline constexpr const char* VolumeParameterT_fieldTypes[] = {"T"};

/// The generic definition VolumeParameter<T> (Unity.RenderPipelines.Core.Runtime), a volume parameter's value holder.
/// Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeParameter_1{.key = "VolumeParameter`1", .name = "VolumeParameter",
                                                      .assembly = "Unity.RenderPipelines.Core.Runtime",
                                                      .ns = "UnityEngine.Rendering",
                                                      .fieldTypes = VolumeParameterT_fieldTypes};

/// Sets a float parameter's value (ClampedFloatParameter, MinFloatParameter): VolumeParameter<float>.set_value.
/// Checked at startup only: the instance's code is the runtime's. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeParameter_float_set_value{
    .key = "VolumeParameter<float>.set_value", .kind = pm::sdk::Kind::Inflated, .owner = "VolumeParameter`1",
    .name = "set_value", .params = PostProcessing_T, .typeArgs = PostProcessing_Single};

/// URP's depth-of-field modes (Unity.RenderPipelines.Universal.Runtime). Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfFieldMode{.key = "DepthOfFieldMode", .name = "DepthOfFieldMode",
                                                     .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                                     .ns = "UnityEngine.Rendering.Universal"};

/// Gaussian: a cheap blur of what lies beyond a start distance. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfFieldMode_Gaussian{.key = "DepthOfFieldMode.Gaussian",
                                                              .kind = pm::sdk::Kind::EnumValue,
                                                              .owner = "DepthOfFieldMode", .name = "Gaussian"};

/// Sets the depth-of-field mode parameter's value: VolumeParameter<DepthOfFieldMode>.set_value. Checked at startup
/// only. Inferred: the pipeline's documented API.
inline constexpr pm::sdk::EntryDesc VolumeParameter_DepthOfFieldMode_set_value{
    .key = "VolumeParameter<DepthOfFieldMode>.set_value", .kind = pm::sdk::Kind::Inflated,
    .owner = "VolumeParameter`1", .name = "set_value", .params = PostProcessing_T,
    .typeArgs = PostProcessing_DepthOfFieldMode};

/// URP's vignette effect, a VolumeComponent. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette{.key = "Vignette", .name = "Vignette",
                                             .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                             .ns = "UnityEngine.Rendering.Universal"};

/// How dark the edges are, 0 to 1, a ClampedFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette_intensity{.key = "Vignette.intensity", .kind = pm::sdk::Kind::Field,
                                                       .owner = "Vignette", .name = "intensity"};

/// How soft the edge is, 0.01 to 1, a ClampedFloatParameter. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc Vignette_smoothness{.key = "Vignette.smoothness", .kind = pm::sdk::Kind::Field,
                                                        .owner = "Vignette", .name = "smoothness"};

/// URP's depth-of-field effect, a VolumeComponent. Inferred: URP's documented API.
inline constexpr pm::sdk::EntryDesc DepthOfField{.key = "DepthOfField", .name = "DepthOfField",
                                                 .assembly = "Unity.RenderPipelines.Universal.Runtime",
                                                 .ns = "UnityEngine.Rendering.Universal"};

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
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_get_renderPostProcessing, bool(void*)>
      get_renderPostProcessing{};
  /// set_renderPostProcessing(data, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_set_renderPostProcessing, void(void*, bool)>
      set_renderPostProcessing{};
  /// get_volumeLayerMask(data): a layer mask. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_get_volumeLayerMask, int32_t(void*)>
      get_volumeLayerMask{};
  /// set_volumeLayerMask(data, mask). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UniversalAdditionalCameraData_set_volumeLayerMask, void(void*, int32_t)>
      set_volumeLayerMask{};
};

/// Unity's ScriptableObject.
struct ScriptableObject {
  /// CreateInstance(type): a new one of the System.Type given. Unresolved: null.
  static constexpr pm::api::Method<pm::map::ScriptableObject_CreateInstance, void*(void*)> CreateInstance{};
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
  static constexpr pm::api::Field<pm::map::Volume_sharedProfile, void*> sharedProfile{};
};

/// A volume's effects.
struct VolumeProfile {
  /// The type: type_object() for ScriptableObject::CreateInstance. Unresolved: null.
  static constexpr pm::api::Type<pm::map::VolumeProfile> type{};
  /// Add(profile, componentType, overrides): the new effect. Unresolved: null.
  static constexpr pm::api::Method<pm::map::VolumeProfile_Add, void*(void*, void*, bool)> Add{};
};

/// Volume parameters' value setters.
struct VolumeParameter {
  /// SetFloat(parameter, value): a float parameter's value. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VolumeParameter_float_set_value, void(void*, float)> SetFloat{};
  /// SetDepthOfFieldMode(parameter, mode): a DepthOfFieldMode number. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VolumeParameter_DepthOfFieldMode_set_value, void(void*, int32_t)>
      SetDepthOfFieldMode{};
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
  static constexpr pm::api::Field<pm::map::Vignette_intensity, void*> intensity{};
  /// Its smoothness parameter: smoothness.get(vignette). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Vignette_smoothness, void*> smoothness{};
};

/// URP's depth of field.
struct DepthOfField {
  /// The type: type_object() for VolumeProfile::Add. Unresolved: null.
  static constexpr pm::api::Type<pm::map::DepthOfField> type{};
  /// Its mode parameter: mode.get(dof). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_mode, void*> mode{};
  /// Its Gaussian start parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianStart, void*> gaussianStart{};
  /// Its Gaussian end parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianEnd, void*> gaussianEnd{};
  /// Its Gaussian maximum radius parameter. Unresolved: null.
  static constexpr pm::api::Field<pm::map::DepthOfField_gaussianMaxRadius, void*> gaussianMaxRadius{};
};

/// @}
}  // namespace phasmo
