#include <windows.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "game.h"
#include "menu_panel.h"
#include "mods.h"
#include "ui_fixture.h"

namespace {
struct Node {
  std::string name, text;
  Node* parent = nullptr;
  bool alive = true, active = true;
  pg::ui::Vec2 at{}, size{1, 1};
  std::vector<Node*> children;
};
std::vector<std::unique_ptr<Node>> nodes;
Node* fixtureMenu = nullptr;
bool fixtureVisible = true, clickTape = false;
bool fixtureButtons = true;  // the game's menu state shows its own buttons (Singleplayer, Multiplayer)
std::vector<void*> holds;    // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
int clickButton = -1, clickRow = -1;
int unavailableRow = -1;
pg::menu_panel::Panel* opened = nullptr;
std::vector<pg::mods::Mod> installed;
std::vector<std::string> started;
const PgModInfo baseInfo{PGMOD_API_VERSION, "base", "Base Mod", 1, 0};
const PgModInfo dependentInfo{PGMOD_API_VERSION, "extra", "Extra Mod", 2, 0};
Node* clone(Node* original, Node* parent) {
  auto* copy = ui_fixture::make_node(nodes, original->name.c_str(), parent);
  copy->active = original->active;
  for (auto* child : original->children) clone(child, copy);
  return copy;
}
ULONGLONG WINAPI clock_ms() { return 1000; }
void* find_menu(il2cpp::Object*, const void*) { return fixtureMenu; }
void* menu_instance(const void*) { return fixtureMenu; }
pg::ui::Vec2 anchored(void* value, const void*) { return static_cast<Node*>(value)->at; }
void set_anchored(void* value, pg::ui::Vec2 at, const void*) { static_cast<Node*>(value)->at = at; }
pg::ui::Vec2 size_delta(void* value, const void*) { return static_cast<Node*>(value)->size; }
}

#define GetTickCount64 clock_ms
#include "../host/mods_panel.cpp"
#undef GetTickCount64

namespace il2cpp {
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
Object* type_object(Class* klass) { return reinterpret_cast<Object*>(klass); }
void* method_ptr(Class*, const char* name, int) {
#define BIND(text, fn) if (std::strcmp(name, text) == 0) return reinterpret_cast<void*>(&fn)
  BIND("FindObjectOfType", find_menu);
  BIND("get_Instance", menu_instance);
  BIND("get_anchoredPosition", anchored);
  BIND("set_anchoredPosition", set_anchored);
  BIND("get_sizeDelta", size_delta);
#undef BIND
  return nullptr;
}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}
namespace pg::game {
bool alive(void* value) { return value && static_cast<Node*>(value)->alive; }
void* transform_of(void* value) { return value; }
void* game_object_of(void* value) { return value; }
void* parent(void* value) { return static_cast<Node*>(value)->parent; }
void* find_child(void* value, const char* path) {
  if (!value) return nullptr;
  std::string full(path);
  const size_t slash = full.find('/');
  for (auto* child : static_cast<Node*>(value)->children)
    if (child->name == full.substr(0, slash))
      return slash == std::string::npos ? child : find_child(child, full.c_str() + slash + 1);
  return nullptr;
}
void* instantiate(void* value, void* parent, bool world) {
  CHECK(!world);
  return clone(static_cast<Node*>(value), static_cast<Node*>(parent));
}
void* get_component(void* value, il2cpp::Object*) { return value; }
void set_active(void* value, bool active) { static_cast<Node*>(value)->active = active; }
bool active_self(void* value) { return value && static_cast<Node*>(value)->active; }
bool active_in_hierarchy(void* value) { return value && ui_fixture::active_in_hierarchy(static_cast<Node*>(value)); }
void destroy(void* value) { static_cast<Node*>(value)->alive = false; }
}
namespace pg::ui {
void set_text(void* value, const char* text) { static_cast<Node*>(value)->text = text; }
}
namespace pg::log { void write(const char*, ...) {} }
namespace pg::mods {
const std::vector<Mod>& loaded() { return installed; }
std::vector<std::string> started_ids() { return started; }
std::vector<modlist::Manifest> manifests() {
  std::vector<modlist::Manifest> result;
  for (const auto& mod : installed)
    result.push_back({mod.info->id, mod.info->name, "mod.dll", mod.description, mod.required});
  return result;
}
}
namespace pg::menu_panel {
bool main_menu_visible() { return fixtureVisible && fixtureButtons; }
bool menu_buttons_shown() { return fixtureMenu && fixtureMenu->alive && fixtureButtons; }
void make_inert(void*) {}
Control control_of(void* root) {
  Control c;
  c.image = il2cpp::Held(root);
  return c;
}
bool press(Control& c) { return clickTape && game::alive(c.image.get()) && game::active_in_hierarchy(c.image.get()); }
bool open(Panel& panel, const std::string&, const std::string&, const std::vector<std::string>&) {
  panel.root = ui_fixture::make_node(nodes, "Popup");
  panel.body = ui_fixture::make_node(nodes, "Status", static_cast<Node*>(panel.root));
  opened = &panel;
  return true;
}
bool is_open(const Panel& panel) { return game::alive(panel.root); }
void close(Panel& panel) {
  if (panel.root) game::destroy(panel.root);
  panel = {};
  opened = nullptr;
}
int tick(Panel&) { return clickButton; }
int row_clicked(const Panel&) { return clickRow; }
bool add_row(Panel& panel, int index, int, const std::string& name, const std::string& description) {
  if (index == unavailableRow) return false;
  auto* hit = ui_fixture::make_node(nodes, "Row", static_cast<Node*>(panel.root));
  auto* nameText = ui_fixture::make_node(nodes, "Name", hit);
  nameText->text = name;
  auto* descriptionText = ui_fixture::make_node(nodes, "Description", hit);
  descriptionText->text = description;
  auto* mark = ui_fixture::make_node(nodes, "Checkmark", hit);
  mark->active = false;
  panel.rows.push_back({hit, nameText, descriptionText, mark});
  return true;
}
void set_row_checked(Panel& panel, size_t index, bool checked) {
  if (index < panel.rows.size()) game::set_active(panel.rows[index].checkmark, checked);
}
void set_text(void* value, const std::string& text) { ui::set_text(value, text.c_str()); }
}

namespace {
Node* make_menu() {
  auto* result = ui_fixture::make_node(nodes, "Menu");
  auto* buttons = ui_fixture::make_node(nodes, "Buttons", result);
  auto* news = ui_fixture::make_node(nodes, "News", buttons);
  news->active = false;
  ui_fixture::make_node(nodes, "Text (TMP)", news);
  return result;
}
void reset() {
  pg::mods_panel::teardown();
  for (void* object : holds) CHECK(!object);
  holds.clear();
  nodes.clear();
  fixtureMenu = make_menu();
  fixtureVisible = true;
  fixtureButtons = true;
  clickTape = false;
  clickButton = clickRow = -1;
  unavailableRow = -1;
  installed = {pg::mods::Mod{}, pg::mods::Mod{}};
  installed[0].info = &baseInfo;
  installed[0].description = "An invented base feature.";
  installed[1].info = &dependentInfo;
  installed[1].description = "An invented extra feature.";
  installed[1].required = "base";
  started = {"base"};
  CHECK(pg::mods_panel::resolve());
}
std::vector<Node*> tapes() {
  std::vector<Node*> result;
  for (const auto& value : nodes)
    if (value->name == "News" && value->alive && value->children.size() == 1 &&
        value->children[0]->text == "Mods") result.push_back(value.get());
  return result;
}
void open_panel() {
  pg::mods_panel::tick(true);
  clickTape = true;
  pg::mods_panel::tick(true);
  clickTape = false;
  CHECK(opened && pg::menu_panel::is_open(*opened));
}
bool tape_shown() { return tapes().size() == 1 && ui_fixture::active_in_hierarchy(tapes()[0]); }
/** The tape is shown and hidden with the game's own menu buttons, and opens only on the focused board. */
void tape_follows_native_buttons() {
  reset();
  fixtureButtons = false;  // Connecting: the game hides Singleplayer and Multiplayer.
  clickTape = true;
  pg::mods_panel::tick(true);
  CHECK(!opened && (tapes().empty() || !ui_fixture::active_in_hierarchy(tapes()[0])));
  clickTape = false;
  fixtureButtons = true;
  pg::mods_panel::tick(true);
  CHECK(tape_shown());
  fixtureVisible = false;  // Panned to the map or the shop: the board is not focused, its buttons stay.
  clickTape = true;
  pg::mods_panel::tick(true);
  CHECK(!opened && tape_shown());
  clickTape = false;
  fixtureButtons = false;
  pg::mods_panel::tick(true);
  CHECK(tapes().size() == 1 && !ui_fixture::active_in_hierarchy(tapes()[0]));
  fixtureButtons = fixtureVisible = true;
  pg::mods_panel::tick(true);
  CHECK(tape_shown());
}
/** The tape kept between frames stays reachable for the GC, and is released with it. */
void tape_is_held() {
  reset();
  pg::mods_panel::tick(true);
  CHECK(tapes().size() == 1 && held(tapes()[0]));
  auto* previous = tapes()[0];
  fixtureMenu = make_menu();  // the scene reloaded
  pg::mods_panel::tick(true);
  CHECK(!held(previous) && tapes().size() == 1 && held(tapes()[0]));
}
void hidden_open_panel_ignores_apply() {
  reset();
  open_panel();
  clickRow = 1;
  pg::mods_panel::tick(true);
  clickRow = -1;
  fixtureVisible = false;
  clickButton = 0;
  pg::mods_panel::tick(true);
  std::vector<std::string> choice;
  CHECK(!pg::mods_panel::take_choice(choice));
  CHECK(!opened);
  CHECK(tape_shown());  // like the game's buttons, still on the board while it is not focused
}
void replacement_menu_replaces_tape() {
  reset();
  pg::mods_panel::tick(true);
  auto* previous = tapes()[0];
  fixtureMenu = make_menu();  // An old menu can remain alive briefly during a scene transition.
  pg::mods_panel::tick(true);
  CHECK(!previous->alive);
  CHECK(tapes().size() == 1);
  if (!tapes().empty()) CHECK(tapes()[0]->parent->parent == fixtureMenu);
}
void missing_menu_removes_tape() {
  reset();
  pg::mods_panel::tick(true);
  auto* previous = tapes()[0];
  fixtureMenu = nullptr;  // between scenes, ConnectToServer.Instance is null
  pg::mods_panel::tick(true);
  CHECK(!previous->alive);
  CHECK(tapes().empty());
  CHECK(!opened);
  fixtureMenu = make_menu();
  fixtureVisible = true;
  pg::mods_panel::tick(true);
  CHECK(tapes().size() == 1);
  if (!tapes().empty()) CHECK(ui_fixture::active_in_hierarchy(tapes()[0]));
}
void close_discards_and_apply_exports_once() {
  reset();
  open_panel();
  clickRow = 1;
  pg::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 1;
  pg::mods_panel::tick(true);
  clickButton = -1;
  std::vector<std::string> choice;
  CHECK(!pg::mods_panel::take_choice(choice));
  open_panel();
  clickButton = 0;
  pg::mods_panel::tick(true);
  CHECK(!pg::mods_panel::take_choice(choice));
  clickButton = -1;
  open_panel();
  clickRow = 1;
  pg::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 0;
  pg::mods_panel::tick(true);
  CHECK(pg::mods_panel::take_choice(choice));
  CHECK(pg::modlist::enabled(choice, "base") && pg::modlist::enabled(choice, "extra"));
  CHECK(!pg::mods_panel::take_choice(choice));
}
void room_blocks_toggle_and_apply() {
  reset();
  open_panel();
  clickRow = 1;
  pg::mods_panel::tick(false);
  clickRow = -1;
  clickButton = 0;
  pg::mods_panel::tick(false);
  std::vector<std::string> choice;
  CHECK(!pg::mods_panel::take_choice(choice));
  clickButton = 0;
  pg::mods_panel::tick(true);
  CHECK(!pg::mods_panel::take_choice(choice));  // The blocked row must not have changed pending.
}
bool checked(size_t row) {
  return opened && row < opened->rows.size() && pg::game::active_self(opened->rows[row].checkmark);
}
void checklist_matches_pending_and_dependencies() {
  reset();
  started.clear();
  open_panel();
  CHECK(opened && opened->rows.size() == 2);
  if (!opened || opened->rows.size() != 2) return;
  CHECK(static_cast<Node*>(opened->rows[0].name)->text == "Base Mod");
  CHECK(static_cast<Node*>(opened->rows[1].name)->text == "Extra Mod");
  CHECK(static_cast<Node*>(opened->rows[0].description)->text == installed[0].description);
  CHECK(static_cast<Node*>(opened->rows[1].description)->text == installed[1].description);
  CHECK(!checked(0) && !checked(1));
  clickRow = 1;
  pg::mods_panel::tick(true);
  clickRow = -1;
  pg::mods_panel::tick(true);
  CHECK(checked(0) && checked(1));  // Selecting Extra visibly selects its requirement as well.
  CHECK(started.empty());          // The checklist shows pending choices before any Apply.
  clickRow = 0;
  pg::mods_panel::tick(true);
  clickRow = -1;
  pg::mods_panel::tick(true);
  CHECK(!checked(0) && !checked(1));  // Removing Base visibly deselects its dependent too.
  clickRow = 1;
  pg::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 1;
  pg::mods_panel::tick(true);
  clickButton = -1;
  open_panel();
  CHECK(!checked(0) && !checked(1));  // Close discards every pending checkbox change.
}
void missing_checkbox_closes_incomplete_list() {
  reset();
  unavailableRow = 1;
  pg::mods_panel::tick(true);
  clickTape = true;
  pg::mods_panel::tick(true);
  clickTape = false;
  CHECK(!opened);
  std::vector<std::string> choice;
  clickButton = 0;
  pg::mods_panel::tick(true);
  CHECK(!pg::mods_panel::take_choice(choice));
}
}
int main() {
  tape_follows_native_buttons();
  tape_is_held();
  hidden_open_panel_ignores_apply();
  replacement_menu_replaces_tape();
  missing_menu_removes_tape();
  close_discards_and_apply_exports_once();
  room_blocks_toggle_and_apply();
  checklist_matches_pending_and_dependencies();
  missing_checkbox_closes_incomplete_list();
  pg::mods_panel::teardown();
  return g_failures ? 1 : 0;
}
