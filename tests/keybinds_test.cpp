// Offline tests for a mod's rebindable keys (sdk/api/keybinds.h, binding_rule.h): the pure rules that pick a default
// the game leaves free and keep the player's rebinding, and over the fake backend the actions a key makes, its state
// from the Input System, its saved rebinding and its row in the game's keybinds menu. The game's side is described in
// sdk/systems/menus.md, "Key bindings". Run: build\keybinds_test.exe
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "Input.h"
#include "JournalController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "UnityUI.h"
#include "binding_rule.h"
#include "check.h"
#include "fake_game.h"
#include "keybinds.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace keys = pm::api::keys;
namespace map = pm::map;
namespace fake = pm::fake;
using keys::Device;

// ---- The pure rules ----

void paths_compare_as_the_input_system_does() {
  CHECK(keys::same_path("<Gamepad>/dpad/down", "<gamepad>/dpad/down"));
  CHECK(keys::same_path("<Keyboard>/b", "<keyboard>/B"));
  CHECK(!keys::same_path("<Keyboard>/b", "<Keyboard>/x"));
  CHECK(!keys::same_path("<Keyboard>/b", ""));
}

void a_default_the_game_uses_falls_back() {
  const std::vector<std::string> game{"<Keyboard>/b", "<Gamepad>/dpad/down", "<Gamepad>/dpad/up"};
  CHECK(keys::path_taken("<keyboard>/b", game));
  CHECK(!keys::path_taken("<Keyboard>/x", game));
  // Wanted and free: kept.
  CHECK(keys::pick_default("<Keyboard>/x", {}, game) == "<Keyboard>/x");
  // Wanted but taken: the first free fallback.
  const std::vector<std::string> pad{"<Gamepad>/dpad/up", "<Gamepad>/dpad/right"};
  CHECK(keys::pick_default("<Gamepad>/dpad/down", pad, game) == "<Gamepad>/dpad/right");
  // All taken: none.
  const std::vector<std::string> taken{"<Gamepad>/dpad/up"};
  CHECK(keys::pick_default("<Gamepad>/dpad/down", taken, game).empty());
  CHECK(keys::pick_default("", {}, game).empty());
}

void menu_navigation_is_not_taken() {
  CHECK(keys::menu_only("UIUp") && keys::menu_only("UISelectSouth"));
  CHECK(!keys::menu_only("Torch") && !keys::menu_only("") && !keys::menu_only("Ui"));
}

void a_path_names_its_device() {
  CHECK(keys::device_of("<Keyboard>/x") == Device::Keyboard);
  CHECK(keys::device_of("<Mouse>/leftButton") == Device::Keyboard);
  CHECK(keys::device_of("<Gamepad>/dpad/up") == Device::Gamepad);
  CHECK(keys::device_of("<XInputController>/buttonSouth") == Device::Gamepad);
  CHECK(keys::device_of("<DualShockGamepad>/touchpadButton") == Device::Gamepad);
  CHECK(keys::device_of("<XRController>{LeftHand}/{PrimaryButton}") == Device::Vr);
}

void a_saved_rebinding_reads_back() {
  const std::vector<keys::SavedBinding> saved{{"emote", Device::Keyboard, "<Keyboard>/n"},
                                              {"emote", Device::Gamepad, "<Gamepad>/buttonNorth"}};
  const std::string text = keys::format_saved(saved);
  const std::vector<keys::SavedBinding> back = keys::parse_saved(text);
  CHECK(back.size() == 2);
  CHECK(back.size() == 2 && back[0].key == "emote" && back[0].device == Device::Keyboard && back[0].path == "<Keyboard>/n");
  CHECK(back.size() == 2 && back[1].device == Device::Gamepad && back[1].path == "<Gamepad>/buttonNorth");
  // A line it cannot read is passed over, the rest kept.
  CHECK(keys::parse_saved("junk\nemote\t9\t<Keyboard>/n\nemote\t0\t<Keyboard>/m\n").size() == 1);
  CHECK(keys::parse_saved("").empty());
}

// ---- Over the fake game ----

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x80]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

// Offsets in the fake objects.
constexpr int32_t kMapBindings = 0x08;                                   // InputActionMap.m_Bindings
constexpr int32_t kActionMap = 0x08, kActionPressed = 0x10, kActionDown = 0x11, kActionUp = 0x12;  // the fake's own
constexpr int32_t kActionIndex = 0x14;  // the fake's own: the action's binding in its map
constexpr int32_t kBindPath = 0x00, kBindOverride = 0x08, kBindId = 0x10, kBindAction = 0x18;  // in an InputBinding
constexpr int32_t kBindingSize = 0x20;
constexpr int32_t kAssetMaps = 0x08;                                                            // m_ActionMaps
constexpr int32_t kLocalControls = 0x08, kControlsActions = 0x08;  // LocalPCPlayer.controls, PCControls.actions
constexpr int32_t kRowAction = 0x08, kRowBindingId = 0x10, kRowLabel = 0x18;  // RebindActionUI's fields
constexpr int32_t kRefAction = 0x08;                                          // the fake reference's action

Obj g_assetClass, g_mapClass, g_actionClass, g_rowClass, g_refClass, g_textClass;
Obj g_gameAsset, g_gameMap, g_localRig, g_controls;
std::vector<Obj*> g_made;  // every fake object the code under test made
std::vector<std::pair<void*, std::string>> g_overrides;  // ApplyBindingOverride(action, path)
std::map<void*, std::string> g_texts;                    // TMP_Text.set_text
std::vector<void*> g_rows;                                // what FindObjectsOfType answers for RebindActionUI
Obj g_net;
constexpr int32_t kNetLocalRig = 0x08;  // Network.localRig
void* fake_network(void*) { return &g_net; }

Obj* make_obj(void* klass) {
  auto* o = new Obj{};
  o->klass = klass;
  g_made.push_back(o);
  return o;
}

// The game's map: Player's bindings, one of them D-pad down.
void* binding_array(const std::vector<std::string>& paths, const std::vector<std::string>& actions = {}) {
  void* arr = fake::array(kBindingSize, paths.size());
  pm::api::ValueArray va{arr};
  for (size_t i = 0; i < paths.size(); ++i) {
    at<void*>(va.at(i), kBindPath) = pm::api::make_string(paths[i]);
    at<void*>(va.at(i), kBindId) = pm::api::make_string("game-" + std::to_string(i));
    if (i < actions.size()) at<void*>(va.at(i), kBindAction) = pm::api::make_string(actions[i]);
  }
  return arr;
}

void* fake_create_instance(void*, void*) { return make_obj(&g_assetClass); }
void fake_set_hide_flags(void*, int32_t, void*) {}
void* fake_add_map(void* asset, void*, void*) {
  Obj* m = make_obj(&g_mapClass);
  at<void*>(m, kMapBindings) = fake::array(kBindingSize, 0);
  (void)asset;
  return m;
}
// Unity refuses to change an asset while one of its actions is enabled ("Cannot add, remove, or change elements of
// InputActionAsset ... while one or more of its actions are enabled"): the fake counts such an add instead of throwing.
int g_enabledActions = 0, g_addsWhileEnabled = 0;
void* fake_add_action(void* map, void* name, int32_t, void* path, void*, void*, void*, void*, void*) {
  if (g_enabledActions > 0) ++g_addsWhileEnabled;
  Obj* a = make_obj(&g_actionClass);
  at<void*>(a, kActionMap) = map;
  at<int32_t>(a, kActionIndex) = static_cast<int32_t>(pm::api::ValueArray{at<void*>(map, kMapBindings)}.size());
  // The map's bindings grow by one, its path and a new id.
  pm::api::ValueArray old{at<void*>(map, kMapBindings)};
  void* grown = fake::array(kBindingSize, old.size() + 1);
  pm::api::ValueArray now{grown};
  for (size_t i = 0; i < old.size(); ++i) std::memcpy(now.at(i), old.at(i), kBindingSize);
  at<void*>(now.at(old.size()), kBindPath) = path;
  at<void*>(now.at(old.size()), kBindId) = pm::api::make_string("id-" + pm::api::to_utf8(name));
  at<void*>(map, kMapBindings) = grown;
  return a;
}
void fake_enable(void*, void*) { ++g_enabledActions; }
void fake_apply_override(void* action, void* path, void*, void*, void*) {
  g_overrides.emplace_back(action, pm::api::to_utf8(path));
  // As the Input System does: the binding's override path is set.
  void* map = at<void*>(action, kActionMap);
  pm::api::ValueArray b{at<void*>(map, kMapBindings)};
  at<void*>(b.at(static_cast<size_t>(at<int32_t>(action, kActionIndex))), kBindOverride) = path;
}
void* fake_action_map(void* action, void*) { return at<void*>(action, kActionMap); }
bool fake_is_pressed(void* a, void*) { return at<bool>(a, kActionPressed); }
bool fake_was_pressed(void* a, void*) { return at<bool>(a, kActionDown); }
bool fake_was_released(void* a, void*) { return at<bool>(a, kActionUp); }
void* fake_find_all(void*, bool, void*) {
  void* arr = fake::array(sizeof(void*), g_rows.size());
  for (size_t i = 0; i < g_rows.size(); ++i) *static_cast<void**>(pm::api::ValueArray{arr}.at(i)) = g_rows[i];
  return arr;
}
void* fake_get_action(void* ref, void*) { return at<void*>(ref, kRefAction); }
void* fake_create_ref(void* action, void*) {
  Obj* r = make_obj(&g_refClass);
  at<void*>(r, kRefAction) = action;
  return r;
}
void fake_set_ref(void* row, void* ref, void*) { at<void*>(row, kRowAction) = ref; }
void fake_set_binding_id(void* row, void* id, void*) { at<void*>(row, kRowBindingId) = id; }
void fake_set_text(void* text, void* s, void*) { g_texts[text] = pm::api::to_utf8(s); }

void give_entries() {
  fake::reset();
  for (Obj* o : g_made) delete o;
  g_made.clear();
  g_overrides.clear();
  g_enabledActions = g_addsWhileEnabled = 0;
  g_texts.clear();
  g_rows.clear();
  fake::type(map::InputActionAsset, &g_assetClass);
  fake::type(map::RebindActionUI, &g_rowClass);
  fake::method(map::ScriptableObject_CreateInstance, reinterpret_cast<void*>(&fake_create_instance));
  fake::method(map::UnityObject_set_hideFlags, reinterpret_cast<void*>(&fake_set_hide_flags));
  fake::value(map::HideFlags_DontUnloadUnusedAsset, 32);
  fake::value(map::InputActionType_Button, 1);
  fake::method(map::InputActionSetupExtensions_AddActionMap, reinterpret_cast<void*>(&fake_add_map));
  fake::method(map::InputActionSetupExtensions_AddAction, reinterpret_cast<void*>(&fake_add_action));
  fake::method(map::InputAction_Enable, reinterpret_cast<void*>(&fake_enable));
  fake::method(map::InputActionRebindingExtensions_ApplyBindingOverride,
               reinterpret_cast<void*>(&fake_apply_override));
  fake::field(map::InputAction_m_ActionMap, kActionMap);
  fake::field(map::InputActionMap_m_Bindings, kMapBindings);
  fake::field(map::InputBinding_m_Path, kBindPath);
  fake::field(map::InputBinding_m_OverridePath, kBindOverride);
  fake::field(map::InputBinding_m_Id, kBindId);
  fake::field(map::InputBinding_m_Action, kBindAction);
  fake::field(map::InputActionAsset_m_ActionMaps, kAssetMaps);
  fake::field(map::LocalPCPlayer_controls, kLocalControls);
  fake::field(map::PCControls_actions, kControlsActions);
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localRig, kNetLocalRig);
  fake::method(map::InputAction_IsPressed, reinterpret_cast<void*>(&fake_is_pressed));
  fake::method(map::InputAction_WasPressedThisFrame, reinterpret_cast<void*>(&fake_was_pressed));
  fake::method(map::InputAction_WasReleasedThisFrame, reinterpret_cast<void*>(&fake_was_released));
  fake::method(map::UnityObject_FindObjectsOfType_inactive, reinterpret_cast<void*>(&fake_find_all));
  fake::field(map::RebindActionUI_m_Action, kRowAction);
  fake::field(map::RebindActionUI_m_BindingId, kRowBindingId);
  fake::field(map::RebindActionUI_m_ActionLabel, kRowLabel);
  fake::method(map::InputActionReference_get_action, reinterpret_cast<void*>(&fake_get_action));
  fake::method(map::InputActionReference_Create, reinterpret_cast<void*>(&fake_create_ref));
  fake::method(map::RebindActionUI_set_actionReference, reinterpret_cast<void*>(&fake_set_ref));
  fake::method(map::RebindActionUI_set_bindingId, reinterpret_cast<void*>(&fake_set_binding_id));
  fake::method(map::TMP_Text_set_text, reinterpret_cast<void*>(&fake_set_text));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  // The local rig's controls hold the game's asset with one map: Player's bindings.
  void* maps = fake::array(sizeof(void*), 1);
  *static_cast<void**>(pm::api::ValueArray{maps}.at(0)) = &g_gameMap;
  at<void*>(&g_gameAsset, kAssetMaps) = maps;
  at<void*>(&g_gameMap, kMapBindings) =
      binding_array({"<Keyboard>/b", "<Gamepad>/dpad/down", "<Gamepad>/dpad/up", "<Keyboard>/e"},
                    {"GlobalPushToTalk", "Torch", "UIUp", "Pickup"});
  at<void*>(&g_controls, kControlsActions) = &g_gameAsset;
  at<void*>(&g_localRig, kLocalControls) = &g_controls;
  at<void*>(&g_net, kNetLocalRig) = &g_localRig;
}

std::filesystem::path temp_file(const char* name) {
  const auto p = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove(p);
  return p;
}

keys::ModKey emote_key() {
  keys::ModKey k;
  k.id = "emote";
  k.label = "Emote";
  k.paths[0] = "<Keyboard>/b";
  k.fallbacks[0] = {"<Keyboard>/x"};
  k.paths[1] = "<Gamepad>/dpad/down";
  k.fallbacks[1] = {"<Gamepad>/dpad/up", "<Gamepad>/dpad/right"};
  k.paths[2] = "<XRController>{LeftHand}/{PrimaryButton}";
  return k;
}

void a_key_takes_the_defaults_the_game_leaves_free() {
  give_entries();
  keys::begin_keys("test.mod", "Test Mod", temp_file("pm_keys_free.txt"));
  CHECK(keys::add_key(emote_key()));
  CHECK(!keys::add_key(emote_key()));  // one id once
  CHECK(!keys::keys_ready());
  keys::tick_keys();
  CHECK(keys::keys_ready());
  // Every action is added before any is enabled, as Unity requires of an asset.
  CHECK(g_enabledActions > 0 && g_addsWhileEnabled == 0);
  // B and D-pad down are the game's (walkie push to talk; torch), so the fallbacks are taken: X, and D-pad right
  // because D-pad up is the game's too.
  CHECK(keys::bound_path("emote", Device::Keyboard) == "<Keyboard>/x");
  // D-pad up is the game's too, but only for menu navigation (UIUp): a key may share it.
  CHECK(keys::bound_path("emote", Device::Gamepad) == "<Gamepad>/dpad/up");
  CHECK(keys::bound_path("emote", Device::Vr) == "<XRController>{LeftHand}/{PrimaryButton}");
  CHECK(keys::bound_path("other", Device::Keyboard).empty());
  keys::clear_keys();
}

void a_key_waits_for_the_games_bindings() {
  give_entries();
  at<void*>(&g_net, kNetLocalRig) = nullptr;  // no local player and no menu rows yet
  keys::begin_keys("test.mod", "Test Mod", temp_file("pm_keys_wait.txt"));
  keys::add_key(emote_key());
  keys::tick_keys();
  CHECK(!keys::keys_ready());
  CHECK(!keys::key_state("emote").held);
  at<void*>(&g_net, kNetLocalRig) = &g_localRig;
  keys::tick_keys();
  CHECK(keys::keys_ready());
  keys::clear_keys();
}

void the_state_is_any_devices() {
  give_entries();
  keys::begin_keys("test.mod", "Test Mod", temp_file("pm_keys_state.txt"));
  keys::add_key(emote_key());
  keys::tick_keys();
  // The actions the key made, in device order.
  std::vector<Obj*> actions;
  for (Obj* o : g_made)
    if (o->klass == &g_actionClass) actions.push_back(o);
  CHECK(actions.size() == 3);
  if (actions.size() != 3) return;
  CHECK(!keys::key_state("emote").held);
  at<bool>(actions[1], kActionPressed) = true;
  at<bool>(actions[1], kActionDown) = true;
  keys::KeyState s = keys::key_state("emote");
  CHECK(s.held && s.pressed && !s.released);
  at<bool>(actions[1], kActionPressed) = false;
  at<bool>(actions[1], kActionDown) = false;
  at<bool>(actions[1], kActionUp) = true;
  s = keys::key_state("emote");
  CHECK(!s.held && !s.pressed && s.released);
  // Released on one device while held on another is no release.
  at<bool>(actions[0], kActionPressed) = true;
  s = keys::key_state("emote");
  CHECK(s.held && !s.released);
  keys::clear_keys();
}

void a_rebinding_is_saved_and_read_back() {
  give_entries();
  const auto file = temp_file("pm_keys_saved.txt");
  {
    std::ofstream out(file, std::ios::binary);
    const std::vector<keys::SavedBinding> one{{"emote", Device::Keyboard, "<Keyboard>/n"}};
    out << keys::format_saved(one);
  }
  keys::begin_keys("test.mod", "Test Mod", file);
  keys::add_key(emote_key());
  keys::tick_keys();
  CHECK(g_overrides.size() == 1 && g_overrides[0].second == "<Keyboard>/n");
  CHECK(keys::bound_path("emote", Device::Keyboard) == "<Keyboard>/n");
  // The player rebinds the gamepad's in the menu: the next tick saves it beside the keyboard's.
  std::vector<Obj*> actions;
  for (Obj* o : g_made)
    if (o->klass == &g_actionClass) actions.push_back(o);
  if (actions.size() == 3) {
    void* map = at<void*>(actions[1], kActionMap);
    at<void*>(pm::api::ValueArray{at<void*>(map, kMapBindings)}.at(static_cast<size_t>(at<int32_t>(actions[1], kActionIndex))), kBindOverride) =
        pm::api::make_string("<Gamepad>/buttonNorth");
  }
  keys::tick_keys();
  std::ifstream in(file, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const auto saved = keys::parse_saved(text);
  CHECK(saved.size() == 2);
  bool pad = false, board = false;
  for (const auto& s : saved) {
    pad |= s.device == Device::Gamepad && s.path == "<Gamepad>/buttonNorth";
    board |= s.device == Device::Keyboard && s.path == "<Keyboard>/n";
  }
  CHECK(pad && board);
  keys::clear_keys();
}

// ---- A row in the keybinds menu ----

// A game row is a component on a GameObject whose transform sits in a list (its parent). The fake keeps each object's
// parts in one Obj: the row component, the GameObject and the Transform are the same fake object, as far as the code
// under test can tell, except through these fields.
constexpr int32_t kPartParent = 0x20, kPartSibling = 0x28, kPartActive = 0x2C;  // the fake's own
Obj g_list, g_gameRow, g_gameLabel, g_localiserClass;
// The menu's list (Layout: laid out, stretched in its Content) under the local player's PCPlayer, with a divider
// before the game's row; a training whiteboard's row elsewhere, on no layout.
Obj g_content, g_playerRoot, g_divider, g_layoutGroup, g_layoutClass, g_board, g_boardRow;
constexpr int32_t kPartHeight = 0x30;  // the fake's own: a RectTransform's height
constexpr int32_t kPartLabel = 0x38;   // the fake's own: the row's name text (ActionNameText), not m_ActionLabel
bool g_menuShown = false;              // whether the keybinds menu is on screen (GameObject.activeInHierarchy)
int g_disables = 0;                    // InputAction.Disable calls
constexpr int32_t kNetLocalPlayer = 0x10;
constexpr float kSpacing = 15.5f;
phasmo::Vector2 g_contentSize{1736.0f, 2575.0f};
std::vector<void*> g_copies;  // what Object.Instantiate made
std::vector<void*> g_destroyed;
std::vector<void*> g_destroyedNow;          // what Object.DestroyImmediate took
std::map<void*, std::vector<void*>> g_kids;  // each copy's children: its label's holder, then its buttons
std::map<void*, void*> g_localiserOf;         // each label's TextLocaliserUI

void* fake_self(void* o, void*) { return o; }
void* fake_parent(void* t, void*) { return at<void*>(t, kPartParent); }
void* fake_instantiate(void* original, void* parent, bool, void*) {
  Obj* c = make_obj(static_cast<Obj*>(original)->klass);
  std::memcpy(c->bytes, static_cast<Obj*>(original)->bytes, sizeof c->bytes);
  at<void*>(c, kPartParent) = parent;
  // The copy's own label, in a holder of its own beside the row's buttons, with the game's localiser on it.
  Obj* holder = make_obj(&g_textClass);
  Obj* buttons = make_obj(&g_textClass);
  Obj* label = make_obj(&g_textClass);
  at<void*>(holder, kPartParent) = c;
  at<void*>(buttons, kPartParent) = c;
  at<void*>(label, kPartParent) = holder;
  at<bool>(holder, kPartActive) = at<bool>(buttons, kPartActive) = true;
  at<void*>(c, kPartLabel) = label;  // the game's row leaves m_ActionLabel empty (read from the PCPlayer prefab)
  g_kids[c] = {holder, buttons};
  g_localiserOf[label] = make_obj(&g_localiserClass);
  g_copies.push_back(c);
  return c;
}
void* fake_in_children(void* t, void* type, bool, void*) {
  if (type != phasmo::TextLocaliserUI::type.type_object()) return nullptr;
  const auto it = g_localiserOf.find(at<void*>(t, kPartLabel));
  return it == g_localiserOf.end() ? nullptr : it->second;
}
bool fake_in_hierarchy(void*, void*) { return g_menuShown; }
void fake_disable(void*, void*) { ++g_disables; }
void* fake_get_component(void* t, void* type, void*) {
  if (type == phasmo::TextMeshProUGUI::type.type_object()) {  // the localiser's own text
    for (auto& [label, localiser] : g_localiserOf)
      if (localiser == t) return label;
    return nullptr;
  }
  if (type == phasmo::TextLocaliserUI::type.type_object()) {
    const auto it = g_localiserOf.find(t);
    return it == g_localiserOf.end() ? nullptr : it->second;
  }
  if (type == phasmo::HorizontalOrVerticalLayoutGroup::type.type_object()) return t == &g_list ? &g_layoutGroup : nullptr;
  // A row (or a copy of one) is its own RebindActionUI; a divider has none.
  return static_cast<Obj*>(t)->klass == &g_rowClass ? t : nullptr;
}
phasmo::Rect fake_rect(void* t, void*) { return {0.0f, 0.0f, 1642.0f, at<float>(t, kPartHeight)}; }
phasmo::Vector2 fake_anchor_min(void* t, void*) { return t == &g_list ? phasmo::Vector2{0, 0} : phasmo::Vector2{0.5f, 0.5f}; }
phasmo::Vector2 fake_anchor_max(void* t, void*) { return t == &g_list ? phasmo::Vector2{1, 1} : phasmo::Vector2{0.5f, 0.5f}; }
phasmo::Vector2 fake_get_size(void* t, void*) { return t == &g_content ? g_contentSize : phasmo::Vector2{0, 0}; }
void fake_set_size(void* t, phasmo::Vector2 v, void*) {
  if (t == &g_content) g_contentSize = v;
}
float fake_spacing(void* group, void*) { return group == &g_layoutGroup ? kSpacing : 0.0f; }
int32_t fake_child_count(void* t, void*) { return static_cast<int32_t>(g_kids[t].size()); }
void* fake_get_child(void* t, int32_t i, void*) { return g_kids[t].at(static_cast<size_t>(i)); }
bool fake_is_child_of(void* t, void* parent, void*) {
  for (void* at_ = t; at_; at_ = at<void*>(at_, kPartParent))
    if (at_ == parent) return true;
  return false;
}
void fake_destroy_now(void* o, bool) {
  g_destroyedNow.push_back(o);
  for (auto& [label, localiser] : g_localiserOf)
    if (localiser == o) localiser = nullptr;
}
int32_t fake_get_sibling(void* t, void*) { return at<int32_t>(t, kPartSibling); }
void fake_set_sibling(void* t, int32_t i, void*) { at<int32_t>(t, kPartSibling) = i; }
bool fake_active_self(void* o, void*) { return at<bool>(o, kPartActive); }
void fake_set_active(void* o, bool on, void*) { at<bool>(o, kPartActive) = on; }
void fake_destroy(void* o, float) {
  g_destroyed.push_back(o);
  fake::destroy(o);
}

void a_mod_key_gets_a_row_beside_the_games() {
  give_entries();
  g_copies.clear();
  g_destroyed.clear();
  g_destroyedNow.clear();
  g_kids.clear();
  g_localiserOf.clear();
  g_copies.clear();
  fake::type(map::TextLocaliserUI, &g_localiserClass);
  fake::type(map::HorizontalOrVerticalLayoutGroup, &g_layoutClass);
  fake::type(map::TextMeshProUGUI, &g_textClass);
  fake::method(map::Component_GetComponentInChildren, reinterpret_cast<void*>(&fake_in_children));
  fake::method(map::GameObject_get_activeInHierarchy, reinterpret_cast<void*>(&fake_in_hierarchy));
  fake::method(map::InputAction_Disable, reinterpret_cast<void*>(&fake_disable));
  g_menuShown = false;
  g_disables = 0;
  fake::method(map::HorizontalOrVerticalLayoutGroup_get_spacing, reinterpret_cast<void*>(&fake_spacing));
  fake::method(map::RectTransform_get_rect, reinterpret_cast<void*>(&fake_rect));
  fake::method(map::RectTransform_get_anchorMin, reinterpret_cast<void*>(&fake_anchor_min));
  fake::method(map::RectTransform_get_anchorMax, reinterpret_cast<void*>(&fake_anchor_max));
  fake::method(map::RectTransform_get_sizeDelta, reinterpret_cast<void*>(&fake_get_size));
  fake::method(map::RectTransform_set_sizeDelta, reinterpret_cast<void*>(&fake_set_size));
  fake::field(map::Network_localPlayer, kNetLocalPlayer);
  fake::method(map::Transform_get_childCount, reinterpret_cast<void*>(&fake_child_count));
  fake::method(map::Transform_GetChild, reinterpret_cast<void*>(&fake_get_child));
  fake::method(map::Transform_IsChildOf, reinterpret_cast<void*>(&fake_is_child_of));
  fake::icall(map::UnityObject_DestroyImmediate, reinterpret_cast<void*>(&fake_destroy_now));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_self));
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_self));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_self));
  fake::method(map::Transform_get_parent, reinterpret_cast<void*>(&fake_parent));
  fake::method(map::UnityObject_Instantiate, reinterpret_cast<void*>(&fake_instantiate));
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::Transform_GetSiblingIndex, reinterpret_cast<void*>(&fake_get_sibling));
  fake::method(map::Transform_SetSiblingIndex, reinterpret_cast<void*>(&fake_set_sibling));
  fake::method(map::GameObject_get_activeSelf, reinterpret_cast<void*>(&fake_active_self));
  fake::method(map::GameObject_SetActive, reinterpret_cast<void*>(&fake_set_active));
  fake::icall(map::UnityObject_Destroy, reinterpret_cast<void*>(&fake_destroy));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  // The game's row for its Torch, on the gamepad: the game map's second binding.
  g_gameRow.klass = &g_rowClass;
  Obj* gameRef = make_obj(&g_refClass);
  Obj* gameAction = make_obj(&g_actionClass);
  at<void*>(gameAction, kActionMap) = &g_gameMap;
  at<void*>(gameRef, kRefAction) = gameAction;
  at<void*>(&g_gameRow, kRowAction) = gameRef;
  at<void*>(&g_gameRow, kRowBindingId) = pm::api::make_string("game-1");
  at<void*>(&g_gameRow, kRowLabel) = nullptr;  // as the game's rows: the name is ActionNameText, wired to nothing
  at<void*>(&g_gameRow, kPartParent) = &g_list;
  at<int32_t>(&g_gameRow, kPartSibling) = 1;
  at<bool>(&g_gameRow, kPartActive) = true;
  at<float>(&g_gameRow, kPartHeight) = 100.0f;
  g_divider.klass = &g_textClass;
  at<void*>(&g_divider, kPartParent) = &g_list;
  at<bool>(&g_divider, kPartActive) = true;
  at<float>(&g_divider, kPartHeight) = 6.0f;
  g_kids[&g_list] = {&g_divider, &g_gameRow};
  at<void*>(&g_list, kPartParent) = &g_content;
  at<void*>(&g_content, kPartParent) = &g_playerRoot;
  at<void*>(&g_net, kNetLocalPlayer) = &g_playerRoot;
  g_contentSize = {1736.0f, 2575.0f};
  // A whiteboard's row for the same binding, found after the menu's: it is never copied from.
  std::memcpy(g_boardRow.bytes, g_gameRow.bytes, sizeof g_boardRow.bytes);
  g_boardRow.klass = &g_rowClass;
  at<void*>(&g_boardRow, kPartParent) = &g_board;
  g_rows = {&g_gameRow, &g_boardRow};
  keys::begin_keys("test.mod", "Test Mod", temp_file("pm_keys_row.txt"));
  keys::add_key(emote_key());
  keys::tick_keys();
  // Four copies in the gamepad's list (no keyboard row is loaded to copy), each at its end as Instantiate puts it, in
  // the list's own pattern: a divider, the mod's heading, a divider, a row for the key's gamepad action.
  CHECK(g_copies.size() == 4);
  if (g_copies.size() != 4) return;
  void* heading = g_copies[1];
  void* copy = g_copies[3];
  CHECK(static_cast<Obj*>(g_copies[0])->klass == &g_textClass && static_cast<Obj*>(g_copies[2])->klass == &g_textClass);
  for (void* c : g_copies) CHECK(at<void*>(c, kPartParent) == &g_list);  // the menu's list, not the whiteboard
  CHECK(at<int32_t>(copy, kPartSibling) == 1);  // not moved beside the game's row
  // The list's Content grows by what was added, its spacing with each, so the scroll reaches the new rows.
  CHECK(std::fabs(g_contentSize.y - (2575.0f + 6 + 100 + 6 + 100 + 4 * kSpacing)) < 1e-3f);
  // The heading: the mod's name on its label, its row's own rebinding gone, its buttons hidden, its label shown.
  void* headingLabel = at<void*>(heading, kPartLabel);
  CHECK(g_texts[headingLabel] == "<b><size=125%>Test Mod</size></b>");  // bold and larger than a row
  CHECK(std::find(g_destroyedNow.begin(), g_destroyedNow.end(), heading) != g_destroyedNow.end());
  CHECK(at<bool>(g_kids[heading][0], kPartActive) && !at<bool>(g_kids[heading][1], kPartActive));
  // Every copy's localiser is taken off, or it would write the game's words back when the menu wakes it.
  CHECK(!g_localiserOf[headingLabel] && !g_localiserOf[at<void*>(copy, kPartLabel)]);
  void* ref = at<void*>(copy, kRowAction);
  CHECK(ref && ref != gameRef);
  void* action = ref ? at<void*>(ref, kRefAction) : nullptr;
  CHECK(action && at<void*>(action, kActionMap) != &g_gameMap);  // the mod's own action
  CHECK(pm::api::to_utf8(at<void*>(copy, kRowBindingId)) == "id-emote.1");
  CHECK(g_texts[at<void*>(copy, kPartLabel)] == "Emote");
  CHECK(g_texts.count(&g_gameLabel) == 0);  // the game's row is untouched
  CHECK(at<bool>(copy, kPartActive));
  CHECK(at<bool>(g_kids[copy][1], kPartActive));  // a row keeps its buttons
  // While the menu is on screen the key's actions are off: Unity refuses to rebind an enabled action ("Cannot rebind
  // action ... while it is enabled", the game's log of the fifth run), and the player is in the menu, not playing.
  g_menuShown = true;
  keys::tick_keys();
  CHECK(g_disables == 3);  // the keyboard's, the gamepad's and VR's
  keys::tick_keys();
  CHECK(g_disables == 3);  // once
  const int enabledBefore = g_enabledActions;
  g_menuShown = false;
  keys::tick_keys();
  CHECK(g_enabledActions == enabledBefore + 3);  // on again with the menu closed
  // A later tick, with the copies still there, makes no more.
  for (int i = 0; i < 120; ++i) keys::tick_keys();
  CHECK(g_copies.size() == 4);
  // Clearing the keys takes the heading and the row out of the menu.
  keys::clear_keys();
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), copy) != g_destroyed.end());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), heading) != g_destroyed.end());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), g_copies[0]) != g_destroyed.end());  // the dividers too
  CHECK(std::fabs(g_contentSize.y - 2575.0f) < 1e-3f);  // and the Content back to its size
}

}  // namespace

int main() {
  paths_compare_as_the_input_system_does();
  a_default_the_game_uses_falls_back();
  a_path_names_its_device();
  menu_navigation_is_not_taken();
  a_saved_rebinding_reads_back();
  a_key_takes_the_defaults_the_game_leaves_free();
  a_key_waits_for_the_games_bindings();
  the_state_is_any_devices();
  a_rebinding_is_saved_and_read_back();
  a_mod_key_gets_a_row_beside_the_games();
  std::printf("%s\n", g_failures ? "keybinds tests FAILED" : "keybinds tests passed");
  return g_failures ? 1 : 0;
}
