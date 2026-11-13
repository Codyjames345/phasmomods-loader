#pragma once

// Doozy UI Manager (DoozyRuntime), the UI library the game's menus are made with: its button, the behaviours a click
// runs, the ModyEvent each behaviour holds and the colour animator of a taped button. Doozy's own names are real; the
// enum of behaviour names is obfuscated and found by its values. The host's menu controls are copies of these buttons
// that keep them working (host/menu_panel.h); sdk/systems/menus.md says how a click reaches a listener. Notes read from
// decompiled code are from build 25811160.

#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityUI.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// Doozy's base of its controls (DoozyRuntime, namespace Doozy.Runtime.UIManager.Components), a Unity Selectable:
/// selection, navigation and the states its animators play. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc UISelectable{.key = "UISelectable", .name = "UISelectable",
                                                 .assembly = "DoozyRuntime",
                                                 .ns = "Doozy.Runtime.UIManager.Components", .base = &Selectable};

/// Doozy's button (DoozyRuntime, namespace Doozy.Runtime.UIManager.Components), a UISelectable: the main menu's taped
/// buttons and the error popup's. Decompiled.
inline constexpr pm::sdk::EntryDesc UIButton{.key = "UIButton", .confidence = pm::sdk::Confidence::Decompiled,
                                             .name = "UIButton", .assembly = "DoozyRuntime",
                                             .ns = "Doozy.Runtime.UIManager.Components", .base = &UISelectable};

inline constexpr const char* Doozy_PointerEventData[] = {"PointerEventData"};
inline constexpr const char* Doozy_BaseEventData[] = {"BaseEventData"};

/// The pointer's click on the button, from the EventSystem. For the left button only, while the button is active
/// and interactable (both asked as virtual calls): starts the pressed look and sends the button's signal on its
/// stream (its UIButtonId's category and name). It runs no behaviour itself: the click behaviour
/// (DoozyBehaviourName.PointerClick) is run by Doozy's pointer-click trigger on the button's object, through the
/// behaviour's signal receiver. Hook it to follow every click of every Doozy button. Decompiled.
inline constexpr pm::sdk::EntryDesc UIButton_OnPointerClick{
    .key = "UIButton.OnPointerClick", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "UIButton", .name = "OnPointerClick", .params = Doozy_PointerEventData};

/// The EventSystem's submit (the keyboard's or a controller's), on the selected button. While the button is active
/// and interactable: the pressed state, the click (as UIButton.OnPointerClick's), and then, when Doozy's input
/// settings say a submit counts as a click, the button's PointerClick and PointerLeftClick behaviours. Decompiled.
inline constexpr pm::sdk::EntryDesc UIButton_OnSubmit{
    .key = "UIButton.OnSubmit", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "UIButton", .name = "OnSubmit", .params = Doozy_BaseEventData};

/// The control's behaviours, a DoozyBehaviours: what runs on a click, an enter, a selection. Saved with the scene,
/// so a copy (Object.Instantiate) has its own, with its original's listeners. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectable_Behaviours{
    .key = "UISelectable.Behaviours", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "UISelectable", .name = "Behaviours", .type = "@DoozyBehaviours"};

inline constexpr const char* Doozy_BehaviourName[] = {"@DoozyBehaviourName"};

/// The control's behaviour of that name, or null when it has none: the first in its list with that name. It
/// makes none. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectable_GetBehaviour{
    .key = "UISelectable.GetBehaviour", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "UISelectable", .name = "GetBehaviour", .params = Doozy_BehaviourName};

/// A control's list of behaviours (DoozyRuntime, namespace Doozy.Runtime.UIManager, class UIBehaviours). The control's
/// Awake gives each its control and object. Decompiled.
inline constexpr pm::sdk::EntryDesc DoozyBehaviours{.key = "DoozyBehaviours", .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "UIBehaviours", .assembly = "DoozyRuntime",
                                                    .ns = "Doozy.Runtime.UIManager"};

/// The behaviours, a List<UIBehaviour> (DoozyBehaviour). Matched by name alone: Unity's own UIBehaviour shares the
/// element type's name. Decompiled.
inline constexpr pm::sdk::EntryDesc DoozyBehaviours_Behaviours{
    .key = "DoozyBehaviours.Behaviours", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DoozyBehaviours", .name = "Behaviours"};

/// Doozy's base of its events (DoozyRuntime, namespace Doozy.Runtime.Mody): a name and the runners of Doozy's own
/// actions. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc ModyEventBase{.key = "ModyEventBase", .name = "ModyEventBase",
                                                  .assembly = "DoozyRuntime", .ns = "Doozy.Runtime.Mody"};

/// Doozy's event (DoozyRuntime, namespace Doozy.Runtime.Mody), a ModyEventBase with a UnityEvent. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc ModyEvent{.key = "ModyEvent", .name = "ModyEvent", .assembly = "DoozyRuntime",
                                              .ns = "Doozy.Runtime.Mody", .base = &ModyEventBase};

/// One behaviour of a control (DoozyRuntime, namespace Doozy.Runtime.UIManager, class UIBehaviour), a ModyEvent with
/// a name (DoozyBehaviourName). When its signal comes, and its control is alive, active and interactable, it runs its
/// runners (ModyEventBase.Runners) and then its event (ModyEvent.Event). Decompiled.
inline constexpr pm::sdk::EntryDesc DoozyBehaviour{.key = "DoozyBehaviour", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "UIBehaviour", .assembly = "DoozyRuntime",
                                                   .ns = "Doozy.Runtime.UIManager", .base = &ModyEvent};

inline constexpr const char* DoozyBehaviourName_values[] = {
    "PointerEnter",     "PointerExit",       "PointerDown",        "PointerUp",         "PointerClick",
    "PointerDoubleClick", "PointerLongClick", "PointerLeftClick", "PointerMiddleClick", "PointerRightClick",
    "Selected",         "Deselected",        "Submit"};

// Found by its values.
/// What a behaviour answers to, an enum nested in DoozyBehaviour (name obfuscated). Inferred: from the values'
/// names.
inline constexpr pm::sdk::EntryDesc DoozyBehaviourName{.key = "DoozyBehaviourName", .owner = "DoozyBehaviour",
                                                       .assembly = "DoozyRuntime",
                                                       .values = DoozyBehaviourName_values};

/// The click: the behaviour a button's click (and, when Doozy's input settings say so, its submit) runs.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DoozyBehaviourName_PointerClick{
    .key = "DoozyBehaviourName.PointerClick", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DoozyBehaviourName", .name = "PointerClick"};

/// The event's runners, a List<ModyActionRunner>: Doozy's own actions it runs, before its UnityEvent. Saved with the
/// scene. Decompiled.
inline constexpr pm::sdk::EntryDesc ModyEventBase_Runners{
    .key = "ModyEventBase.Runners", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ModyEventBase", .name = "Runners", .type = "List<ModyActionRunner>"};

/// The event's UnityEvent, read when it runs (not kept elsewhere), so a new one set here is the one that runs. Its
/// listeners from the scene are persistent ones. Matched by name alone: the models name UnityEvent and its generic
/// kin alike. Decompiled.
inline constexpr pm::sdk::EntryDesc ModyEvent_Event{.key = "ModyEvent.Event", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "ModyEvent", .name = "Event"};

/// Moves the control to a selection state (0 normal, 1 highlighted, 2 pressed, 3 selected, 4 disabled; the state's enum
/// is obfuscated) and returns the control: raises its selection-state event, which its animators listen to, stores the
/// state and runs the state's own event. With "deselect after press" on, pressed to selected clears the EventSystem's
/// selection and goes to normal instead. The Selectable's own transitions call it. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectable_SetState{
    .key = "UISelectable.SetState", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "UISelectable", .name = "SetState"};

/// The colour animator of a taped button (DoozyRuntime, namespace Doozy.Runtime.UIManager.Animators), which holds its
/// UISelectable: as it switches on, a coroutine adds a listener on the control's selection-state event, and the
/// listener plays the state, so the control's state drives it by itself. Optional. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectableColorAnimator{
    .key = "UISelectableColorAnimator", .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional,
    .name = "UISelectableColorAnimator",
    .assembly = "DoozyRuntime", .ns = "Doozy.Runtime.UIManager.Animators", .base = &MonoBehaviour};

/// Plays the colour of a selection state: 0 normal, 1 highlighted, 2 pressed, 3 selected, 4 disabled. The state's enum is
/// obfuscated. Optional, as its type. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectableColorAnimator_Play{
    .key = "UISelectableColorAnimator.Play", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .owner = "UISelectableColorAnimator",
    .name = "Play"};

/// The colour the animator plays for the disabled state (4), a ColorAnimation saved with the scene by value, so a copy
/// (Object.Instantiate) has its own. Its siblings NormalAnimation, HighlightedAnimation, PressedAnimation and
/// SelectedAnimation are the other states'. A Unity Selectable is disabled while a CanvasGroup above it is not
/// interactable, and the game switches the main menu canvas's group off while the camera looks at another board
/// (sdk/systems/menus.md). Optional, as its type. Decompiled.
inline constexpr pm::sdk::EntryDesc UISelectableColorAnimator_DisabledAnimation{
    .key = "UISelectableColorAnimator.DisabledAnimation", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .owner = "UISelectableColorAnimator",
    .name = "DisabledAnimation", .type = "@ColorAnimation"};

/// Doozy's colour animation (DoozyRuntime, namespace Doozy.Runtime.Reactor.Animations), a plain serialized class:
/// a colour target and the reaction that moves it. Optional. Decompiled.
inline constexpr pm::sdk::EntryDesc ColorAnimation{.key = "ColorAnimation", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .flags = pm::sdk::kOptional, .name = "ColorAnimation",
                                                   .assembly = "DoozyRuntime", .ns = "Doozy.Runtime.Reactor.Animations"};

/// The animation's reaction, a ColorTargetReaction: where the colour starts and ends. Optional. Decompiled.
inline constexpr pm::sdk::EntryDesc ColorAnimation_Animation{
    .key = "ColorAnimation.Animation", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kOptional, .owner = "ColorAnimation", .name = "Animation", .type = "@ColorTargetReaction"};

/// Doozy's colour reaction on a target (DoozyRuntime, namespace Doozy.Runtime.Reactor.Reactions). Each end of the
/// move names a reference value: the start colour, the target's current colour, or a custom colour of its own. The
/// start and current colours take its hue, saturation, lightness and alpha offsets; a custom colour is used as it is,
/// with no offset. The reaction reads them each time it sets its values. Optional. Decompiled.
inline constexpr pm::sdk::EntryDesc ColorTargetReaction{
    .key = "ColorTargetReaction", .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional,
    .name = "ColorTargetReaction", .assembly = "DoozyRuntime", .ns = "Doozy.Runtime.Reactor.Reactions"};

/// The custom colour the move ends on, a Color, used when the end's reference value is the custom one. The main menu's
/// taped buttons end every state on their custom colour. For the disabled state the visible picture tapes
/// (Singleplayer, Training) save an opaque white, while News and the icon tapes save white at half alpha, so those show
/// see-through while the board is not focused. Optional. Decompiled.
inline constexpr pm::sdk::EntryDesc ColorTargetReaction_ToCustomValue{
    .key = "ColorTargetReaction.ToCustomValue", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kOptional, .owner = "ColorTargetReaction",
    .name = "ToCustomValue", .type = "Color"};

}  // namespace pm::map

/// @file
/// @brief The game API for Doozy's UI (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Doozy's button. `button` below is the UIButton.
struct UIButton {
  /// The type: type_object() for GetComponent, is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::UIButton> type{};
  /// OnPointerClick(button, eventData): hook it to follow clicks. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UIButton_OnPointerClick, void(void*, void*)> OnPointerClick{};
  /// OnSubmit(button, eventData): hook it to follow submits. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UIButton_OnSubmit, void(void*, void*)> OnSubmit{};
};

/// Doozy's base of its controls. `control` below is one, a UIButton for example.
struct UISelectable {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::UISelectable> type{};
  /// Its behaviours, a DoozyBehaviours: Behaviours.get(control). Unresolved: null.
  static constexpr pm::api::Field<pm::map::UISelectable_Behaviours, void*> Behaviours{};
  /// GetBehaviour(control, name): its behaviour of that DoozyBehaviourName, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::UISelectable_GetBehaviour, void*(void*, int32_t)> GetBehaviour{};
  /// SetState(control, state): 0 normal, 1 highlighted, 2 pressed, 3 selected, 4 disabled; returns the control.
  /// Unresolved: null.
  static constexpr pm::api::Method<pm::map::UISelectable_SetState, void*(void*, int32_t)> SetState{};
};

/// A control's list of behaviours. `behaviours` below is one.
struct DoozyBehaviours {
  /// The list, a List<UIBehaviour>: Behaviours.get(behaviours). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DoozyBehaviours_Behaviours, void*> Behaviours{};
};

/// One behaviour of a control, a ModyEvent.
struct DoozyBehaviour {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::DoozyBehaviour> type{};
};

/// What a behaviour answers to.
namespace DoozyBehaviourName {
/// The click. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DoozyBehaviourName_PointerClick> PointerClick{};
}  // namespace DoozyBehaviourName

/// Doozy's base of its events. `event` below is one.
struct ModyEventBase {
  /// Its runners, a List<ModyActionRunner>: Runners.get(event). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ModyEventBase_Runners, void*> Runners{};
};

/// Doozy's event. `event` below is one, a DoozyBehaviour for example.
struct ModyEvent {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::ModyEvent> type{};
  /// Its UnityEvent: Event.get(event), Event.set(event, unityEvent). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::ModyEvent_Event, void*> Event{};
};

/// A taped button's colour animator. `animator` below is one. Optional.
struct UISelectableColorAnimator {
  /// The type: type_object() for GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::UISelectableColorAnimator> type{};
  /// Play(animator, state): state 0 normal, 1 highlighted, 2 pressed, 3 selected, 4 disabled. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UISelectableColorAnimator_Play, void(void*, int32_t)> Play{};
  /// The disabled state's colour animation: DisabledAnimation.get(animator). Unresolved: null.
  static constexpr pm::api::Field<pm::map::UISelectableColorAnimator_DisabledAnimation, void*> DisabledAnimation{};
};

/// A Doozy colour animation. `animation` below is one, as UISelectableColorAnimator.DisabledAnimation gives it.
struct ColorAnimation {
  /// Its reaction: Animation.get(animation). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ColorAnimation_Animation, void*> Animation{};
};

/// A Doozy colour reaction. `reaction` below is one, as ColorAnimation.Animation gives it.
struct ColorTargetReaction {
  /// The custom colour it ends on: ToCustomValue.get(reaction), ToCustomValue.set(reaction, color). Takes effect the
  /// next time the reaction sets its values, as it does when its animation plays. Unresolved: all zero, writes nothing.
  static constexpr pm::api::Field<pm::map::ColorTargetReaction_ToCustomValue, Color> ToCustomValue{};
};

/// @}
}  // namespace phasmo
