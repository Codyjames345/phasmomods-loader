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

A row's label carries a `TextLocaliserUI`, which writes the game's words for its key whenever it wakes. `RebindActionUI`
itself never writes the label: neither `RebindActionUI.set_actionReference` nor its own enabling touches it; both only
refresh the binding's text. So a copied row keeps a new label only once its localiser is gone. A copy made while the
menu is hidden wakes when the player opens it.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/menus.md).
