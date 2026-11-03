#include "bindings.h"

#include <windows.h>

#include <cctype>
#include <cstring>

#include "Input.h"
#include "Player.h"
#include "runtime.h"

namespace pm::bindings {
namespace {

// Visits each input action of the PC player's controls: the InputActionAsset of LocalPCPlayer.controls,
// every action of each of its maps, with the map's and the action's names. Nothing for another kind of
// player rig, or a destroyed one.
template <class Visit>
void each_action(void* localPlayer, Visit visit) {
  if (!api::alive(localPlayer) || !phasmo::LocalPCPlayer::type.is(localPlayer)) return;
  void* controls = phasmo::LocalPCPlayer::controls.get(localPlayer);
  void* asset = controls ? phasmo::PCControls::actions.get(controls) : nullptr;
  const api::RefArray maps{asset ? phasmo::InputActionAsset::m_ActionMaps.get(asset) : nullptr};
  for (size_t i = 0; i < maps.size(); ++i) {
    void* map = maps[i];
    if (!map) continue;
    const std::string mapName = api::to_utf8(phasmo::InputActionMap::m_Name.get(map));
    const api::RefArray actions{phasmo::InputActionMap::m_Actions.get(map)};
    for (size_t j = 0; j < actions.size(); ++j)
      if (void* action = actions[j]) visit(action, mapName, api::to_utf8(phasmo::InputAction::m_Name.get(action)));
  }
}

struct Named {
  const char* control;
  int vk;
  const char* label;
};
constexpr Named kNamed[] = {
    {"<Mouse>/leftButton", VK_LBUTTON, "LMB"},     {"<Mouse>/rightButton", VK_RBUTTON, "RMB"},
    {"<Mouse>/middleButton", VK_MBUTTON, "MMB"},   {"<Mouse>/backButton", VK_XBUTTON1, "Mouse 4"},
    {"<Mouse>/forwardButton", VK_XBUTTON2, "Mouse 5"}, {"<Keyboard>/space", VK_SPACE, "Space"},
    {"<Keyboard>/enter", VK_RETURN, "Enter"},      {"<Keyboard>/tab", VK_TAB, "Tab"},
    {"<Keyboard>/escape", VK_ESCAPE, "Esc"},
    {"<Keyboard>/leftShift", VK_LSHIFT, "Shift"},  {"<Keyboard>/rightShift", VK_RSHIFT, "Shift"},
    {"<Keyboard>/leftCtrl", VK_LCONTROL, "Ctrl"},  {"<Keyboard>/rightCtrl", VK_RCONTROL, "Ctrl"},
    {"<Keyboard>/leftAlt", VK_LMENU, "Alt"},       {"<Keyboard>/rightAlt", VK_RMENU, "Alt"},
    {"<Keyboard>/backquote", VK_OEM_3, "`"},       {"<Keyboard>/capsLock", VK_CAPITAL, "Caps Lock"},
    {"<Keyboard>/upArrow", VK_UP, "Up"},           {"<Keyboard>/downArrow", VK_DOWN, "Down"},
    {"<Keyboard>/leftArrow", VK_LEFT, "Left"},     {"<Keyboard>/rightArrow", VK_RIGHT, "Right"},
};

}  // namespace

Key key_for_path(const std::string& path) {
  for (const Named& n : kNamed)
    if (_stricmp(path.c_str(), n.control) == 0) return {n.vk, n.label};
  constexpr char kKeyboard[] = "<Keyboard>/";
  if (path.rfind(kKeyboard, 0) != 0) return {};
  std::string control = path.substr(sizeof kKeyboard - 1);
  if (control.size() == 1 && std::isalnum(static_cast<unsigned char>(control[0]))) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(control[0])));
    return {c, std::string(1, c)};
  }
  if ((control[0] == 'f' || control[0] == 'F') && control.size() <= 3) {
    const int n = std::atoi(control.c_str() + 1);
    if (n >= 1 && n <= 12) return {VK_F1 + n - 1, "F" + std::to_string(n)};
  }
  return {};
}

Key for_action(void* localPlayer, const char* actionName, const char* part) {
  Key found;
  each_action(localPlayer, [&](void* action, const std::string&, const std::string& name) {
    if (found.vk || name != actionName) return;
    void* map = phasmo::InputAction::m_ActionMap.get(action);
    // The map's bindings, structs whose size the runtime gives (ValueArray's stride).
    const api::ValueArray list{map ? phasmo::InputActionMap::m_Bindings.get(map) : nullptr};
    const std::string id = api::to_utf8(phasmo::InputAction::m_Id.get(action));
    for (size_t i = 0; i < list.size() && !found.vk; ++i) {
      void* b = list.at(i);
      if (!b) break;
      const std::string target = api::to_utf8(phasmo::InputBinding::m_Action.get(b));  // name or ID
      if (target != actionName && (id.empty() || target != id)) continue;
      if (part && _stricmp(api::to_utf8(phasmo::InputBinding::m_Name.get(b)).c_str(), part) != 0) continue;
      std::string path = api::to_utf8(phasmo::InputBinding::m_OverridePath.get(b));
      if (path.empty()) path = api::to_utf8(phasmo::InputBinding::m_Path.get(b));
      found = key_for_path(path);  // the first keyboard/mouse binding wins
    }
  });
  return found;
}

bool down(const Key& key) { return key.vk && (GetAsyncKeyState(key.vk) & 0x8000) != 0; }  // pm:not-game-data: the key-down bit

}  // namespace pm::bindings
