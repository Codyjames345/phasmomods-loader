# The menus

How the main menu's buttons are clicked, where their listeners come from, and how a copy of one keeps working
without running the game's action. The entries are in `sdk/map/Doozy.h`, `sdk/map/UnityEvents.h`,
`sdk/map/UnityUI.h` and `sdk/map/MainMenu.h`; their `///` notes are the authority. Code facts were read from
decompiled code of Steam build 25811160 (`re/decomp.mjs <key>`), scene facts from the `Main_Menu` scene (`level2`)
with UnityPy, as `tools/assetcheck.py` reads it (a MonoBehaviour's typetree is stripped, so its fields were read from
its raw data). This page names map keys, never addresses.

## The main menu's buttons

The menu's buttons sit under `Main Menu/Buttons` on the wall's corkboard (`Main Menu UI Canvas - DISABLE FOR
BAKE`). Each taped button (Singleplayer, Training, News, Go Online, Options, Credits, Quit) is an `Image`, a Doozy
`UIButton`, a `UISelectableColorAnimator` and Doozy's `ImageColorTarget`. Multiplayer is a plain Unity `Button`
with its `Image`. News ships switched off in the scene.

**A tape's colours.** The animator plays a colour for each state the button is in (normal, highlighted, pressed,
selected, disabled), and each tape saves every state's colour as a custom colour of its own
(`ColorTargetReaction.ToCustomValue`, reached through `UISelectableColorAnimator.DisabledAnimation` and
`ColorAnimation.Animation` for the disabled one). A custom colour is used as saved: the hue, saturation, lightness
and alpha offsets a reaction also keeps apply only to a start or current colour. The disabled colour shows whenever
the board is not focused. The camera's focus switches every board's `CanvasGroup` on the main menu canvas to
interactable only for the board it looks at, and Unity's `Selectable` is disabled while a group above it is not
interactable. It reads that again when it is enabled, so a tape switched on while the board is not focused starts
disabled. The picture tapes the menu shows (Singleplayer, Training) save an opaque white for disabled, so they look
the same either way. News and the icon tapes (Go Online, Options, Credits, Quit) save white at half alpha, so News
would show see-through until the board is focused.

The error popup (`ErrorManager.errorParent`) has one Doozy button, `Error Accept Button`, built as a tape is.

The lobby paper's checkboxes (`Create Lobby/Hardware/Toggle.1` and its kin) are Unity `Toggle`s: the box
(`Background`) is the toggle's target graphic, which it tints, and the tick (`Background/Checkmark`) its graphic, which
it fades in and out. `Toggle.1` belongs to no `ToggleGroup`.

## Where the popup can show

The popup is on the corkboard, not on the main menu's paper. `Wall Corkboard` holds the board's `CameraFocusUI` and
`Main Menu UI Canvas - DISABLE FOR BAKE`, whose children include `Main Menu` (`ConnectToServer`), `Create Lobby`,
`Game Lobby` (`ServerManager`) and `Error - Do not turn off` (`ErrorManager`), the last with `ErrorParent` under it. So
the popup draws over the lobby's paper as it does over the main menu's, while the board is focused
(`CameraFocusUI.get_isFocused`). The game uses it there itself: adding to the game's error queue shows the popup with no
check of which paper is up, and `ServerManager.KickPlayerNetworked` and the lobby's ID cards add to the queue. Whether
`ConnectToServer` and its buttons are active says only whether the main menu's own paper is up, not whether the popup
can be seen.

## The loader's controls

The loader's menu controls are copies of the game's (`host/menu_panel.h`), as the user's rule asks: the game's own
screens and widgets. Each keeps the game's button working, so the game drives its hover, press and colour, and the
game's own input clicks it, exactly as far as the original. With the navigation mode None above, that means the
pointer, and no selected state from a click.

- **The Mods tape and the popups' buttons** are copies of News and of `Error Accept Button`. Every behaviour of the
  copy gets a new, empty `UnityEvent` and loses its runners; the click behaviour's new event then gets the loader's
  listener. The copy's other scripts that act for the game (its localisers, the error manager) stay switched off.
  Before it wakes, the Mods tape takes Training's disabled colour in place of News's see-through one
  (`menu_panel::match_disabled_look`), so it looks like the picture tape beside it while the board is not focused.
- **The Mods panel's rows** are copies of `Toggle.1`, each with a new `ToggleEvent` and no group. The game's toggle
  flips the tick. The host reads `Toggle.get_isOn` each frame, as Playable Ghost's lobby tape does, because a listener
  on a `ToggleEvent` would be a `UnityAction<bool>`, a generic delegate.
- The loader draws no window of its own here. A copy is made only when every entry it uses resolved.
- The consent notice is only ever the popup's copy. It waits for the menu with no time limit. It answers Declined,
  with a log line naming why, when this build gives the host no way to take a click or to copy the popup, or when the
  popup cannot be made with the menu in view (`host/consent.h`).
- The host's messages about a lobby's mods are the same copy, shown when the popup's board is focused, on the lobby's
  paper as well (`menu_panel::message`).
- A Unity `Button` added at run time navigates automatically, unlike the game's saved controls: a press would select
  it and a later Enter would click it. The `Selectable`'s navigation setter takes the `Navigation` struct by address and
  copies all of it into the control.

A copy's selection states keep their (empty) persistent listeners: the host does not replace those events. A build
that gives a tape's states a listener would run it on the copy too.

## Key bindings

The game's actions and their default bindings are listed in [players](players.md#what-the-player-does-by-input-action).
Every D-pad direction is bound on a gamepad (up and right only to the menu actions UIUp and UIRight). The keyboard's B is
the walkie-talkie's push to talk. The VR controllers' actions are XR Interaction actions; the left controller's
PrimaryButton and SecondaryButton (X and Y on a Touch controller) are bound to nothing.

Each row of the keybinds menu is a `RebindActionUI`, Unity's rebinding sample: its `RebindActionUI.m_Action` (an
`InputActionReference`) and `RebindActionUI.m_BindingId` name the binding it rebinds, and `RebindActionUI.m_ActionLabel`
shows the action's name. The game saves its own actions' overrides as a JSON list of binding ids and paths. A mod key
(`sdk/api/keybinds.h`) is a copy of a row pointed at a standalone action of the mod's own (`InputAction.ctor`,
`InputActionReference.Create`), whose rebinding the SDK saves in the mod's data folder.

The game's rows leave `RebindActionUI.m_ActionLabel` empty: a row's name is a child text of its own, `ActionNameText`
(its first `TextLocaliserUI`), wired to nothing. Its other children are `TriggerRebindButton`, whose click runs the row's
own `StartInteractiveRebind`, and `ResetToDefaultButton`. The page (`ControlsManager`) decides when a rebinding counts:
`ControlsManager.ApplyButton` stores the overrides, `ControlsManager.ResetButton` takes every override off without
storing, and its `ControlsManager.OnDisable` loads the stored ones back, so leaving without Apply drops a change. Unity refuses to rebind an action while it is enabled ("Cannot
rebind action ... while it is enabled"); the game's own actions are off while the menu is open, so a mod's must be too.

A row's name text carries a `TextLocaliserUI`, which writes the game's words for its key whenever it wakes. `RebindActionUI`
itself never writes the label: neither `RebindActionUI.set_actionReference` nor its own enabling touches it; both only
refresh the binding's text. So a copied row keeps a new label only once its localiser is gone. A copy made while the
menu is hidden wakes when the player opens it.

**Where the menu is.** The keybinds menu is part of each player's journal, not of a scene: the `PCPlayer` prefab holds it
under `NetworkedJournal/Journal_anim/Journal2.0Canvas/Content/Pause Menu/3 Options/Controls Settings`, with one list for
the keyboard and mouse (`PC/_Keyboard and mouse`) and one for a controller (`PC/_Controller`). The menu scene spawns the
local player at its start (see [the lobby's journal](journal.md#the-lobbys-journal)), so the menu is there from the main
menu on; every other player's journal holds a copy too. Each list is a scroll view whose `Layout` (a
`HorizontalOrVerticalLayoutGroup`, spacing 15.5) stretches to fill a `Content` of fixed height, sized to the game's own
rows: rows 100 high alternate with dividers, small images with no `RebindActionUI`. A row added at the end lies past the
scroll's reach until the `Content` grows by its height and the spacing. The training whiteboards in each map show
bindings with `RebindActionUI` too, on rows with no layout: a search for rows finds them as well.

## The audio page

The journal's audio page (`GameMyAudioManager`, in each player's journal like the keybinds page) is a list of the same
kind: rows with a localised name, dividers between them, a scroll content sized to the rows. The master volume's row
holds its name, its number (`GameMyAudioManager.masterVolumeValueText`) and its slider
(`GameMyAudioManager.masterVolumeSlider`, an `XRSlider`, the game's own slider class). `GameMyAudioManager.ApplyButton`
reads the slider into the audio settings and sets the mixer, so a change counts on Apply. A copy of the slider keeps
the listeners the prefab put on its event (`XRSlider.m_OnValueChanged`), so a copy needs a new `SliderEvent` of its own.
`sdk/api/settings.h` puts a mod's sliders there, under a heading with the mod's name.

The two journal lists divide their rows differently, as the game shows them (the user's screenshots, 2026-11-10). The
keybinds page's rows sit one thin line apart. The audio page's list holds its rows and dividers alternating, but not
evenly: it switches a row off when it does not apply (the speaker mode row when
there is one output device), which leaves two of its dividers side by side reading as one thick band, and it groups two
of its rows ("Current Microphone Input" and "Voice Input Mode") with no divider between them at all. `rows::space_out`
lays a whole list out with exactly one line between every pair of rows the player can see: it puts the list's own
dividers away and copies one line in before every row but the first, and gives it all back when the mod's rows go. `rows::line_divider` picks the line to copy: the keybinds
page's, when one of its lists is loaded, and the page's own divider when none is (the user's ruling).

## Input glyphs

The game shows a gamepad button's glyph with a `GamepadButton` widget (the journal's and pause screen's LB and RB tabs
are ones). It holds an Xbox and a PS5 set of sprites and picks one by the connected pad's name and the platform; its
`GamepadButton.image` shows the picked sprite for its action (`GamepadButton.action`) in full colour while a gamepad is
in use, and turns clear for keyboard and mouse. Which of the two is in use is the game's own check,
`InputDevices.UsesKeyboard`, of the player's PlayerInput (`LocalPlayer.playerInput`): it compares the PlayerInput's
current control scheme with the game's keyboard scheme, and answers false while VR runs
(`GamepadButton.CheckInputDevice` hides the glyph while it, or VR, holds). `pm::api::input::keyboard_in_use()`
(`sdk/api/input_device.h`) asks it about the local rig. A mod that wants the same glyph reads `Image.get_sprite` of a
live widget whose action is bound to that button, so the glyph follows the pad the game detected, and shows it while
`keyboard_in_use()` answers false. The game has no glyph for a mouse button, the scroll wheel or a key.

Each widget holds both sets whole: `GamepadButton.xboxIcons` (the first of its two `GamepadIcons` fields) and
`GamepadButton.ps5Icons` (the second), each a struct of one Sprite per gamepad control named for the Input System's
control (`buttonSouth`, `leftShoulder` and so on). The map reads each set's sprites inside the widget
(`GamepadButton.xbox.buttonWest`, `GamepadButton.ps5.leftShoulder`), so a mod can show the game's own glyph for any
button, from the set the game picked (the one its widgets' Images show).

## Unity's UI input during a contract

Every click, hover and keyboard or gamepad move in any UI, the game's or a mod's, is routed by Unity's one
`EventSystem` through its one input module (`EventSystem.currentInputModule`). The game keeps that module
switched off during a contract: `JournalController.OpenCloseJournal`, the journal's own toggle, switches it
on with `Behaviour.set_enabled` only while the journal shows, and off again when it closes (read from its
decompile, 2026-11-11). So outside the journal no canvas takes clicks and no menu takes arrow keys: a mod
that wants its own UI to answer the player has to run that module while its UI is up, and give the state
back afterwards, exactly as `sdk/api/buttons.h` does. The same journal toggle calls
`PCCanvas.PausePlayerInPlace` on the local player's HUD (its entry note in `sdk/map/Player.h`): the game's
way to hold the player still and free the cursor while an overlay is up.

That hold does not stop a click from reaching the world. The investigator's interact and primary-use handlers
check `JournalController.open` on the local player's journal instead, and do nothing while it is set (read from
their decompile, 2026-11-13). A mod screen that takes clicks raises that flag while it is up and puts back the
value it found when it closes, as the Dev Menu does. Setting the field sends nothing and shows no book.

## What a canvas needs before it answers the pointer

Three things have to hold, and a mod's own canvas starts with none of them (all read from the build's own
compiled Unity UI code, 2026-11-12):

- The canvas needs a `GraphicRaycaster`. A `Canvas` has no hit-testing of its own: no member of
  UnityEngine.UIModule's `Canvas` mentions a raycast at all, and the hit-test is
  `GraphicRaycaster.Raycast` in UnityEngine.UI. A canvas made at runtime therefore catches nothing until one
  is added.
- The graphic hit needs its `raycastTarget` on. `Graphic.set_raycastTarget` is what puts a graphic into, or
  out of, the raycast list of the canvas it draws on (UnityEngine.UI's own graphic registry), and only while
  the component is active and enabled; the raycaster walks nothing but that list. A graphic with it off is
  never hovered, pressed or clicked, whatever sits on the same object. `pm::ui::create_image` makes its
  pictures with it off.
- The object hit needs a `Button` or other `Selectable` on it, since that is what the event system selects,
  hovers and submits.

`sdk/api/buttons.h`'s `attach` and `catch_pointer` do all three, and take the object as either its GameObject
or any component on it, which is what `pm::ui`'s creators hand back. Each half goes where it belongs: the
map's `Component.GetComponent` is `Component`'s own method, so its object has to be a component (for a
GameObject the helpers use that object's `Transform`, which every GameObject has), while
`GameObject.AddComponent` runs on the GameObject. Both mixed up is what left every row of the dev menu
without a button on 2026-11-11.

## How a click reaches a Button

`Button.onClick` (a `ButtonClickedEvent`, Unity's own) is invoked by two paths, read from the build on
2026-11-12. A pointer click runs `Button.OnPointerClick`, which returns at once unless the event's button is
the left one, and then runs the button's own invoke list after two guards on the button itself: Unity's UI
runs its active and interactable checks here, so a button that is off, hidden or not interactable runs
nothing. A submit of the selected object (Enter, or the gamepad's A button) runs `Button.OnSubmit`, which
calls `Button.Press` and starts a short finish coroutine, and `Press` walks the same invoke list. Either way
the listener runs inside the game's managed call, so a mod's handler must not let an exception escape
(`sdk/api/buttons.h`).

A scene always has an `EventSystem`: the game's `EventSystemChecker`, present where Unity's own may be
absent, makes one with its input module at scene start when the scene has none (decompiled 2026-11-11).

## A list that scrolls

`ScrollRect` (UnityEngine.UI) moves a content RectTransform inside a viewport, keeps it inside the viewport's
bounds and answers the pointer itself: it is a drag handler and a scroll handler, so a wheel turn or a drag only
reaches it through the canvas's `GraphicRaycaster` and one graphic inside the viewport whose `raycastTarget` is
on (`Graphic.set_raycastTarget`). It clips nothing of its own: put a `RectMask2D` on the viewport, not on the
content, or rows outside the view draw over the rest of the screen.

Two kinds of object are in play, and they are not interchangeable. A `GameObject` is not a `Component`: its own
transform comes from `GameObject.get_transform` and a component's from `Component.get_transform`, and asking one
through the other's accessor reads a layout that object does not have. The game faults on it, which is what the
dev menu's first scroll list did on 2026-11-12: the helper that made the viewport and the content returned
GameObjects, and the layout helper asked them through the component accessor. `pm::game::transform_of` and
`pm::game::game_object_of` ask the object what it is (`GameObject` type through `pm::api::Type::is`) and answer
null when the class cannot be read rather than guess.

The same shape decides what a layout entry may act on. The entries that place a UI object (`RectTransform.set_anchorMin`,
`set_anchorMax`, `set_pivot`, `set_anchoredPosition`, `set_sizeDelta`), the `ScrollRect.content` and
`ScrollRect.viewport` fields and both `RectTransformUtility` hit tests all want a RectTransform, and an object only
has one once something asks for it: every UI class in the game's own UnityEngine.UI declares
`[RequireComponent(typeof(RectTransform))]` (`Graphic`, so `Image` and `RawImage`, `TextMeshProUGUI`, `RectMask2D`,
`ScrollRect` and `LayoutElement`). In game an object made bare and then given one of those components does answer
the layout entries; that is how every row of the dev menu and of the emotes wheel is made. Laying an object out
before it takes any component has never worked. What converts the transform has not been read, and a mod does not
need it: put the components on first and lay out afterwards, and read the transform back through
`pm::game::rect_transform_of`, which answers null for an object that has no RectTransform instead of faulting.
A list's content shows nothing itself, so it carries a `LayoutElement`: it draws nothing, and only a LayoutGroup
ever reads what it holds.

Unity's editor builds the three objects into the scene; a mod makes them at run time, and a ScrollRect made that
way holds the free-running mode, because the editor's Reset never runs. So `ScrollRect.movementType` wants
`ScrollRect.MovementType.Clamped`, or one drag can throw the list out of the menu for good.
`ScrollRect.content` has to be a child of `ScrollRect.viewport`. Pin the content to the viewport's top edge
(anchors (0,1) to (1,1), pivot (0.5,1)) and give it a `sizeDelta` height of the whole list, and every row then
sits at a negative y below that edge: the list can grow and shrink without any row moving.
`ScrollRect.set_verticalNormalizedPosition` moves it, 1 at the top and 0 at the bottom; the getter updates the
rect's own bounds first, so it answers correctly even on a view built this frame. `ScrollRect.StopMovement`
drops a drag's inertia.

Keyboard and gamepad selection moves inside the game, which tells no mod. The event system's
`get_currentSelectedGameObject` says where it got to, which is how a mod keeps the selected row in view. The
loader's own helpers do all of this (create_scroll_view, set_scroll_height and scroll_reveal in `core/ui.h`),
and `pm::ui::scroll_top_needed` is the same arithmetic for a list laid out differently.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/menus.md).
