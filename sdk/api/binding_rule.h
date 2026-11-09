#pragma once

/// @file
/// @brief The rules behind a mod's rebindable keys (keybinds.h), free of game calls: which default a key takes when
/// the game already binds the one it wants, which device a binding path is for, and the text a player's rebinding is
/// saved as.
/// @ingroup runtime
/// @since Contract 2.3

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pm::api::keys {

/// The devices a key binds on, one binding each.
enum class Device : uint8_t {
  Keyboard = 0,  ///< a keyboard or mouse path ("<Keyboard>/x", "<Mouse>/leftButton")
  Gamepad = 1,   ///< a gamepad path ("<Gamepad>/dpad/up" and the vendor layouts)
  Vr = 2,        ///< a VR controller path ("<XRController>{LeftHand}/{PrimaryButton}")
};

/// How many devices a key binds on.
inline constexpr size_t kDevices = 3;

/// Whether two Input System binding paths name the same control: compared without regard to case, as the Input System
/// matches control paths.
/// @param a A binding path.
/// @param b Another.
/// @return True when equal ignoring case; false when either is empty.
bool same_path(std::string_view a, std::string_view b);

/// Whether the game binds a path already.
/// @param path The path a key wants.
/// @param gameBindings Every binding path of the game's own actions.
/// @return True when one of them is the same path (same_path).
bool path_taken(std::string_view path, std::span<const std::string> gameBindings);

/// The default a key takes on one device: the path it wants, or else the first of its fallbacks the game leaves free.
/// @param wanted The path the mod wants; empty for none.
/// @param fallbacks Paths to try in order when `wanted` is taken.
/// @param gameBindings Every binding path of the game's own actions.
/// @return The path chosen, or empty when `wanted` is empty or every choice is taken.
std::string pick_default(std::string_view wanted, std::span<const std::string> fallbacks,
                         std::span<const std::string> gameBindings);

/// Whether a game action only moves through menus (the game names those "UI...": UIUp, UIDown, UISelectSouth): a
/// mod's key may share its binding, since the game reads it only while a menu is open, so its bindings are not counted
/// as taken.
/// @param action The action's name.
/// @return True for a name that begins with "UI".
bool menu_only(std::string_view action);

/// The device a binding path is for, by its layout: XR controllers are Vr; gamepads and their vendor layouts
/// (XInput, DualShock, DualSense, Switch Pro, joysticks) are Gamepad; anything else Keyboard.
/// @param path A binding path.
/// @return Its device.
Device device_of(std::string_view path);

/// One saved rebinding: a key's binding on one device.
struct SavedBinding {
  std::string key;   ///< the key's id
  Device device;     ///< its device
  std::string path;  ///< the path the player chose
};

/// The text a mod's rebindings are saved as: one line each, the key's id, the device's number and the path, separated
/// by tabs.
/// @param saved The rebindings.
/// @return The text.
std::string format_saved(std::span<const SavedBinding> saved);

/// Reads the text format_saved writes. A line without three fields, with an unknown device or an empty key or path is
/// passed over.
/// @param text The file's text.
/// @return The rebindings it holds, in order.
std::vector<SavedBinding> parse_saved(std::string_view text);

}  // namespace pm::api::keys
