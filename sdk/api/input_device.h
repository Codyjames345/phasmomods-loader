#pragma once

/// @file
/// @brief Which input the local player uses, as the game decides it: keyboard and mouse, or a gamepad.
///
/// The game's glyph widgets (GamepadButton) show a gamepad button's glyph only while its own check,
/// InputDevices.UsesKeyboard, answers false for the player's PlayerInput (GamepadButton.CheckInputDevice): the check
/// compares the PlayerInput's current control scheme with the game's keyboard scheme, and answers false while VR runs.
/// keyboard_in_use() asks that check about the local rig's PlayerInput (LocalPlayer.playerInput), so a mod shows a
/// gamepad glyph exactly when the game would.
///
/// Main thread. Local only.
/// @ingroup runtime
/// @since Contract 2.6

#include <optional>

namespace pm::api::input {

/// Whether this client's player uses keyboard and mouse now, by the game's own check (InputDevices.UsesKeyboard of
/// LocalPlayer.playerInput). False while VR runs, as the game's check answers then.
/// @return The answer, or nothing with no local rig or no PlayerInput, or where an entry it reads did not resolve.
std::optional<bool> keyboard_in_use();

}  // namespace pm::api::input
