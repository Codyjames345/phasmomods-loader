# The menus: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [The menus](../systems/menus.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## How a Doozy button is clicked

`UIButton` is a `UISelectable`, which is a Unity `Selectable`.

- **Selection and navigation.** The menu's tapes, the popup's button, Multiplayer and `Toggle.1` are saved with the
  navigation mode None. A Selectable's `OnPointerDown` selects it only for the left button, while it is interactable and
  its mode is not None, so a click does not select these, and the arrow keys or a controller's stick do not move to or
  between them. Submit reaches a button only if something has selected it. A copy keeps its original's mode, so it
  behaves as the original does. Some other controls of the scene use Automatic or Explicit navigation.
- **The pointer.** The EventSystem calls `UIButton.OnPointerClick`. For the left button only, while the button is
  active and interactable, it starts the pressed look and sends the button's signal on Doozy's button stream (its
  `UIButtonId`, a category and a name; the menu's buttons have `None` and `None`). It runs no behaviour itself.
- **The click behaviour.** A button's behaviours (`UISelectable.Behaviours`, a `DoozyBehaviours` whose
  `DoozyBehaviours.Behaviours` is the list) are `DoozyBehaviour`s, each a Doozy `ModyEvent` named by a
  `DoozyBehaviourName`. `UISelectable.GetBehaviour` finds one by its name. The click is
  `DoozyBehaviourName.PointerClick`. Its signal receiver listens to Doozy's pointer-click trigger on the button's
  own object (the scene saves it as the stream `Pointer`, `Click`), a signal provider that is itself a pointer-click
  handler. When the signal comes, and the button is alive, active and interactable, the behaviour runs its
  runners (`ModyEventBase.Runners`, Doozy's own actions) and then its UnityEvent (`ModyEvent.Event`), read as it
  runs.
- **How a behaviour connects.** One frame after the control is enabled, a coroutine gives its behaviours the control and
  the control's own object as their signal source, then connects them. A receiver finds its provider on that object
  (`GetComponent` of the provider's type, `AddComponent` when there is none), for the local provider type the menu's
  buttons are saved with, and the behaviour adds itself to the receiver's delegate. Whether a receiver is connected is
  not saved, so a copy connects afresh to a trigger on its own object. Disabling the control disconnects them.
- **Submit.** The keyboard's or a controller's submit on the selected button calls `UIButton.OnSubmit`: while the
  button is active and interactable, the pressed state, the click as above, and then, when Doozy's input settings say
  a submit counts as a click, the button's `PointerClick` and `PointerLeftClick` behaviours. So a submit runs the
  click's listeners only where the game's settings say so, for the game's own buttons too.
- **The state and the colour.** The Selectable's state transition (a pointer entering, leaving, pressing) reaches
  `UISelectable.SetState`, unless that state is already set: it raises the control's selection-state event, stores the
  state and runs the state's own `ModyEvent`. With "deselect after press" on, a move from pressed to selected clears the
  EventSystem's selection and goes to normal instead. `UISelectableColorAnimator` is enabled with its control saved
  in it: as it switches on, a coroutine adds a listener on that control's selection-state event, and the listener plays
  the state (`UISelectableColorAnimator.Play`: 0 normal, 1 highlighted, 2 pressed, 3 selected, 4 disabled). An
  animator of a copy holds the copy's button, since `Object.Instantiate` remaps references inside what it copies. So
  nobody but the button need call `Play`.

## How a Unity button is clicked

The Multiplayer tape is a Unity `Button`. Its `OnPointerClick` checks the event is the left button's and runs
`Button.Press`, which, while the button is active and interactable, invokes its click event (`Button.get_onClick`).
`OnSubmit` runs `Press` and then shows the pressed state. Its colours are the Selectable's own tint; it has no Doozy
animator. A `Toggle`'s click, while it is active and interactable, flips its `isOn`, tells its group, fades its graphic
and raises its `ToggleEvent` (a `UnityEvent<bool>`, `Toggle.onValueChanged`); `Toggle.SetIsOnWithoutNotify` sets it
without raising the event.

## Where the listeners come from

The menu's listeners are **persistent**: saved with the scene, as the inspector's list.

- News's click behaviour has one listener, the main menu's `ConnectToServer.TrainingButton`, and no runners.
- The error popup's `Error Accept Button` has one, `ErrorManager.NextMessageButton`, which moves the game's error
  queue on.
- Multiplayer's click event (a `Button`'s) has one, `ConnectToServer.MultiplayerButton`.
- The tapes' selection states (the normal, highlighted, pressed, selected and disabled states, each a `ModyEvent`)
  have no listener, and `Toggle.1`'s change event has none.

A copy of an object (`Object.Instantiate`) keeps its persistent listeners, still pointing at the original's targets:
clicking a copy of News would call `ConnectToServer.TrainingButton`. Runtime listeners (`UnityEvent.AddListener`) are never saved, so a
copy has none of its original's.

This build has no runtime way to remove a persistent listener: `UnityEventBase`'s `SetPersistentListenerState` is
stripped, and `RemoveAllListeners` clears the runtime listeners only (`UnityEventBase.GetPersistentEventCount` counts
the others). What it does have is the event itself: `ModyEvent.Event` is a field read each time the behaviour runs,
and `Toggle.onValueChanged` one read each time the toggle changes. A copy given a new event (`UnityEvent.ctor`, or a
new `ToggleEvent`) runs none of its original's listeners.

## A listener in native code

`UnityAction.ctor` is a delegate's constructor, `(object target, IntPtr method)`: it keeps both and binds the
method's code to the target, refusing a null target for an instance method. `UnityEvent.Invoke` runs the persistent
listeners, then the runtime ones, and skips a listener whose target is a destroyed Unity object. So a listener whose
target is the copied button stops with the copy.

The host makes such a delegate for its own code through the runtime services (`host/runtime_services.h`): the runtime's
own constructor, with a method the host describes to the runtime as an instance method of no arguments whose code is
the host's. It is the host's alone; the host contract does not offer it to mods.

## A keybinds row's label (2026-11-10)

`RebindActionUI.set_actionReference` and the row's enabling were decompiled for the Emotes mod's key. Both read the
action's binding and write only the text showing the binding, and
raise the row's update event; neither writes `RebindActionUI.m_ActionLabel`. The first in-game run showed a copied row
labelled with the copied row's words ("Forward Movement"), which is the label's `TextLocaliserUI` writing its key's text
when the copy woke. The SDK takes the localiser off a copy's label (`sdk/api/keybinds.cpp`).

The menu's place and layout were read from `resources.assets` with UnityPy (the `PCPlayer` prefab's hierarchy and its
RectTransforms; the layout group's spacing from its raw serialized fields, its type tree being stripped). The main menu's
scene (`level2`) holds no `RebindActionUI`; the maps' (the prison's `level11`, the training map's `level3`) hold only the
whiteboards'. The row's object references were read from its serialized bytes (the PPtrs that resolve in the file):
`m_ActionLabel` is null, `m_BindingText` is `TriggerRebindButton/ActionBindingText`, and the rebind button's persistent
call names `StartInteractiveRebind` on the row. The rebinding error was in the game's own log of the fifth run. The fourth in-game run showed why the list's size matters: the row was made, at the end of a list whose `Content` it
did not grow, and could not be scrolled to.

## The audio page's lines (2026-11-10)

The doubled lines the player saw on the audio page were read from the same `PCPlayer` prefab, again with UnityPy. Under
`Audio Settings/Scroll View/Viewport/Content` the children alternate a row and a divider, but not evenly: the divider
after the master volume and the one before the microphone have the speaker mode row between them, saved switched off
when there is one output device, so the two dividers sit side by side; and the voice bar ("Current Microphone Input")
and the voice input mode row touch, with no divider between them in the prefab at all. A divider is an image with no
text under it; the rows all have a `TextLocaliserUI` name. That is what `rows::space_out` works from: a child with a
name or children is a row, one with neither is a line, and a switched-off child is neither, since the layout gives it
no room. The keybinds page hangs its rows one level deeper, under a single `Layout` child of its `Content`, and its
dividers (1679 wide, 6 high, named `-` and its clones) are the thin lines the player called perfect. UnityPy notes from
this pass: a `GameObject`'s switch is `m_IsActive`; a `RectTransform` gives `m_SizeDelta`, not a finished rect; a
`MonoBehaviour` with a stripped type tree is only recognizable by its raw length and its script PPtr.
