#pragma once

/// @file
/// @brief Buttons from the game's own UI: a mod's graphic gets the game's Button component, so hover, press,
/// selection and arrow-key navigation are the game's own, and the mod's code runs on the click.
///
/// Two things must hold before a mod's canvas answers the player at all. The canvas needs a GraphicRaycaster
/// (catch_pointer): without one its graphics catch nothing and clicks and hover pass through to the game's UI.
/// And Unity's UI input runs through the EventSystem's one input module, which the game keeps switched off
/// except while its journal is open (JournalController.OpenCloseJournal): in a contract no canvas of any kind
/// takes clicks and no menu takes arrow keys until that module is on again (input_module, input_on, input_set).
///
/// A click reaches the mod through a UnityAction the host makes (the contract's unity_action_new), added to the
/// button's own onClick: the game's Button.Press and a submit of the selected button both invoke it, and the
/// listener runs inside the game's managed call, so the mod's function must not let an exception escape.
///
/// Main thread. Local client only.
/// @ingroup runtime
/// @since Contract 2.4

#include "held.h"

namespace pm::api::buttons {

/// The game's Button on `gameObject`: the one it already has, or a new one added to it. The object needs a
/// Graphic for the pointer to hit it (pm::ui::create_image makes one, and it catches the pointer by default).
/// @param gameObject The object to make a button.
/// @return The Button component; null for null, or when the game's entries did not resolve.
void* attach(void* gameObject);

/// The GraphicRaycaster of the canvas `gameObject`: the one it already has, or a new one added to it.
/// @param canvasGameObject The canvas's own GameObject.
/// @return The GraphicRaycaster; null for null, or when the game's entries did not resolve.
void* catch_pointer(void* canvasGameObject);

/// Runs `fn(user)` when the game invokes that button's onClick: its own Press, or Enter or the gamepad A
/// button on the selected button. Main thread, inside the game's managed call: `fn` must not let an exception
/// escape (the SDK logs one instead of letting it reach the game). Re-wiring a button replaces its wiring.
/// The wiring lives while the button does; `user` must live at least as long, or the button be re-wired first.
/// @param buttonOrGameObject The Button component, or the object it sits on.
/// @param fn The mod's handler, run with `user` on each click.
/// @param user The mod's own value, handed to `fn`; may be null.
/// @return False when there is no Button, or its onClick or the action could not be made.
bool on_click(void* buttonOrGameObject, void (*fn)(void* user), void* user);

/// Takes a button's wiring off: it no longer calls the mod's handler. The action stays in the button's
/// onClick and does nothing; the game's own listeners are untouched. Nothing for a button that was not
/// wired or is already gone.
/// @param buttonOrGameObject The Button component, or the object it sits on.
void forget(void* buttonOrGameObject);

/// Unity's UI input module, the EventSystem's one (a Behaviour).
/// @return The module; null when no EventSystem is running or the game's entries did not resolve.
void* input_module();

/// Whether that module takes input; the game runs it only while its journal is open.
/// @param module The module from input_module().
/// @return Its enabled state; false for null.
bool input_on(void* module);

/// Switches that module's input on or off, as the game's journal toggle does.
/// @param module The module from input_module(); null does nothing.
/// @param on Whether it takes input now.
void input_set(void* module, bool on);

/// Moves the keyboard and gamepad selection to the Selectable on `gameObjectOrComponent`, so arrow-key and
/// stick navigation starts from it. Null clears the selection.
/// @param gameObjectOrComponent The GameObject of the row to select, or a component on it; null clears.
/// @return False when no EventSystem is running or the game's entries did not resolve.
bool select(void* gameObjectOrComponent);

}  // namespace pm::api::buttons
