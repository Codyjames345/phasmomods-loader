#include "bindings.h"

#include <windows.h>

#include <cctype>
#include <cstring>

#include "game.h"
#include "il2cpp.h"

namespace pg::bindings {
namespace {

// UnityEngine.InputSystem layouts (Unity.InputSystem in this build).
constexpr size_t kActionId = 0x28;             // InputAction.m_Id
constexpr size_t kActionMap = 0xC8;            // InputAction.m_ActionMap
constexpr size_t kMapBindings = 0x30;          // InputActionMap.m_Bindings (InputBinding[])
constexpr size_t kBindingSize = 0x58;          // sizeof(InputBinding)
constexpr size_t kBindingName = 0x00;          // InputBinding.m_Name (composite part name)
constexpr size_t kBindingPath = 0x10;          // InputBinding.m_Path
constexpr size_t kBindingAction = 0x30;        // InputBinding.m_Action
constexpr size_t kBindingOverridePath = 0x40;  // InputBinding.m_OverridePath

std::string str(const il2cpp::String* s) {
  char buf[128];
  il2cpp::to_utf8(s, buf, sizeof buf);
  return buf;
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

}  // namespace

Key for_action(void* localPlayer, const char* actionName, const char* part) {
  Key found;
  game::for_each_input_action(localPlayer, [&](void* action, const char*, const char* name) {
    if (found.vk || std::strcmp(name, actionName) != 0) return;
    void* map = il2cpp::field<void*>(action, kActionMap);
    auto* list = map ? il2cpp::field<il2cpp::Array*>(map, kMapBindings) : nullptr;
    const std::string id = str(il2cpp::field<il2cpp::String*>(action, kActionId));
    for (uintptr_t i = 0; list && i < list->max_length && !found.vk; ++i) {
      const char* b = list->data<char>() + i * kBindingSize;
      const std::string target = str(il2cpp::field<il2cpp::String*>(b, kBindingAction));  // name or ID
      if (target != actionName && (id.empty() || target != id)) continue;
      if (part && _stricmp(str(il2cpp::field<il2cpp::String*>(b, kBindingName)).c_str(), part) != 0) continue;
      std::string path = str(il2cpp::field<il2cpp::String*>(b, kBindingOverridePath));
      if (path.empty()) path = str(il2cpp::field<il2cpp::String*>(b, kBindingPath));
      found = key_for_path(path);  // the first keyboard/mouse binding wins
    }
  });
  return found;
}

bool down(const Key& key) { return key.vk && (GetAsyncKeyState(key.vk) & 0x8000) != 0; }

}  // namespace pg::bindings
