#include "keybinds.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>

#include "Input.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "UnityUI.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::keys {

// ---- The rules (binding_rule.h) ----

namespace {
char lower_char(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
bool contains_nocase(std::string_view text, std::string_view part) {
  return std::search(text.begin(), text.end(), part.begin(), part.end(),
                     [](char a, char b) { return lower_char(a) == lower_char(b); }) != text.end();
}
}  // namespace

bool same_path(std::string_view a, std::string_view b) {
  if (a.empty() || b.empty() || a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (lower_char(a[i]) != lower_char(b[i])) return false;
  return true;
}

bool path_taken(std::string_view path, std::span<const std::string> gameBindings) {
  return std::any_of(gameBindings.begin(), gameBindings.end(),
                     [&](const std::string& g) { return same_path(path, g); });
}

std::string pick_default(std::string_view wanted, std::span<const std::string> fallbacks,
                         std::span<const std::string> gameBindings) {
  if (wanted.empty()) return {};
  if (!path_taken(wanted, gameBindings)) return std::string(wanted);
  for (const std::string& f : fallbacks)
    if (!f.empty() && !path_taken(f, gameBindings)) return f;
  return {};
}

bool menu_only(std::string_view action) { return action.size() > 2 && action.starts_with("UI"); }

Device device_of(std::string_view path) {
  const size_t end = path.find('>');
  const std::string_view layout = end == std::string_view::npos ? path : path.substr(0, end);
  if (contains_nocase(layout, "XR") || contains_nocase(layout, "Oculus") || contains_nocase(layout, "OpenVR") ||
      contains_nocase(layout, "Index"))
    return Device::Vr;
  for (const char* pad : {"Gamepad", "XInput", "DualShock", "DualSense", "SwitchPro", "Joystick"})
    if (contains_nocase(layout, pad)) return Device::Gamepad;
  return Device::Keyboard;
}

std::string format_saved(std::span<const SavedBinding> saved) {
  std::string out;
  for (const SavedBinding& s : saved) {
    out += s.key;
    out += '\t';
    out += std::to_string(static_cast<int>(s.device));
    out += '\t';
    out += s.path;
    out += '\n';
  }
  return out;
}

std::vector<SavedBinding> parse_saved(std::string_view text) {
  std::vector<SavedBinding> out;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const size_t a = line.find('\t');
    const size_t b = a == std::string_view::npos ? a : line.find('\t', a + 1);
    if (b == std::string_view::npos) continue;
    const std::string_view key = line.substr(0, a), dev = line.substr(a + 1, b - a - 1), path = line.substr(b + 1);
    if (key.empty() || path.empty() || dev.size() != 1 || dev[0] < '0' || dev[0] >= '0' + static_cast<int>(kDevices))
      continue;
    out.push_back({std::string(key), static_cast<Device>(dev[0] - '0'), std::string(path)});
  }
  return out;
}

// ---- The game ----

namespace {

// How often the keybinds menu is searched for, in ticks: a search goes through every loaded object.
constexpr int kRowSearchTicks = 60;

struct Key {
  ModKey desc;
  std::array<Held, kDevices> actions;      // one per device the key binds, null where it binds none
  std::array<int32_t, kDevices> binding{};  // each action's binding's index in the map's bindings
  std::array<std::string, kDevices> chosen;  // the default chosen on each device
  std::array<Held, kDevices> rows;           // this key's rows in the keybinds menu (keyboard and gamepad)
};

struct State {
  std::string owner;
  std::filesystem::path file;
  std::vector<Key> keys;
  Held asset, map;
  bool ready = false;
  std::string lastSaved;  // the text last written or read, so a frame without a change writes nothing
  int rowTicks = 0;
  std::string lastSeen;        // what the last search of the keybinds menu found, for its log line
  bool loggedWaiting = false;  // the wait for the game's bindings was logged
};
State g;

std::string read_file(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return in ? std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()) : std::string();
}

// Every binding path of an InputActionMap's bindings, but those of its menu-only actions (menu_only).
void add_map_paths(void* map, std::vector<std::string>& out) {
  const ValueArray bindings{map ? phasmo::InputActionMap::m_Bindings.get(map) : nullptr};
  for (size_t i = 0; i < bindings.size(); ++i)
    if (void* path = phasmo::InputBinding::m_Path.get(bindings.at(i)))
      if (!menu_only(to_utf8(phasmo::InputBinding::m_Action.get(bindings.at(i))))) out.push_back(to_utf8(path));
}

// The game's own bindings: from the local PC player's controls, else from the keybinds menu's rows. Empty while
// neither is loaded.
std::vector<std::string> game_bindings() {
  std::vector<std::string> out;
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  void* controls = rig && alive(rig) ? phasmo::LocalPCPlayer::controls.get(rig) : nullptr;
  void* asset = controls && alive(controls) ? phasmo::PCControls::actions.get(controls) : nullptr;
  const RefArray maps{asset ? phasmo::InputActionAsset::m_ActionMaps.get(asset) : nullptr};
  for (size_t i = 0; i < maps.size(); ++i) add_map_paths(maps[i], out);
  if (!out.empty()) return out;
  void* rowType = phasmo::RebindActionUI::type.type_object();
  const RefArray rows{rowType ? phasmo::Object::FindAllOfType(rowType, true) : nullptr};
  for (size_t i = 0; i < rows.size() && out.empty(); ++i) {
    void* ref = rows[i] ? phasmo::RebindActionUI::m_Action.get(rows[i]) : nullptr;
    void* action = ref ? phasmo::InputActionReference::get_action(ref) : nullptr;
    add_map_paths(action ? phasmo::InputAction::m_ActionMap.get(action) : nullptr, out);
  }
  return out;
}

// The binding at `index` of the mod's map: its address in place, valid until the next managed call.
void* binding_at(int32_t index) {
  const ValueArray bindings{g.map ? phasmo::InputActionMap::m_Bindings.get(g.map) : nullptr};
  return index >= 0 && static_cast<size_t>(index) < bindings.size() ? bindings.at(static_cast<size_t>(index)) : nullptr;
}

std::string effective_path(const Key& k, size_t d) {
  if (!k.actions[d]) return {};
  if (void* b = binding_at(k.binding[d])) {
    const std::string over = to_utf8(phasmo::InputBinding::m_OverridePath.get(b));
    if (!over.empty()) return over;
  }
  return k.chosen[d];
}

bool make_actions(const std::vector<std::string>& game) {
  void* assetType = phasmo::InputActionAsset::type.type_object();
  void* asset = assetType ? phasmo::ScriptableObject::CreateInstance(assetType) : nullptr;
  if (!asset) return false;
  phasmo::Object::set_hideFlags(asset, phasmo::HideFlags::DontUnloadUnusedAsset());
  void* map = phasmo::InputActionSetupExtensions::AddActionMap(asset, make_string(g.owner));
  if (!map) return false;
  g.asset = asset;
  g.map = map;
  const std::vector<SavedBinding> saved = parse_saved(read_file(g.file));
  const int32_t button = static_cast<int32_t>(phasmo::InputActionType::Button());
  for (Key& k : g.keys) {
    for (size_t d = 0; d < kDevices; ++d) {
      // VR's actions are the XR Interaction toolkit's, which no reachable asset holds: its default is not checked.
      const std::span<const std::string> against =
          static_cast<Device>(d) == Device::Vr ? std::span<const std::string>{} : std::span<const std::string>{game};
      k.chosen[d] = pick_default(k.desc.paths[d], k.desc.fallbacks[d], against);
      if (k.chosen[d].empty()) {
        if (!k.desc.paths[d].empty())
          PM_LOG("keys: %s.%s takes no default on device %d: the game binds every choice", g.owner.c_str(),
                 k.desc.id.c_str(), static_cast<int>(d));
        continue;
      }
      k.binding[d] = static_cast<int32_t>(ValueArray{phasmo::InputActionMap::m_Bindings.get(map)}.size());
      const std::string name = k.desc.id + "." + std::to_string(d);
      void* action = phasmo::InputActionSetupExtensions::AddAction(map, make_string(name), button,
                                                                   make_string(k.chosen[d]), nullptr, nullptr,
                                                                   nullptr, nullptr);
      if (!action) continue;
      k.actions[d] = action;
      for (const SavedBinding& s : saved)
        if (s.key == k.desc.id && s.device == static_cast<Device>(d))
          phasmo::InputActionRebindingExtensions::ApplyBindingOverride(action, make_string(s.path), nullptr, nullptr);
      phasmo::InputAction::Enable(action);
    }
  }
  return true;
}

// Saves the players' rebindings when they changed since the last save.
void save_changes() {
  std::vector<SavedBinding> now;
  for (const Key& k : g.keys)
    for (size_t d = 0; d < kDevices; ++d) {
      void* b = k.actions[d] ? binding_at(k.binding[d]) : nullptr;
      const std::string over = b ? to_utf8(phasmo::InputBinding::m_OverridePath.get(b)) : std::string();
      if (!over.empty()) now.push_back({k.desc.id, static_cast<Device>(d), over});
    }
  const std::string text = format_saved(now);
  if (text == g.lastSaved) return;
  std::error_code ec;
  std::filesystem::create_directories(g.file.parent_path(), ec);
  std::ofstream out(g.file, std::ios::binary | std::ios::trunc);
  if (!out) {
    PM_LOG("keys: cannot write %s", g.file.string().c_str());
    return;
  }
  out << text;
  g.lastSaved = text;
}

// The device of a keybinds row, by the path of the binding it rebinds; false for one of this mod's own rows or one
// whose binding cannot be read.
bool row_device(void* row, Device& out) {
  void* ref = phasmo::RebindActionUI::m_Action.get(row);
  void* action = ref ? phasmo::InputActionReference::get_action(ref) : nullptr;
  void* map = action ? phasmo::InputAction::m_ActionMap.get(action) : nullptr;
  if (!map || map == g.map.get()) return false;
  const std::string id = to_utf8(phasmo::RebindActionUI::m_BindingId.get(row));
  const ValueArray bindings{phasmo::InputActionMap::m_Bindings.get(map)};
  for (size_t i = 0; i < bindings.size(); ++i)
    if (to_utf8(phasmo::InputBinding::m_Id.get(bindings.at(i))) == id) {
      out = device_of(to_utf8(phasmo::InputBinding::m_Path.get(bindings.at(i))));
      return true;
    }
  return false;
}

// A copy of `templ` (a game row) for a key's action on one device, beside it in the menu.
void* make_row(void* templ, Key& k, size_t d) {
  void* object = phasmo::Component::get_gameObject(templ);
  void* transform = phasmo::Component::get_transform(templ);
  void* parent = transform ? phasmo::Transform::get_parent(transform) : nullptr;
  if (!object || !parent) {
    PM_LOG("keys: %s.%s gets no row: the menu's row has no parent", g.owner.c_str(), k.desc.id.c_str());
    return nullptr;
  }
  void* copyObject = phasmo::Object::Instantiate(object, parent, false);
  if (!copyObject) {
    PM_LOG("keys: %s.%s gets no row: the menu's row could not be copied", g.owner.c_str(), k.desc.id.c_str());
    return nullptr;
  }
  void* copyTransform = phasmo::GameObject::get_transform(copyObject);
  void* copy = copyTransform ? phasmo::Component::GetComponent(copyTransform, phasmo::RebindActionUI::type.type_object())
                             : nullptr;
  if (!copy) {
    PM_LOG("keys: %s.%s gets no row: its copy has no RebindActionUI", g.owner.c_str(), k.desc.id.c_str());
    phasmo::Object::Destroy(copyObject, 0.0f);
    return nullptr;
  }
  phasmo::Transform::SetSiblingIndex(copyTransform,
                                     phasmo::Transform::GetSiblingIndex(transform) + 1);
  void* binding = binding_at(k.binding[d]);
  void* id = binding ? phasmo::InputBinding::m_Id.get(binding) : nullptr;
  phasmo::RebindActionUI::set_bindingId(copy, id);
  phasmo::RebindActionUI::set_actionReference(copy, phasmo::InputActionReference::Create(k.actions[d]));
  if (void* label = phasmo::RebindActionUI::m_ActionLabel.get(copy))
    phasmo::TMP_Text::set_text(label, make_string(k.desc.label));
  // Off and on, so the row shows the new binding as it does when the menu opens.
  const bool on = phasmo::GameObject::get_activeSelf(copyObject);
  phasmo::GameObject::SetActive(copyObject, false);
  phasmo::GameObject::SetActive(copyObject, on);
  return copy;
}

void ensure_rows() {
  if (++g.rowTicks < kRowSearchTicks) return;
  g.rowTicks = 0;
  bool missing = false;
  for (const Key& k : g.keys)
    for (size_t d = 0; d < kDevices; ++d)
      if (k.actions[d] && static_cast<Device>(d) != Device::Vr && (!k.rows[d] || !alive(k.rows[d])))
        missing = true;
  if (!missing) return;
  void* rowType = phasmo::RebindActionUI::type.type_object();
  const RefArray rows{rowType ? phasmo::Object::FindAllOfType(rowType, true) : nullptr};
  std::array<void*, kDevices> templ{};
  for (size_t i = 0; i < rows.size(); ++i) {
    Device d{};
    if (rows[i] && row_device(rows[i], d)) templ[static_cast<size_t>(d)] = rows[i];
  }
  // What the search found, logged when it changes (the keybinds menu loads with some scenes and not others).
  const std::string seen = std::to_string(rows.size()) + (templ[0] ? " k" : " -") + (templ[1] ? "g" : "-");
  if (seen != g.lastSeen) {
    PM_LOG("keys: %s finds %zu keybinds rows (a keyboard row to copy: %s, a gamepad row: %s)", g.owner.c_str(),
           rows.size(), templ[0] ? "yes" : "no", templ[1] ? "yes" : "no");
    g.lastSeen = seen;
  }
  for (Key& k : g.keys)
    for (size_t d = 0; d < kDevices; ++d)
      if (k.actions[d] && templ[d] && (!k.rows[d] || !alive(k.rows[d]))) {
        k.rows[d] = make_row(templ[d], k, d);
        if (k.rows[d])
          PM_LOG("keys: %s.%s has its row in the keybinds menu (device %zu)", g.owner.c_str(), k.desc.id.c_str(), d);
      }
}

}  // namespace

void begin_keys(std::string_view owner, std::filesystem::path file) {
  clear_keys();
  g.owner = owner;
  g.file = std::move(file);
}

bool add_key(const ModKey& key) {
  if (key.id.empty() || g.ready) return false;
  if (std::any_of(g.keys.begin(), g.keys.end(), [&](const Key& k) { return k.desc.id == key.id; })) return false;
  if (std::all_of(key.paths.begin(), key.paths.end(), [](const std::string& p) { return p.empty(); })) return false;
  g.keys.push_back(Key{key, {}, {}, {}, {}});
  return true;
}

void tick_keys() {
  if (g.keys.empty()) return;
  if (!g.ready) {
    const std::vector<std::string> game = game_bindings();
    if (game.empty()) {
      if (!g.loggedWaiting) PM_LOG("keys: %s waits for the game's bindings (no player controls or keybinds menu yet)",
                                   g.owner.c_str());
      g.loggedWaiting = true;
      return;
    }
    if (!make_actions(game)) {
      PM_LOG("keys: %s cannot make its input actions in this game build", g.owner.c_str());
      g.keys.clear();
      return;
    }
    g.ready = true;
    for (const Key& k : g.keys)
      for (size_t d = 0; d < kDevices; ++d)
        if (k.actions[d])
          PM_LOG("keys: %s.%s is on %s (device %zu; the game has %zu bindings)", g.owner.c_str(), k.desc.id.c_str(),
                 effective_path(k, d).c_str(), d, game.size());
    g.lastSaved = format_saved(parse_saved(read_file(g.file)));
    g.rowTicks = kRowSearchTicks - 1;  // look for the menu at once
  }
  save_changes();
  ensure_rows();
}

bool keys_ready() { return g.ready; }

KeyState key_state(std::string_view id) {
  KeyState s;
  if (!g.ready) return s;
  bool releasedAny = false;
  for (const Key& k : g.keys) {
    if (k.desc.id != id) continue;
    for (const Held& a : k.actions) {
      if (!a) continue;
      s.held |= phasmo::InputAction::IsPressed(a);
      s.pressed |= phasmo::InputAction::WasPressedThisFrame(a);
      releasedAny |= phasmo::InputAction::WasReleasedThisFrame(a);
    }
  }
  s.released = releasedAny && !s.held;
  return s;
}

std::string bound_path(std::string_view id, Device device) {
  if (!g.ready) return {};
  for (const Key& k : g.keys)
    if (k.desc.id == id) return effective_path(k, static_cast<size_t>(device));
  return {};
}

void clear_keys() {
  for (Key& k : g.keys) {
    for (Held& r : k.rows)
      if (r && alive(r)) phasmo::Object::Destroy(phasmo::Component::get_gameObject(r), 0.0f);
    for (Held& a : k.actions)
      if (a) phasmo::InputAction::Disable(a);
  }
  g = State{};
}

}  // namespace pm::api::keys
