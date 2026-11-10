// Offline tests for a mod's own settings on the journal's audio page (sdk/api/settings.h, menu_rows.h) over the fake
// backend: a slider row copied from the game's master volume row under the mod's heading, its own change event, its
// number following the slider, and its value counting as the game's do: on the page's Apply, put back when the page
// closes unapplied, saved and read back. The game's side is in sdk/systems/menus.md. Run: build\settings_test.exe
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "Input.h"
#include "JournalController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"
#include "settings.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace settings = pm::api::settings;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x80]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

// Offsets in the fake objects: the game's fields the code reads, then the fake's own.
constexpr int32_t kPageSlider = 0x08, kPageNumber = 0x10;  // GameMyAudioManager.masterVolumeSlider, masterVolumeValueText
constexpr int32_t kSliderEvent = 0x08;                    // XRSlider.m_OnValueChanged
constexpr int32_t kNetLocalPlayer = 0x08;                 // Network.localPlayer

// The fake's scene: each object is its own GameObject, Transform and component, as far as the code can tell.
struct Node {
  void* parent = nullptr;
  std::vector<void*> kids;
  bool active = true;
  float height = 0.0f;
  std::string text;
  void* localiser = nullptr;  // a name text's TextLocaliserUI
  float value = 0.0f, min = 0.0f, max = 1.0f;  // a slider's
  float fontSize = 40.0f;
  bool autoSize = true;
};
Obj g_rowClass, g_textClass, g_sliderClass, g_dividerClass, g_localiserClass, g_eventClass, g_pageClass, g_groupClass,
    g_otherClass, g_rebindClass, g_lineClass, g_gameObjectClass;
std::map<void*, Node> g_nodes;
// Each Transform's GameObject is an object of its own, as in the game: a GameObject handed to a Component or Transform
// call, or a component handed to a GameObject call, reads the wrong native object in the game and crashes (the audio
// page's lines, 2026-11-10: Component.get_transform given a GameObject). The fake counts each such call.
std::map<void*, void*> g_gameObjectOf, g_transformOf;  // a Transform's GameObject, and a GameObject's Transform
int g_wrongObject = 0;
std::vector<Obj*> g_made;
std::vector<void*> g_destroyed, g_copies;
Obj g_net, g_player, g_content, g_list, g_group, g_page, g_row, g_title, g_number, g_slider, g_divider, g_gameEvent;
// The rest of the audio page, as the game lays it out (sdk/systems/menus.md): its dividers double up around a row it
// switches off, and it groups two rows with no divider between them (the user's screenshot, 2026-11-10).
Obj g_offRow, g_offName, g_divider2, g_row2, g_row2Name, g_row3, g_row3Name, g_divider3;
// The keybinds page beside the audio one: its rows are RebindActionUIs with a thin line between them, which is the line
// a mod's rows copy (the user, 2026-11-10: the audio page's own dividers are double rules, too heavy between rows).
Obj g_kbList, g_kbRow, g_kbLine;
bool g_keybinds = true;  // whether the keybinds page is loaded in this fake scene
phasmo::Vector2 g_contentSize{1700.0f, 1200.0f};

Obj* make(void* klass) {
  g_made.push_back(new Obj{});
  g_made.back()->klass = klass;
  return g_made.back();
}
void place(void* child, void* parent) {
  g_nodes[child].parent = parent;
  if (parent) g_nodes[parent].kids.push_back(child);
}
void* name_localiser(void* t);

bool is_game_object(void* o) { return g_transformOf.count(o) != 0; }
// `o` as a component or Transform: a GameObject here is the game's crash, counted.
void* component(void* o) {
  if (is_game_object(o)) ++g_wrongObject;
  return o;
}
// `o` as a GameObject: its Transform, where the fake keeps its state. A component here is counted.
void* transform_of_game_object(void* o) {
  if (!is_game_object(o)) {
    ++g_wrongObject;
    return o;
  }
  return g_transformOf[o];
}
void* game_object_of(void* t) {
  void*& go = g_gameObjectOf[t];
  if (!go) {
    go = make(&g_gameObjectClass);
    g_transformOf[go] = t;
  }
  return go;
}

void* fake_component_transform(void* c, void*) { return component(c); }
void* fake_component_game_object(void* c, void*) { return game_object_of(component(c)); }
void* fake_game_object_transform(void* go, void*) { return transform_of_game_object(go); }
void* fake_network(void*) { return &g_net; }
void* fake_parent(void* t, void*) { return g_nodes[component(t)].parent; }
int32_t fake_child_count(void* t, void*) { return static_cast<int32_t>(g_nodes[component(t)].kids.size()); }
void* fake_get_child(void* t, int32_t i, void*) { return g_nodes[component(t)].kids.at(static_cast<size_t>(i)); }
int32_t fake_sibling(void* t, void*) {
  const auto& kids = g_nodes[g_nodes[component(t)].parent].kids;
  for (size_t i = 0; i < kids.size(); ++i)
    if (kids[i] == t) return static_cast<int32_t>(i);
  return -1;
}
bool fake_active_self(void* o, void*) { return g_nodes[transform_of_game_object(o)].active; }
void fake_set_sibling(void* t, int32_t i, void*) {
  auto& kids = g_nodes[g_nodes[component(t)].parent].kids;
  const auto at = std::find(kids.begin(), kids.end(), t);
  if (at == kids.end()) return;
  void* moved = *at;
  kids.erase(at);
  kids.insert(kids.begin() + std::min<size_t>(static_cast<size_t>(i), kids.size()), moved);
}
bool fake_is_child_of(void* t, void* parent, void*) {
  component(parent);
  for (void* at_ = component(t); at_; at_ = g_nodes[at_].parent)
    if (at_ == parent) return true;
  return false;
}
// A copy of `original` and everything under it, last in `parent`.
void* copy_tree(void* original, void* parent) {
  Obj* c = make(static_cast<Obj*>(original)->klass);
  std::memcpy(c->bytes, static_cast<Obj*>(original)->bytes, sizeof c->bytes);
  Node n = g_nodes[original];
  n.kids.clear();
  n.parent = nullptr;
  if (n.localiser) n.localiser = make(&g_localiserClass);
  g_nodes[c] = n;
  place(c, parent);
  for (void* kid : g_nodes[original].kids) copy_tree(kid, c);
  return c;
}
// A copy of a GameObject is a GameObject, a copy of a component the copy's same component. The test keeps each copy's
// Transform.
void* fake_instantiate(void* original, void* parent, bool, void*) {
  const bool whole = is_game_object(original);
  void* c = copy_tree(whole ? g_transformOf[original] : original, component(parent));
  g_copies.push_back(c);
  return whole ? game_object_of(c) : c;
}
void* name_localiser(void* t) {
  if (g_nodes[t].localiser) return g_nodes[t].localiser;
  for (void* kid : g_nodes[t].kids)
    if (void* l = name_localiser(kid)) return l;
  return nullptr;
}
void* fake_in_children(void* t, void* type, bool, void*) {
  return type == phasmo::TextLocaliserUI::type.type_object() ? name_localiser(component(t)) : nullptr;
}
void* fake_get_component(void* t, void* type, void*) {
  component(t);
  if (type == phasmo::TextLocaliserUI::type.type_object()) return g_nodes[t].localiser;
  if (type == phasmo::TextMeshProUGUI::type.type_object()) {
    for (auto& [obj, node] : g_nodes)
      if (node.localiser && node.localiser == t) return obj;  // a localiser's own text
    return static_cast<Obj*>(t)->klass == &g_textClass ? t : nullptr;
  }
  if (type == phasmo::XRSlider::type.type_object()) return static_cast<Obj*>(t)->klass == &g_sliderClass ? t : nullptr;
  if (type == phasmo::HorizontalOrVerticalLayoutGroup::type.type_object())
    return (t == &g_list || t == &g_kbList) ? &g_group : nullptr;
  return nullptr;
}
void* fake_find_all(void* type, bool, void*) {
  const bool audio = type == phasmo::GameMyAudioManager::type.type_object();
  const bool rebind = type == phasmo::RebindActionUI::type.type_object();
  void* arr = fake::array(sizeof(void*), audio || (rebind && g_keybinds) ? 1 : 0);
  if (audio)
    *static_cast<void**>(pm::api::ValueArray{arr}.at(0)) = &g_page;
  else if (rebind && g_keybinds)
    *static_cast<void**>(pm::api::ValueArray{arr}.at(0)) = &g_kbRow;
  return arr;
}
void fake_destroy(void* o, float) { g_destroyed.push_back(is_game_object(o) ? g_transformOf[o] : o); }
void fake_destroy_now(void* o, bool) {
  for (auto& [obj, node] : g_nodes)
    if (node.localiser == o) node.localiser = nullptr;
}
void fake_set_active(void* o, bool on, void*) { g_nodes[transform_of_game_object(o)].active = on; }
void fake_set_text(void* t, void* s, void*) { g_nodes[t].text = pm::api::to_utf8(s); }
void* fake_get_text(void* t, void*) { return pm::api::make_string(g_nodes[t].text); }
float fake_font_size(void* t, void*) { return g_nodes[t].fontSize; }
float fake_font_max(void*, void*) { return 60.0f; }
void fake_set_font_size(void* t, float v, void*) { g_nodes[t].fontSize = v; }
void fake_set_auto(void* t, bool on, void*) { g_nodes[t].autoSize = on; }
float fake_get_value(void* s, void*) { return g_nodes[s].value; }
void fake_set_value(void* s, float v, void*) { g_nodes[s].value = std::clamp(v, g_nodes[s].min, g_nodes[s].max); }
void fake_set_min(void* s, float v, void*) { g_nodes[s].min = v; }
void fake_set_max(void* s, float v, void*) { g_nodes[s].max = v; }
void fake_event_ctor(void*, void*) {}
phasmo::Rect fake_rect(void* t, void*) { return {0.0f, 0.0f, 1600.0f, g_nodes[component(t)].height}; }
phasmo::Vector2 fake_anchor_min(void* t, void*) { return t == &g_list ? phasmo::Vector2{0, 0} : phasmo::Vector2{0.5f, 0.5f}; }
phasmo::Vector2 fake_anchor_max(void* t, void*) { return t == &g_list ? phasmo::Vector2{1, 1} : phasmo::Vector2{0.5f, 0.5f}; }
phasmo::Vector2 fake_get_size(void* t, void*) { return t == &g_content ? g_contentSize : phasmo::Vector2{0, 0}; }
void fake_set_size(void* t, phasmo::Vector2 v, void*) {
  if (t == &g_content) g_contentSize = v;
}
float fake_spacing(void*, void*) { return 15.5f; }
void fake_page_method(void*, void*) {}

template <class W>
void call_hooked(void* page) {
  reinterpret_cast<void (*)(void*, const void*)>(pm::api::detail::Bodies<W, pm::api::detail::Hooked<W>>::entry(0))(
      page, nullptr);
}
using WApply = std::remove_cvref_t<decltype(phasmo::GameMyAudioManager::ApplyButton)>;
using WClosed = std::remove_cvref_t<decltype(phasmo::GameMyAudioManager::OnDisable)>;

void give() {
  fake::reset();
  for (Obj* o : g_made) delete o;
  g_made.clear();
  g_nodes.clear();
  g_destroyed.clear();
  g_copies.clear();
  g_gameObjectOf.clear();
  g_transformOf.clear();
  g_wrongObject = 0;
  g_contentSize = {1700.0f, 1200.0f};
  fake::type(map::GameMyAudioManager, &g_pageClass);
  fake::type(map::XRSlider, &g_sliderClass);
  fake::type(map::SliderEvent, &g_eventClass);
  fake::type(map::TextLocaliserUI, &g_localiserClass);
  fake::type(map::TextMeshProUGUI, &g_textClass);
  fake::type(map::HorizontalOrVerticalLayoutGroup, &g_groupClass);
  fake::type(map::GameObject, &g_gameObjectClass);
  fake::type(map::RebindActionUI, &g_rebindClass);
  fake::field(map::GameMyAudioManager_masterVolumeSlider, kPageSlider);
  fake::field(map::GameMyAudioManager_masterVolumeValueText, kPageNumber);
  fake::field(map::XRSlider_m_OnValueChanged, kSliderEvent);
  fake::field(map::Network_localPlayer, kNetLocalPlayer);
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_component_transform));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_component_game_object));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_game_object_transform));
  fake::method(map::Transform_get_parent, reinterpret_cast<void*>(&fake_parent));
  fake::method(map::Transform_get_childCount, reinterpret_cast<void*>(&fake_child_count));
  fake::method(map::Transform_GetChild, reinterpret_cast<void*>(&fake_get_child));
  fake::method(map::Transform_GetSiblingIndex, reinterpret_cast<void*>(&fake_sibling));
  fake::method(map::Transform_SetSiblingIndex, reinterpret_cast<void*>(&fake_set_sibling));
  fake::method(map::GameObject_get_activeSelf, reinterpret_cast<void*>(&fake_active_self));
  fake::method(map::Transform_IsChildOf, reinterpret_cast<void*>(&fake_is_child_of));
  fake::method(map::UnityObject_Instantiate, reinterpret_cast<void*>(&fake_instantiate));
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::Component_GetComponentInChildren, reinterpret_cast<void*>(&fake_in_children));
  fake::method(map::UnityObject_FindObjectsOfType_inactive, reinterpret_cast<void*>(&fake_find_all));
  fake::icall(map::UnityObject_Destroy, reinterpret_cast<void*>(&fake_destroy));
  fake::icall(map::UnityObject_DestroyImmediate, reinterpret_cast<void*>(&fake_destroy_now));
  fake::method(map::GameObject_SetActive, reinterpret_cast<void*>(&fake_set_active));
  fake::method(map::TMP_Text_set_text, reinterpret_cast<void*>(&fake_set_text));
  fake::method(map::TMP_Text_get_text, reinterpret_cast<void*>(&fake_get_text));
  fake::method(map::TMP_Text_get_fontSize, reinterpret_cast<void*>(&fake_font_size));
  fake::method(map::TMP_Text_get_fontSizeMax, reinterpret_cast<void*>(&fake_font_max));
  fake::method(map::TMP_Text_set_fontSize, reinterpret_cast<void*>(&fake_set_font_size));
  fake::method(map::TMP_Text_set_enableAutoSizing, reinterpret_cast<void*>(&fake_set_auto));
  fake::method(map::XRSlider_get_value, reinterpret_cast<void*>(&fake_get_value));
  fake::method(map::XRSlider_SetValueWithoutNotify, reinterpret_cast<void*>(&fake_set_value));
  fake::method(map::XRSlider_set_minValue, reinterpret_cast<void*>(&fake_set_min));
  fake::method(map::XRSlider_set_maxValue, reinterpret_cast<void*>(&fake_set_max));
  fake::method(map::SliderEvent_ctor, reinterpret_cast<void*>(&fake_event_ctor));
  fake::method(map::RectTransform_get_rect, reinterpret_cast<void*>(&fake_rect));
  fake::method(map::RectTransform_get_anchorMin, reinterpret_cast<void*>(&fake_anchor_min));
  fake::method(map::RectTransform_get_anchorMax, reinterpret_cast<void*>(&fake_anchor_max));
  fake::method(map::RectTransform_get_sizeDelta, reinterpret_cast<void*>(&fake_get_size));
  fake::method(map::RectTransform_set_sizeDelta, reinterpret_cast<void*>(&fake_set_size));
  fake::method(map::HorizontalOrVerticalLayoutGroup_get_spacing, reinterpret_cast<void*>(&fake_spacing));
  fake::method(map::GameMyAudioManager_ApplyButton, reinterpret_cast<void*>(&fake_page_method));
  fake::method(map::GameMyAudioManager_OnDisable, reinterpret_cast<void*>(&fake_page_method));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  // The audio page under the local player: its list (stretched in its content) holds the master volume row (its name
  // with a localiser, its number "100%", its slider with the game's listeners on its event) and a divider after it.
  at<void*>(&g_net, kNetLocalPlayer) = &g_player;
  for (Obj* o : {&g_content, &g_list, &g_row, &g_title, &g_number, &g_divider}) o->klass = &g_otherClass;
  g_title.klass = g_number.klass = &g_textClass;
  g_slider.klass = &g_sliderClass;
  g_page.klass = &g_pageClass;
  place(&g_content, &g_player);
  place(&g_list, &g_content);
  place(&g_row, &g_list);
  place(&g_divider, &g_list);
  place(&g_offRow, &g_list);
  place(&g_divider2, &g_list);
  place(&g_row2, &g_list);
  place(&g_row3, &g_list);
  place(&g_divider3, &g_list);
  place(&g_title, &g_row);
  place(&g_number, &g_row);
  place(&g_slider, &g_row);
  place(&g_offName, &g_offRow);
  place(&g_row2Name, &g_row2);
  place(&g_row3Name, &g_row3);
  g_nodes[&g_row].height = 100.0f;
  g_nodes[&g_divider].height = 6.0f;
  // The rest of the page: a row the game has switched off between two of its dividers, and two rows with no divider
  // between them.
  for (Obj* o : {&g_offRow, &g_divider2, &g_row2, &g_row3, &g_divider3, &g_offName, &g_row2Name, &g_row3Name})
    o->klass = &g_otherClass;
  g_offName.klass = g_row2Name.klass = g_row3Name.klass = &g_textClass;
  g_nodes[&g_offRow].active = false;
  g_nodes[&g_offRow].height = 100.0f;
  g_nodes[&g_divider2].height = 6.0f;
  g_nodes[&g_divider3].height = 6.0f;
  g_nodes[&g_row2].height = 100.0f;
  g_nodes[&g_row3].height = 100.0f;
  g_nodes[&g_offName].localiser = make(&g_localiserClass);
  g_nodes[&g_row2Name].localiser = make(&g_localiserClass);
  g_nodes[&g_row3Name].localiser = make(&g_localiserClass);
  g_nodes[&g_title].localiser = make(&g_localiserClass);
  g_nodes[&g_title].text = "Master Volume";
  g_nodes[&g_number].text = "100%";
  g_nodes[&g_slider].value = 1.0f;
  at<void*>(&g_slider, kSliderEvent) = &g_gameEvent;
  at<void*>(&g_page, kPageSlider) = &g_slider;
  at<void*>(&g_page, kPageNumber) = &g_number;
  // The keybinds page in the same journal: its list holds a RebindActionUI row and the thin line beside it, with no name
  // of their own to read.
  if (g_keybinds) {
    g_kbList.klass = &g_otherClass;
    g_kbRow.klass = &g_rebindClass;
    g_kbLine.klass = &g_lineClass;
    place(&g_kbList, &g_player);
    place(&g_kbRow, &g_kbList);
    place(&g_kbLine, &g_kbList);
    g_nodes[&g_kbRow].height = 45.0f;
    g_nodes[&g_kbLine].height = 4.0f;
  }
}

std::filesystem::path temp_file(const char* name) {
  const auto p = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove(p);
  return p;
}

// The copy of a row whose name is `name`.
void* copy_named(const std::string& name) {
  for (void* c : g_copies)
    for (void* kid : g_nodes[c].kids)
      if (g_nodes[kid].text == name) return c;
  return nullptr;
}
void* kid(void* row, size_t i) { return g_nodes[row].kids.at(i); }

void a_slider_sits_under_the_mods_heading() {
  give();
  settings::begin_settings("test.mod", "Test Mod", temp_file("pm_settings_row.txt"));
  settings::Slider volume;
  volume.id = "volume";
  volume.label = "Volume";
  CHECK(settings::add_slider(volume));
  CHECK(!settings::add_slider(volume));  // one id once
  settings::tick_settings();
  // Every call got the object it is the game's call for: a GameObject where it takes one, a component elsewhere.
  CHECK(g_wrongObject == 0);
  // The heading and the row, then a line where the page's own rows touched and one before the mod's row.
  CHECK(g_copies.size() == 4);
  const auto& kids = g_nodes[&g_list].kids;
  CHECK(kids.size() == 11);
  // The page doubled its dividers around a row it has switched off: one of the pair is put away, and its single ones
  // stay. Where its rows touch with no divider at all, one is copied in (the user, 2026-11-10).
  CHECK(!g_nodes[&g_divider2].active && g_nodes[&g_divider].active && g_nodes[&g_divider3].active);
  CHECK(kids[5] == g_copies[3] && kids[9] == g_copies[2]);
  // The lines copied in are the keybinds page's, one line to a line, not the audio page's 6-high divider.
  CHECK(static_cast<Obj*>(g_copies[2])->klass == &g_lineClass && static_cast<Obj*>(g_copies[3])->klass == &g_lineClass);
  void* heading = copy_named("<b>Test Mod</b>");
  CHECK(heading != nullptr);
  if (heading) {
    CHECK(g_nodes[kid(heading, 0)].active && !g_nodes[kid(heading, 1)].active && !g_nodes[kid(heading, 2)].active);
    CHECK(!g_nodes[kid(heading, 0)].autoSize && std::fabs(g_nodes[kid(heading, 0)].fontSize - 50.0f) < 1e-3f);
  }
  void* row = copy_named("Volume");
  CHECK(row != nullptr);
  if (!row) return;
  CHECK(!g_nodes[kid(row, 0)].localiser);  // its localiser off, or it would write the game's words back
  void* slider = kid(row, 2);
  CHECK(at<void*>(slider, kSliderEvent) && at<void*>(slider, kSliderEvent) != &g_gameEvent);  // an event of its own
  CHECK(g_nodes[slider].value == 1.0f && g_nodes[kid(row, 1)].text == "100%");
  // The list's content grows by the two rows and the two lines copied in, less the divider put away, each with its
  // spacing.
  CHECK(std::fabs(g_contentSize.y - (1200.0f + 2 * (100.0f + 15.5f) + 2 * (4.0f + 15.5f) - (6.0f + 15.5f))) < 1e-3f);
  // The game's own row is untouched.
  CHECK(g_nodes[&g_title].text == "Master Volume" && g_nodes[&g_slider].value == 1.0f);
  // The number follows the slider as it is dragged.
  g_nodes[slider].value = 0.42f;
  settings::tick_settings();
  CHECK(g_nodes[kid(row, 1)].text == "42%");
  settings::clear_settings();
  CHECK(std::fabs(g_contentSize.y - 1200.0f) < 1e-3f);
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), row) != g_destroyed.end());
  // The page is the game's again: the divider that was put away is on and the copied lines are gone.
  CHECK(g_nodes[&g_divider2].active);
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), g_copies[2]) != g_destroyed.end());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), g_copies[3]) != g_destroyed.end());
  CHECK(g_wrongObject == 0);
}

// With no other journal menu loaded, the rows copy the audio page's own divider, as they did before the keybinds page's
// line was chosen (the fallback has to keep working when a player opens the audio page on its own).
void a_row_copies_the_pages_own_line_without_a_keybinds_page() {
  g_keybinds = false;
  give();
  settings::begin_settings("test.mod", "Test Mod", temp_file("pm_settings_own_line.txt"));
  settings::Slider volume;
  volume.id = "volume";
  volume.label = "Volume";
  CHECK(settings::add_slider(volume));
  settings::tick_settings();
  // The heading, the row, and two lines: with no keybinds page loaded the lines are copies of the audio page's own
  // divider, which is still one line to a line once space_out has put the doubled one away.
  CHECK(g_copies.size() == 4);
  CHECK(static_cast<Obj*>(g_copies[2])->klass == &g_otherClass && static_cast<Obj*>(g_copies[3])->klass == &g_otherClass);
  CHECK(std::fabs(g_contentSize.y - (1200.0f + 2 * (100.0f + 15.5f) + 2 * (6.0f + 15.5f) - (6.0f + 15.5f))) < 1e-3f);
  settings::clear_settings();
  CHECK(g_wrongObject == 0);
  g_keybinds = true;
}

void a_value_counts_once_applied() {
  give();
  const auto file = temp_file("pm_settings_saved.txt");
  settings::begin_settings("test.mod", "Test Mod", file);
  settings::Slider volume;
  volume.id = "volume";
  volume.label = "Volume";
  settings::add_slider(volume);
  settings::tick_settings();
  CHECK(settings::value("volume") == 1.0f);  // its initial value
  void* row = copy_named("Volume");
  CHECK(row != nullptr);
  if (!row) return;
  void* slider = kid(row, 2);
  // Dragged: nothing counts until Apply.
  g_nodes[slider].value = 0.3f;
  settings::tick_settings();
  CHECK(settings::value("volume") == 1.0f);
  call_hooked<WApply>(&g_page);
  CHECK(std::fabs(settings::value("volume") - 0.3f) < 1e-6f);
  // Dragged again, then the page left without Apply: the slider goes back.
  g_nodes[slider].value = 0.9f;
  call_hooked<WClosed>(&g_page);
  CHECK(std::fabs(g_nodes[slider].value - 0.3f) < 1e-6f && std::fabs(settings::value("volume") - 0.3f) < 1e-6f);
  settings::clear_settings();
  // Saved and read back at the next start.
  give();
  settings::begin_settings("test.mod", "Test Mod", file);
  settings::add_slider(volume);
  CHECK(std::fabs(settings::value("volume") - 0.3f) < 1e-4f);
  settings::tick_settings();
  void* again = copy_named("Volume");
  CHECK(again && std::fabs(g_nodes[kid(again, 2)].value - 0.3f) < 1e-4f && g_nodes[kid(again, 1)].text == "30%");
  CHECK(settings::value("other") == 0.0f);
  settings::clear_settings();
  CHECK(g_wrongObject == 0);
}

}  // namespace

int main() {
  a_slider_sits_under_the_mods_heading();
  a_row_copies_the_pages_own_line_without_a_keybinds_page();
  a_value_counts_once_applied();
  std::printf("%s\n", g_failures ? "settings tests FAILED" : "settings tests passed");
  return g_failures ? 1 : 0;
}
