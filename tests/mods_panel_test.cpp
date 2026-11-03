#include <windows.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "text_fixture.h"
#include "Transform.h"
#include "MainMenu.h"
#include "UnityUI.h"
#include "fake_game.h"
#include "game_api.h"
#include "host_game.h"
#include "il2cpp.h"
#include "menu_panel.h"
#include "mods.h"
#include "ui_fixture.h"

namespace {
struct Node {
  std::string name, text;
  Node* parent = nullptr;
  bool alive = true, active = true;
  bool on = false;  // a row's toggle: isOn
  pm::ui::Vec2 at{}, size{1, 1};
  std::vector<Node*> children;
};
std::vector<std::unique_ptr<Node>> nodes;
Node* fixtureMenu = nullptr;
bool fixtureVisible = true, clickTape = false, fixtureCanCopy = true, fixtureCanCopyRows = true;
bool fixtureClickable = true;   // the News copy's button takes the loader's listener
bool rowsInteractable = true;   // what the panel last let the rows' toggles take
bool fixtureButtons = true;  // the game's menu state shows its own buttons (Singleplayer, Multiplayer)
std::vector<void*> holds;    // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
int clickButton = -1, clickRow = -1;
int unavailableRow = -1;
pm::menu_panel::Panel* opened = nullptr;
std::vector<pm::mods::Mod> installed;
std::vector<pm::modlist::Refused> refusedMods;
std::vector<std::string> started;
std::vector<std::string> savedChoice;  // launcher.ini's
std::vector<std::string> openedButtons;  // what the panel last asked for
std::map<std::string, std::string> offReasons;  // what the last start said of each mod it left off
const PmModInfo baseInfo{PMMOD_API_VERSION, "base", "Base Mod", 1, 0};
const PmModInfo dependentInfo{PMMOD_API_VERSION, "extra", "Extra Mod", 2, 0};
Node* clone(Node* original, Node* parent) {
  auto* copy = ui_fixture::make_node(nodes, original->name.c_str(), parent);
  copy->active = original->active;
  for (auto* child : original->children) clone(child, copy);
  return copy;
}
ULONGLONG fixtureNow = 1000;
int newsRefusedLogs = 0;  // the log's lines that the News copy was refused
ULONGLONG WINAPI clock_ms() { return fixtureNow; }
void* menu_instance(const void*) { return fixtureMenu; }
pm::ui::Vec2 anchored(void* value, const void*) { return static_cast<Node*>(value)->at; }
void set_anchored(void* value, pm::ui::Vec2 at, const void*) { static_cast<Node*>(value)->at = at; }
pm::ui::Vec2 size_delta(void* value, const void*) { return static_cast<Node*>(value)->size; }
}

#define GetTickCount64 clock_ms
#include "../host/mods_panel.cpp"
#undef GetTickCount64

namespace il2cpp {
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}
namespace pm::hostgame {
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
void* get_component(void* value, void*) { return value; }
void set_active(void* value, bool active) { static_cast<Node*>(value)->active = active; }
bool active_self(void* value) { return value && static_cast<Node*>(value)->active; }
bool active_in_hierarchy(void* value) { return value && ui_fixture::active_in_hierarchy(static_cast<Node*>(value)); }
void destroy(void* value) { static_cast<Node*>(value)->alive = false; }
}
namespace pm::ui {
void set_text(void* value, const char* text) { static_cast<Node*>(value)->text = text; }
}
namespace pm::log {
void write(const char* format, ...) {
  if (std::strstr(format, "could not take the mod's listener")) ++newsRefusedLogs;
}
}
namespace pm::mods {
const std::vector<Mod>& installed() { return ::installed; }
const std::vector<modlist::Refused>& refused() { return refusedMods; }
std::vector<std::string> started_ids() { return ::started; }
std::vector<std::string> chosen() { return savedChoice; }
std::string off_reason(const std::string& id) {
  const auto it = offReasons.find(id);
  return it == offReasons.end() ? std::string() : it->second;
}
std::vector<modlist::Manifest> manifests() {
  std::vector<modlist::Manifest> result;
  for (const auto& mod : ::installed) result.push_back(mod.manifest);
  return result;
}
}
namespace pm::menu_panel {
bool can_copy() { return fixtureCanCopy; }
bool can_copy_rows() { return fixtureCanCopyRows; }
bool main_menu_visible() { return fixtureVisible && fixtureButtons; }
bool menu_buttons_shown() { return fixtureMenu && fixtureMenu->alive && fixtureButtons; }
void make_inert(void*) {}
// The copy keeps the game's button with the loader's listener: the fixture notes which copy, and fixtureClickable
// says whether its listener could be put on.
bool clickable(Control& c, void* root) {
  if (!fixtureClickable) return false;
  c.button = il2cpp::Held(root);
  return true;
}
// The game clicked the tape (clickTape) while it is in the scene: the listener counted it.
bool clicked(Control& c) { return clickTape && hostgame::alive(c.button.get()) && hostgame::active_in_hierarchy(c.button.get()); }
void release(Control& c) { c.button = nullptr; }
bool open(Panel& panel, const std::string&, const std::string&, const std::vector<std::string>& buttons, OpenFailure*) {
  openedButtons = buttons;
  panel.root = ui_fixture::make_node(nodes, "Popup");
  panel.body = ui_fixture::make_node(nodes, "Status", static_cast<Node*>(panel.root));
  opened = &panel;
  return true;
}
bool is_open(const Panel& panel) { return hostgame::alive(panel.root); }
void close(Panel& panel) {
  if (panel.root) hostgame::destroy(panel.root);
  panel = {};
  opened = nullptr;
}
int tick(Panel&) { return clickButton; }
int row_clicked(const Panel&) { return clickRow; }
// A row's toggle: the fixture's node stands for it, `on` for its isOn.
bool add_row(Panel& panel, int index, int, const std::string& name, const std::string& description, bool locked) {
  if (index == unavailableRow) return false;
  auto* toggle = ui_fixture::make_node(nodes, "Row", static_cast<Node*>(panel.root));
  auto* nameText = ui_fixture::make_node(nodes, "Name", toggle);
  nameText->text = name;
  auto* descriptionText = ui_fixture::make_node(nodes, "Description", toggle);
  descriptionText->text = description;
  panel.rows.push_back({toggle, nameText, descriptionText, false, locked});
  return true;
}
void set_row_checked(Panel& panel, size_t index, bool checked) {
  if (index >= panel.rows.size()) return;
  static_cast<Node*>(panel.rows[index].toggle)->on = checked;
  panel.rows[index].shownOn = checked;
}
void set_rows_interactable(Panel&, bool on) { rowsInteractable = on; }
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
  pm::mods_panel::teardown();
  for (void* object : holds) CHECK(!object);
  holds.clear();
  nodes.clear();
  fixtureMenu = make_menu();
  fixtureVisible = true;
  fixtureButtons = true;
  fixtureCanCopy = fixtureCanCopyRows = fixtureClickable = rowsInteractable = true;
  fixtureNow = 1000;
  newsRefusedLogs = 0;
  clickTape = false;
  clickButton = clickRow = -1;
  unavailableRow = -1;
  installed.assign(2, pm::mods::Mod{});
  installed[0].info = &baseInfo;
  installed[0].manifest.id = "base";
  installed[0].manifest.name = "Base Mod";
  installed[0].manifest.author = "Someone";
  installed[0].manifest.version = "1.2.3";
  installed[0].manifest.description = "An invented base feature.";
  installed[1].info = &dependentInfo;
  installed[1].manifest.id = "extra";
  installed[1].manifest.name = "Extra Mod";  // in the old format: no author, and a version it cannot say
  installed[1].manifest.description = "An invented extra feature.";
  installed[1].manifest.dependencies = {{"base", "", true}};
  offReasons.clear();
  refusedMods.clear();
  started = {"base"};
  savedChoice = {"base"};
  openedButtons.clear();
  pm::fake::reset();
  static int textClass;
  pm::fake::type(pm::map::TextMeshProUGUI, &textClass);
  pm::fake::method(pm::map::ConnectToServer_get_Instance, reinterpret_cast<void*>(&menu_instance));
  pm::fake::method(pm::map::RectTransform_get_anchoredPosition, reinterpret_cast<void*>(&anchored));
  pm::fake::method(pm::map::RectTransform_set_anchoredPosition, reinterpret_cast<void*>(&set_anchored));
  pm::fake::method(pm::map::RectTransform_get_sizeDelta, reinterpret_cast<void*>(&size_delta));
  std::string missing;
  pm::api::bind(pm::fake::host(), missing);
}
std::vector<Node*> tapes() {
  std::vector<Node*> result;
  for (const auto& value : nodes)
    if (value->name == "News" && value->alive && value->children.size() == 1 &&
        value->children[0]->text == "Mods") result.push_back(value.get());
  return result;
}
void open_panel() {
  pm::mods_panel::tick(true);
  clickTape = true;
  pm::mods_panel::tick(true);
  clickTape = false;
  CHECK(opened && pm::menu_panel::is_open(*opened));
}
bool tape_shown() { return tapes().size() == 1 && ui_fixture::active_in_hierarchy(tapes()[0]); }
/** The tape is shown and hidden with the game's own menu buttons, and opens only on the focused board. */
void tape_follows_native_buttons() {
  reset();
  fixtureButtons = false;  // Connecting: the game hides Singleplayer and Multiplayer.
  clickTape = true;
  pm::mods_panel::tick(true);
  CHECK(!opened && (tapes().empty() || !ui_fixture::active_in_hierarchy(tapes()[0])));
  clickTape = false;
  fixtureButtons = true;
  pm::mods_panel::tick(true);
  CHECK(tape_shown());
  fixtureVisible = false;  // Panned to the map or the shop: the board is not focused, its buttons stay.
  clickTape = true;
  pm::mods_panel::tick(true);
  CHECK(!opened && tape_shown());
  clickTape = false;
  fixtureButtons = false;
  pm::mods_panel::tick(true);
  CHECK(tapes().size() == 1 && !ui_fixture::active_in_hierarchy(tapes()[0]));
  fixtureButtons = fixtureVisible = true;
  pm::mods_panel::tick(true);
  CHECK(tape_shown());
}
/** The tape kept between frames stays reachable for the GC, and is released with it. */
void tape_is_held() {
  reset();
  pm::mods_panel::tick(true);
  CHECK(tapes().size() == 1 && held(tapes()[0]));
  auto* previous = tapes()[0];
  fixtureMenu = make_menu();  // the scene reloaded
  pm::mods_panel::tick(true);
  CHECK(!held(previous) && tapes().size() == 1 && held(tapes()[0]));
}
void hidden_open_panel_ignores_apply() {
  reset();
  open_panel();
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  fixtureVisible = false;
  clickButton = 0;
  pm::mods_panel::tick(true);
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
  CHECK(!opened);
  CHECK(tape_shown());  // like the game's buttons, still on the board while it is not focused
}
void replacement_menu_replaces_tape() {
  reset();
  pm::mods_panel::tick(true);
  auto* previous = tapes()[0];
  fixtureMenu = make_menu();  // An old menu can remain alive briefly during a scene transition.
  pm::mods_panel::tick(true);
  CHECK(!previous->alive);
  CHECK(tapes().size() == 1);
  if (!tapes().empty()) CHECK(tapes()[0]->parent->parent == fixtureMenu);
}
void missing_menu_removes_tape() {
  reset();
  pm::mods_panel::tick(true);
  auto* previous = tapes()[0];
  fixtureMenu = nullptr;  // between scenes, ConnectToServer.Instance is null
  pm::mods_panel::tick(true);
  CHECK(!previous->alive);
  CHECK(tapes().empty());
  CHECK(!opened);
  fixtureMenu = make_menu();
  fixtureVisible = true;
  pm::mods_panel::tick(true);
  CHECK(tapes().size() == 1);
  if (!tapes().empty()) CHECK(ui_fixture::active_in_hierarchy(tapes()[0]));
}
void close_discards_and_apply_exports_once() {
  reset();
  open_panel();
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 1;
  pm::mods_panel::tick(true);
  clickButton = -1;
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
  open_panel();
  clickButton = 0;
  pm::mods_panel::tick(true);
  CHECK(!pm::mods_panel::take_choice(choice));
  clickButton = -1;
  open_panel();
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 0;
  pm::mods_panel::tick(true);
  CHECK(pm::mods_panel::take_choice(choice));
  CHECK(pm::modlist::enabled(choice, "base") && pm::modlist::enabled(choice, "extra"));
  CHECK(!pm::mods_panel::take_choice(choice));
}
bool checked(size_t row) {
  return opened && row < opened->rows.size() && static_cast<Node*>(opened->rows[row].toggle.get())->on;
}
void room_blocks_toggle_and_apply() {
  reset();
  open_panel();
  clickRow = 1;
  if (opened && opened->rows.size() == 2) static_cast<Node*>(opened->rows[1].toggle.get())->on = true;  // the game flips it
  pm::mods_panel::tick(false);
  clickRow = -1;
  CHECK(!rowsInteractable);  // in a room the game greys the checkboxes
  CHECK(!checked(1));        // and one the game flipped anyway is set back
  clickRow = 1;              // again, with nothing else changed since the last frame
  if (opened && opened->rows.size() == 2) static_cast<Node*>(opened->rows[1].toggle.get())->on = true;
  pm::mods_panel::tick(false);
  clickRow = -1;
  CHECK(!checked(1));
  clickButton = 0;
  pm::mods_panel::tick(false);
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
  clickButton = 0;
  pm::mods_panel::tick(true);
  CHECK(!pm::mods_panel::take_choice(choice));  // The blocked row must not have changed pending.
}
void checklist_matches_pending_and_dependencies() {
  reset();
  started.clear();
  savedChoice.clear();
  open_panel();
  CHECK(opened && opened->rows.size() == 2);
  if (!opened || opened->rows.size() != 2) return;
  CHECK(static_cast<Node*>(opened->rows[0].name)->text == "Base Mod 1.2.3");
  CHECK(static_cast<Node*>(opened->rows[1].name)->text == "Extra Mod (version unknown)");
  CHECK(static_cast<Node*>(opened->rows[0].description)->text == "By Someone. An invented base feature.");
  CHECK(static_cast<Node*>(opened->rows[1].description)->text == "An invented extra feature.");
  CHECK(!checked(0) && !checked(1));
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  pm::mods_panel::tick(true);
  CHECK(checked(0) && checked(1));  // Selecting Extra visibly selects its requirement as well.
  CHECK(started.empty());          // The checklist shows pending choices before any Apply.
  clickRow = 0;
  pm::mods_panel::tick(true);
  clickRow = -1;
  pm::mods_panel::tick(true);
  CHECK(!checked(0) && !checked(1));  // Removing Base visibly deselects its dependent too.
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 1;
  pm::mods_panel::tick(true);
  clickButton = -1;
  open_panel();
  CHECK(!checked(0) && !checked(1));  // Close discards every pending checkbox change.
}
// A mod that stays off has a row saying why, after the loaded mods', which never takes a click.
void refused_mod_has_a_row_saying_why() {
  reset();
  refusedMods = {{"broken", "Its version is not MAJOR.MINOR.PATCH."}};
  open_panel();
  CHECK(opened && opened->rows.size() == 3);
  if (!opened || opened->rows.size() != 3) return;
  CHECK(static_cast<Node*>(opened->rows[2].name)->text == "broken");
  CHECK(static_cast<Node*>(opened->rows[2].description)->text == "Off. Its version is not MAJOR.MINOR.PATCH.");
  CHECK(opened->rows[2].locked && !opened->rows[0].locked && !opened->rows[1].locked);
  CHECK(!checked(2));
  // The choice is about the loaded mods: its row changes nothing.
  clickRow = 2;
  pm::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
}
// A loaded mod the last start left off for a dependency says why on its row, before what it is; an Apply that starts it
// takes the reason away while the panel stays open.
void a_row_says_why_its_mod_is_off() {
  reset();
  offReasons["extra"] = "Needs Base Mod, which did not start";
  open_panel();
  CHECK(opened && opened->rows.size() == 2);
  if (!opened || opened->rows.size() != 2) return;
  CHECK(static_cast<Node*>(opened->rows[1].description)->text == "Needs Base Mod, which did not start. An invented extra feature.");
  CHECK(static_cast<Node*>(opened->rows[0].description)->text == "By Someone. An invented base feature.");
  CHECK(!opened->rows[1].locked);  // it can still be switched; the reason says what else it needs
  offReasons.clear();
  started = {"base", "extra"};
  pm::mods_panel::applied(true);
  pm::mods_panel::tick(true);
  CHECK(static_cast<Node*>(opened->rows[1].description)->text == "An invented extra feature.");
}

// A game update leaves a chosen mod off, and whatever requires it. The player opens the panel and changes something else:
// Apply must not forget them, or they stay off once a build that can run them arrives. The rows start from the saved
// choice, and so does what Apply saves. Nothing changed, Apply has nothing to do, however little of the choice runs.
void apply_keeps_a_chosen_mod_that_could_not_run() {
  reset();
  static const PmModInfo tellsInfo{PMMOD_API_VERSION, "tells", "Tells", 3, 0};
  installed.resize(3);
  installed[2].info = &tellsInfo;
  installed[2].manifest.id = "tells";
  installed[2].manifest.name = "Tells";
  installed[2].manifest.description = "Tells.";
  savedChoice = {"base", "extra", "tells", "not-installed-here"};
  started = {"tells"};  // base and extra could not run on this build
  offReasons["base"] = "This game build lacks Some.Entry";
  offReasons["extra"] = "Needs Base Mod, which did not start";
  open_panel();
  CHECK(checked(0) && checked(1) && checked(2));  // chosen, whether or not it runs
  CHECK(static_cast<Node*>(opened->body.get())->text == "Click a mod to switch it on or off.");
  std::vector<std::string> choice;
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  CHECK(!pm::mods_panel::take_choice(choice) && !opened);  // nothing changed

  open_panel();
  clickRow = 2;  // Tells off
  pm::mods_panel::tick(true);
  clickRow = -1;
  CHECK(static_cast<Node*>(opened->body.get())->text == "Apply to restart the mods with your choice. The game keeps running.");
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  CHECK(pm::mods_panel::take_choice(choice));
  CHECK(choice == (std::vector<std::string>{"base", "extra", "not-installed-here"}));

  // A choice that could not be saved is said to be lost at the next start, not done.
  pm::mods_panel::applied(true, false);
  pm::mods_panel::tick(true);
  CHECK(static_cast<Node*>(opened->body)->text == "The mods restarted, but your choice couldn't be saved, so the next start uses the old one.");

  // Applied, the panel holds that choice, not what started from it.
  started.clear();
  savedChoice = choice;
  pm::mods_panel::applied(false);
  pm::mods_panel::tick(true);
  CHECK(checked(0) && checked(1) && !checked(2));
  clickRow = 2;  // Tells back on
  pm::mods_panel::tick(true);
  clickRow = -1;
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  CHECK(pm::mods_panel::take_choice(choice));
  CHECK(pm::modlist::enabled(choice, "base") && pm::modlist::enabled(choice, "extra") && pm::modlist::enabled(choice, "tells"));
}

// In a room nothing changes, and the rows say what runs there. Out of it, with the panel still open, they show the saved
// choice again, so Apply cannot save what a room's rules left running.
void leaving_a_room_with_the_panel_open_reads_the_choice_again() {
  reset();
  savedChoice = {"base", "extra"};
  started = {"base"};  // the room's rules switched extra off
  pm::mods_panel::tick(false);
  clickTape = true;
  pm::mods_panel::tick(false);
  clickTape = false;
  CHECK(opened && checked(0) && !checked(1));
  pm::mods_panel::tick(true);
  CHECK(checked(0) && checked(1));
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
}

// Nobody changes the list from inside a room, the host neither: a click and an Apply there change nothing and hand the host
// no choice, and the panel says to leave first. Out of the room the same click counts.
void panel_changes_wait_until_out_of_the_room() {
  reset();
  savedChoice = {"base", "extra"};
  started = {"base", "extra"};
  pm::mods_panel::tick(false);
  clickTape = true;
  pm::mods_panel::tick(false);
  clickTape = false;
  CHECK(opened && static_cast<Node*>(opened->body)->text == "Leave the lobby to change mods.");
  CHECK(openedButtons == std::vector<std::string>{"Close"} || !rowsInteractable);
  clickRow = 1;
  pm::mods_panel::tick(false);
  clickRow = -1;
  CHECK(checked(0) && checked(1));
  clickButton = 0;
  pm::mods_panel::tick(false);
  clickButton = -1;
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
  if (!opened) {
    clickTape = true;
    pm::mods_panel::tick(true);
    clickTape = false;
  }
  pm::mods_panel::tick(true);
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  CHECK(checked(0) && !checked(1));
  clickButton = 0;
  pm::mods_panel::tick(true);
  clickButton = -1;
  CHECK(pm::mods_panel::take_choice(choice) && choice == std::vector<std::string>{"base"});
}

// The notice could not be shown, so no mod runs, and the player did not choose that: the panel names why, where this
// build can make it. Each mod is listed, off, and nothing can be switched on from here.
void a_panel_names_why_no_mod_runs() {
  reset();
  savedChoice = {"base"};
  started.clear();
  const std::string cause = "The notice can't be shown on this game build, so the mods stay off. mods.log says why.";
  pm::mods_panel::tick_off(cause);
  CHECK(tapes().size() == 1);
  clickTape = true;
  pm::mods_panel::tick_off(cause);
  clickTape = false;
  CHECK(opened && opened->rows.size() == 2);
  if (!opened || opened->rows.size() != 2) return;
  CHECK(openedButtons == std::vector<std::string>{"Close"});
  CHECK(static_cast<Node*>(opened->body.get())->text == cause);
  CHECK(!checked(0) && !checked(1) && !rowsInteractable);
  clickRow = 0;
  pm::mods_panel::tick_off(cause);
  clickRow = -1;
  CHECK(!checked(0));
  std::vector<std::string> choice;
  CHECK(!pm::mods_panel::take_choice(choice));
  clickButton = 0;
  pm::mods_panel::tick_off(cause);
  clickButton = -1;
  CHECK(!opened);
  CHECK(!pm::mods_panel::take_choice(choice));
  // A build that cannot make the panel shows nothing: the log and the player guide are all there is.
  reset();
  fixtureCanCopy = false;
  clickTape = true;
  pm::mods_panel::tick_off(cause);
  clickTape = false;
  CHECK(tapes().empty() && !opened);
}

// An optional dependency switches nothing, either way.
void optional_dependencies_switch_nothing() {
  reset();
  installed[1].manifest.dependencies = {{"base", "", false}};
  started.clear();
  savedChoice.clear();
  open_panel();
  clickRow = 1;
  pm::mods_panel::tick(true);
  clickRow = -1;
  pm::mods_panel::tick(true);
  CHECK(!checked(0) && checked(1));  // on alone
  for (bool baseOn : {true, false}) {
    clickRow = 0;
    pm::mods_panel::tick(true);
    clickRow = -1;
    pm::mods_panel::tick(true);
    CHECK(checked(0) == baseOn && checked(1));  // its user stays on as Base goes on and off
  }
}

void missing_checkbox_closes_incomplete_list() {
  reset();
  unavailableRow = 1;
  pm::mods_panel::tick(true);
  clickTape = true;
  pm::mods_panel::tick(true);
  clickTape = false;
  CHECK(!opened);
  std::vector<std::string> choice;
  clickButton = 0;
  pm::mods_panel::tick(true);
  CHECK(!pm::mods_panel::take_choice(choice));
}
}
namespace pm::host {
void* find_child(void* value, const char* path) { return pm::hostgame::find_child(value, path); }
}  // namespace pm::host

namespace {
// Without a member the tape needs, or a copy that cannot be made safe, no tape is made and the launcher still chooses.
void no_tape_without_what_it_needs() {
  reset();
  pm::mods_panel::tick(true);
  CHECK(tapes().size() == 1);  // what the cases below lack is all that keeps the tape away
  pm::mods_panel::teardown();
  reset();
  fixtureCanCopy = false;
  pm::mods_panel::tick(true);
  CHECK(tapes().empty());
  // Rows that cannot be made would leave the panel without its list: no tape.
  reset();
  fixtureCanCopyRows = false;
  pm::mods_panel::tick(true);
  CHECK(tapes().empty());
  // A copy of News whose button cannot take the loader's listener would start the training: it is destroyed.
  reset();
  fixtureClickable = false;
  pm::mods_panel::tick(true);
  CHECK(tapes().empty());
  int liveNews = 0, copies = 0;
  for (const auto& node : nodes)
    if (node->name == "News") {
      ++copies;
      liveNews += node->alive;
    }
  CHECK(copies == 2 && liveNews == 1);  // the copy was made, and only the game's own News is left
  // Refused once, it is logged once and not tried again for this menu, however long the menu is shown.
  CHECK(newsRefusedLogs == 1);
  for (int i = 0; i < 5; ++i) {
    fixtureNow += 2000;
    pm::mods_panel::tick(true);
  }
  CHECK(newsRefusedLogs == 1);
  copies = 0;
  for (const auto& node : nodes) copies += node->name == "News";
  CHECK(copies == 2);
  // Another menu instance may be another build's scene state: it is tried, and once it takes the listener a tape is made.
  fixtureMenu = make_menu();
  fixtureClickable = true;
  fixtureNow += 2000;
  pm::mods_panel::tick(true);
  CHECK(tapes().size() == 1);
  fixtureCanCopy = true;
  for (const pm::sdk::EntryDesc* d : {&pm::map::ConnectToServer_get_Instance, &pm::map::TextMeshProUGUI,
                                      &pm::map::RectTransform_get_anchoredPosition, &pm::map::RectTransform_set_anchoredPosition,
                                      &pm::map::RectTransform_get_sizeDelta}) {
    reset();
    pm::fake::forget(*d);
    std::string missing;
    pm::api::bind(pm::fake::host(), missing);
    pm::mods_panel::tick(true);
    CHECK(tapes().empty());
  }
}
}  // namespace

int main() {
  use_text({"i18n/default.json"});
  no_tape_without_what_it_needs();
  tape_follows_native_buttons();
  tape_is_held();
  hidden_open_panel_ignores_apply();
  replacement_menu_replaces_tape();
  missing_menu_removes_tape();
  close_discards_and_apply_exports_once();
  room_blocks_toggle_and_apply();
  checklist_matches_pending_and_dependencies();
  refused_mod_has_a_row_saying_why();
  a_row_says_why_its_mod_is_off();
  apply_keeps_a_chosen_mod_that_could_not_run();
  leaving_a_room_with_the_panel_open_reads_the_choice_again();
  panel_changes_wait_until_out_of_the_room();
  a_panel_names_why_no_mod_runs();
  optional_dependencies_switch_nothing();
  missing_checkbox_closes_incomplete_list();
  pm::mods_panel::teardown();
  return g_failures ? 1 : 0;
}
