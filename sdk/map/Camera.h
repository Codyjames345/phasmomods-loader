#pragma once

// UnityEngine.Camera: Unity's own camera component. Not obfuscated, so every entry is by real name.

#include "entry.h"
#include "Transform.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* Camera_float[] = {"float"};
inline constexpr const char* Camera_int[] = {"int"};
inline constexpr const char* Camera_Vector3[] = {"Vector3"};

/// Unity's camera component (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Camera{.key = "Camera", .name = "Camera", .assembly = "UnityEngine.CoreModule",
                                           .ns = "UnityEngine"};

/// Where a world point lies on the screen, for the camera's own eye: x and y in pixels from the bottom
/// left, z in metres in front of the camera (negative behind it). The overload without an eye. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc Camera_WorldToScreenPoint{
    .key = "Camera.WorldToScreenPoint", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "WorldToScreenPoint",
    .params = Camera_Vector3};

/// Sets the vertical field of view, in degrees. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Camera_set_fieldOfView{
    .key = "Camera.set_fieldOfView", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "set_fieldOfView",
    .params = Camera_float};

/// Returns the layer mask of what the camera renders. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_cullingMask{
    .key = "Camera.get_cullingMask", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "get_cullingMask"};

/// Sets the layer mask of what the camera renders. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Camera_set_cullingMask{
    .key = "Camera.set_cullingMask", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "set_cullingMask",
    .params = Camera_int};

/// Returns the near clipping plane's distance. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_nearClipPlane{
    .key = "Camera.get_nearClipPlane", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "get_nearClipPlane"};

/// Sets the near clipping plane's distance. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Camera_set_nearClipPlane{
    .key = "Camera.set_nearClipPlane", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "set_nearClipPlane"};

/// Returns the far clipping plane's distance. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_farClipPlane{
    .key = "Camera.get_farClipPlane", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "get_farClipPlane"};

/// Sets the far clipping plane's distance. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Camera_set_farClipPlane{
    .key = "Camera.set_farClipPlane", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "set_farClipPlane"};

/// Every enabled camera in the scene, a new Camera[]. Inferred: Unity's documented static property.
inline constexpr pm::sdk::EntryDesc Camera_get_allCameras{.key = "Camera.get_allCameras", .kind = pm::sdk::Kind::Method,
                                                          .flags = pm::sdk::kStatic, .owner = "Camera",
                                                          .name = "get_allCameras"};

/// The RenderTexture the camera draws into, or null when it draws to the screen. Optional. Inferred:
/// Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_targetTexture{
    .key = "Camera.get_targetTexture", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional, .owner = "Camera",
    .name = "get_targetTexture"};

/// The ray from the camera through a point of its viewport (x and y 0 to 1 from the bottom left), the
/// overload without an eye. Optional. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Camera_ViewportPointToRay{
    .key = "Camera.ViewportPointToRay", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional, .owner = "Camera",
    .name = "ViewportPointToRay", .params = Camera_Vector3};

/// Where a world point lies in the camera's viewport: x and y 0 to 1 from the bottom left, z in metres
/// in front of the camera. The overload without an eye. Optional. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Camera_WorldToViewportPoint{
    .key = "Camera.WorldToViewportPoint", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
    .owner = "Camera", .name = "WorldToViewportPoint", .params = Camera_Vector3};

/// The camera's vertical field of view, in degrees. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_fieldOfView{
    .key = "Camera.get_fieldOfView", .kind = pm::sdk::Kind::Method, .owner = "Camera", .name = "get_fieldOfView"};

/// The camera's aspect ratio, width over height. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Camera_get_aspect{.key = "Camera.get_aspect", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Camera", .name = "get_aspect"};

}  // namespace pm::map

/// @file
/// @brief The game API for Camera (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// A ray, as Unity's Ray: where it starts and its direction (unit length).
struct Ray {
  Vector3 origin;     ///< where it starts, in world space
  Vector3 direction;  ///< its direction, unit length
};

/// Unity's camera component. `camera` below is the Camera object.
struct Camera {
  /// The Camera type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null
  /// and false.
  static constexpr pm::api::Type<pm::map::Camera> type{};
  /// Where `world` lies on the screen: WorldToScreenPoint(camera, world), z behind the camera
  /// negative. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Camera_WorldToScreenPoint, Vector3(void*, Vector3)> WorldToScreenPoint{};
  /// Sets the vertical field of view in degrees: set_fieldOfView(camera, degrees). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Camera_set_fieldOfView, void(void*, float)> set_fieldOfView{};
  /// The layer mask of what the camera renders: get_cullingMask(camera). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Camera_get_cullingMask, int32_t(void*)> get_cullingMask{};
  /// Every enabled camera, a Camera[]: get_allCameras(). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Camera_get_allCameras, void*()> get_allCameras{};
  /// Sets the layer mask of what the camera renders: set_cullingMask(camera, mask). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Camera_set_cullingMask, void(void*, int32_t)> set_cullingMask{};
  /// The near clipping plane's distance: get_nearClipPlane(camera). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Camera_get_nearClipPlane, float(void*)> get_nearClipPlane{};
  /// Sets the near clipping plane's distance: set_nearClipPlane(camera, distance). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Camera_set_nearClipPlane, void(void*, float)> set_nearClipPlane{};
  /// The far clipping plane's distance: get_farClipPlane(camera). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Camera_get_farClipPlane, float(void*)> get_farClipPlane{};
  /// Sets the far clipping plane's distance: set_farClipPlane(camera, distance). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Camera_set_farClipPlane, void(void*, float)> set_farClipPlane{};
  /// The RenderTexture it draws into, or null: get_targetTexture(camera). Optional. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Camera_get_targetTexture, void*(void*)> get_targetTexture{};
  /// The ray through a viewport point: ViewportPointToRay(camera, point). Optional. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Camera_ViewportPointToRay, Ray(void*, Vector3)> ViewportPointToRay{};
  /// Where `world` lies in the viewport: WorldToViewportPoint(camera, world). Optional. Unresolved: all
  /// zero.
  static constexpr pm::api::Method<pm::map::Camera_WorldToViewportPoint, Vector3(void*, Vector3)>
      WorldToViewportPoint{};
  /// get_fieldOfView(camera): its vertical field of view in degrees. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Camera_get_fieldOfView, float(void*)> get_fieldOfView{};
  /// get_aspect(camera): its width over its height. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Camera_get_aspect, float(void*)> get_aspect{};
};

/// @}
}  // namespace phasmo
