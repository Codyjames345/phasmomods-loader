#pragma once

// UnityEngine.Camera: Unity's own camera component. Not obfuscated, so every entry is by real name.

#include "entry.h"
#include "game_api.h"

namespace pg::map {

inline constexpr const char* Camera_float[] = {"float"};
inline constexpr const char* Camera_int[] = {"int"};

/// Unity's camera component (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc Camera{.key = "Camera", .name = "Camera", .assembly = "UnityEngine.CoreModule",
                                           .ns = "UnityEngine"};

/// Sets the vertical field of view, in degrees. Inferred: Unity's documented property setter.
inline constexpr pg::sdk::EntryDesc Camera_set_fieldOfView{
    .key = "Camera.set_fieldOfView", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "set_fieldOfView",
    .params = Camera_float};

/// Returns the layer mask of what the camera renders. Inferred: Unity's documented property getter.
inline constexpr pg::sdk::EntryDesc Camera_get_cullingMask{
    .key = "Camera.get_cullingMask", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "get_cullingMask"};

/// Sets the layer mask of what the camera renders. Inferred: Unity's documented property setter.
inline constexpr pg::sdk::EntryDesc Camera_set_cullingMask{
    .key = "Camera.set_cullingMask", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "set_cullingMask",
    .params = Camera_int};

/// Returns the near clipping plane's distance. Inferred: Unity's documented property getter.
inline constexpr pg::sdk::EntryDesc Camera_get_nearClipPlane{
    .key = "Camera.get_nearClipPlane", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "get_nearClipPlane"};

/// Sets the near clipping plane's distance. Inferred: Unity's documented property setter.
inline constexpr pg::sdk::EntryDesc Camera_set_nearClipPlane{
    .key = "Camera.set_nearClipPlane", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "set_nearClipPlane"};

/// Returns the far clipping plane's distance. Inferred: Unity's documented property getter.
inline constexpr pg::sdk::EntryDesc Camera_get_farClipPlane{
    .key = "Camera.get_farClipPlane", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "get_farClipPlane"};

/// Sets the far clipping plane's distance. Inferred: Unity's documented property setter.
inline constexpr pg::sdk::EntryDesc Camera_set_farClipPlane{
    .key = "Camera.set_farClipPlane", .kind = pg::sdk::Kind::Method, .owner = "Camera", .name = "set_farClipPlane"};

}  // namespace pg::map

// The game API for Camera (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build"
// once and returns its type's default (0, null) without touching the game.
namespace phasmo {

/// Unity's camera component. `camera` below is the Camera object.
struct Camera {
  /// The Camera type: klass() and is(obj). Unresolved: null and false.
  static constexpr pg::api::Type<pg::map::Camera> type{};
  /// Sets the vertical field of view in degrees: set_fieldOfView(camera, degrees). Unresolved: does
  /// nothing.
  static constexpr pg::api::Method<pg::map::Camera_set_fieldOfView, void(void*, float)> set_fieldOfView{};
  /// The layer mask of what the camera renders: get_cullingMask(camera). Unresolved: 0.
  static constexpr pg::api::Method<pg::map::Camera_get_cullingMask, int32_t(void*)> get_cullingMask{};
  /// Sets the layer mask of what the camera renders: set_cullingMask(camera, mask). Unresolved: does
  /// nothing.
  static constexpr pg::api::Method<pg::map::Camera_set_cullingMask, void(void*, int32_t)> set_cullingMask{};
  /// The near clipping plane's distance: get_nearClipPlane(camera). Unresolved: 0.
  static constexpr pg::api::Method<pg::map::Camera_get_nearClipPlane, float(void*)> get_nearClipPlane{};
  /// Sets the near clipping plane's distance: set_nearClipPlane(camera, distance). Unresolved: does
  /// nothing.
  static constexpr pg::api::Method<pg::map::Camera_set_nearClipPlane, void(void*, float)> set_nearClipPlane{};
  /// The far clipping plane's distance: get_farClipPlane(camera). Unresolved: 0.
  static constexpr pg::api::Method<pg::map::Camera_get_farClipPlane, float(void*)> get_farClipPlane{};
  /// Sets the far clipping plane's distance: set_farClipPlane(camera, distance). Unresolved: does
  /// nothing.
  static constexpr pg::api::Method<pg::map::Camera_set_farClipPlane, void(void*, float)> set_farClipPlane{};
};

}  // namespace phasmo
