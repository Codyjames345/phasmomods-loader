#pragma once

// Unity's UI package (UnityEngine.UI): the grid layout, the button and the controls' bases the journal's
// widgets use. Not obfuscated, so every entry is by real name. The host's journal
// (host/content_journal.h) lays out the ghost grids and follows clicks on its list entries with them.
// Like every phasmo:: wrapper, these reach every mod.

#include "Transform.h"
#include "UnityRendering.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* UnityUI_Vector2[] = {"Vector2"};
inline constexpr const char* UnityUI_int[] = {"int"};
inline constexpr const char* UnityUI_float[] = {"float"};

/// Unity's grid layout (UnityEngine.UI), a Behaviour: it places its active children in cells of one
/// size, row by row, in their sibling order. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup{.key = "GridLayoutGroup", .name = "GridLayoutGroup",
                                                    .assembly = "UnityEngine.UI", .ns = "UnityEngine.UI"};

/// The cell size, two floats. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_get_cellSize{
    .key = "GridLayoutGroup.get_cellSize", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "get_cellSize"};

/// Sets the cell size. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_set_cellSize{
    .key = "GridLayoutGroup.set_cellSize", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "set_cellSize", .params = UnityUI_Vector2};

/// The space between cells, two floats. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_get_spacing{
    .key = "GridLayoutGroup.get_spacing", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "get_spacing"};

/// How the grid's columns are counted, a GridConstraint. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_get_constraint{
    .key = "GridLayoutGroup.get_constraint", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "get_constraint"};

/// Sets how the grid's columns are counted, a GridConstraint. Matched by name alone, its one overload: the model names
/// its parameter's type only "Constraint", which Final IK's own Constraint class shares. Inferred: Unity's documented
/// property setter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_set_constraint{
    .key = "GridLayoutGroup.set_constraint", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "set_constraint"};

/// The fixed number of columns (or rows) the constraint keeps. Inferred: Unity's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_get_constraintCount{
    .key = "GridLayoutGroup.get_constraintCount", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "get_constraintCount"};

/// Sets the fixed number of columns (or rows). Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc GridLayoutGroup_set_constraintCount{
    .key = "GridLayoutGroup.set_constraintCount", .kind = pm::sdk::Kind::Method, .owner = "GridLayoutGroup",
    .name = "set_constraintCount", .params = UnityUI_int};

/// How a grid counts its columns, an enum nested in GridLayoutGroup. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GridConstraint{.key = "GridConstraint", .owner = "GridLayoutGroup",
                                                   .name = "Constraint"};

/// A fixed number of columns. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GridConstraint_FixedColumnCount{.key = "GridConstraint.FixedColumnCount",
                                                                    .kind = pm::sdk::Kind::EnumValue,
                                                                    .owner = "GridConstraint",
                                                                    .name = "FixedColumnCount"};

/// Unity's base of the vertical and horizontal layouts (UnityEngine.UI), a Behaviour: it places its
/// active children in a column or a row, `spacing` apart. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc HorizontalOrVerticalLayoutGroup{.key = "HorizontalOrVerticalLayoutGroup",
                                                                    .name = "HorizontalOrVerticalLayoutGroup",
                                                                    .assembly = "UnityEngine.UI",
                                                                    .ns = "UnityEngine.UI"};

/// The space between children, a float. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc HorizontalOrVerticalLayoutGroup_get_spacing{
    .key = "HorizontalOrVerticalLayoutGroup.get_spacing", .kind = pm::sdk::Kind::Method,
    .owner = "HorizontalOrVerticalLayoutGroup", .name = "get_spacing"};

/// Sets the space between children. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc HorizontalOrVerticalLayoutGroup_set_spacing{
    .key = "HorizontalOrVerticalLayoutGroup.set_spacing", .kind = pm::sdk::Kind::Method,
    .owner = "HorizontalOrVerticalLayoutGroup", .name = "set_spacing", .params = UnityUI_float};

/// Unity's button (UnityEngine.UI), a Selectable. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Button{.key = "Button", .name = "Button", .assembly = "UnityEngine.UI",
                                           .ns = "UnityEngine.UI"};

/// Runs the button's click: while the button is active (IsActive) and interactable (IsInteractable),
/// both asked as virtual calls, it invokes onClick. OnPointerClick checks the event is the left
/// button's and jumps here; OnSubmit calls it, then, still active and interactable, shows the
/// pressed state. Private. Decompiled.
inline constexpr pm::sdk::EntryDesc Button_Press{.key = "Button.Press", .kind = pm::sdk::Kind::Method,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "Button",
                                                 .name = "Press"};

/// The button's click event, a ButtonClickedEvent (a UnityEvent): what Button.Press invokes. The main menu's
/// Multiplayer tape is a Button whose event has one persistent listener, saved with the scene. Inferred: Unity's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc Button_get_onClick{.key = "Button.get_onClick", .kind = pm::sdk::Kind::Method,
                                                       .owner = "Button", .name = "get_onClick"};

/// Unity's base of the UI's components (UnityEngine.UI, namespace UnityEngine.EventSystems). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc UIBehaviour{.key = "UIBehaviour", .name = "UIBehaviour",
                                                .assembly = "UnityEngine.UI", .ns = "UnityEngine.EventSystems"};

/// Whether the component is active and enabled, virtual: Button.Press asks it first. Decompiled.
inline constexpr pm::sdk::EntryDesc UIBehaviour_IsActive{.key = "UIBehaviour.IsActive", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "UIBehaviour", .name = "IsActive"};

/// Unity UI's image (UnityEngine.UI), a Graphic. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Image{.key = "Image", .name = "Image", .assembly = "UnityEngine.UI",
                                          .ns = "UnityEngine.UI"};

/// Unity's canvas component (UnityEngine.UIModule), the root a UI is drawn in. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Canvas{.key = "Canvas", .name = "Canvas", .assembly = "UnityEngine.UIModule",
                                           .ns = "UnityEngine"};

/// The canvas's scale: screen pixels per canvas unit. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Canvas_get_scaleFactor{.key = "Canvas.get_scaleFactor",
                                                           .kind = pm::sdk::Kind::Method, .owner = "Canvas",
                                                           .name = "get_scaleFactor"};

/// The camera a canvas that is not drawn over the screen is seen through. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Canvas_get_worldCamera{.key = "Canvas.get_worldCamera", .kind = pm::sdk::Kind::Method,
                                                           .owner = "Canvas", .name = "get_worldCamera"};

/// Unity's base of the controls (UnityEngine.UI), a UIBehaviour. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Selectable{.key = "Selectable", .name = "Selectable", .assembly = "UnityEngine.UI",
                                               .ns = "UnityEngine.UI"};

/// Whether the control takes input, virtual: Button.Press asks it after IsActive. Decompiled.
inline constexpr pm::sdk::EntryDesc Selectable_IsInteractable{
    .key = "Selectable.IsInteractable", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Selectable", .name = "IsInteractable"};

/// Whether the control can be used, as its inspector sets it. Optional: only diagnostics read it.
/// Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Selectable_get_interactable{
    .key = "Selectable.get_interactable", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
    .owner = "Selectable", .name = "get_interactable"};

inline constexpr const char* UnityUI_bool[] = {"bool"};

/// Sets whether the control can be used. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Selectable_set_interactable{
    .key = "Selectable.set_interactable", .kind = pm::sdk::Kind::Method, .owner = "Selectable",
    .name = "set_interactable", .params = UnityUI_bool};

/// The Graphic the control tints as its state changes. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Selectable_get_targetGraphic{
    .key = "Selectable.get_targetGraphic", .kind = pm::sdk::Kind::Method, .owner = "Selectable",
    .name = "get_targetGraphic"};

/// Sets the Graphic the control tints. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Selectable_set_targetGraphic{
    .key = "Selectable.set_targetGraphic", .kind = pm::sdk::Kind::Method, .owner = "Selectable",
    .name = "set_targetGraphic"};

/// Unity's toggle (UnityEngine.UI), a Selectable. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Toggle{.key = "Toggle", .name = "Toggle", .assembly = "UnityEngine.UI",
                                           .ns = "UnityEngine.UI"};

/// The Graphic the toggle shows while it is on. Inferred: Unity's documented field.
inline constexpr pm::sdk::EntryDesc Toggle_graphic{.key = "Toggle.graphic", .kind = pm::sdk::Kind::Field,
                                                   .owner = "Toggle", .name = "graphic"};

/// The ToggleGroup the toggle belongs to, or null: the field behind the group property, which registers
/// with the group when set. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc Toggle_m_Group{.key = "Toggle.m_Group", .kind = pm::sdk::Kind::Field,
                                                   .owner = "Toggle", .name = "m_Group"};

/// The toggle's change event, a ToggleEvent its listeners are on. Inferred: Unity's documented field.
inline constexpr pm::sdk::EntryDesc Toggle_onValueChanged{.key = "Toggle.onValueChanged",
                                                          .kind = pm::sdk::Kind::Field, .owner = "Toggle",
                                                          .name = "onValueChanged"};

/// Whether the toggle is on. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Toggle_get_isOn{.key = "Toggle.get_isOn", .kind = pm::sdk::Kind::Method,
                                                    .owner = "Toggle", .name = "get_isOn"};

/// Sets the toggle on or off without raising its change event. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Toggle_SetIsOnWithoutNotify{
    .key = "Toggle.SetIsOnWithoutNotify", .kind = pm::sdk::Kind::Method, .owner = "Toggle",
    .name = "SetIsOnWithoutNotify", .params = UnityUI_bool};

/// The toggle's change event type (a UnityEvent<bool>), nested in Toggle. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc ToggleEvent{.key = "ToggleEvent", .owner = "Toggle", .name = "ToggleEvent",
                                                .assembly = "UnityEngine.UI"};

/// The event's constructor: no listeners. Optional: the Mods panel's rows (menu_panel::can_copy_rows) and the Playable
/// Ghost mod's lobby tape use it, each checking it with pm::api::all_resolved; without it there is no Mods button, and
/// mods are chosen in the launcher only. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc ToggleEvent_ctor{.key = "ToggleEvent.ctor", .kind = pm::sdk::Kind::Method,
                                                     .flags = pm::sdk::kOptional, .owner = "ToggleEvent",
                                                     .name = ".ctor"};

inline constexpr const char* UnityUI_PointerEventData[] = {"PointerEventData"};

/// Moves the control into its highlighted state, as the pointer entering it does. The event data is not
/// read. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Selectable_OnPointerEnter{
    .key = "Selectable.OnPointerEnter", .kind = pm::sdk::Kind::Method, .owner = "Selectable",
    .name = "OnPointerEnter", .params = UnityUI_PointerEventData};

/// Moves the control out of its highlighted state, as the pointer leaving it does. The event data is
/// not read. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Selectable_OnPointerExit{
    .key = "Selectable.OnPointerExit", .kind = pm::sdk::Kind::Method, .owner = "Selectable",
    .name = "OnPointerExit", .params = UnityUI_PointerEventData};

/// Unity's UI event system (UnityEngine.UI, namespace UnityEngine.EventSystems): it sends input to the
/// UI through its current input module. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc EventSystem{.key = "EventSystem", .name = "EventSystem",
                                                .assembly = "UnityEngine.UI", .ns = "UnityEngine.EventSystems"};

/// The event system in use, or null. Static. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc EventSystem_get_current{.key = "EventSystem.get_current",
                                                            .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
                                                            .owner = "EventSystem", .name = "get_current"};

/// The input module the event system runs, a BaseInputModule (a Behaviour), or null. The game switches
/// it on while a player's journal is open. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc EventSystem_currentInputModule{
    .key = "EventSystem.currentInputModule", .kind = pm::sdk::Kind::Field, .owner = "EventSystem",
    .name = "m_CurrentInputModule"};

/// A group of UI objects that can stop taking clicks together (UnityEngine.UIModule, namespace
/// UnityEngine), a Behaviour. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CanvasGroup{.key = "CanvasGroup", .name = "CanvasGroup",
                                                .assembly = "UnityEngine.UIModule", .ns = "UnityEngine"};

/// Whether the group's objects take the pointer's clicks. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CanvasGroup_set_blocksRaycasts{
    .key = "CanvasGroup.set_blocksRaycasts", .kind = pm::sdk::Kind::Method, .owner = "CanvasGroup",
    .name = "set_blocksRaycasts", .params = UnityUI_bool};

/// TextMesh Pro's UI text (Unity.TextMeshPro, namespace TMPro): every text on the journal's pages.
/// Inferred: TextMesh Pro's documented API.
inline constexpr pm::sdk::EntryDesc TextMeshProUGUI{.key = "TextMeshProUGUI", .name = "TextMeshProUGUI",
                                                    .assembly = "Unity.TextMeshPro", .ns = "TMPro"};

/// How a Canvas is drawn (UnityEngine.UIModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderMode{.key = "RenderMode", .name = "RenderMode",
                                               .assembly = "UnityEngine.UIModule", .ns = "UnityEngine"};

/// A canvas drawn over the screen, after every camera. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RenderMode_ScreenSpaceOverlay{
    .key = "RenderMode.ScreenSpaceOverlay", .kind = pm::sdk::Kind::EnumValue, .owner = "RenderMode",
    .name = "ScreenSpaceOverlay"};

/// Sets how the canvas is drawn, a RenderMode. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Canvas_set_renderMode{.key = "Canvas.set_renderMode", .kind = pm::sdk::Kind::Method,
                                                          .owner = "Canvas", .name = "set_renderMode"};

/// Sets the canvas's order among the screen's canvases: a higher order draws on top. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc Canvas_set_sortingOrder{.key = "Canvas.set_sortingOrder",
                                                            .kind = pm::sdk::Kind::Method, .owner = "Canvas",
                                                            .name = "set_sortingOrder"};

/// Scales a canvas to the screen (UnityEngine.UI, namespace UnityEngine.UI). Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc CanvasScaler{.key = "CanvasScaler", .name = "CanvasScaler",
                                                 .assembly = "UnityEngine.UI", .ns = "UnityEngine.UI"};

/// How a CanvasScaler scales, an enum nested in it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CanvasScaler_ScaleMode{.key = "CanvasScaler.ScaleMode", .owner = "CanvasScaler",
                                                           .name = "ScaleMode"};

/// The canvas scales with the screen's size against a reference resolution. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc CanvasScaler_ScaleWithScreenSize{
    .key = "CanvasScaler.ScaleMode.ScaleWithScreenSize", .kind = pm::sdk::Kind::EnumValue,
    .owner = "CanvasScaler.ScaleMode", .name = "ScaleWithScreenSize"};

/// Sets how the scaler scales, a CanvasScaler.ScaleMode. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CanvasScaler_set_uiScaleMode{.key = "CanvasScaler.set_uiScaleMode",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .owner = "CanvasScaler", .name = "set_uiScaleMode"};

/// Sets the resolution the canvas is laid out for, a Vector2. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc CanvasScaler_set_referenceResolution{
    .key = "CanvasScaler.set_referenceResolution", .kind = pm::sdk::Kind::Method, .owner = "CanvasScaler",
    .name = "set_referenceResolution"};

/// Sets how much the width (0) or the height (1) decides the scale. Inferred: Unity's documented property
/// setter.
inline constexpr pm::sdk::EntryDesc CanvasScaler_set_matchWidthOrHeight{
    .key = "CanvasScaler.set_matchWidthOrHeight", .kind = pm::sdk::Kind::Method, .owner = "CanvasScaler",
    .name = "set_matchWidthOrHeight"};

/// Lets a canvas catch the pointer, so its graphics that catch it keep clicks from the UI beneath
/// (UnityEngine.UI). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GraphicRaycaster{.key = "GraphicRaycaster", .name = "GraphicRaycaster",
                                                     .assembly = "UnityEngine.UI", .ns = "UnityEngine.UI"};

/// Unity UI's base of everything drawn (UnityEngine.UI). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Graphic{.key = "Graphic", .name = "Graphic", .assembly = "UnityEngine.UI",
                                            .ns = "UnityEngine.UI"};

inline constexpr const char* Graphic_Color[] = {"Color"};
inline constexpr const char* Graphic_bool[] = {"bool"};

/// The graphic's colour. Virtual: TextMeshPro keeps its own (TMP_Text overrides it), so call it as C#
/// does (virtual_call). Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Graphic_get_color{.key = "Graphic.get_color", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Graphic", .name = "get_color"};

/// Sets the graphic's colour. Virtual, as get_color. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Graphic_set_color{.key = "Graphic.set_color", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Graphic", .name = "set_color", .params = Graphic_Color};

/// Sets whether the graphic catches the pointer. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Graphic_set_raycastTarget{.key = "Graphic.set_raycastTarget",
                                                              .kind = pm::sdk::Kind::Method, .owner = "Graphic",
                                                              .name = "set_raycastTarget", .params = Graphic_bool};

/// The CanvasRenderer that draws the graphic. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Graphic_get_canvasRenderer{.key = "Graphic.get_canvasRenderer",
                                                               .kind = pm::sdk::Kind::Method, .owner = "Graphic",
                                                               .name = "get_canvasRenderer"};

/// What draws a Graphic on its canvas (UnityEngine.UIModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CanvasRenderer{.key = "CanvasRenderer", .name = "CanvasRenderer",
                                                   .assembly = "UnityEngine.UIModule", .ns = "UnityEngine"};

/// The colour it multiplies the graphic by; a Toggle fades a graphic through it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CanvasRenderer_GetColor{.key = "CanvasRenderer.GetColor",
                                                            .kind = pm::sdk::Kind::Method, .owner = "CanvasRenderer",
                                                            .name = "GetColor"};

/// Sets that colour. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc CanvasRenderer_SetColor{.key = "CanvasRenderer.SetColor",
                                                            .kind = pm::sdk::Kind::Method, .owner = "CanvasRenderer",
                                                            .name = "SetColor", .params = Graphic_Color};

/// A graphic that shows a Texture (UnityEngine.UI); without one, a solid quad of its colour. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc RawImage{.key = "RawImage", .name = "RawImage", .assembly = "UnityEngine.UI",
                                             .ns = "UnityEngine.UI"};

/// Sets the texture it shows, or null. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc RawImage_set_texture{.key = "RawImage.set_texture", .kind = pm::sdk::Kind::Method,
                                                         .owner = "RawImage", .name = "set_texture"};

/// Sets the part of the texture it shows, a Rect in 0..1. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc RawImage_set_uvRect{.key = "RawImage.set_uvRect", .kind = pm::sdk::Kind::Method,
                                                        .owner = "RawImage", .name = "set_uvRect"};

/// Unity UI's hit tests in screen space (UnityEngine.UIModule), a static class. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc RectTransformUtility{.key = "RectTransformUtility", .name = "RectTransformUtility",
                                                         .assembly = "UnityEngine.UIModule", .ns = "UnityEngine"};

inline constexpr const char* RectTransformUtility_Contains_params[] = {"RectTransform", "Vector2", "Camera"};

/// Whether a screen point falls inside a RectTransform, as the camera that renders it sees it (null for a
/// canvas drawn over the screen). Static; another overload takes an offset too. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc RectTransformUtility_RectangleContainsScreenPoint{
    .key = "RectTransformUtility.RectangleContainsScreenPoint", .kind = pm::sdk::Kind::Method,
    .flags = pm::sdk::kStatic, .owner = "RectTransformUtility", .name = "RectangleContainsScreenPoint",
    .params = RectTransformUtility_Contains_params};

inline constexpr const char* RectTransformUtility_ToLocal_params[] = {"RectTransform", "Vector2", "Camera",
                                                                      "out Vector2"};

/// Where a screen point falls in a RectTransform's own space, into the Vector2 given; false when the
/// point's ray misses its plane. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc RectTransformUtility_ScreenPointToLocalPointInRectangle{
    .key = "RectTransformUtility.ScreenPointToLocalPointInRectangle", .kind = pm::sdk::Kind::Method,
    .flags = pm::sdk::kStatic, .owner = "RectTransformUtility", .name = "ScreenPointToLocalPointInRectangle",
    .params = RectTransformUtility_ToLocal_params};

/// TextMeshPro's base text (Unity.TextMeshPro, namespace TMPro). Inferred: TextMeshPro's documented API.
inline constexpr pm::sdk::EntryDesc TMP_Text{.key = "TMP_Text", .name = "TMP_Text", .assembly = "Unity.TextMeshPro",
                                             .ns = "TMPro"};

/// Sets its text, TextMeshPro rich text. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_text{.key = "TMP_Text.set_text", .kind = pm::sdk::Kind::Method,
                                                      .owner = "TMP_Text", .name = "set_text"};

/// Its text as last set, TextMeshPro rich text. Inferred: TextMeshPro's documented property getter.
inline constexpr pm::sdk::EntryDesc TMP_Text_get_text{.key = "TMP_Text.get_text", .kind = pm::sdk::Kind::Method,
                                                      .owner = "TMP_Text", .name = "get_text"};

/// Its font size. Inferred: TextMeshPro's documented property getter.
inline constexpr pm::sdk::EntryDesc TMP_Text_get_fontSize{.key = "TMP_Text.get_fontSize", .kind = pm::sdk::Kind::Method,
                                                          .owner = "TMP_Text", .name = "get_fontSize"};

/// Sets its font size. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_fontSize{.key = "TMP_Text.set_fontSize", .kind = pm::sdk::Kind::Method,
                                                          .owner = "TMP_Text", .name = "set_fontSize"};

/// The largest size auto-sizing may use, as configured. Inferred: TextMeshPro's documented property getter.
inline constexpr pm::sdk::EntryDesc TMP_Text_get_fontSizeMax{.key = "TMP_Text.get_fontSizeMax",
                                                             .kind = pm::sdk::Kind::Method, .owner = "TMP_Text",
                                                             .name = "get_fontSizeMax"};

/// Sets the largest size auto-sizing may use. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_fontSizeMax{.key = "TMP_Text.set_fontSizeMax",
                                                             .kind = pm::sdk::Kind::Method, .owner = "TMP_Text",
                                                             .name = "set_fontSizeMax"};

/// Sets the smallest size auto-sizing may use. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_fontSizeMin{.key = "TMP_Text.set_fontSizeMin",
                                                             .kind = pm::sdk::Kind::Method, .owner = "TMP_Text",
                                                             .name = "set_fontSizeMin"};

/// Sets whether the text shrinks until it fits its rectangle. Inferred: TextMeshPro's documented property
/// setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_enableAutoSizing{.key = "TMP_Text.set_enableAutoSizing",
                                                                  .kind = pm::sdk::Kind::Method, .owner = "TMP_Text",
                                                                  .name = "set_enableAutoSizing"};

/// Sets its alignment, a TextAlignmentOptions. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_alignment{.key = "TMP_Text.set_alignment",
                                                           .kind = pm::sdk::Kind::Method, .owner = "TMP_Text",
                                                           .name = "set_alignment"};

/// Sets whether its lines wrap at its rectangle's edge. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_enableWordWrapping{.key = "TMP_Text.set_enableWordWrapping",
                                                                    .kind = pm::sdk::Kind::Method,
                                                                    .owner = "TMP_Text",
                                                                    .name = "set_enableWordWrapping"};

/// Its font, a TMP_FontAsset. Inferred: TextMeshPro's documented property getter.
inline constexpr pm::sdk::EntryDesc TMP_Text_get_font{.key = "TMP_Text.get_font", .kind = pm::sdk::Kind::Method,
                                                      .owner = "TMP_Text", .name = "get_font"};

/// Sets its font. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_font{.key = "TMP_Text.set_font", .kind = pm::sdk::Kind::Method,
                                                      .owner = "TMP_Text", .name = "set_font"};

/// The material its font is drawn with, shared with the texts of that font. Inferred: TextMeshPro's
/// documented property getter.
inline constexpr pm::sdk::EntryDesc TMP_Text_get_fontSharedMaterial{.key = "TMP_Text.get_fontSharedMaterial",
                                                                    .kind = pm::sdk::Kind::Method,
                                                                    .owner = "TMP_Text",
                                                                    .name = "get_fontSharedMaterial"};

/// Sets the material its font is drawn with. Inferred: TextMeshPro's documented property setter.
inline constexpr pm::sdk::EntryDesc TMP_Text_set_fontSharedMaterial{.key = "TMP_Text.set_fontSharedMaterial",
                                                                    .kind = pm::sdk::Kind::Method,
                                                                    .owner = "TMP_Text",
                                                                    .name = "set_fontSharedMaterial"};

/// How a TextMeshPro text is aligned (Unity.TextMeshPro, namespace TMPro). Inferred: TextMeshPro's
/// documented API.
inline constexpr pm::sdk::EntryDesc TextAlignmentOptions{.key = "TextAlignmentOptions", .name = "TextAlignmentOptions",
                                                         .assembly = "Unity.TextMeshPro", .ns = "TMPro"};

/// Top left. Inferred: TextMeshPro's documented API.
inline constexpr pm::sdk::EntryDesc TextAlignmentOptions_TopLeft{
    .key = "TextAlignmentOptions.TopLeft", .kind = pm::sdk::Kind::EnumValue, .owner = "TextAlignmentOptions",
    .name = "TopLeft"};

/// Top, centred across. Inferred: TextMeshPro's documented API.
inline constexpr pm::sdk::EntryDesc TextAlignmentOptions_Top{.key = "TextAlignmentOptions.Top",
                                                             .kind = pm::sdk::Kind::EnumValue,
                                                             .owner = "TextAlignmentOptions", .name = "Top"};

/// Middle left. Inferred: TextMeshPro's documented API.
inline constexpr pm::sdk::EntryDesc TextAlignmentOptions_Left{.key = "TextAlignmentOptions.Left",
                                                              .kind = pm::sdk::Kind::EnumValue,
                                                              .owner = "TextAlignmentOptions", .name = "Left"};

/// Middle, centred across. Inferred: TextMeshPro's documented API.
inline constexpr pm::sdk::EntryDesc TextAlignmentOptions_Center{.key = "TextAlignmentOptions.Center",
                                                                .kind = pm::sdk::Kind::EnumValue,
                                                                .owner = "TextAlignmentOptions", .name = "Center"};

}  // namespace pm::map

// The game API for Unity's UI (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build" once
// and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's grid layout. `grid` below is the GridLayoutGroup.
struct GridLayoutGroup {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GridLayoutGroup> type{};
  /// get_cellSize(grid). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_get_cellSize, Vector2(void*)> get_cellSize{};
  /// set_cellSize(grid, size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_set_cellSize, void(void*, Vector2)> set_cellSize{};
  /// get_spacing(grid). Unresolved: zero.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_get_spacing, Vector2(void*)> get_spacing{};
  /// get_constraint(grid), a GridConstraint value. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_get_constraint, int32_t(void*)> get_constraint{};
  /// set_constraint(grid, value). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_set_constraint, void(void*, int32_t)> set_constraint{};
  /// get_constraintCount(grid). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_get_constraintCount, int32_t(void*)> get_constraintCount{};
  /// set_constraintCount(grid, count). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GridLayoutGroup_set_constraintCount, void(void*, int32_t)>
      set_constraintCount{};
};

/// How a grid counts its columns.
namespace GridConstraint {
/// A fixed number of columns: FixedColumnCount(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::GridConstraint_FixedColumnCount> FixedColumnCount{};
}  // namespace GridConstraint

/// Unity's vertical or horizontal layout. `layout` below is the HorizontalOrVerticalLayoutGroup.
struct HorizontalOrVerticalLayoutGroup {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::HorizontalOrVerticalLayoutGroup> type{};
  /// get_spacing(layout). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::HorizontalOrVerticalLayoutGroup_get_spacing, float(void*)> get_spacing{};
  /// set_spacing(layout, spacing). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HorizontalOrVerticalLayoutGroup_set_spacing, void(void*, float)>
      set_spacing{};
};

/// Unity's button. `button` below is the Button.
struct Button {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Button> type{};
  /// Press(button): hook it to follow clicks and submits. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Button_Press, void(void*)> Press{};
  /// get_onClick(button): its click event, a UnityEvent. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Button_get_onClick, void*(void*)> get_onClick{};
};

/// Unity's base of the UI's components. `component` below is one.
struct UIBehaviour {
  /// IsActive.virtual_call(component). Unresolved: false.
  static constexpr pm::api::Method<pm::map::UIBehaviour_IsActive, bool(void*)> IsActive{};
};

/// Unity UI's image.
struct Image {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Image> type{};
};

/// Unity's canvas component.
struct Canvas {
  /// Screen pixels per canvas unit: get_scaleFactor(canvas). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Canvas_get_scaleFactor, float(void*)> get_scaleFactor{};
  /// set_renderMode(canvas, mode): how it is drawn, a RenderMode value. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Canvas_set_renderMode, void(void*, int32_t)> set_renderMode{};
  /// set_sortingOrder(canvas, order): a higher order draws on top. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Canvas_set_sortingOrder, void(void*, int32_t)> set_sortingOrder{};
  /// The type: type_object() (for AddComponent). Unresolved: null.
  static constexpr pm::api::Type<pm::map::Canvas> type{};
  /// get_worldCamera(canvas): the camera it is seen through, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Canvas_get_worldCamera, void*(void*)> get_worldCamera{};
};

/// Unity's base of the controls. `control` below is one.
struct Selectable {
  /// IsInteractable.virtual_call(control). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Selectable_IsInteractable, bool(void*)> IsInteractable{};
  /// get_interactable(control). Optional. Unresolved: false.
  static constexpr pm::api::Method<pm::map::Selectable_get_interactable, bool(void*)> get_interactable{};
  /// set_interactable(control, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Selectable_set_interactable, void(void*, bool)> set_interactable{};
  /// get_targetGraphic(control): the Graphic it tints, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Selectable_get_targetGraphic, void*(void*)> get_targetGraphic{};
  /// set_targetGraphic(control, graphic). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Selectable_set_targetGraphic, void(void*, void*)> set_targetGraphic{};

  /// OnPointerEnter(control, eventData): highlighted, as when the pointer enters it; eventData may be
  /// null. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Selectable_OnPointerEnter, void(void*, void*)> OnPointerEnter{};
  /// OnPointerExit(control, eventData): no longer highlighted; eventData may be null. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Selectable_OnPointerExit, void(void*, void*)> OnPointerExit{};
};

/// Unity's UI event system.
struct EventSystem {
  /// get_current(): the event system in use, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::EventSystem_get_current, void*()> get_current{};
  /// Its input module, a Behaviour: currentInputModule.get(system), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::EventSystem_currentInputModule, void*> currentInputModule{};
};

/// A group of UI objects. `group` below is the CanvasGroup.
struct CanvasGroup {
  /// The type: klass(), is(obj), type_object() (for GetComponent and AddComponent). Unresolved: null and
  /// false.
  static constexpr pm::api::Type<pm::map::CanvasGroup> type{};
  /// set_blocksRaycasts(group, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CanvasGroup_set_blocksRaycasts, void(void*, bool)> set_blocksRaycasts{};
};

/// TextMesh Pro's UI text.
struct TextMeshProUGUI {
  /// The type: klass(), is(obj), type_object() (for GetComponent). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::TextMeshProUGUI> type{};
};

/// Unity's toggle, a Selectable. `toggle` below is the Toggle.
struct Toggle {
  /// The type: type_object() for GetComponent, is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Toggle> type{};
  /// The Graphic shown while on: graphic.get(toggle). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Toggle_graphic, void*> graphic{};
  /// Its ToggleGroup, or null: m_Group.get(toggle); a write sets the field alone, registering nothing
  /// with any group. Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::Toggle_m_Group, void*> m_Group{};
  /// Its change event, a ToggleEvent: onValueChanged.get(toggle). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::Toggle_onValueChanged, void*> onValueChanged{};
  /// get_isOn(toggle). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Toggle_get_isOn, bool(void*)> get_isOn{};
  /// SetIsOnWithoutNotify(toggle, on): no listener runs. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Toggle_SetIsOnWithoutNotify, void(void*, bool)> SetIsOnWithoutNotify{};
};

/// A toggle's change event.
struct ToggleEvent {
  /// The type: make() for a new one, then ctor. Unresolved: null.
  static constexpr pm::api::Type<pm::map::ToggleEvent> type{};
  /// ctor(event): runs the constructor of an event made with type.make(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ToggleEvent_ctor, void(void*)> ctor{};
};

/// How a Canvas is drawn. Unresolved: 0.
namespace RenderMode {
inline constexpr pm::api::EnumValue<pm::map::RenderMode_ScreenSpaceOverlay> ScreenSpaceOverlay{};
}  // namespace RenderMode

/// Scales a canvas to the screen. `scaler` below is the CanvasScaler.
struct CanvasScaler {
  /// The type: klass(), is(obj), type_object() (for AddComponent). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::CanvasScaler> type{};
  /// set_uiScaleMode(scaler, mode): how it scales, a ScaleMode value. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CanvasScaler_set_uiScaleMode, void(void*, int32_t)> set_uiScaleMode{};
  /// set_referenceResolution(scaler, size): the resolution laid out for. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CanvasScaler_set_referenceResolution, void(void*, Vector2)>
      set_referenceResolution{};
  /// set_matchWidthOrHeight(scaler, match): 0 the width decides the scale, 1 the height. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::CanvasScaler_set_matchWidthOrHeight, void(void*, float)>
      set_matchWidthOrHeight{};
  /// How it scales. Unresolved: 0.
  static constexpr pm::api::EnumValue<pm::map::CanvasScaler_ScaleWithScreenSize> ScaleWithScreenSize{};
};

/// Lets a canvas catch the pointer.
struct GraphicRaycaster {
  /// The type: type_object() (for AddComponent). Unresolved: null.
  static constexpr pm::api::Type<pm::map::GraphicRaycaster> type{};
};

/// Unity UI's base of everything drawn. `graphic` below is the Graphic.
struct Graphic {
  /// get_color.virtual_call(graphic): its colour, TextMeshPro's own on a text. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::Graphic_get_color, Color(void*)> get_color{};
  /// set_color.virtual_call(graphic, &colour): sets it, TextMeshPro's own on a text. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Graphic_set_color, void(void*, const Color*)> set_color{};
  /// set_raycastTarget(graphic, on): whether it catches the pointer. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Graphic_set_raycastTarget, void(void*, bool)> set_raycastTarget{};
  /// get_canvasRenderer(graphic): what draws it. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Graphic_get_canvasRenderer, void*(void*)> get_canvasRenderer{};
};

/// What draws a Graphic. `renderer` below is the CanvasRenderer.
struct CanvasRenderer {
  /// GetColor(renderer). Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::CanvasRenderer_GetColor, Color(void*)> GetColor{};
  /// SetColor(renderer, &colour). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::CanvasRenderer_SetColor, void(void*, const Color*)> SetColor{};
};

/// A graphic that shows a Texture. `image` below is the RawImage.
struct RawImage {
  /// The type: type_object() (for AddComponent). Unresolved: null.
  static constexpr pm::api::Type<pm::map::RawImage> type{};
  /// set_texture(image, texture): the texture it shows, or null. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RawImage_set_texture, void(void*, void*)> set_texture{};
  /// set_uvRect(image, &rect): the part of the texture it shows, in 0..1. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RawImage_set_uvRect, void(void*, const Rect*)> set_uvRect{};
};

/// Unity UI's screen-space hit tests, a static class.
struct RectTransformUtility {
  /// RectangleContainsScreenPoint(rect, point, camera): whether the screen point is inside. Unresolved: false.
  static constexpr pm::api::Method<pm::map::RectTransformUtility_RectangleContainsScreenPoint,
                                   bool(void*, Vector2, void*)>
      RectangleContainsScreenPoint{};
  /// ScreenPointToLocalPointInRectangle(rect, point, camera, &local): the point in the rect's own space.
  /// Unresolved: false.
  static constexpr pm::api::Method<pm::map::RectTransformUtility_ScreenPointToLocalPointInRectangle,
                                   bool(void*, Vector2, void*, Vector2*)>
      ScreenPointToLocalPointInRectangle{};
};

/// TextMeshPro's base text. `text` below is the TMP_Text (a TextMeshProUGUI).
struct TMP_Text {
  /// set_text(text, string): its text. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_text, void(void*, void*)> set_text{};
  /// get_text(text): its text, a managed string. Unresolved: null.
  static constexpr pm::api::Method<pm::map::TMP_Text_get_text, void*(void*)> get_text{};
  /// get_fontSize(text). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::TMP_Text_get_fontSize, float(void*)> get_fontSize{};
  /// set_fontSize(text, size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_fontSize, void(void*, float)> set_fontSize{};
  /// get_fontSizeMax(text): auto-sizing's largest size as configured. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::TMP_Text_get_fontSizeMax, float(void*)> get_fontSizeMax{};
  /// set_fontSizeMax(text, size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_fontSizeMax, void(void*, float)> set_fontSizeMax{};
  /// set_fontSizeMin(text, size). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_fontSizeMin, void(void*, float)> set_fontSizeMin{};
  /// set_enableAutoSizing(text, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_enableAutoSizing, void(void*, bool)> set_enableAutoSizing{};
  /// set_alignment(text, options): a TextAlignmentOptions value. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_alignment, void(void*, int32_t)> set_alignment{};
  /// set_enableWordWrapping(text, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_enableWordWrapping, void(void*, bool)>
      set_enableWordWrapping{};
  /// get_font(text): its TMP_FontAsset. Unresolved: null.
  static constexpr pm::api::Method<pm::map::TMP_Text_get_font, void*(void*)> get_font{};
  /// set_font(text, font). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_font, void(void*, void*)> set_font{};
  /// get_fontSharedMaterial(text): its font's shared material. Unresolved: null.
  static constexpr pm::api::Method<pm::map::TMP_Text_get_fontSharedMaterial, void*(void*)> get_fontSharedMaterial{};
  /// set_fontSharedMaterial(text, material). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TMP_Text_set_fontSharedMaterial, void(void*, void*)>
      set_fontSharedMaterial{};
};

/// How a TextMeshPro text is aligned. Unresolved: 0.
namespace TextAlignmentOptions {
inline constexpr pm::api::EnumValue<pm::map::TextAlignmentOptions_TopLeft> TopLeft{};
inline constexpr pm::api::EnumValue<pm::map::TextAlignmentOptions_Top> Top{};
inline constexpr pm::api::EnumValue<pm::map::TextAlignmentOptions_Left> Left{};
inline constexpr pm::api::EnumValue<pm::map::TextAlignmentOptions_Center> Center{};
}  // namespace TextAlignmentOptions

/// @}
}  // namespace phasmo
