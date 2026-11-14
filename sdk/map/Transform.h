#pragma once

// UnityEngine.Transform: Unity's transform component, which holds the scene's hierarchy, and
// RectTransform, a UI object's. Not obfuscated, so every entry is by real name. The host uses Find to
// find Scene entries (Scene.h); a mod gets no wrapper for it, since a lookup by a raw path string would
// put asset facts back into mod code. The others are phasmo:: wrappers, which reach every mod; the
// host's journal (host/content_journal.h) walks and orders children with them.

#include "entry.h"
#include "game_api.h"
#include "UnityObject.h"

namespace pm::map {

inline constexpr const char* Transform_string[] = {"string"};
inline constexpr const char* Transform_int[] = {"int"};

/// Unity's transform component (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform{.key = "Transform", .name = "Transform",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine",
                                              .base = &Component};

/// Finds a child by its name, or a descendant by a path of names separated by '/', inactive ones
/// included; null when there is none. Where several children share a name it takes the first that
/// leads to the rest of the path. The host's scene_find calls it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_Find{.key = "Transform.Find", .kind = pm::sdk::Kind::Method,
                                                   .owner = "Transform", .name = "Find", .params = Transform_string};

/// The number of children. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_childCount{
    .key = "Transform.get_childCount", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_childCount"};

/// The child at an index, in the hierarchy's order. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_GetChild{.key = "Transform.GetChild", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Transform", .name = "GetChild",
                                                       .params = Transform_int};

/// The Transform's index among its parent's children. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_GetSiblingIndex{
    .key = "Transform.GetSiblingIndex", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "GetSiblingIndex"};

/// Moves the Transform to an index among its parent's children; a layout group lays children out in
/// this order. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_SetSiblingIndex{
    .key = "Transform.SetSiblingIndex", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "SetSiblingIndex",
    .params = Transform_int};

/// The Transform's position in world space. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_position{
    .key = "Transform.get_position", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_position"};

inline constexpr const char* Transform_Vector3[] = {"Vector3"};

/// Unity's Vector3 (UnityEngine.CoreModule), a struct of three floats. Its type makes a Vector3[], as Mesh.set_vertices
/// takes. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Vector3{.key = "Vector3", .name = "Vector3", .assembly = "UnityEngine.CoreModule",
                                            .ns = "UnityEngine"};

/// The Transform's forward direction in world space, a unit vector. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_forward{
    .key = "Transform.get_forward", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_forward"};

/// The Transform's rotation relative to its parent as Euler angles in degrees. Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_localEulerAngles{.key = "Transform.get_localEulerAngles",
                                                                   .kind = pm::sdk::Kind::Method,
                                                                   .owner = "Transform",
                                                                   .name = "get_localEulerAngles"};

/// The Transform's scale relative to its parent. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_localScale{
    .key = "Transform.get_localScale", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_localScale"};

/// Sets the Transform's scale relative to its parent. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_localScale{.key = "Transform.set_localScale",
                                                             .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                             .name = "set_localScale", .params = Transform_Vector3};

/// Moves the Transform to a position in world space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_position{.key = "Transform.set_position",
                                                           .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                           .name = "set_position", .params = Transform_Vector3};

inline constexpr const char* Transform_Quaternion[] = {"Quaternion"};

/// Turns the Transform to a rotation in world space. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_rotation{.key = "Transform.set_rotation",
                                                           .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                           .name = "set_rotation", .params = Transform_Quaternion};

inline constexpr const char* Transform_SetParent_params[] = {"Transform", "bool"};

/// Moves the Transform under another parent (null: the scene's root), keeping its place in the world
/// when the second argument is true. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_SetParent{.key = "Transform.SetParent", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Transform", .name = "SetParent",
                                                        .params = Transform_SetParent_params};

/// The Transform's parent, or null at the scene's root. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_parent{.key = "Transform.get_parent", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Transform", .name = "get_parent"};

/// A UI object's transform (UnityEngine.CoreModule), a Transform. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RectTransform{.key = "RectTransform", .name = "RectTransform",
                                                  .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine",
                                                  .base = &Transform};

/// Its rectangle in its own space: x, y, width and height, four floats. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc RectTransform_get_rect{
    .key = "RectTransform.get_rect", .kind = pm::sdk::Kind::Method, .owner = "RectTransform", .name = "get_rect"};

/// Its position relative to its anchors: two floats. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc RectTransform_get_anchoredPosition{
    .key = "RectTransform.get_anchoredPosition", .kind = pm::sdk::Kind::Method, .owner = "RectTransform",
    .name = "get_anchoredPosition"};

inline constexpr const char* RectTransform_Vector2[] = {"Vector2"};

// A GridLayoutGroup places each active child by setting four of its properties: its anchors to the
// parent's top-left corner, its size to the cell and its position (GridLayoutGroup.SetCellsAlongAxis
// and LayoutGroup.SetChildAlongAxisWithScale, decompiled). The host's journal reads and writes them to
// put a list's hand placement back after switching its grid on (host/content_journal.h).

/// Sets its position relative to its anchors. Decompiled: a grid layout writes it for each child it
/// places (LayoutGroup.SetChildAlongAxisWithScale).
inline constexpr pm::sdk::EntryDesc RectTransform_set_anchoredPosition{
    .key = "RectTransform.set_anchoredPosition", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RectTransform",
    .name = "set_anchoredPosition", .params = RectTransform_Vector2};

/// Its lower-left anchor, as fractions of the parent's size: two floats. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc RectTransform_get_anchorMin{
    .key = "RectTransform.get_anchorMin", .kind = pm::sdk::Kind::Method, .owner = "RectTransform",
    .name = "get_anchorMin"};

/// Sets its lower-left anchor. Decompiled: a grid layout sets it to (0, 1) for each child it places
/// (GridLayoutGroup.SetCellsAlongAxis, LayoutGroup.SetChildAlongAxisWithScale).
inline constexpr pm::sdk::EntryDesc RectTransform_set_anchorMin{
    .key = "RectTransform.set_anchorMin", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RectTransform",
    .name = "set_anchorMin", .params = RectTransform_Vector2};

/// Its upper-right anchor, as fractions of the parent's size: two floats. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc RectTransform_get_anchorMax{
    .key = "RectTransform.get_anchorMax", .kind = pm::sdk::Kind::Method, .owner = "RectTransform",
    .name = "get_anchorMax"};

/// Sets its upper-right anchor. Decompiled: a grid layout sets it to (0, 1) for each child it places
/// (GridLayoutGroup.SetCellsAlongAxis, LayoutGroup.SetChildAlongAxisWithScale).
inline constexpr pm::sdk::EntryDesc RectTransform_set_anchorMax{
    .key = "RectTransform.set_anchorMax", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RectTransform",
    .name = "set_anchorMax", .params = RectTransform_Vector2};

/// Its size less the anchors' span: two floats. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc RectTransform_get_sizeDelta{
    .key = "RectTransform.get_sizeDelta", .kind = pm::sdk::Kind::Method, .owner = "RectTransform",
    .name = "get_sizeDelta"};

/// Sets its size less the anchors' span. Decompiled: a grid layout sets it to the cell size for each
/// child it places (GridLayoutGroup.SetCellsAlongAxis).
inline constexpr pm::sdk::EntryDesc RectTransform_set_sizeDelta{
    .key = "RectTransform.set_sizeDelta", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RectTransform",
    .name = "set_sizeDelta", .params = RectTransform_Vector2};

/// Sets the point the RectTransform is placed and scaled about, in 0..1 of its own rectangle. Inferred:
/// Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc RectTransform_set_pivot{.key = "RectTransform.set_pivot",
                                                            .kind = pm::sdk::Kind::Method, .owner = "RectTransform",
                                                            .name = "set_pivot", .params = RectTransform_Vector2};

/// The Transform's rotation in world space. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_rotation{
    .key = "Transform.get_rotation", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_rotation"};

/// The Transform's position relative to its parent. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_localPosition{.key = "Transform.get_localPosition",
                                                                .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                                .name = "get_localPosition"};

/// Sets the Transform's position relative to its parent. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_localPosition{
    .key = "Transform.set_localPosition", .kind = pm::sdk::Kind::Method, .owner = "Transform",
    .name = "set_localPosition", .params = Transform_Vector3};

/// The Transform's rotation relative to its parent. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_localRotation{.key = "Transform.get_localRotation",
                                                                .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                                .name = "get_localRotation"};

/// A point given in the Transform's own space, in world space. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_TransformPoint{.key = "Transform.TransformPoint",
                                                             .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                             .name = "TransformPoint", .params = Transform_Vector3};

/// A world point, in the Transform's own space. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Transform_InverseTransformPoint{.key = "Transform.InverseTransformPoint",
                                                                    .kind = pm::sdk::Kind::Method, .owner = "Transform",
                                                                    .name = "InverseTransformPoint",
                                                                    .params = Transform_Vector3};

/// Sets the Transform's rotation relative to its parent from Euler angles in degrees. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_localEulerAngles{
    .key = "Transform.set_localEulerAngles", .kind = pm::sdk::Kind::Method, .owner = "Transform",
    .name = "set_localEulerAngles", .params = Transform_Vector3};

/// Sets the Transform's rotation relative to its parent. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Transform_set_localRotation{
    .key = "Transform.set_localRotation", .kind = pm::sdk::Kind::Method, .owner = "Transform",
    .name = "set_localRotation", .params = Transform_Quaternion};

/// The Transform's scale in the world, its own scale times its parents'. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc Transform_get_lossyScale{
    .key = "Transform.get_lossyScale", .kind = pm::sdk::Kind::Method, .owner = "Transform", .name = "get_lossyScale"};

inline constexpr const char* Transform_Transform[] = {"Transform"};

/// Whether the Transform is the given one or lies under it in the hierarchy. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Transform_IsChildOf{.key = "Transform.IsChildOf", .kind = pm::sdk::Kind::Method,
                                                        .owner = "Transform", .name = "IsChildOf",
                                                        .params = Transform_Transform};

}  // namespace pm::map

/// @file
/// @brief The game API for the hierarchy (sdk/api/game_api.h), but Find, which only the host's scene facts use.
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game. A
/// Transform it returns is a managed reference: keep it across frames only in a pm::api::Held.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Two floats, as Unity's Vector2.
struct Vector2 {
  float x;  ///< x
  float y;  ///< y
};

/// Three floats, as Unity's Vector3.
struct Vector3 {
  /// The type: make_array() for a Vector3[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Vector3> type{};
  float x;  ///< x, in metres for a position
  float y;  ///< y, up, in metres for a position
  float z;  ///< z, in metres for a position
};

/// Four floats, as Unity's Quaternion: a rotation.
struct Quaternion {
  float x;  ///< the rotation's x part
  float y;  ///< the rotation's y part
  float z;  ///< the rotation's z part
  float w;  ///< the rotation's w part; (0, 0, 0, 1) is no rotation
};

/// Four floats, as Unity's Rect: its corner and its size.
struct Rect {
  float x;       ///< the corner's x
  float y;       ///< the corner's y
  float width;   ///< its width
  float height;  ///< its height
};

/// Unity's transform component. `transform` below is the Transform.
struct Transform {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Transform> type{};
  /// get_childCount(transform). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Transform_get_childCount, int32_t(pm::api::Ref<pm::map::Transform>)> get_childCount{};
  /// GetChild(transform, index). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Transform_GetChild, pm::api::Object(pm::api::Ref<pm::map::Transform>, int32_t)> GetChild{};
  /// GetSiblingIndex(transform). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Transform_GetSiblingIndex, int32_t(pm::api::Ref<pm::map::Transform>)> GetSiblingIndex{};
  /// SetSiblingIndex(transform, index). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_SetSiblingIndex, void(pm::api::Ref<pm::map::Transform>, int32_t)> SetSiblingIndex{};
  /// get_position(transform): its world position. Unresolved: zero.
  static constexpr pm::api::Method<pm::map::Transform_get_position, Vector3(pm::api::Ref<pm::map::Transform>)> get_position{};
  /// get_forward(transform): its forward direction. Unresolved: zero.
  static constexpr pm::api::Method<pm::map::Transform_get_forward, Vector3(pm::api::Ref<pm::map::Transform>)> get_forward{};
  /// get_localEulerAngles(transform): its local rotation in degrees. Unresolved: zero.
  static constexpr pm::api::Method<pm::map::Transform_get_localEulerAngles, Vector3(pm::api::Ref<pm::map::Transform>)> get_localEulerAngles{};
  /// get_localScale(transform). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::Transform_get_localScale, Vector3(pm::api::Ref<pm::map::Transform>)> get_localScale{};
  /// set_localScale(transform, &scale), the Vector3 passed by address. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_localScale, void(pm::api::Ref<pm::map::Transform>, const Vector3*)> set_localScale{};
  /// set_position(transform, &position). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_position, void(pm::api::Ref<pm::map::Transform>, const Vector3*)> set_position{};
  /// set_rotation(transform, &rotation). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_rotation, void(pm::api::Ref<pm::map::Transform>, const Quaternion*)> set_rotation{};
  /// SetParent(transform, parent, worldPositionStays). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_SetParent, void(pm::api::Ref<pm::map::Transform>, pm::api::Ref<pm::map::Transform>, bool)> SetParent{};
  /// get_parent(transform): its parent, or null at the root. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Transform_get_parent, pm::api::Object(pm::api::Ref<pm::map::Transform>)> get_parent{};
  /// get_rotation(t): its rotation in world space. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Transform_get_rotation, Quaternion(pm::api::Ref<pm::map::Transform>)> get_rotation{};
  /// get_localPosition(t): its position relative to its parent. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Transform_get_localPosition, Vector3(pm::api::Ref<pm::map::Transform>)> get_localPosition{};
  /// set_localPosition(t, &p): sets its position relative to its parent. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_localPosition, void(pm::api::Ref<pm::map::Transform>, const Vector3*)> set_localPosition{};
  /// get_localRotation(t): its rotation relative to its parent. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Transform_get_localRotation, Quaternion(pm::api::Ref<pm::map::Transform>)> get_localRotation{};
  /// TransformPoint(t, &local): a point of its own space in world space. Unresolved: zeros.
  static constexpr pm::api::Method<pm::map::Transform_TransformPoint, Vector3(pm::api::Ref<pm::map::Transform>, const Vector3*)> TransformPoint{};
  /// InverseTransformPoint(t, &world): a world point in its own space. Unresolved: zeros.
  static constexpr pm::api::Method<pm::map::Transform_InverseTransformPoint, Vector3(pm::api::Ref<pm::map::Transform>, const Vector3*)>
      InverseTransformPoint{};
  /// set_localEulerAngles(t, &e): sets its rotation relative to its parent, in degrees. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_localEulerAngles, void(pm::api::Ref<pm::map::Transform>, const Vector3*)>
      set_localEulerAngles{};
  /// set_localRotation(t, &r): sets its rotation relative to its parent. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Transform_set_localRotation, void(pm::api::Ref<pm::map::Transform>, const Quaternion*)>
      set_localRotation{};
  /// get_lossyScale(t): its scale in the world. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Transform_get_lossyScale, Vector3(pm::api::Ref<pm::map::Transform>)> get_lossyScale{};
  /// IsChildOf(t, parent): whether t is parent or lies under it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::Transform_IsChildOf, bool(pm::api::Ref<pm::map::Transform>, pm::api::Ref<pm::map::Transform>)> IsChildOf{};
};

/// A UI object's transform. `rect` below is the RectTransform.
struct RectTransform {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::RectTransform> type{};
  /// get_rect(rect). Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::RectTransform_get_rect, Rect(pm::api::Ref<pm::map::RectTransform>)> get_rect{};
  /// get_anchoredPosition(rect). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::RectTransform_get_anchoredPosition, Vector2(pm::api::Ref<pm::map::RectTransform>)> get_anchoredPosition{};
  /// set_anchoredPosition(rect, position). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RectTransform_set_anchoredPosition, void(pm::api::Ref<pm::map::RectTransform>, Vector2)>
      set_anchoredPosition{};
  /// get_anchorMin(rect). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::RectTransform_get_anchorMin, Vector2(pm::api::Ref<pm::map::RectTransform>)> get_anchorMin{};
  /// set_anchorMin(rect, anchor). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RectTransform_set_anchorMin, void(pm::api::Ref<pm::map::RectTransform>, Vector2)> set_anchorMin{};
  /// get_anchorMax(rect). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::RectTransform_get_anchorMax, Vector2(pm::api::Ref<pm::map::RectTransform>)> get_anchorMax{};
  /// set_anchorMax(rect, anchor). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RectTransform_set_anchorMax, void(pm::api::Ref<pm::map::RectTransform>, Vector2)> set_anchorMax{};
  /// get_sizeDelta(rect). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::RectTransform_get_sizeDelta, Vector2(pm::api::Ref<pm::map::RectTransform>)> get_sizeDelta{};
  /// set_sizeDelta(rect, size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RectTransform_set_sizeDelta, void(pm::api::Ref<pm::map::RectTransform>, Vector2)> set_sizeDelta{};
  /// set_pivot(rect, pivot): the point it is placed about, in 0..1 of itself. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RectTransform_set_pivot, void(pm::api::Ref<pm::map::RectTransform>, Vector2)> set_pivot{};
};

/// @}
}  // namespace phasmo
