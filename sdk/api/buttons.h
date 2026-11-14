#pragma once

/// @file
/// @brief Buttons from the game's own UI: a mod's graphic gets the game's Button component, so hover, press,
/// selection and arrow-key navigation are the game's own, and the mod's code runs on the click.
///
/// Three things must hold before a mod's canvas answers the player at all. The canvas needs a GraphicRaycaster
/// (catch_pointer): a Canvas does no hit-testing of its own, so without one its graphics are never hit and
/// clicks and hover go to whatever else has a raycaster. Each graphic that should be hit needs its
/// raycastTarget on (attach switches it on for the row's own graphic). And Unity's UI input runs through the
/// EventSystem's one input module, which the game keeps switched off except while its journal is open
/// (JournalController.OpenCloseJournal): in a contract no canvas of any kind takes clicks and no menu takes
/// arrow keys until that module is on again (input_module, input_on, input_set).
///
/// A click reaches the mod through a UnityAction the host makes (the contract's unity_action_new), added to the
/// button's own onClick, which the game invokes on both paths: Button.OnPointerClick does it for a pointer
/// click of the left button, and Button.OnSubmit calls Button.Press, which does it for Enter or the gamepad A
/// button on the selected button. The listener runs inside the game's managed call either way, so the mod's
/// function must not let an exception escape. A Button that is off, hidden or not interactable runs nothing:
/// the game guards both paths before invoking.
///
/// Main thread. Local client only.
/// @ingroup runtime
/// @since Contract 2.4

#include "UnityObject.h"
#include "held.h"

namespace pm::api::buttons {

/// The game's Button on the object: the one it already has, or a new one added to it. Pass the object's
/// GameObject or any component on it (pm::ui::create_image hands back the component). The Button's graphic
/// is the object's own Graphic, and attach switches that Graphic's raycastTarget on, so the game's pointer
/// actually hits the row (pm::ui::create_image makes pictures that do not catch the pointer, and one that
/// does not catch it leaves the Button unhoverable and unclickable).
/// @param gameObjectOrComponent The object to make a button: its GameObject, or a component on it.
/// @return The Button component as an Object handle; null for null, or when the game's entries did not
/// resolve.
Object attach(void* gameObjectOrComponent);

/// The GraphicRaycaster of the canvas: the one it already has, or a new one added to it. Pass the canvas's
/// GameObject or its Canvas component (pm::ui::create_canvas hands back the component).
/// @param canvasGameObjectOrComponent The canvas: its GameObject, or a component on it.
/// @return The GraphicRaycaster as an Object handle; null for null, or when the game's entries did not
/// resolve.
Object catch_pointer(void* canvasGameObjectOrComponent);

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
/// @return The module as an Object handle; null when no EventSystem is running or the game's entries did
/// not resolve.
Object input_module();

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

/// The GameObject the keyboard or gamepad selection is on now: what select() set, or wherever the game's own
/// arrow-key and stick navigation has moved it. A mod with a scrolling list reads it each frame to follow
/// the player's selection back into view (the arrow keys move the selection inside the game, which tells the
/// mod nothing).
/// @return A Ref<GameObject> to the selected GameObject; null when no EventSystem is running, nothing is
/// selected, or the game's entries did not resolve.
Ref<pm::map::GameObject> selected();

}  // namespace pm::api::buttons
