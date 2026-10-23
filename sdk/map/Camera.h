#pragma once

// UnityEngine.Camera: Unity's own camera component. Not obfuscated, so every entry is by real name.

#include "entry.h"

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
