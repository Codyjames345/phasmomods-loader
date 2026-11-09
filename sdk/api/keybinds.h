#pragma once

/// @file
/// @brief A mod's own keys, which players rebind in the game's keybinds menu: a key per action a mod's player presses
/// (the Emotes mod's wheel is one), with a default per device that falls back when the game already uses it.
///
/// A key is three Input System actions in an InputActionAsset of the mod's own (Unity refuses a keybinds row for an
/// action outside an asset), one per device. Its defaults are chosen against the game's own bindings, read from the
/// local player's controls (LocalPCPlayer.controls) or a row of the keybinds menu, so a key is not ready until one of
/// those is loaded. The keyboard's and the gamepad's bindings get a row each in the game's keybinds menu: a copy of one
/// of the game's rows (RebindActionUI) pointed at the mod's action, labelled with the mod's own text, so the game's own
/// rebinding runs on it. A player's rebinding is saved to the file the mod names and read back at the next start.
/// The game's side is in sdk/systems/menus.md, "Key bindings".
///
/// Main thread. Local only: nothing is sent. Call begin_keys and add_key from pmmod_start, tick_keys every frame, and
/// clear_keys from pmmod_stop.
/// @ingroup runtime
/// @since Contract 2.3

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "binding_rule.h"

namespace pm::api::keys {

/// A key a mod declares.
struct ModKey {
  std::string id;     ///< unique within the mod; it names the key's actions and its saved rebindings
  std::string label;  ///< what its keybinds rows show: player-facing text from the mod's string table
  /// The path wanted on each device (indexed by Device), or empty for none (no action on that device).
  std::array<std::string, kDevices> paths;
  /// Paths to fall back to, in order, on each device when the game binds the one wanted.
  std::array<std::vector<std::string>, kDevices> fallbacks;
};

/// A key's state in this frame, over all its devices.
struct KeyState {
  bool held = false;      ///< held on any device now
  bool pressed = false;   ///< went down on any device this frame
  bool released = false;  ///< went up this frame on a device, and is held on none
};

/// Starts a mod's keys: the name of the mod's action asset and map, and the file its players' rebindings are saved
/// in (its folder is made when the file is written). Forgets any keys added before.
/// @param owner The mod's id, which names its action asset and map.
/// @param file The rebindings' file, normally in the mod's own folder of the host's data directory.
void begin_keys(std::string_view owner, std::filesystem::path file);

/// Declares a key. Its actions are made on a later tick_keys, once the game's bindings can be read.
/// @param key The key.
/// @return False for an empty id, an id already added, or a key with no path on any device.
bool add_key(const ModKey& key);

/// Each frame: makes the keys' actions once the game's bindings can be read (choosing the defaults, applying saved
/// rebindings and enabling them), puts a row for each in the keybinds menu wherever the menu is loaded and has none,
/// and saves a rebinding the player made since the last frame.
void tick_keys();

/// Whether the keys' actions are made.
/// @return False until a tick_keys has read the game's bindings.
bool keys_ready();

/// A key's state this frame.
/// @param id The key's id.
/// @return Its state over its devices; all false before keys_ready, for an unknown id, or while unresolved.
KeyState key_state(std::string_view id);

/// The path a key is bound to on a device now: the player's rebinding, or its default.
/// @param id The key's id.
/// @param device The device.
/// @return The path, or empty before keys_ready, for an unknown id or a device it does not bind.
std::string bound_path(std::string_view id, Device device);

/// Ends the keys: disables their actions, removes their rows from the keybinds menu and forgets them. Nothing is saved
/// here: a rebinding was saved on the frame it was made.
void clear_keys();

}  // namespace pm::api::keys
