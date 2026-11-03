#pragma once

#include <string>

// The keyboard/mouse key the player has bound to one of the game's own input actions
// (including rebinds), read from the action's InputBinding data. Used for the ghost's controls so
// they follow the player's settings even while the hunter's actions are disabled.
namespace pm::bindings {

struct Key {
  int vk = 0;            // Windows virtual-key code, 0 if unbound or unsupported
  std::string label;     // short display name, e.g. "E", "LMB"
};

// part selects a composite part ("up", "down", "left", "right" for Move), or null.
Key for_action(void* localPlayer, const char* actionName, const char* part = nullptr);
bool down(const Key& key);  // GetAsyncKeyState; the caller checks focus and cursor lock
// The key a control path names ("<Keyboard>/e", "<Mouse>/leftButton"), or none when it is not a keyboard or
// mouse control this reads.
Key key_for_path(const std::string& path);

}  // namespace pm::bindings
