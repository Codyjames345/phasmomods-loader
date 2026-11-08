#pragma once

// The player's input: Unity's Cursor, and the Input System's mouse, buttons and input actions
// (Unity.InputSystem). None of it is obfuscated, so every entry is by real name. Playable Ghost reads
// the player's own key bindings from the game's input actions, so its controls follow the player's
// settings, and switches the investigator's actions off while the player steers the ghost.

#include "Transform.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// Unity's mouse cursor settings (UnityEngine.CoreModule), a static class. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Cursor{.key = "Cursor", .name = "Cursor", .assembly = "UnityEngine.CoreModule",
                                           .ns = "UnityEngine"};

/// How the cursor is held (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CursorLockMode{.key = "CursorLockMode", .name = "CursorLockMode",
                                                   .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The cursor moves freely and shows wherever the game shows it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CursorLockMode_None{.key = "CursorLockMode.None",
                                                        .kind = pm::sdk::Kind::EnumValue, .owner = "CursorLockMode",
                                                        .name = "None"};

/// The cursor is held at the centre of the game's window and hidden, as while a player walks. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc CursorLockMode_Locked{.key = "CursorLockMode.Locked",
                                                          .kind = pm::sdk::Kind::EnumValue,
                                                          .owner = "CursorLockMode", .name = "Locked"};

inline constexpr const char* Cursor_bool[] = {"bool"};
inline constexpr const char* Cursor_CursorLockMode[] = {"CursorLockMode"};

/// How the cursor is held now, a CursorLockMode. Static. The game frees it while a menu (pause, journal)
/// is open. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Cursor_get_lockState{.key = "Cursor.get_lockState", .kind = pm::sdk::Kind::Method,
                                                         .flags = pm::sdk::kStatic, .owner = "Cursor",
                                                         .name = "get_lockState"};

/// Sets how the cursor is held. Static. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Cursor_set_lockState{.key = "Cursor.set_lockState", .kind = pm::sdk::Kind::Method,
                                                         .flags = pm::sdk::kStatic, .owner = "Cursor",
                                                         .name = "set_lockState", .params = Cursor_CursorLockMode};

/// Shows or hides the cursor. Static. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Cursor_set_visible{.key = "Cursor.set_visible", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "Cursor",
                                                       .name = "set_visible", .params = Cursor_bool};

/// An input action (Unity.InputSystem, namespace UnityEngine.InputSystem): "Move", "Interact" and the like,
/// with the bindings that trigger it. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction{.key = "InputAction", .name = "InputAction",
                                                .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem"};

/// The action's name, a string ("Move"). Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputAction_m_Name{.key = "InputAction.m_Name", .kind = pm::sdk::Kind::Field,
                                                       .owner = "InputAction", .name = "m_Name"};

/// The action's id, a string (a GUID); a binding names its action by name or by this id. Inferred: the
/// Input System's own field.
inline constexpr pm::sdk::EntryDesc InputAction_m_Id{.key = "InputAction.m_Id", .kind = pm::sdk::Kind::Field,
                                                     .owner = "InputAction", .name = "m_Id"};

/// The InputActionMap the action belongs to. Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputAction_m_ActionMap{.key = "InputAction.m_ActionMap",
                                                            .kind = pm::sdk::Kind::Field, .owner = "InputAction",
                                                            .name = "m_ActionMap"};

/// Whether the action is enabled, so its bindings trigger it. Inferred: the Input System's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc InputAction_get_enabled{.key = "InputAction.get_enabled",
                                                            .kind = pm::sdk::Kind::Method, .owner = "InputAction",
                                                            .name = "get_enabled"};

/// Enables the action. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction_Enable{.key = "InputAction.Enable", .kind = pm::sdk::Kind::Method,
                                                       .owner = "InputAction", .name = "Enable"};

/// Disables the action: its bindings no longer trigger it. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction_Disable{.key = "InputAction.Disable", .kind = pm::sdk::Kind::Method,
                                                        .owner = "InputAction", .name = "Disable"};

/// A named group of input actions (Unity.InputSystem), "Player" and the like. Inferred: the Input System's
/// documented API.
inline constexpr pm::sdk::EntryDesc InputActionMap{.key = "InputActionMap", .name = "InputActionMap",
                                                   .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem"};

/// The map's name, a string. Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputActionMap_m_Name{.key = "InputActionMap.m_Name",
                                                          .kind = pm::sdk::Kind::Field, .owner = "InputActionMap",
                                                          .name = "m_Name"};

/// The map's actions, an InputAction[]. Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputActionMap_m_Actions{.key = "InputActionMap.m_Actions",
                                                             .kind = pm::sdk::Kind::Field, .owner = "InputActionMap",
                                                             .name = "m_Actions"};

/// Every binding of the map's actions, an InputBinding[] of structs; each names the action it triggers.
/// Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputActionMap_m_Bindings{.key = "InputActionMap.m_Bindings",
                                                              .kind = pm::sdk::Kind::Field, .owner = "InputActionMap",
                                                              .name = "m_Bindings"};

/// A set of action maps (Unity.InputSystem), a ScriptableObject. Inferred: the Input System's documented
/// API.
inline constexpr pm::sdk::EntryDesc InputActionAsset{.key = "InputActionAsset", .name = "InputActionAsset",
                                                     .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem"};

/// The asset's action maps, an InputActionMap[]. Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputActionAsset_m_ActionMaps{.key = "InputActionAsset.m_ActionMaps",
                                                                  .kind = pm::sdk::Kind::Field,
                                                                  .owner = "InputActionAsset", .name = "m_ActionMaps"};

/// One binding of an action to a control (Unity.InputSystem), a struct; the fields below lie at their
/// place in the struct itself. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputBinding{.key = "InputBinding", .name = "InputBinding",
                                                 .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem"};

/// The binding's name, a string: a composite's part ("up", "down", "left", "right" of Move), else empty.
/// Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputBinding_m_Name{.key = "InputBinding.m_Name", .kind = pm::sdk::Kind::Field,
                                                        .owner = "InputBinding", .name = "m_Name"};

/// The control the binding names, a string path ("<Keyboard>/e"). Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputBinding_m_Path{.key = "InputBinding.m_Path", .kind = pm::sdk::Kind::Field,
                                                        .owner = "InputBinding", .name = "m_Path"};

/// The action the binding triggers, by its name or its id, a string. Inferred: the Input System's own field.
inline constexpr pm::sdk::EntryDesc InputBinding_m_Action{.key = "InputBinding.m_Action", .kind = pm::sdk::Kind::Field,
                                                          .owner = "InputBinding", .name = "m_Action"};

/// The player's rebind of the binding, a string path, or empty when they kept the default. Inferred: the
/// Input System's own field.
inline constexpr pm::sdk::EntryDesc InputBinding_m_OverridePath{
    .key = "InputBinding.m_OverridePath", .kind = pm::sdk::Kind::Field, .owner = "InputBinding",
    .name = "m_OverridePath"};

/// UnityEngine.Vector2, a type argument below. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Vector2{.key = "Vector2", .name = "Vector2", .assembly = "UnityEngine.CoreModule",
                                            .ns = "UnityEngine"};

// The name alone is shared with the non-generic InputControl, its base; only this one holds a value of its parameter.
inline constexpr const char* InputControlT_fieldTypes[] = {"TValue"};

/// The generic definition InputControl<TValue> (Unity.InputSystem), an input control of a value type. Inferred: the
/// Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputControlT{.key = "InputControl`1", .name = "InputControl",
                                                  .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem",
                                                  .fieldTypes = InputControlT_fieldTypes};

inline constexpr const char* InputControl_Vector2_ReadValue_args[] = {"@Vector2"};

/// The value of a Vector2 control, such as Pointer.get_delta and Pointer.get_position give, with its processors applied:
/// InputControl<Vector2>.ReadValue. Checked at startup only: the instance's code is the runtime's. Inferred: the Input
/// System's documented API.
inline constexpr pm::sdk::EntryDesc InputControl_Vector2_ReadValue{
    .key = "InputControl<Vector2>.ReadValue", .kind = pm::sdk::Kind::Inflated, .owner = "InputControl`1",
    .name = "ReadValue", .type = "TValue", .typeArgs = InputControl_Vector2_ReadValue_args};

/// The mouse (Unity.InputSystem). Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc Mouse{.key = "Mouse", .name = "Mouse", .assembly = "Unity.InputSystem",
                                          .ns = "UnityEngine.InputSystem"};

/// The mouse in use, or null when there is none. Static. Inferred: the Input System's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc Mouse_get_current{.key = "Mouse.get_current", .kind = pm::sdk::Kind::Method,
                                                      .flags = pm::sdk::kStatic, .owner = "Mouse",
                                                      .name = "get_current"};

/// The mouse's left button, a ButtonControl. Inferred: the Input System's documented property getter.
inline constexpr pm::sdk::EntryDesc Mouse_get_leftButton{.key = "Mouse.get_leftButton", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Mouse", .name = "get_leftButton"};

/// A pointer device (Unity.InputSystem), the mouse's base class. Inferred: the Input System's documented
/// API.
inline constexpr pm::sdk::EntryDesc Pointer{.key = "Pointer", .name = "Pointer", .assembly = "Unity.InputSystem",
                                            .ns = "UnityEngine.InputSystem"};

/// The pointer's movement since the last frame, a Vector2 control (a DeltaControl), in pixels. Inferred:
/// the Input System's documented property getter.
inline constexpr pm::sdk::EntryDesc Pointer_get_delta{.key = "Pointer.get_delta", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Pointer", .name = "get_delta"};

/// The pointer's position on the screen, a Vector2Control, in pixels from the bottom left. Inferred: the
/// Input System's documented property getter.
inline constexpr pm::sdk::EntryDesc Pointer_get_position{.key = "Pointer.get_position", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Pointer", .name = "get_position"};

/// A button control (Unity.InputSystem, namespace UnityEngine.InputSystem.Controls). Inferred: the Input
/// System's documented API.
inline constexpr pm::sdk::EntryDesc ButtonControl{.key = "ButtonControl", .name = "ButtonControl",
                                                  .assembly = "Unity.InputSystem",
                                                  .ns = "UnityEngine.InputSystem.Controls"};

/// Whether the button is held down now. Inferred: the Input System's documented property getter.
inline constexpr pm::sdk::EntryDesc ButtonControl_get_isPressed{.key = "ButtonControl.get_isPressed",
                                                                .kind = pm::sdk::Kind::Method, .owner = "ButtonControl",
                                                                .name = "get_isPressed"};

/// Whether the button went down this frame. Inferred: the Input System's documented property getter.
inline constexpr pm::sdk::EntryDesc ButtonControl_get_wasPressedThisFrame{
    .key = "ButtonControl.get_wasPressedThisFrame", .kind = pm::sdk::Kind::Method, .owner = "ButtonControl",
    .name = "get_wasPressedThisFrame"};

}  // namespace pm::map

/// @file
/// @brief The game API for input (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null, zero, false) without touching the
/// game.
namespace phasmo {
/// @addtogroup players
/// @{

/// Unity's cursor, a static class.
struct Cursor {
  /// get_lockState(): how the cursor is held now, a CursorLockMode value. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Cursor_get_lockState, int32_t()> get_lockState{};
  /// set_lockState(mode): holds the cursor so. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Cursor_set_lockState, void(int32_t)> set_lockState{};
  /// set_visible(on): shows or hides it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Cursor_set_visible, void(bool)> set_visible{};
};

/// How the cursor is held. Unresolved: 0.
namespace CursorLockMode {
inline constexpr pm::api::EnumValue<pm::map::CursorLockMode_None> None{};  ///< None(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::CursorLockMode_Locked> Locked{};  ///< Locked(): the value's number in this game build
}  // namespace CursorLockMode

/// A Vector2 control. `control` below is one (Pointer::get_delta, Pointer::get_position give one).
struct Vector2Control {
  /// ReadValue(control): its value now, processors applied. Unresolved: the zero vector.
  static constexpr pm::api::Method<pm::map::InputControl_Vector2_ReadValue, Vector2(void*)> ReadValue{};
};

/// An input action. `action` below is the InputAction.
struct InputAction {
  /// Its name, a string: m_Name.get(action). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputAction_m_Name, void*> m_Name{};
  /// Its id, a string: m_Id.get(action). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputAction_m_Id, void*> m_Id{};
  /// Its InputActionMap: m_ActionMap.get(action). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputAction_m_ActionMap, void*> m_ActionMap{};
  /// get_enabled(action): whether it is enabled. Unresolved: false.
  static constexpr pm::api::Method<pm::map::InputAction_get_enabled, bool(void*)> get_enabled{};
  /// Enable(action): enables it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::InputAction_Enable, void(void*)> Enable{};
  /// Disable(action): disables it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::InputAction_Disable, void(void*)> Disable{};
};

/// A group of input actions. `map` below is the InputActionMap.
struct InputActionMap {
  /// Its name, a string: m_Name.get(map). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputActionMap_m_Name, void*> m_Name{};
  /// Its actions, an InputAction[]: m_Actions.get(map). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputActionMap_m_Actions, void*> m_Actions{};
  /// Its actions' bindings, an InputBinding[] of structs (read with pm::api::ValueArray): m_Bindings.get(map).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputActionMap_m_Bindings, void*> m_Bindings{};
};

/// A set of action maps. `asset` below is the InputActionAsset.
struct InputActionAsset {
  /// Its action maps, an InputActionMap[]: m_ActionMaps.get(asset). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputActionAsset_m_ActionMaps, void*> m_ActionMaps{};
};

/// One binding, a struct. `binding` below is its address (an element of a ValueArray).
struct InputBinding {
  /// Its name, a composite's part, a string: m_Name.get(binding). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputBinding_m_Name, void*> m_Name{};
  /// The control's path, a string: m_Path.get(binding). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputBinding_m_Path, void*> m_Path{};
  /// Its action's name or id, a string: m_Action.get(binding). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputBinding_m_Action, void*> m_Action{};
  /// The player's rebind, a string path, or empty: m_OverridePath.get(binding). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputBinding_m_OverridePath, void*> m_OverridePath{};
};

/// The mouse. `mouse` below is the Mouse.
struct Mouse {
  /// get_current(): the mouse in use, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Mouse_get_current, void*()> get_current{};
  /// get_leftButton(mouse): its left button, a ButtonControl. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Mouse_get_leftButton, void*(void*)> get_leftButton{};
};

/// A pointer device, the mouse's base. `pointer` below is the Pointer.
struct Pointer {
  /// get_delta(pointer): its movement since the last frame, a Vector2 control. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Pointer_get_delta, void*(void*)> get_delta{};
  /// get_position(pointer): its screen position, a Vector2Control. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Pointer_get_position, void*(void*)> get_position{};
};

/// A button control. `button` below is the ButtonControl.
struct ButtonControl {
  /// get_isPressed(button): whether it is held down. Unresolved: false.
  static constexpr pm::api::Method<pm::map::ButtonControl_get_isPressed, bool(void*)> get_isPressed{};
  /// get_wasPressedThisFrame(button): whether it went down this frame. Unresolved: false.
  static constexpr pm::api::Method<pm::map::ButtonControl_get_wasPressedThisFrame, bool(void*)>
      get_wasPressedThisFrame{};
};

/// @}
}  // namespace phasmo
