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

/// The binding's id, a string (a Guid's text), which a keybinds row (RebindActionUI.m_BindingId) names. Inferred: the
/// Input System's own field.
inline constexpr pm::sdk::EntryDesc InputBinding_m_Id{.key = "InputBinding.m_Id", .kind = pm::sdk::Kind::Field,
                                                     .owner = "InputBinding", .name = "m_Id"};

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

// A mod's own input actions, and the game's keybinds menu (the Emotes mod's key; sdk/api/keybinds.h). The game's
// actions, their bindings and the menu are described in sdk/systems/menus.md, "Key bindings".

/// What an InputAction reads (Unity.InputSystem). Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionType{.key = "InputActionType", .name = "InputActionType",
                                                   .assembly = "Unity.InputSystem", .ns = "UnityEngine.InputSystem"};

/// A button: the action is pressed past its press point and released again. Inferred: the Input System's documented
/// API.
inline constexpr pm::sdk::EntryDesc InputActionType_Button{.key = "InputActionType.Button",
                                                          .kind = pm::sdk::Kind::EnumValue, .owner = "InputActionType",
                                                          .name = "Button"};

/// The Input System's helpers that build actions in code, a static class. Unity refuses an InputActionReference to an
/// action outside an InputActionAsset ("must be part of an InputActionAsset"), so a mod's action for a keybinds row
/// lives in an asset of its own (ScriptableObject.CreateInstance), a map and an action added by these. Inferred: the
/// Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionSetupExtensions{
    .key = "InputActionSetupExtensions", .name = "InputActionSetupExtensions", .assembly = "Unity.InputSystem",
    .ns = "UnityEngine.InputSystem"};

inline constexpr const char* InputActionSetupExtensions_AddActionMap_params[] = {"InputActionAsset", "string"};

/// AddActionMap(asset, name): a new map in the asset, returned. Static; another overload takes a map. Inferred: the
/// Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionSetupExtensions_AddActionMap{
    .key = "InputActionSetupExtensions.AddActionMap", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "InputActionSetupExtensions", .name = "AddActionMap", .type = "InputActionMap",
    .params = InputActionSetupExtensions_AddActionMap_params};

inline constexpr const char* InputActionSetupExtensions_AddAction_params[] = {
    "InputActionMap", "string", "InputActionType", "string", "string", "string", "string", "string"};

/// AddAction(map, name, type, binding, interactions, processors, groups, expectedControlLayout): a new action in the
/// map with one binding to the path given, returned; the strings after the binding may be null. Static. Inferred: the
/// Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionSetupExtensions_AddAction{
    .key = "InputActionSetupExtensions.AddAction", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "InputActionSetupExtensions", .name = "AddAction", .params = InputActionSetupExtensions_AddAction_params};

/// Whether the action was pressed in this frame. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction_WasPressedThisFrame{
    .key = "InputAction.WasPressedThisFrame", .kind = pm::sdk::Kind::Method, .owner = "InputAction",
    .name = "WasPressedThisFrame"};

/// Whether the action was released in this frame. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction_WasReleasedThisFrame{
    .key = "InputAction.WasReleasedThisFrame", .kind = pm::sdk::Kind::Method, .owner = "InputAction",
    .name = "WasReleasedThisFrame"};

/// Whether the action is held past its press point now. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputAction_IsPressed{.key = "InputAction.IsPressed", .kind = pm::sdk::Kind::Method,
                                                         .owner = "InputAction", .name = "IsPressed"};


/// The Input System's binding override helpers, a static class. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionRebindingExtensions{
    .key = "InputActionRebindingExtensions", .name = "InputActionRebindingExtensions", .assembly = "Unity.InputSystem",
    .ns = "UnityEngine.InputSystem"};

inline constexpr const char* InputActionRebindingExtensions_ApplyBindingOverride_params[] = {"InputAction", "string",
                                                                                             "string", "string"};

/// ApplyBindingOverride(action, newPath, group, path): overrides the action's bindings (those of `group`, or matching
/// `path`, or all when both are null) with `newPath`. Static. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionRebindingExtensions_ApplyBindingOverride{
    .key = "InputActionRebindingExtensions.ApplyBindingOverride", .kind = pm::sdk::Kind::Method,
    .flags = pm::sdk::kStatic, .owner = "InputActionRebindingExtensions", .name = "ApplyBindingOverride",
    .params = InputActionRebindingExtensions_ApplyBindingOverride_params};

inline constexpr const char* InputActionRebindingExtensions_RemoveAllBindingOverrides_params[] = {
    "IInputActionCollection2"};

/// RemoveAllBindingOverrides(actions): takes every override off the bindings of an action collection (an
/// InputActionAsset or map). Static. ControlsManager.ResetButton runs it on the game's controls. Decompiled.
inline constexpr pm::sdk::EntryDesc InputActionRebindingExtensions_RemoveAllBindingOverrides{
    .key = "InputActionRebindingExtensions.RemoveAllBindingOverrides", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic,
    .owner = "InputActionRebindingExtensions", .name = "RemoveAllBindingOverrides",
    .params = InputActionRebindingExtensions_RemoveAllBindingOverrides_params};

/// The keybinds menu's page (Assembly-CSharp, global namespace), a MonoBehaviour of each player's journal. Its buttons
/// decide when a rebinding counts: ApplyButton stores the game's overrides (PCControls.StoreControlOverrides),
/// ResetButton takes them all off without storing, and leaving the page (its OnDisable) loads the stored ones back
/// (PCControls.LoadControlOverrides), so a change not applied is dropped. Decompiled.
inline constexpr pm::sdk::EntryDesc ControlsManager{.key = "ControlsManager",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "ControlsManager", .assembly = "Assembly-CSharp", .ns = ""};

/// The page's Apply: stores the game's binding overrides. A mod hooks it to store its own. Decompiled.
inline constexpr pm::sdk::EntryDesc ControlsManager_ApplyButton{
    .key = "ControlsManager.ApplyButton", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ControlsManager", .name = "ApplyButton"};

/// The page's Reset: takes every override off the game's controls, unstored until Apply. Decompiled.
inline constexpr pm::sdk::EntryDesc ControlsManager_ResetButton{
    .key = "ControlsManager.ResetButton", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ControlsManager", .name = "ResetButton"};

/// The page leaving the screen: loads the stored overrides back, dropping what was not applied. Decompiled.
inline constexpr pm::sdk::EntryDesc ControlsManager_OnDisable{
    .key = "ControlsManager.OnDisable", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ControlsManager", .name = "OnDisable"};

// The journal's audio page. Read from the class dump, the PCPlayer prefab and decompiled code.

inline constexpr const char* Input_float[] = {"float"};

/// The game's slider (Assembly-CSharp, UnityEngine.UI): Unity's Slider as the game rewrote it for VR, a Selectable.
/// The audio page's master volume is one. Decompiled.
inline constexpr pm::sdk::EntryDesc XRSlider{.key = "XRSlider", .confidence = pm::sdk::Confidence::Decompiled,
                                             .name = "XRSlider", .assembly = "Assembly-CSharp", .ns = "UnityEngine.UI"};
/// Its value. Read from the class dump.
inline constexpr pm::sdk::EntryDesc XRSlider_get_value{.key = "XRSlider.get_value", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "XRSlider", .name = "get_value"};
/// Sets its value without raising its change event. Read from the class dump.
inline constexpr pm::sdk::EntryDesc XRSlider_SetValueWithoutNotify{
    .key = "XRSlider.SetValueWithoutNotify", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "XRSlider", .name = "SetValueWithoutNotify", .params = Input_float};
/// Sets the lowest value. Read from the class dump.
inline constexpr pm::sdk::EntryDesc XRSlider_set_minValue{.key = "XRSlider.set_minValue", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "XRSlider", .name = "set_minValue",
                                                          .params = Input_float};
/// Sets the highest value. Read from the class dump.
inline constexpr pm::sdk::EntryDesc XRSlider_set_maxValue{.key = "XRSlider.set_maxValue", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "XRSlider", .name = "set_maxValue",
                                                          .params = Input_float};
/// Its change event, a SliderEvent whose listeners the prefab wired: a copy of the slider keeps them, so a copy gets
/// an event of its own. Read from the class dump.
inline constexpr pm::sdk::EntryDesc XRSlider_m_OnValueChanged{
    .key = "XRSlider.m_OnValueChanged", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "XRSlider", .name = "m_OnValueChanged"};
/// The change event's type (a UnityEvent<float>), nested in XRSlider. Read from the class dump.
inline constexpr pm::sdk::EntryDesc SliderEvent{.key = "SliderEvent", .confidence = pm::sdk::Confidence::Decompiled,
                                                .owner = "XRSlider", .name = "SliderEvent",
                                                .assembly = "Assembly-CSharp"};
/// The event's constructor: no listeners. Read from the class dump.
inline constexpr pm::sdk::EntryDesc SliderEvent_ctor{.key = "SliderEvent.ctor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "SliderEvent", .name = ".ctor"};

/// The journal's audio page (Assembly-CSharp, global namespace), a MonoBehaviour of each player's journal. Its
/// ApplyButton reads the master volume slider into the audio settings and sets the mixer: a change counts on Apply.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GameMyAudioManager{.key = "GameMyAudioManager",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "GameMyAudioManager", .assembly = "Assembly-CSharp",
                                                       .ns = ""};
/// The master volume's slider, an XRSlider in the page's list (its row: a title, a number and the slider). Decompiled.
inline constexpr pm::sdk::EntryDesc GameMyAudioManager_masterVolumeSlider{
    .key = "GameMyAudioManager.masterVolumeSlider", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameMyAudioManager", .name = "masterVolumeSlider"};
/// The master volume's number, a TextMeshProUGUI in the slider's row. Decompiled.
inline constexpr pm::sdk::EntryDesc GameMyAudioManager_masterVolumeValueText{
    .key = "GameMyAudioManager.masterVolumeValueText", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameMyAudioManager", .name = "masterVolumeValueText"};
/// The page's Apply: stores the audio settings and sets the mixer. A mod hooks it to store its own. Decompiled.
inline constexpr pm::sdk::EntryDesc GameMyAudioManager_ApplyButton{
    .key = "GameMyAudioManager.ApplyButton", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GameMyAudioManager", .name = "ApplyButton"};
/// The page leaving the screen. A mod hooks it to put back what was not applied. Read from the class dump.
inline constexpr pm::sdk::EntryDesc GameMyAudioManager_OnDisable{
    .key = "GameMyAudioManager.OnDisable", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GameMyAudioManager", .name = "OnDisable"};

/// A reference to an action, as a ScriptableObject a component can hold. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionReference{.key = "InputActionReference", .name = "InputActionReference",
                                                        .assembly = "Unity.InputSystem",
                                                        .ns = "UnityEngine.InputSystem"};

inline constexpr const char* InputActionReference_InputAction[] = {"InputAction"};

/// A new reference to the action given. Static. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionReference_Create{
    .key = "InputActionReference.Create", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "InputActionReference", .name = "Create", .params = InputActionReference_InputAction};

/// The action a reference names, found in its asset. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc InputActionReference_get_action{.key = "InputActionReference.get_action",
                                                                   .kind = pm::sdk::Kind::Method,
                                                                   .owner = "InputActionReference",
                                                                   .name = "get_action"};

/// A game UI widget that shows the gamepad glyph of one input action (Assembly-CSharp, global namespace): the
/// journal's and pause screen's LB and RB page tabs are ones. It holds two GamepadIcons sets, an Xbox one and a PS5
/// one (sprites named for their buttons, such as leftShoulder and rightShoulder). When the player's controls change
/// (PlayerInput's controls-changed event) it shows, for a gamepad scheme, the sprite for the action's gamepad binding
/// from the PS5 set when the gamepad's name says it is a PlayStation pad or the game runs on a PS5, else from the
/// Xbox set, on its Image in full colour; for keyboard and mouse it sets that Image's colour to clear, so no glyph
/// shows. The game has no glyph for the mouse or keyboard. Decompiled.
inline constexpr pm::sdk::EntryDesc GamepadButton{.key = "GamepadButton", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .name = "GamepadButton", .assembly = "Assembly-CSharp", .ns = ""};

/// The action whose glyph it shows: GamepadButton's one InputActionReference field. Decompiled.
inline constexpr pm::sdk::EntryDesc GamepadButton_action{.key = "GamepadButton.action", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GamepadButton", .type = "InputActionReference"};

/// The Image the glyph shows on: GamepadButton's one Image field. Its sprite is the glyph the game picked for the
/// current pad; its colour is clear while the player uses keyboard and mouse. Decompiled.
inline constexpr pm::sdk::EntryDesc GamepadButton_image{.key = "GamepadButton.image", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GamepadButton", .type = "Image"};

/// One row of the game's keybinds menu (Assembly-CSharp, global namespace): Unity's rebinding sample. Its button starts
/// an interactive rebind of the action it references, and it shows the binding's current key. Its fields keep their
/// real names. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RebindActionUI{.key = "RebindActionUI", .name = "RebindActionUI",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// The row's action, an InputActionReference. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RebindActionUI_m_Action{.key = "RebindActionUI.m_Action",
                                                           .kind = pm::sdk::Kind::Field, .owner = "RebindActionUI",
                                                           .name = "m_Action"};

/// The id of the binding the row rebinds, a string (a Guid's text). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RebindActionUI_m_BindingId{.key = "RebindActionUI.m_BindingId",
                                                              .kind = pm::sdk::Kind::Field, .owner = "RebindActionUI",
                                                              .name = "m_BindingId"};

/// The row's label, a TMP_Text, which shows the action's name. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RebindActionUI_m_ActionLabel{.key = "RebindActionUI.m_ActionLabel",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .owner = "RebindActionUI", .name = "m_ActionLabel"};

inline constexpr const char* RebindActionUI_InputActionReference[] = {"InputActionReference"};
inline constexpr const char* RebindActionUI_string[] = {"string"};

/// Sets the row's action and shows its name and current binding (the sample's setter: it updates the label and the
/// binding text). Inferred: from the class dump and Unity's rebinding sample.
inline constexpr pm::sdk::EntryDesc RebindActionUI_set_actionReference{
    .key = "RebindActionUI.set_actionReference", .kind = pm::sdk::Kind::Method, .owner = "RebindActionUI",
    .name = "set_actionReference", .params = RebindActionUI_InputActionReference};

/// Sets the id of the binding the row rebinds, and shows it. Inferred: from the class dump and Unity's rebinding
/// sample.
inline constexpr pm::sdk::EntryDesc RebindActionUI_set_bindingId{.key = "RebindActionUI.set_bindingId",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .owner = "RebindActionUI", .name = "set_bindingId",
                                                                 .params = RebindActionUI_string};

/// A gamepad (Unity.InputSystem). Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc Gamepad{.key = "Gamepad", .name = "Gamepad", .assembly = "Unity.InputSystem",
                                           .ns = "UnityEngine.InputSystem"};

/// The gamepad last used, or null when there is none. Static. Inferred: the Input System's documented API.
inline constexpr pm::sdk::EntryDesc Gamepad_get_current{.key = "Gamepad.get_current", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "Gamepad",
                                                       .name = "get_current"};

/// The right stick, a StickControl, read with InputControl<Vector2>.ReadValue. Inferred: the Input System's documented
/// API.
inline constexpr pm::sdk::EntryDesc Gamepad_get_rightStick{.key = "Gamepad.get_rightStick",
                                                          .kind = pm::sdk::Kind::Method, .owner = "Gamepad",
                                                          .name = "get_rightStick"};

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
  /// WasPressedThisFrame(action). Unresolved: false.
  static constexpr pm::api::Method<pm::map::InputAction_WasPressedThisFrame, bool(void*)> WasPressedThisFrame{};
  /// WasReleasedThisFrame(action). Unresolved: false.
  static constexpr pm::api::Method<pm::map::InputAction_WasReleasedThisFrame, bool(void*)> WasReleasedThisFrame{};
  /// IsPressed(action). Unresolved: false.
  static constexpr pm::api::Method<pm::map::InputAction_IsPressed, bool(void*)> IsPressed{};
};

/// What an action reads.
namespace InputActionType {
/// Button(): the value's number in this game build. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::InputActionType_Button> Button{};
}  // namespace InputActionType

/// The Input System's binding overrides.
struct InputActionRebindingExtensions {
  /// ApplyBindingOverride(action, newPath, group, path). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::InputActionRebindingExtensions_ApplyBindingOverride,
                                   void(void*, void*, void*, void*)>
      ApplyBindingOverride{};
  /// RemoveAllBindingOverrides(actions): every override off an InputActionAsset's or map's bindings. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::InputActionRebindingExtensions_RemoveAllBindingOverrides, void(void*)>
      RemoveAllBindingOverrides{};
};

/// The game's slider. `slider` below is the XRSlider.
struct XRSlider {
  /// The type: type_object() for Component.GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::XRSlider> type{};
  /// get_value(slider). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::XRSlider_get_value, float(void*)> get_value{};
  /// SetValueWithoutNotify(slider, value). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::XRSlider_SetValueWithoutNotify, void(void*, float)> SetValueWithoutNotify{};
  /// set_minValue(slider, value). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::XRSlider_set_minValue, void(void*, float)> set_minValue{};
  /// set_maxValue(slider, value). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::XRSlider_set_maxValue, void(void*, float)> set_maxValue{};
  /// Its change event, a SliderEvent: m_OnValueChanged.get(slider). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::XRSlider_m_OnValueChanged, void*> m_OnValueChanged{};
};

/// The slider's change event type.
struct SliderEvent {
  /// The type: make() and is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::SliderEvent> type{};
  /// ctor(event): runs the constructor of an event made with type.make(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::SliderEvent_ctor, void(void*)> ctor{};
};

/// The journal's audio page. `page` below is the GameMyAudioManager.
struct GameMyAudioManager {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::GameMyAudioManager> type{};
  /// The master volume's XRSlider: masterVolumeSlider.get(page). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GameMyAudioManager_masterVolumeSlider, void*> masterVolumeSlider{};
  /// The master volume's number text: masterVolumeValueText.get(page). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GameMyAudioManager_masterVolumeValueText, void*> masterVolumeValueText{};
  /// ApplyButton(page): stores the audio settings. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameMyAudioManager_ApplyButton, void(void*)> ApplyButton{};
  /// OnDisable(page): the page leaving the screen. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameMyAudioManager_OnDisable, void(void*)> OnDisable{};
};

/// The keybinds menu's page. `page` below is the ControlsManager.
struct ControlsManager {
  /// ApplyButton(page): stores the game's overrides. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ControlsManager_ApplyButton, void(void*)> ApplyButton{};
  /// ResetButton(page): every override off the game's controls, unstored. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ControlsManager_ResetButton, void(void*)> ResetButton{};
  /// OnDisable(page): the stored overrides loaded back. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ControlsManager_OnDisable, void(void*)> OnDisable{};
};

/// Building a mod's own actions.
struct InputActionSetupExtensions {
  /// AddActionMap(asset, name): the new InputActionMap. Unresolved: null.
  static constexpr pm::api::Method<pm::map::InputActionSetupExtensions_AddActionMap, void*(void*, void*)> AddActionMap{};
  /// AddAction(map, name, type, binding, interactions, processors, groups, layout): the new InputAction. Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::InputActionSetupExtensions_AddAction,
                                   void*(void*, void*, int32_t, void*, void*, void*, void*, void*)>
      AddAction{};
};

/// A reference to an action.
struct InputActionReference {
  /// Create(action): a new reference to it. Unresolved: null.
  static constexpr pm::api::Method<pm::map::InputActionReference_Create, void*(void*)> Create{};
  /// get_action(reference): the InputAction, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::InputActionReference_get_action, void*(void*)> get_action{};
};

/// A widget showing one action's gamepad glyph. `button` below is the GamepadButton.
struct GamepadButton {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::GamepadButton> type{};
  /// The action whose glyph it shows, an InputActionReference: action.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GamepadButton_action, void*> action{};
  /// The Image it shows the glyph on: image.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GamepadButton_image, void*> image{};
};

/// A row of the game's keybinds menu.
struct RebindActionUI {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::RebindActionUI> type{};
  /// Its InputActionReference: m_Action.get(row). Unresolved: null.
  static constexpr pm::api::Field<pm::map::RebindActionUI_m_Action, void*> m_Action{};
  /// The binding's id, a string: m_BindingId.get(row). Unresolved: null.
  static constexpr pm::api::Field<pm::map::RebindActionUI_m_BindingId, void*> m_BindingId{};
  /// Its label, a TMP_Text: m_ActionLabel.get(row). Unresolved: null.
  static constexpr pm::api::Field<pm::map::RebindActionUI_m_ActionLabel, void*> m_ActionLabel{};
  /// set_actionReference(row, reference). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RebindActionUI_set_actionReference, void(void*, void*)>
      set_actionReference{};
  /// set_bindingId(row, id): id a string. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RebindActionUI_set_bindingId, void(void*, void*)> set_bindingId{};
};

/// A gamepad.
struct Gamepad {
  /// get_current(): the gamepad last used, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Gamepad_get_current, void*()> get_current{};
  /// get_rightStick(pad): its right stick, a StickControl (read with Vector2Control::ReadValue). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Gamepad_get_rightStick, void*(void*)> get_rightStick{};
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
  /// The type: type_object() for ScriptableObject::CreateInstance. Unresolved: null.
  static constexpr pm::api::Type<pm::map::InputActionAsset> type{};
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
  /// Its id, a string: m_Id.get(binding). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InputBinding_m_Id, void*> m_Id{};
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
