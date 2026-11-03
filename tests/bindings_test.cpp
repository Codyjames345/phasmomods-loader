// Offline tests for core/bindings.cpp: the player's own key for one of the game's input actions, read
// from the PC controls' InputActionAsset through the map, over the fake game backend.
// Run: build\bindings_test.exe
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Input.h"
#include "Player.h"
#include "bindings.h"
#include "fake_game.h"
#include "game_api.h"
#include "runtime.h"

#include "check.h"

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;
using pm::bindings::Key;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x70]{};
};

// The fake's field offsets, the class's own fields after its header.
constexpr int32_t kControls = 0x10, kActions = 0x18, kMaps = 0x20;            // rig, controls, asset
constexpr int32_t kMapName = 0x10, kMapActions = 0x18, kMapBindings = 0x20;    // InputActionMap
constexpr int32_t kName = 0x10, kId = 0x18, kActionMap = 0x20;                 // InputAction
constexpr int32_t kBindName = 0x00, kBindPath = 0x08, kBindAction = 0x10, kBindOverride = 0x18;  // InputBinding
constexpr size_t kBindingSize = 0x28;  // the fake's InputBinding, read by the runtime's stride

int g_rigClass = 0, g_otherClass = 0;
Obj g_rig, g_controls, g_asset, g_map, g_move, g_interact;

void give_entries() {
  fake::reset();
  fake::type(map::LocalPCPlayer, &g_rigClass);
  fake::field(map::LocalPCPlayer_controls, kControls);
  fake::field(map::PCControls_actions, kActions);
  fake::field(map::InputActionAsset_m_ActionMaps, kMaps);
  fake::field(map::InputActionMap_m_Name, kMapName);
  fake::field(map::InputActionMap_m_Actions, kMapActions);
  fake::field(map::InputActionMap_m_Bindings, kMapBindings);
  fake::field(map::InputAction_m_Name, kName);
  fake::field(map::InputAction_m_Id, kId);
  fake::field(map::InputAction_m_ActionMap, kActionMap);
  fake::field(map::InputBinding_m_Name, kBindName);
  fake::field(map::InputBinding_m_Path, kBindPath);
  fake::field(map::InputBinding_m_Action, kBindAction);
  fake::field(map::InputBinding_m_OverridePath, kBindOverride);
  std::string missing;
  api::bind(fake::host(), missing);
}

template <class T>
T& at(void* obj, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<char*>(obj) + offset);
}

void* refs(const std::vector<void*>& items) {
  void* arr = fake::array(sizeof(void*), items.size());
  for (size_t i = 0; i < items.size(); ++i) api::RefArray{arr}.set(i, items[i]);
  return arr;
}

struct Binding {
  const char* action;
  const char* path;
  const char* override = "";
  const char* part = "";
};

// The rig with one map, "Player", of the actions Move (its id "move-id") and Interact, and these bindings.
void rig_with(const std::vector<Binding>& bindings) {
  give_entries();
  for (Obj* o : {&g_rig, &g_controls, &g_asset, &g_map, &g_move, &g_interact}) *o = Obj{};
  g_rig.klass = &g_rigClass;
  at<void*>(&g_rig, kControls) = &g_controls;
  at<void*>(&g_controls, kActions) = &g_asset;
  at<void*>(&g_asset, kMaps) = refs({&g_map});
  at<void*>(&g_map, kMapName) = api::make_string("Player");
  at<void*>(&g_map, kMapActions) = refs({&g_move, &g_interact});
  at<void*>(&g_move, kName) = api::make_string("Move");
  at<void*>(&g_move, kId) = api::make_string("move-id");
  at<void*>(&g_interact, kName) = api::make_string("Interact");
  for (Obj* a : {&g_move, &g_interact}) at<void*>(a, kActionMap) = &g_map;
  void* arr = fake::array(kBindingSize, bindings.size());
  const api::ValueArray list{arr};
  for (size_t i = 0; i < bindings.size(); ++i) {
    void* b = list.at(i);
    at<void*>(b, kBindAction) = api::make_string(bindings[i].action);
    at<void*>(b, kBindPath) = api::make_string(bindings[i].path);
    at<void*>(b, kBindOverride) = api::make_string(bindings[i].override);
    at<void*>(b, kBindName) = api::make_string(bindings[i].part);
  }
  at<void*>(&g_map, kMapBindings) = arr;
}

bool is(const Key& key, int vk, const char* label) { return key.vk == vk && key.label == label; }

// The player's rebind wins over the default path, and the first keyboard or mouse binding counts.
void rebind_wins_over_default() {
  rig_with({{"Interact", "<Gamepad>/buttonSouth"}, {"Interact", "<Keyboard>/e", "<Keyboard>/f"},
            {"Interact", "<Keyboard>/g"}});
  CHECK(is(pm::bindings::for_action(&g_rig, "Interact"), 'F', "F"));
  rig_with({{"Interact", "<Keyboard>/e"}});
  CHECK(is(pm::bindings::for_action(&g_rig, "Interact"), 'E', "E"));
}

// A composite's part is chosen by its name, and a binding may name its action by id.
void composite_part_and_action_id() {
  rig_with({{"move-id", "<Keyboard>/w", "", "up"}, {"move-id", "<Keyboard>/s", "", "down"},
            {"Move", "<Keyboard>/upArrow", "", "UP"}});
  CHECK(is(pm::bindings::for_action(&g_rig, "Move", "up"), 'W', "W"));
  CHECK(is(pm::bindings::for_action(&g_rig, "Move", "down"), 'S', "S"));
  CHECK(pm::bindings::for_action(&g_rig, "Move", "left").vk == 0);  // no such part bound
  CHECK(pm::bindings::for_action(&g_rig, "Jump").vk == 0);          // no such action
}

// Only the PC player's rig has these controls: another rig, a null one or a destroyed one gives nothing.
void other_rigs_give_nothing() {
  rig_with({{"Interact", "<Keyboard>/e"}});
  g_rig.klass = &g_otherClass;
  CHECK(pm::bindings::for_action(&g_rig, "Interact").vk == 0);
  CHECK(pm::bindings::for_action(nullptr, "Interact").vk == 0);
  g_rig.klass = &g_rigClass;
  fake::destroy(&g_rig);
  CHECK(pm::bindings::for_action(&g_rig, "Interact").vk == 0);
}

// The paths the ghost's controls understand.
void control_paths() {
  CHECK(is(pm::bindings::key_for_path("<Mouse>/leftButton"), VK_LBUTTON, "LMB"));
  CHECK(is(pm::bindings::key_for_path("<keyboard>/SPACE"), VK_SPACE, "Space"));
  CHECK(is(pm::bindings::key_for_path("<Keyboard>/q"), 'Q', "Q"));
  CHECK(is(pm::bindings::key_for_path("<Keyboard>/f12"), VK_F12, "F12"));
  CHECK(pm::bindings::key_for_path("<Keyboard>/f13").vk == 0);
  CHECK(pm::bindings::key_for_path("<Gamepad>/buttonSouth").vk == 0);
}

}  // namespace

int main() {
  rebind_wins_over_default();
  composite_part_and_action_id();
  other_rigs_give_nothing();
  control_paths();
  std::printf(g_failures ? "%d check(s) failed\n" : "all bindings tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
